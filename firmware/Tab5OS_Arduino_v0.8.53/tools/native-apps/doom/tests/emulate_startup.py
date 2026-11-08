#!/usr/bin/env python3
"""Execute an actual Doom RV32 ELF with Unicorn and mocked Tab5 ABI-4 services.

Install the CI dependency with: python -m pip install unicorn==2.1.4
Success means the requested number of real video_present callbacks was reached.
This exercises the compiled application, not ESP32 hardware or physical input.
The IWAD is supplied explicitly and is never downloaded or modified here.
"""

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import time

sys.dont_write_bytecode = True

PAGE = 4096
IMAGE_BASE = 0x10000000
API_BASE = 0x01000000
STUB_BASE = API_BASE + 0x1000
STACK_BASE = 0x30000000
HEAP_BASE = 0x20000000
HEAP_LIMIT = 20 * 1024 * 1024
MASK32 = 0xffffffff

# Tab5AppApi.h: all fields are uint32_t or 32-bit pointers under ILP32.
# API1=52, API2=76, API3=80, API4=92 bytes. In particular, floating is a
# function pointer, so it does not add uint64_t alignment to the API structure.
API_OFFSETS = {
    "log": 12, "should_stop": 16, "sleep_ms": 20, "allocate": 24,
    "release": 28, "open_read": 32, "read": 36, "seek": 40,
    "close": 44, "millis": 48, "file_size": 52, "video_begin": 56,
    "video_present": 60, "buttons": 64, "audio_pcm": 68, "save": 72,
    "hardware": 76, "unix_time": 80, "floating": 84, "calendar": 88,
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def rounded(value):
    return (value + PAGE - 1) & -PAGE


def signed(value, bits=32):
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def double_bits(value):
    return struct.unpack("<Q", struct.pack("<d", float(value)))[0]


def as_double(bits):
    return struct.unpack("<d", struct.pack("<Q", bits))[0]


def floating_operation(operation, a, b):
    """Mirror appFloating, including the ordinary ILP32 uint64_t result."""
    x, y = as_double(a), as_double(b)
    if operation == 4:
        return double_bits(signed(a & MASK32))
    if operation == 5:
        return double_bits(a & MASK32)
    if operation == 6:
        return double_bits(signed(a, 64))
    if operation == 7:
        return double_bits(a)
    if operation == 8:
        return double_bits(struct.unpack("<f", struct.pack("<I", a & MASK32))[0])
    if operation == 9:
        try:
            return struct.unpack("<I", struct.pack("<f", x))[0]
        except OverflowError:
            return 0xff800000 if x < 0 else 0x7f800000
    if operation in (10, 11, 12, 13):
        return int(x) & ((1 << (32 if operation in (10, 11) else 64)) - 1)
    functions = {
        0: lambda: x + y, 1: lambda: x - y, 2: lambda: x * y,
        3: lambda: x / y, 20: lambda: math.sin(x), 21: lambda: math.cos(x),
        22: lambda: math.tan(x), 23: lambda: math.atan(x),
        24: lambda: math.atan2(x, y), 25: lambda: math.acos(x),
        26: lambda: math.asin(x), 27: lambda: math.sqrt(x),
        28: lambda: math.floor(x), 29: lambda: math.ceil(x),
        30: lambda: math.fmod(x, y), 31: lambda: math.pow(x, y),
        32: lambda: math.exp(x), 33: lambda: math.log(x),
        34: lambda: math.ldexp(x, signed(b & MASK32)),
    }
    if operation not in functions:
        raise ValueError("Unsupported floating operation: " + str(operation))
    try:
        result = functions[operation]()
    except ZeroDivisionError:
        result = math.nan if x == 0 else math.copysign(math.inf, x * math.copysign(1, y))
    except ValueError:
        result = -math.inf if operation == 33 and x == 0 else math.nan
    except OverflowError:
        result = math.copysign(math.inf, x) if operation == 34 else math.inf
    return double_bits(result)


class ElfImage:
    """Read the segment and RELA formats used by the firmware's ELF loader."""

    def __init__(self, data):
        if len(data) < 52 or data[:7] != b"\x7fELF\x01\x01\x01":
            raise ValueError("Expected ELF32 little-endian version 1")
        header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
        _, kind, machine, version, self.entry, phoff, shoff, flags, size, phsize, phnum, shsize, shnum, shnames = header
        if kind != 3 or machine != 243 or version != 1 or flags & ~1:
            raise ValueError("Expected RISC-V ET_DYN with the soft-float ILP32 ABI")
        if size != 52 or phsize != 32 or shsize != 40 or not phnum or not shnum:
            raise ValueError("Invalid ELF header tables")
        self.data = data
        self.programs = [struct.unpack_from("<8I", data, phoff + i * phsize) for i in range(phnum)]
        self.sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shnum)]
        loads = [segment for segment in self.programs if segment[0] == 1]
        if not loads:
            raise ValueError("ELF has no loadable segments")
        self.start = min(segment[2] for segment in loads)
        self.end = max(segment[2] + segment[5] for segment in loads)
        if self.start % PAGE or not 0 < self.end - self.start <= 8 * 1024 * 1024:
            raise ValueError("Invalid ELF image span")
        self.stack_bytes = 16384
        names = self.section_bytes(self.sections[shnames])
        for section in self.sections:
            name = names[section[0]:].split(b"\0", 1)[0]
            if name == b".tab5.stack":
                self.stack_bytes = struct.unpack("<I", self.section_bytes(section))[0]
        if not 16384 <= self.stack_bytes <= 262144 or self.stack_bytes % PAGE:
            raise ValueError("Invalid ELF stack request")

    def section_bytes(self, section):
        start, size = section[4], section[5]
        if start + size > len(self.data):
            raise ValueError("ELF section exceeds file")
        return self.data[start:start + size]

    def load(self, emulator, resolver):
        bias = IMAGE_BASE - self.start
        emulator.mem_map(IMAGE_BASE, rounded(self.end - self.start))
        for segment in self.programs:
            kind, offset, address, _, size, memory_size, _, _ = segment
            if kind != 1:
                continue
            if size > memory_size or offset + size > len(self.data):
                raise ValueError("Invalid ELF load segment")
            if size:
                emulator.mem_write(bias + address, self.data[offset:offset + size])
        relocations = Counter()
        imports = set()
        for section in self.sections:
            if section[1] != 4:
                continue
            symbols = self.sections[section[6]]
            strings = self.section_bytes(self.sections[symbols[6]])
            for offset, info, addend in struct.iter_unpack("<IIi", self.section_bytes(section)):
                kind, index = info & 255, info >> 8
                relocations[str(kind)] += 1
                if kind == 0:
                    continue
                if kind == 3:
                    value = bias + addend
                elif kind in (1, 5):
                    name, address, _, _, _, owner = struct.unpack_from("<IIIBBH", self.data, symbols[4] + index * 16)
                    if owner:
                        value = bias + address
                    else:
                        name = strings[name:].split(b"\0", 1)[0].decode("ascii")
                        imports.add(name)
                        value = resolver(name)
                    if kind == 1:
                        value += addend
                else:
                    raise ValueError("Unsupported ELF relocation: " + str(kind))
                emulator.mem_write(bias + offset, struct.pack("<I", value & MASK32))
        return {"entry": bias + self.entry, "imageBytes": self.end - self.start,
                "stackBytes": self.stack_bytes, "imports": sorted(imports),
                "relocations": dict(relocations)}


