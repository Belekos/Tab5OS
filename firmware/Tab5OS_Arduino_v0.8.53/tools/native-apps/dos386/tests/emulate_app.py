#!/usr/bin/env python3
"""Execute the exact modern DOS x86 RV32 application with mocked Tab5 ABI7.

The launch scenario runs the real app, runtime, PC core and FreeDOS image.
COM/MZ success is read from rendered RGB565 pixels, never from guest RAM or
test-only logging. No hardware behavior or general DOS compatibility is asserted.
"""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import zlib

from frame_text import decode, _font
from fixtures import create

HERE = Path(__file__).resolve().parent
DOS = HERE.parent
NATIVE = DOS.parent
SKETCH = NATIVE.parent.parent
REPO = SKETCH.parent.parent
DOOM_RUNNER = NATIVE / "doom/tests/emulate_startup.py"
spec = importlib.util.spec_from_file_location("tab5_doom_unicorn", DOOM_RUNNER)
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)

API7_OFFSETS = dict(common.API_OFFSETS, panel_open=92, panel_poll=96,
                   panel_write=100, panel_state=104, network=108, translate=112,
                   canvas_begin=116, input_poll=120, write_begin=124,
                   write_chunk=128, write_finish=132, keyboard_state=136,
                   directory_open=140, directory_next=144)
common.API_OFFSETS = API7_OFFSETS
BOOT_SHA256 = "8c25e36d0a72a77a8c3541ba12f4dd5f4b2dbb12c15e660cc18d7da545020688"
DRIVERS_SHA256 = "0bb65a1c38cb2a94e4f7f7a7d899c905f4c2f7443adf67b413f2d82c34bd2030"
DPMI_SHA256 = "cd093ceb8f22d840f01047c55b1c8e9e2793f02b9453e11da180a73feec0b1e9"
MOUNT = "/DOS/DOS sample folder with spaces"
MOUSE_MARKERS = ("TAB5 MOUSE READY", "TAB5 MOUSE MOVE OK", "TAB5 MOUSE LEFT OK", "TAB5 MOUSE RIGHT OK")
MARKERS = ("TAB5OS DOS READY", "TAB5 MZ EXE OK", "TAB5 ADJACENT DATA OK", "TAB5 COM OK",
           "TAB5 386 DOS OK", "TAB5 486 DOS OK", "TAB5 PM32 RETURN OK", "TAB5 DPMI32 OK", "TAB5 STORAGE COW OK", "TAB5 VGA13 RETURN OK",
           *MOUSE_MARKERS, "TAB5 AUDIO READY", "TAB5 INPUT AUDIO OK")
COMMAND_SEQUENCE = (("hello.com\r", "TAB5 COM OK"),
                    ("test386.com\r", "TAB5 386 DOS OK"),
                    ("test486.exe\r", "TAB5 486 DOS OK"),
                    ("testpm.com\r", "TAB5 PM32 RETURN OK"),
                    ("testdpmi.exe\r", "TAB5 DPMI32 OK"),
                    ("video.com\r", "TAB5 VGA13 RETURN OK"),
                    ("store.com\r", "TAB5 STORAGE COW OK"),
                    ("mouse.com\r", "TAB5 INPUT AUDIO OK"))
KEYBOARD_COMMAND = "".join(command for command, marker in COMMAND_SEQUENCE)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def png_frame(frame):
    """Losslessly preserve the last rendered test frame for CI diagnosis."""
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    pixels = memoryview(frame).cast("H")
    scan = bytearray()
    for y in range(480):
        scan.append(0)
        for color in pixels[y * 800:(y + 1) * 800]:
            r, g, b = color >> 11, (color >> 5) & 63, color & 31
            scan.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 800, 480, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(scan)) + chunk(b"IEND", b""))


