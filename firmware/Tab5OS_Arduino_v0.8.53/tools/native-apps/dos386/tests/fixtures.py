"""Original CC0 DOS fixtures for the exact x86 application integration test.

No assembler, downloaded executable, or commercial game is required. The
programs use normal DOS loading, file services, PS/2 mouse-driver services,
and speaker I/O. Passing these probes is not full x86 conformance.
"""
import importlib.util
from pathlib import Path
import struct

LEGACY = Path(__file__).resolve().parents[2] / "dos/tests"


def _module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def instruction_program(gen, base):
    """386-prefixed arithmetic or 486 BSWAP/XADD/CMPXCHG under real DOS."""
    code = bytearray(b"\x0e\x1f")
    branches, fixups, labels = [], [], {}

    def compare(value):
        code.extend(b"\x66\x3d" + struct.pack("<I", value))
        code.append(0x75)
        branches.append(len(code))
        code.append(0)

    if gen == 3:
        code.extend(bytes.fromhex("66 b8 78 56 34 12 66 b9 00 00 01 00 66 01 c8 66 c1 c0 08"))
        compare(0x35567812)
    else:
        code.extend(bytes.fromhex("66 b8 78 56 34 12 66 0f c8 66 bb 02 00 00 00 66 0f c1 d8"))
        compare(0x78563414)
        code.extend(bytes.fromhex("66 89 c1 66 ba ef be ad de 66 0f b1 d1 66 89 c8"))
        compare(0xdeadbeef)

    def output(label, status):
        code.append(0xba)
        fixups.append((len(code), label))
        code.extend(b"\0\0\xb4\x09\xcd\x21\xb8" + bytes([status, 0x4c]) + b"\xcd\x21")

    output("success", 0)
    fail = len(code)
    output("failed", 1)
    labels["success"] = len(code)
    code.extend(f"TAB5 {gen}86 DOS OK\r\n$".encode())
    labels["failed"] = len(code)
    code.extend(f"TAB5 {gen}86 DOS FAIL\r\n$".encode())
    for pos in branches:
        delta = fail - pos - 1
        if not 0 <= delta < 128:
            raise ValueError("Short branch overflow")
        code[pos] = delta
    for pos, label in fixups:
        struct.pack_into("<H", code, pos, base + labels[label])
    return bytes(code)


def mz(body):
    size = 32 + len(body)
    return struct.pack("<14H", 0x5a4d, size % 512, (size + 511) // 512,
                       0, 2, 32, 0xffff, 0, 0xfffe, 0, 0, 0, 0x1c, 0) + b"\0" * 4 + body