class Startup:
    def __init__(self, elf, wad, frames, play=False):
        import unicorn
        from unicorn import riscv_const as registers
        self.unicorn, self.registers = unicorn, registers
        self.uc = unicorn.Uc(unicorn.UC_ARCH_RISCV, unicorn.UC_MODE_RISCV32)
        self.target_frames = frames
        self.play = play
        self.start_pulses = []
        self.start_holding = 0
        self.play_input_polls = 0
        self.frames = 0
        self.nonzero_frames = 0
        self.unique_frames = set()
        self.stop_reason = None
        self.error = None
        self.return_code = None
        self.calls = Counter()
        self.logs = []
        self.log_bytes = 0
        self.frame_hashes = []
        self.last_frame_hash = None
        self.video_size = None
        self.clock_ms = 1234567890
        self.wad = wad
        self.files = {}
        self.open_attempts = []
        self.next_handle = 1
        self.allocations = {}
        self.next_allocation = HEAP_BASE
        self.callback_names = ["tab5_app_api_v4", "returned"] + list(API_OFFSETS)
        self.callbacks = {STUB_BASE + i * 16: name for i, name in enumerate(self.callback_names)}
        self.uc.mem_map(API_BASE, 0x10000)
        for address in self.callbacks:
            self.uc.mem_write(address, b"\x67\x80\x00\x00")  # ret
        self.uc.hook_add(unicorn.UC_HOOK_CODE, self.callback,
                         begin=STUB_BASE, end=max(self.callbacks) + 3)
        self.uc.hook_add(unicorn.UC_HOOK_MEM_INVALID, self.invalid_memory)
        self.loader = elf.load(self.uc, self.resolve)
        self.uc.mem_map(STACK_BASE, elf.stack_bytes)
        self.uc.reg_write(registers.UC_RISCV_REG_SP, STACK_BASE + elf.stack_bytes - 16)
        self.uc.reg_write(registers.UC_RISCV_REG_RA, self.address("returned"))
        # GCC uses F instructions internally even though public calls use ILP32.
        self.uc.reg_write(registers.UC_RISCV_REG_MSTATUS,
                          self.uc.reg_read(registers.UC_RISCV_REG_MSTATUS) | (3 << 13))
        self.uc.reg_write(registers.UC_RISCV_REG_FCSR, 0)
        language = API_BASE + 0x3000
        argv = API_BASE + 0x3100
        app_path = API_BASE + 0x3200
        wad_path = API_BASE + 0x3300
        self.uc.mem_write(language, b"en\0")
        self.uc.mem_write(app_path, b"/Applications/Doom.app.elf\0")
        self.uc.mem_write(wad_path, b"/Roms/doom1.wad\0")
        self.uc.mem_write(argv, struct.pack("<III", app_path, wad_path, 0))
        self.uc.mem_write(API_BASE, struct.pack("<III", 4, 92, language))
        for name, offset in API_OFFSETS.items():
            self.uc.mem_write(API_BASE + offset, struct.pack("<I", self.address(name)))
        self.uc.reg_write(registers.UC_RISCV_REG_A0, 2)
        self.uc.reg_write(registers.UC_RISCV_REG_A1, argv)
        self.uc.reg_write(registers.UC_RISCV_REG_PC, self.loader["entry"])

    def address(self, name):
        return STUB_BASE + self.callback_names.index(name) * 16

    def resolve(self, name):
        if name != "tab5_app_api_v4":
            raise ValueError("Unexpected imported symbol: " + name)
        return self.address(name)

    def arguments(self):
        return [self.uc.reg_read(getattr(self.registers, "UC_RISCV_REG_A" + str(i))) for i in range(8)]

    def string(self, pointer, limit=4096):
        result = bytearray()
        for offset in range(limit):
            value = self.uc.mem_read(pointer + offset, 1)[0]
            if not value:
                return result.decode("utf-8", errors="replace")
            result.append(value)
        raise ValueError("Unterminated API string")

    def stop(self, reason, error=None):
        self.stop_reason, self.error = reason, error
        self.uc.emu_stop()

    def invalid_memory(self, _uc, access, address, size, value, _user):
        self.stop("invalid-memory", {"access": access, "address": hex(address), "bytes": size, "value": value})
        return False

    def callback(self, _uc, address, _size, _user):
        try:
            name = self.callbacks.get(address)
            if name is None:
                raise ValueError("Execution entered the middle of an API stub")
            self.calls[name] += 1
            args = self.arguments()
            result = self.service(name, args)
            if result is not None:
                self.uc.reg_write(self.registers.UC_RISCV_REG_A0, result & MASK32)
                if name == "floating":
                    self.uc.reg_write(self.registers.UC_RISCV_REG_A1, (result >> 32) & MASK32)
        except Exception as error:
            self.stop("callback-error", {"callback": self.callbacks.get(address), "detail": repr(error)})

    def service(self, name, a):
        if name == "tab5_app_api_v4":
            return API_BASE
        if name == "returned":
            self.return_code = signed(a[0])
            self.stop("application-returned")
            return None
        if name == "log":
            if a[1] > 4096:
                raise ValueError("Oversized log callback")
            text = bytes(self.uc.mem_read(a[0], a[1])).decode("utf-8", errors="replace")
            if self.log_bytes < 262144:
                self.logs.append(text)
                self.log_bytes += a[1]
            return None
        if name == "should_stop":
            return int(self.frames >= self.target_frames)
        if name == "millis":
            self.clock_ms += 1
            return self.clock_ms & MASK32
        if name == "sleep_ms":
            self.clock_ms += a[0]
            return int(self.frames >= self.target_frames)
        if name == "allocate":
            size = rounded(a[0])
            if not a[0] or a[0] > 16 * 1024 * 1024 or sum(self.allocations.values()) + size > HEAP_LIMIT:
                return 0
            pointer = self.next_allocation
            self.next_allocation += size + PAGE
            self.uc.mem_map(pointer, size)
            self.allocations[pointer] = size
            return pointer
        if name == "release":
            if a[0]:
                size = self.allocations.pop(a[0])
                self.uc.mem_unmap(a[0], size)
            return None
        if name == "open_read":
            path = self.string(a[0])
            if len(self.open_attempts) < 128:
                self.open_attempts.append(path)
            if path != "/Roms/doom1.wad" or len(self.files) >= 8:
                return -1
            handle = self.next_handle
            self.next_handle += 1
            self.files[handle] = 0
            return handle
        if name == "file_size":
            return len(self.wad) if a[0] in self.files else 0
        if name == "read":
            if a[0] not in self.files or a[2] > 4096:
                return -1
            offset = self.files[a[0]]
            data = self.wad[offset:offset + a[2]]
            if data:
                self.uc.mem_write(a[1], data)
            self.files[a[0]] += len(data)
            return len(data)
        if name == "seek":
            if a[0] not in self.files or a[1] > len(self.wad):
                return -1
            self.files[a[0]] = a[1]
            return 0
        if name == "close":
            if a[0] not in self.files:
                return -1
            del self.files[a[0]]
            return 0
        if name == "video_begin":
            if self.video_size is not None or a[:2] != [320, 200]:
                return -1
            self.video_size = a[:2]
            return 0
        if name == "video_present":
            if self.video_size != [320, 200] or a[1] != 64000:
                raise ValueError("Unexpected video frame dimensions")
            frame = bytes(self.uc.mem_read(a[0], a[1] * 2))
            self.last_frame_hash = digest(frame)
            self.unique_frames.add(self.last_frame_hash)
            self.nonzero_frames += int(any(frame))
            if len(self.frame_hashes) < 8:
                self.frame_hashes.append(self.last_frame_hash)
            self.frames += 1
            if self.frames >= self.target_frames:
                self.stop("frame-milestone")
            return 0
        if name == "buttons":
            if self.play:
                # Doom shareware: open menu, New Game, episode, then skill.
                # Each press lasts two input polls, with releases between.
                if self.start_holding:
                    self.start_holding -= 1
                    return 1 << 7
                if len(self.start_pulses) < 4 and self.frames >= 5 + len(self.start_pulses) * 5:
                    self.start_pulses.append(self.frames)
                    self.start_holding = 1
                    return 1 << 7
                if 60 <= self.frames < 220:
                    self.play_input_polls += 1
                    return (1 << 2) | (1 << 4)  # Forward and fire.
            return 0
        if name == "audio_pcm":
            if a[1] > 2048 or not 8000 <= a[2] <= 48000:
                return -1
            self.uc.mem_read(a[0], a[1] * 4)
            return 0
        if name == "save":
            return -1  # Startup must not depend on writing configuration/saves.
        if name == "hardware":
            return -1
        if name == "unix_time":
            return 1700000000
        if name == "calendar":
            tm = time.gmtime(a[0])
            values = [tm.tm_sec, tm.tm_min, tm.tm_hour, tm.tm_mday, tm.tm_mon - 1,
                      tm.tm_year - 1900, (tm.tm_wday + 1) % 7, tm.tm_yday - 1, 0]
            self.uc.mem_write(a[2], struct.pack("<9i", *values))
            return 0
        if name == "floating":
            # Named 64-bit arguments use consecutive registers under ILP32:
            # op=a0, a=a1/a2, b=a3/a4; result=a0/a1 (not the variadic rule).
            return floating_operation(a[0], a[1] | (a[2] << 32), a[3] | (a[4] << 32))
        raise ValueError("Unimplemented API callback: " + name)

    def run(self, instruction_limit, timeout_seconds):
        started = time.monotonic()
        budget_used = 0
        while not self.stop_reason and budget_used < instruction_limit:
            if time.monotonic() - started >= timeout_seconds:
                self.stop("wall-time-limit")
                break
            count = min(1000000, instruction_limit - budget_used)
            try:
                pc = self.uc.reg_read(self.registers.UC_RISCV_REG_PC)
                self.uc.emu_start(pc, 0xffffffff, count=count)
            except self.unicorn.UcError as error:
                if not self.stop_reason:
                    self.stop("emulator-error", str(error))
            budget_used += count
            # Progress during computation that does not call the OS clock.
            self.clock_ms += 10
        if not self.stop_reason:
            self.stop("instruction-limit")
        pc = self.uc.reg_read(self.registers.UC_RISCV_REG_PC)
        try:
            instruction = bytes(self.uc.mem_read(pc, 16)).hex()
        except self.unicorn.UcError:
            instruction = None
        logs = "".join(self.logs)
        level_initialized = "P_GetNodesVersion:" in logs and "R_PrecacheLevel:" in logs
        play_passed = (level_initialized and len(self.unique_frames) > 1
                       and self.nonzero_frames > 0 and self.play_input_polls > 0)
        return {
            "passed": self.stop_reason == "frame-milestone" and (not self.play or play_passed),
            "stopReason": self.stop_reason, "error": self.error,
            "returnCode": self.return_code, "frameCount": self.frames,
            "frameTarget": self.target_frames, "loader": self.loader,
            "apiCallCounts": dict(self.calls), "fileOpenAttempts": self.open_attempts,
            "logs": logs, "firstFrameHashes": self.frame_hashes,
            "lastFrameHash": self.last_frame_hash,
            "nonzeroFrameCount": self.nonzero_frames,
            "uniqueFrameCount": len(self.unique_frames),
            "playScenario": {"enabled": self.play, "startPulseFrames": self.start_pulses,
                             "inputPolls": self.play_input_polls,
                             "levelInitialized": level_initialized},
            "elapsedSeconds": round(time.monotonic() - started, 3),
            "instructionBudgetUpperBound": budget_used,
            "finalPC": hex(pc), "finalInstructionBytes": instruction,
            "finalSP": hex(self.uc.reg_read(self.registers.UC_RISCV_REG_SP)),
            "unicornVersion": self.unicorn.__version__,
            "clockModel": "1 ms per millis callback, sleep duration, and 10 ms per instruction chunk",
        }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", required=True, type=Path)
    parser.add_argument("--wad", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--frames", type=int, default=120)
    parser.add_argument("--play", action="store_true",
                        help="Navigate Doom shareware menus with START, then move/fire; require a rendered level")
    parser.add_argument("--instruction-limit", type=int, default=1000000000)
    parser.add_argument("--timeout-seconds", type=int, default=300)
    args = parser.parse_args(argv)
    if min(args.frames, args.instruction_limit, args.timeout_seconds) <= 0:
        parser.error("Frame target and execution limits must be positive")
    if args.play and args.frames <= 60:
        parser.error("--play requires --frames greater than 60")
    report = {"schemaVersion": 1, "passed": False,
              "limitations": ["Mocked OS services; no ESP32 peripherals, physical input, or display hardware.",
                              "A frame milestone proves startup/rendering, not complete gameplay correctness."]}
    try:
        if args.output.resolve() in (args.elf.resolve(), args.wad.resolve()):
            raise ValueError("Output must not overwrite ELF or IWAD input")
        elf_data, wad_data = args.elf.read_bytes(), args.wad.read_bytes()
        report["elf"] = {"path": args.elf.as_posix(), "bytes": len(elf_data), "sha256": digest(elf_data)}
        report["wad"] = {"path": args.wad.as_posix(), "bytes": len(wad_data), "sha256": digest(wad_data)}
        report["scriptSha256"] = digest(Path(__file__).read_bytes())
        if len(wad_data) < 12 or wad_data[:4] != b"IWAD":
            raise ValueError("Expected a Doom IWAD file")
        report.update(Startup(ElfImage(elf_data), wad_data, args.frames, args.play).run(args.instruction_limit, args.timeout_seconds))
    except Exception as error:
        report.update(stopReason="harness-error", error=repr(error))
    # Never publish a report over one of the read-only inputs, even on failure.
    if args.output.resolve() in (args.elf.resolve(), args.wad.resolve()):
        print(json.dumps(report, indent=2))
        return 1
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: report.get(key) for key in ("passed", "stopReason", "returnCode", "frameCount", "error")}))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