class Application(common.Startup):
    def __init__(self, elf, image, drivers, bios, fixtures, scenario, frame_limit):
        super().__init__(elf, image, frame_limit)
        # Reuse the Doom harness's RV32 loader and bounded callback trampoline;
        # the first stub now resolves only the actual ABI7 import.
        self.callback_names[0] = "tab5_app_api_v7"
        self.callbacks[common.STUB_BASE] = "tab5_app_api_v7"
        self.scenario = scenario
        self.virtual_files = {MOUNT + "/" + name: data for name, data in fixtures.items()}
        if scenario != "missing-boot":
            self.virtual_files["/DOS/FREEDOS.IMG"] = image
        self.virtual_files["/System/DOS/DRIVERS.IMG"] = drivers
        for name, data in bios.items():
            if scenario != "missing-bios" or name != "bios.bin":
                self.virtual_files["/System/DOS386/" + name] = data
        self.virtual_files_before = {name: sha(data) for name, data in self.virtual_files.items()}
        self.saves = {}
        self.saved_calls = []
        self.saved_payload_verified = False
        self.directory_attempts = []
        self.directory_names = []
        self.peak_open_handles = 0
        self.peak_heap_bytes = 0
        self.marker_frames = {}
        self.last_screen = ""
        self.last_frame = None
        self.ocr_cache = {}
        self.keyboard_step = 0
        self.keyboard_started = False
        self.keyboard_complete = False
        self.keyboard_reports = 0
        self.keyboard_marker_waits = 0
        self.video_pixels_verified = False
        self.video_continue_polls = 0
        self.key_events = []
        self.duplicate_text_events = 0
        self.exit_tap_sent = False
        self.mouse_steps = []
        self.audio_probe_stage = 0
        self.audio_probe_blocks = 0
        self.muted_guest_frames = 0
        self.mute_call_count = 0
        self.muted_nonzero_seen = False
        self.audio_nonzero_seen = False
        self.audio_muted_seen = False
        self.audio_resumed_seen = False
        self.audio_continue_polls = 0
        self.volume_taps = []
        self.input_events = []
        self.minimum_sp = common.STACK_BASE + elf.stack_bytes - 16
        self.uc.mem_write(common.STACK_BASE, b"\xa5" * elf.stack_bytes)
        language = common.API_BASE + 0x3000
        self.uc.mem_write(common.API_BASE, struct.pack("<III", 7, 148, language))
        self.uc.mem_write(language, b"en\0")
        self.uc.mem_write(common.API_BASE + 0x3200, b"/Applications/DOS x86.app.elf\0")
        self.uc.mem_write(common.API_BASE + 0x3300, (MOUNT + "/Hello long name.EXE\0").encode())

    def resolve(self, name):
        if name != "tab5_app_api_v7":
            raise ValueError("Unexpected DOS import: " + name)
        return common.STUB_BASE

    def callback(self, uc, address, size, user):
        sp = self.uc.reg_read(self.registers.UC_RISCV_REG_SP)
        self.minimum_sp = min(self.minimum_sp, sp)
        if not common.STACK_BASE <= sp < common.STACK_BASE + self.loader["stackBytes"]:
            self.stop("stack-outside-budget", hex(sp))
            return
        super().callback(uc, address, size, user)

    def handle(self, value):
        if len(self.files) >= 8:
            return -1
        result = self.next_handle
        self.next_handle += 1
        self.files[result] = value
        self.peak_open_handles = max(self.peak_open_handles, len(self.files))
        return result

    def expect_struct(self, pointer, size):
        supplied = struct.unpack("<I", self.uc.mem_read(pointer, 4))[0]
        if supplied < size:
            raise ValueError("Undersized ABI structure: " + str(supplied))

    def keyboard(self):
        if self.audio_probe_stage == 1 and self.volume_taps == [1]:
            # keyboard_state occurs once before each bounded CPU run slice.
            # Muting drains already buffered host audio with a bounded silent
            # tail. Every newly submitted sample must be zero throughout mute.
            self.muted_guest_frames += 1
            if self.muted_guest_frames >= 8:
                self.audio_muted_seen = not self.muted_nonzero_seen
                self.audio_probe_stage = 2
        keys = bytearray(32)
        if not self.keyboard_started and all(marker in self.marker_frames for marker in MARKERS[1:3]):
            self.keyboard_started = True
        connected = int(self.keyboard_started and not self.keyboard_complete)
        if connected:
            command = KEYBOARD_COMMAND
            index, phase = divmod(self.keyboard_step, 4)
            video_command_end = KEYBOARD_COMMAND.index("video.com\r") + len("video.com\r")
            if self.video_pixels_verified and index >= video_command_end and self.video_continue_polls < 4:
                if self.video_continue_polls < 2:
                    keys[44 // 8] |= 1 << (44 % 8)  # Space
                self.video_continue_polls += 1
                self.keyboard_reports += 1
                return struct.pack("<III32s", 44, 0, connected, bytes(keys))
            if index < len(command):
                # Only type the next command after the preceding program has
                # rendered success. This bounds BIOS typeahead and avoids
                # treating a program's pending input as a shell command.
                completed_chars = 0
                for text, marker in COMMAND_SEQUENCE:
                    completed_chars += len(text)
                    if index >= completed_chars and marker not in self.marker_frames:
                        self.keyboard_marker_waits += 1
                        self.keyboard_reports += 1
                        return struct.pack("<III32s", 44, 0, connected, bytes(keys))
                char = command[index]
                usage = (ord(char) - ord('a') + 4 if 'a' <= char <= 'z'
                         else ord(char) - ord('1') + 30 if '1' <= char <= '9'
                         else 39 if char == '0' else 55 if char == '.' else 40)
                if phase < 2:
                    keys[usage // 8] |= 1 << (usage % 8)
                if phase == 0:
                    self.key_events.append({"character": repr(char), "usage": usage, "frame": self.frames})
                    # The OS can also queue a text event for this physical key.
                    # The app must not replay it on top of the held-key snapshot.
                    self.input_events.append((4, 0, 0, 0, 0, char.encode()))
                self.keyboard_step += 1
            else:
                self.keyboard_complete = True
                connected = 0
        elif self.audio_probe_stage == 3 and self.audio_continue_polls < 4:
            connected = 1
            if self.audio_continue_polls < 2:
                keys[40 // 8] |= 1 << (40 % 8)
            self.audio_continue_polls += 1
        self.keyboard_reports += 1
        return struct.pack("<III32s", 44, 0, connected, bytes(keys))

    def service(self, name, a):
        if name == "tab5_app_api_v7":
            return common.API_BASE
        if name == "allocate":
            if self.scenario == "allocation-failure":
                return 0
            if len(self.allocations) >= 32:
                return 0
            result = super().service(name, a)
            self.peak_heap_bytes = max(self.peak_heap_bytes, sum(self.allocations.values()))
            return result
        if name == "should_stop":
            return 0
        if name == "sleep_ms":
            self.clock_ms += a[0]
            return 0
        if name == "open_read":
            path = self.string(a[0])
            self.open_attempts.append(path)
            data = self.virtual_files.get(path, self.saves.get(path))
            return -1 if data is None else self.handle({"kind": "file", "path": path, "data": data, "offset": 0})
        if name == "file_size":
            handle = self.files.get(a[0])
            return len(handle["data"]) if handle and handle["kind"] == "file" else 0
        if name == "read":
            handle = self.files.get(a[0])
            if not handle or handle["kind"] != "file" or a[2] > 4096:
                return -1
            data = handle["data"][handle["offset"]:handle["offset"] + a[2]]
            if data:
                self.uc.mem_write(a[1], data)
            handle["offset"] += len(data)
            return len(data)
        if name == "seek":
            handle = self.files.get(a[0])
            if not handle or handle["kind"] != "file" or a[1] > len(handle["data"]):
                return -1
            handle["offset"] = a[1]
            return 0
        if name == "close":
            return super().service(name, a)
        if name == "directory_open":
            path = self.string(a[0])
            self.directory_attempts.append(path)
            if path != MOUNT:
                return -1
            entries = [(key[len(MOUNT) + 1:], len(value)) for key, value in sorted(self.virtual_files.items())
                       if key.startswith(MOUNT + "/") and "/" not in key[len(MOUNT) + 1:]]
            return self.handle({"kind": "directory", "entries": entries, "offset": 0})
        if name == "directory_next":
            self.expect_struct(a[1], 268)
            handle = self.files.get(a[0])
            if not handle or handle["kind"] != "directory":
                return -1
            if handle["offset"] == len(handle["entries"]):
                return 0
            filename, size = handle["entries"][handle["offset"]]
            handle["offset"] += 1
            self.directory_names.append(filename)
            self.uc.mem_write(a[1], struct.pack("<III256s", 268, 0, size, filename.encode() + b"\0"))
            return 1
        if name == "canvas_begin":
            if self.video_size is not None or a[:2] != [800 | 0x8000, 480]:
                return -1
            self.video_size = [800, 480]
            return 0
        if name == "video_present":
            if self.video_size != [800, 480] or a[1] != 800 * 480:
                raise ValueError("Unexpected DOS canvas dimensions")
            frame = bytes(self.uc.mem_read(a[0], a[1] * 2))
            self.last_frame = frame
            self.last_frame_hash = sha(frame)
            self.unique_frames.add(self.last_frame_hash)
            self.nonzero_frames += int(any(frame))
            self.frames += 1
            if len(self.frame_hashes) < 8:
                self.frame_hashes.append(self.last_frame_hash)
            if self.last_frame_hash not in self.ocr_cache:
                if len(self.ocr_cache) >= 256:
                    self.ocr_cache.clear()
                self.ocr_cache[self.last_frame_hash] = decode(frame)
            self.last_screen = self.ocr_cache[self.last_frame_hash]
            # The guest creates this exact VGA13 test image. A full 624x468
            # picture verifies actual scanout, palette conversion, and 4:3
            # layout; no guest-memory inspection or test-only app hook.
            if not self.video_pixels_verified:
                view = memoryview(frame).cast("H")
                samples = ((0, 0, 0xf800), (312, 100, 0xf800), (623, 233, 0xf800),
                           (0, 234, 0x07e0), (312, 350, 0x07e0), (623, 467, 0x07e0),
                           (312, 468, 0))
                self.video_pixels_verified = all(view[y * 800 + x] == color for x, y, color in samples)
            if ("Bad command or filename" in self.last_screen or "TAB5 MOUSE DRIVER MISSING" in self.last_screen
                    or "DOS FAIL" in self.last_screen or "TAB5 DPMI32 FAIL" in self.last_screen):
                self.stop("fixture-command-failed", self.last_screen)
            for marker in MARKERS:
                if marker in self.last_screen:
                    self.marker_frames.setdefault(marker, self.frames)
            if self.frames >= self.target_frames:
                self.stop("frame-limit")
            return 0
        if name == "keyboard_state":
            self.expect_struct(a[0], 44)
            self.uc.mem_write(a[0], self.keyboard())
            return 1
        if name == "input_poll":
            self.expect_struct(a[0], 36)
            steps = (("TAB5 MOUSE READY", 40, 0, 0),
                     ("TAB5 MOUSE MOVE OK", 0, 0, 1),
                     ("TAB5 MOUSE LEFT OK", 0, 0, 2))
            index = len(self.mouse_steps)
            if index < len(steps) and steps[index][0] in self.marker_frames:
                marker, dx, dy, buttons = steps[index]
                self.input_events.extend(((7, 400, 200, buttons, 0x80000000, b""),
                                          (8, dx, dy, buttons, 0x80000000, b"")))
                self.mouse_steps.append(marker)
            if self.audio_probe_stage in (1, 2) and len(self.volume_taps) < self.audio_probe_stage:
                # The center volume button toggles mute/restore.
                self.input_events.extend(((1, 416, 426, 0, 0, b""), (3, 416, 426, 0, 0, b"")))
                self.volume_taps.append(self.audio_probe_stage)
                self.audio_probe_blocks = 0
                if self.audio_probe_stage == 1:
                    self.mute_call_count = self.calls["audio_pcm"]
            if (not self.exit_tap_sent and self.keyboard_complete
                    and all(marker in self.marker_frames for marker in MARKERS)):
                # The native host's Quit button requests the same saved exit.
                self.input_events.append((6, 0, 0, 0, 0, b""))
                self.exit_tap_sent = True
            if not self.input_events:
                return 0
            kind, x, y, key, modifiers, text = self.input_events.pop(0)
            self.duplicate_text_events += int(kind == 4)
            self.uc.mem_write(a[0], struct.pack("<IIiiII12s", 36, kind, x, y, key, modifiers, text))
            return 1
        if name == "audio_pcm":
            if a[1] > 2048 or a[2] != 44100:
                raise ValueError("Unexpected DOS audio format")
            raw = bytes(self.uc.mem_read(a[0], a[1] * 4))
            if self.audio_probe_stage == 1 and self.volume_taps == [1]:
                self.muted_nonzero_seen |= any(raw)
            if "TAB5 AUDIO READY" in self.marker_frames and a[1]:
                nonzero = any(raw)
                self.audio_probe_blocks += 1
                if self.audio_probe_stage == 0 and nonzero:
                    self.audio_nonzero_seen = True
                    self.audio_probe_stage = 1
                elif self.audio_probe_stage == 2 and len(self.volume_taps) == 2 and self.audio_probe_blocks >= 3 and nonzero:
                    self.audio_resumed_seen = True
                    self.audio_probe_stage = 3
            return 0
        if name == "save":
            path = self.string(a[0])
            if not path.startswith("/Saves/DOS/") or not path.endswith(".state") or a[2] > 2 * 1024 * 1024:
                return -1
            data = bytes(self.uc.mem_read(a[1], a[2]))
            if len(data) < 20:
                raise ValueError("Truncated DOS sector overlay")
            magic, version, identity, count, crc = struct.unpack_from("<5I", data)
            if (magic != 0x444f5337 or version != 1 or count > 3072
                    or len(data) != 20 + count * 520 or zlib.crc32(data[20:]) != crc):
                raise ValueError("Invalid saved DOS sector overlay")
            seen = set()
            for offset in range(20, len(data), 520):
                drive, lba = struct.unpack_from("<II", data, offset)
                if drive != 0 or lba >= len(self.virtual_files["/DOS/FREEDOS.IMG"]) // 512 or (drive, lba) in seen:
                    raise ValueError("Invalid or duplicate saved DOS sector")
                seen.add((drive, lba))
                self.saved_payload_verified |= b"TAB5COW!" in data[offset + 8:offset + 520]
            self.saves[path] = data
            self.saved_calls.append({"path": path, "bytes": len(data), "sha256": sha(data),
                                     "identity": identity, "sectors": count, "crcVerified": True})
            return 0
        if name in ("write_begin", "write_chunk", "write_finish", "network", "hardware"):
            raise ValueError("DOS called an unexpected OS write/network service: " + name)
        return super().service(name, a)

    def run(self, instruction_limit, timeout_seconds):
        report = super().run(instruction_limit, timeout_seconds)
        logs = report["logs"]
        returned = report["stopReason"] == "application-returned"
        checks = {"abi7_only": self.loader["imports"] == ["tab5_app_api_v7"],
                  "returned_to_os": returned, "no_callback_or_memory_error": self.error is None,
                  "base_files_unchanged": self.virtual_files_before == {name: sha(data) for name, data in self.virtual_files.items()},
                  "heap_within_budget": self.peak_heap_bytes <= common.HEAP_LIMIT,
                  "file_handles_within_budget": self.peak_open_handles <= 8}
        if self.scenario == "launch":
            checks.update({"return_code_zero": self.return_code == 0,
                           "boot_ready_logged": "FreeDOS ready. C: contains the selected SD folder." in logs,
                           "rendered_boot_com_mz_data": all(marker in self.marker_frames for marker in MARKERS),
                           "physical_keyboard_completed": self.keyboard_complete and len(self.key_events) == len(KEYBOARD_COMMAND),
                           "queued_text_did_not_duplicate_input": self.duplicate_text_events == len(KEYBOARD_COMMAND),
                           "driver_disk_opened": "/System/DOS/DRIVERS.IMG" in self.open_attempts,
                           "mouse_motion_left_right": len(self.mouse_steps) == 3 and all(m in self.marker_frames for m in MOUSE_MARKERS),
                           "386_486_and_pm32_return": all(m in self.marker_frames for m in MARKERS[4:7]),
                           "dpmi_extender_ring3_memory_and_return": "TAB5 DPMI32 OK" in self.marker_frames and "TAB5 VGA13 RETURN OK" in self.marker_frames,
                           "vga13_pixels_4_3_and_text_return": self.video_pixels_verified and "TAB5 VGA13 RETURN OK" in self.marker_frames,
                           "guest_storage_and_persisted_overlay": "TAB5 STORAGE COW OK" in self.marker_frames and self.saved_payload_verified,
                           "bios_pair_opened": all("/System/DOS386/" + name in self.open_attempts for name in ("bios.bin", "vgabios.bin")),
                           "audible_pcm_mute_restore": self.audio_nonzero_seen and self.audio_muted_seen and self.audio_resumed_seen,
                           "pointer_exit": self.exit_tap_sent,
                           "adjacent_data_opened": MOUNT + "/DATA.TXT" in self.open_attempts,
                           "selected_directory_enumerated": MOUNT in self.directory_attempts,
                           "files_closed_on_exit": not self.files,
                           "canvas_started": self.video_size == [800, 480]})
        elif self.scenario == "missing-boot":
            checks.update({"error_returned": self.return_code == -1,
                           "missing_image_explained": any(s in logs for s in ("FreeDOS boot image missing or invalid", "Cannot mount", "Cannot initialize", "Cannot open", "boot image")),
                           "no_canvas_started": self.video_size is None})
        elif self.scenario == "missing-bios":
            checks.update({"error_returned": self.return_code == -1,
                           "missing_bios_explained": any(s in logs for s in ("BIOS", "ROM", "Cannot initialize")),
                           "no_canvas_started": self.video_size is None,
                           "files_closed_on_error": not self.files})
        else:
            checks.update({"error_returned": self.return_code == -1,
                           "memory_error_explained": "DOS x86 stopped after an allocation or runtime error." in logs,
                           "allocation_refusal_exercised": self.calls["allocate"] > 0 and not self.allocations})
        stack = bytes(self.uc.mem_read(common.STACK_BASE, self.loader["stackBytes"]))
        first_written = next((i for i, value in enumerate(stack) if value != 0xa5), len(stack))
        report.update(scenario=self.scenario, passed=all(checks.values()), checks=checks,
                      renderedMarkers=self.marker_frames, lastRenderedText=self.last_screen,
                      keyboardEvents=self.key_events, duplicateQueuedTextEvents=self.duplicate_text_events,
                      keyboardMarkerWaits=self.keyboard_marker_waits,
                      graphicsProbe={"pixelsVerified": self.video_pixels_verified, "continueKeyboardPolls": self.video_continue_polls},
                      mouseSteps=self.mouse_steps, volumeTaps=self.volume_taps,
                      audioProbe={"nonzero": self.audio_nonzero_seen, "muted": self.audio_muted_seen,
                                  "resumed": self.audio_resumed_seen, "mutedGuestFrames": self.muted_guest_frames},
                      directoryOpenAttempts=self.directory_attempts, directoryEntryNames=self.directory_names,
                      peakHeapBytes=self.peak_heap_bytes, heapBytesAtReturn=sum(self.allocations.values()),
                      peakOpenHandles=self.peak_open_handles, openHandlesAtReturn=len(self.files),
                      minimumObservedSP=hex(self.minimum_sp), stackTouchedBytes=len(stack) - first_written,
                      saveCalls=self.saved_calls)
        return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--drivers", type=Path, default=SKETCH / "sdcard/System/DOS/DRIVERS.IMG")
    parser.add_argument("--bios-dir", type=Path, required=True, help="Rebuilt bios.bin and vgabios.bin directory")
    parser.add_argument("--extender", type=Path, required=True, help="TESTDPMI.EXE from tests/extender/prepare.py")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--instruction-limit", type=int, default=15000000000)
    parser.add_argument("--timeout-seconds", type=int, default=600)
    parser.add_argument("--frame-limit", type=int, default=3000)
    parser.add_argument("--scenario", action="append", choices=("allocation-failure", "missing-boot", "missing-bios", "launch"),
                        help="Run selected scenarios for diagnosis; default runs the complete suite")
    args = parser.parse_args()
    input_paths = [args.elf, args.image, args.drivers, args.extender, args.bios_dir / "bios.bin", args.bios_dir / "vgabios.bin"]
    if args.output.resolve() in tuple(path.resolve() for path in input_paths):
        parser.error("Output must differ from the read-only inputs")
    if min(args.instruction_limit, args.timeout_seconds, args.frame_limit) <= 0:
        parser.error("Execution limits must be positive")
    report = {"schemaVersion": 1, "passed": False, "component": "Exact modern DOS x86 RV32 frontend",
              "physicalDeviceTested": False, "scenarios": [],
              "limitations": ["Mocked SD/OS services and synthetic physical-keyboard/pointer reports; no ESP32 hardware test.",
                              "Original COM/MZ386/486, protected32 return, CWSDPMI ring3/memory, storage, VGA, PS/2 and speaker probes; no full x86 or arbitrary DOS-extender compatibility claim.",
                              "The runtime arena is reclaimed by the OS after return; live arena bytes are reported separately."]}
    try:
        elf_data, image_data, driver_data = args.elf.read_bytes(), args.image.read_bytes(), args.drivers.read_bytes()
        extender_data = args.extender.read_bytes()
        if sha(extender_data) != DPMI_SHA256:
            raise ValueError("Unexpected generated CWSDPMI integration fixture")
        bios = {name: (args.bios_dir / name).read_bytes() for name in ("bios.bin", "vgabios.bin")}
        if _font() not in bios["vgabios.bin"]:
            raise ValueError("SeaVGABIOS does not contain the exact VGA font used for pixel recognition")
        if sha(image_data) != BOOT_SHA256:
            raise ValueError("Unexpected FreeDOS fixture image")
        if sha(driver_data) != DRIVERS_SHA256:
            raise ValueError("Unexpected FreeDOS mouse driver image")
        report["elf"] = {"bytes": len(elf_data), "sha256": sha(elf_data)}
        report["image"] = {"bytes": len(image_data), "sha256": sha(image_data)}
        report["drivers"] = {"bytes": len(driver_data), "sha256": sha(driver_data)}
        report["extender"] = {"bytes": len(extender_data), "sha256": sha(extender_data)}
        report["bios"] = {name: {"bytes": len(data), "sha256": sha(data)} for name, data in bios.items()}
        source_files = {p for root in (DOS, NATIVE / "dos", NATIVE / "common") for p in root.rglob("*")
                        if p.is_file() and p.suffix in (".c", ".h", ".py")}
        source_files.update((DOOM_RUNNER, NATIVE / "build.py", SKETCH / "Tab5AppApi.h"))
        before = {p.relative_to(REPO).as_posix(): sha(p.read_bytes()) for p in sorted(source_files)}
        with tempfile.TemporaryDirectory(prefix="tab5-dos-examples-") as temp:
            folder = Path(temp)
            create(folder)
            (folder / "TESTDPMI.EXE").write_bytes(extender_data)
            fixtures = {p.name: p.read_bytes() for p in folder.iterdir()}
            report["fixtureHashes"] = {name: sha(data) for name, data in fixtures.items()}
        scenarios = args.scenario or ("allocation-failure", "missing-boot", "missing-bios", "launch")
        report["completeSuite"] = set(scenarios) == {"allocation-failure", "missing-boot", "missing-bios", "launch"}
        for scenario in scenarios:
            print("Executing DOS x86 ELF scenario: " + scenario, flush=True)
            app = Application(common.ElfImage(elf_data), image_data, driver_data, bios, fixtures, scenario, args.frame_limit)
            result = app.run(args.instruction_limit, args.timeout_seconds)
            if not result["passed"] and app.last_frame is not None:
                screenshot = args.output.with_name(args.output.stem + "-" + scenario + ".png")
                screenshot.parent.mkdir(parents=True, exist_ok=True)
                screenshot.write_bytes(png_frame(app.last_frame))
                result["failureScreenshot"] = str(screenshot)
            report["scenarios"].append(result)
            print(json.dumps({key: result[key] for key in ("scenario", "passed", "stopReason", "returnCode", "frameCount", "checks")}), flush=True)
        after = {p.relative_to(REPO).as_posix(): sha(p.read_bytes()) for p in sorted(source_files)}
        report.update(sourceHashes=after, sourcesUnchanged=before == after,
                      inputsUnchanged=sha(args.elf.read_bytes()) == sha(elf_data) and sha(args.image.read_bytes()) == sha(image_data)
                                      and sha(args.drivers.read_bytes()) == sha(driver_data)
                                      and args.extender.read_bytes() == extender_data
                                      and all((args.bios_dir / name).read_bytes() == data for name, data in bios.items()))
        report["passed"] = report["sourcesUnchanged"] and report["inputsUnchanged"] and all(s["passed"] for s in report["scenarios"])
    except Exception as error:
        report["error"] = repr(error)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
    print(args.output, flush=True)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