def storage_program():
    """Write/read A: through guest DOS; require C: to remain write protected."""
    code = bytearray(b"\x0e\x1f\x0e\x07\xfc")  # DS=ES=CS; CLD
    labels, addresses, jumps = {}, [], []

    def emit(value):
        code.extend(bytes.fromhex(value))

    def address(op, label):
        emit(op)
        addresses.append((len(code), label))
        code.extend(b"\0\0")

    def fail_if(op):
        # Invert a short conditional jump around a 16-bit near jump.
        code.extend(bytes((op ^ 1, 3, 0xe9)))
        jumps.append(len(code))
        code.extend(b"\0\0")

    address("ba", "filename")
    emit("31 c9 b4 3c cd 21")
    fail_if(0x72)  # carry set
    emit("89 c3")
    address("ba", "payload")
    emit("b9 08 00 b4 40 cd 21")
    fail_if(0x72)
    emit("83 f8 08")
    fail_if(0x75)
    emit("b4 3e cd 21")
    fail_if(0x72)
    address("ba", "filename")
    emit("b8 00 3d cd 21")
    fail_if(0x72)
    emit("89 c3")
    address("ba", "buffer")
    emit("b9 08 00 b4 3f cd 21")
    fail_if(0x72)
    emit("83 f8 08")
    fail_if(0x75)
    emit("b4 3e cd 21")
    fail_if(0x72)
    address("be", "payload")
    address("bf", "buffer")
    emit("b9 08 00 f3 a6")
    fail_if(0x75)
    # DOS may invoke INT24 for write-protected media. Return Fail explicitly
    # instead of leaving an automated probe at Abort/Retry/Fail. Preserve the
    # caller's original critical-error vector and the create call's flags.
    emit("b8 24 35 cd 21")
    address("89 1e", "old24")
    address("8c 06", "old24_segment")
    address("ba", "critical")
    emit("b8 24 25 cd 21")
    address("ba", "readonly")
    emit("31 c9 b4 3c cd 21")
    emit("9c 50 1e")
    address("c5 16", "old24")
    emit("b8 24 25 cd 21 1f 58 9d")
    fail_if(0x73)  # carry clear would mean C: was writable
    address("ba", "success")
    emit("b4 09 cd 21 b8 00 4c cd 21")
    labels["failed_code"] = len(code)
    address("ba", "failed")
    emit("b4 09 cd 21 b8 01 4c cd 21")
    for label, data in (("old24", bytes(2)), ("old24_segment", bytes(2)),
                        ("critical", bytes.fromhex("b0 03 cf")),
                        ("filename", b"A:\\TAB5TEST.DAT\0"),
                        ("readonly", b"C:\\TAB5FAIL.DAT\0"),
                        ("payload", b"TAB5COW!"), ("buffer", bytes(8)),
                        ("success", b"TAB5 STORAGE COW OK\r\n$"),
                        ("failed", b"TAB5 STORAGE DOS FAIL\r\n$")):
        labels[label] = len(code)
        code.extend(data)
    for offset, label in addresses:
        struct.pack_into("<H", code, offset, 0x100 + labels[label])
    for offset in jumps:
        struct.pack_into("<h", code, offset, labels["failed_code"] - offset - 2)
    return bytes(code)


def video_program():
    """VGA13 red/green halves, keyboard wait, and DOS text-mode return."""
    code = bytearray(bytes.fromhex("b8 13 00 cd 10 ba c8 03 b0 01 ee 42"))
    # DAC index1=red, index2=green; six bytes via port3C9h.
    for value in (63, 0, 0, 0, 63, 0):
        code.extend(bytes((0xb0, value, 0xee)))
    code.extend(bytes.fromhex("b8 00 a0 8e c0 31 ff fc b9 00 7d b0 01 f3 aa"))
    code.extend(bytes.fromhex("b9 00 7d b0 02 f3 aa 31 c0 cd 16 b8 03 00 cd 10 0e 1f ba"))
    pointer = len(code)
    code.extend(bytes(2))
    code.extend(bytes.fromhex("b4 09 cd 21 b8 00 4c cd 21"))
    struct.pack_into("<H", code, pointer, 0x100 + len(code))
    code.extend(b"TAB5 VGA13 RETURN OK\r\n$")
    return bytes(code)


def create(out):
    out.mkdir(parents=True, exist_ok=True)
    _module("legacy_dos_examples", LEGACY / "make_examples.py").create(out)
    mouse = _module("legacy_dos_peripherals", LEGACY / "peripheral_fixture.py")
    pm = _module("legacy_dos_protected", LEGACY / "tiny386_probe/programs.py")
    extender = _module("dos_x86_extender_wrapper", Path(__file__).resolve().parent / "extender/wrapper.py")
    (out / "MOUSE.COM").write_bytes(mouse.create_mouse_probe())
    (out / "TEST386.COM").write_bytes(instruction_program(3, 0x100))
    (out / "TEST486.EXE").write_bytes(mz(instruction_program(4, 0)))
    (out / "TESTPM.COM").write_bytes(pm.protected_program())
    (out / "DPMI.COM").write_bytes(extender.create_wrapper())
    (out / "STORE.COM").write_bytes(storage_program())
    (out / "VIDEO.COM").write_bytes(video_program())
    (out / "HELLO.EXE").rename(out / "Hello long name.EXE")
