"""CC0: generate an original 32-bit DPMI client as a DJGPP COFF image.

The genuine CWSDSTUB loader supplies the DPMI host, switches to protected mode,
loads this image and starts its 32-bit code. The client validates its ring,
allocates 256 KiB via INT 31h, creates an LDT selector, touches all 64 pages,
checks a page-crossing dword and the last dword, frees resources and exits DOS.
No assembler or guest compiler is required; all branches are range checked.
"""
import hashlib
import struct

STUB_SHA256 = "15cb50f3cc7022cd1148b4569813b33f4b56c3f31ebdd296c7f97906d2543625"
SUCCESS = "TAB5 DPMI32 OK"
FAILURE = "TAB5 DPMI32 FAIL"


def coff_image():
    code = bytearray()
    labels, fixes = {}, []

    def emit(hexadecimal):
        code.extend(bytes.fromhex(hexadecimal))

    def imm(register, value):
        code.append(0xb8 + register)
        code.extend(struct.pack("<I", value))

    def label(name):
        labels[name] = len(code)

    def jump(opcode, name):
        emit(opcode)
        fixes.append((len(code), name))
        code.extend(bytes(4))

    def service(number):
        imm(0, number)
        emit("cd 31")
        jump("0f 82", "failed")

    def output(text):
        for char in (text + "\r\n").encode("ascii"):
            code.extend(bytes((0xb2, char, 0xb4, 2, 0xcd, 0x21)))

    # CWSDSTUB provides DS for the COFF allocation, but its SS is temporary.
    emit("66 8c d8 8e d0")
    imm(4, 0x13000)
    emit("8e c0 fc")
    # This is the ring-3 DPMI host, not the alternate ring-0 CWSDSTR0 build.
    emit("8c c8 83 e0 03 83 f8 03")
    jump("0f 85", "failed")
    service(0x400)
    emit("f6 c3 01")
    jump("0f 84", "failed")
    imm(3, 4)  # BX:CX = 0x40000 bytes.
    imm(1, 0)
    service(0x501)
    emit("66 89 3d 00 20 00 00 66 89 35 02 20 00 00")  # handle DI, SI
    emit("0f b7 c3 c1 e0 10 66 89 c8 a3 08 20 00 00")  # linear BX:CX
    imm(1, 1)
    service(0)  # Allocate an LDT descriptor.
    emit("66 a3 04 20 00 00 66 89 c3")
    emit("8b 0d 08 20 00 00 89 ca c1 e9 10")
    service(7)  # Descriptor base = returned linear address.
    imm(1, 3)
    imm(2, 0xffff)
    service(8)  # Limit = 256 KiB - 1.
    emit("66 8c c9 66 83 e1 03 66 c1 e1 05 66 81 c9 93 40")
    service(9)  # Present, writable, 32-bit data, correct caller privilege.
    emit("8e 05 04 20 00 00")
    imm(7, 0)
    imm(1, 64)
    imm(0, 0x12345678)
    label("write_page")
    emit("26 89 07 05 01 01 01 01 81 c7 00 10 00 00 49")
    jump("0f 85", "write_page")
    imm(7, 0)
    imm(1, 64)
    imm(0, 0x12345678)
    label("read_page")
    emit("26 39 07")
    jump("0f 85", "failed")
    emit("05 01 01 01 01 81 c7 00 10 00 00 49")
    jump("0f 85", "read_page")
    emit("26 c7 05 ff 0f 00 00 ef be ad de 26 81 3d ff 0f 00 00 ef be ad de")
    jump("0f 85", "failed")
    emit("26 c7 05 fc ff 03 00 78 56 34 12 26 81 3d fc ff 03 00 78 56 34 12")
    jump("0f 85", "failed")
    emit("66 8c d8 8e c0 66 8b 1d 04 20 00 00")
    service(1)  # Free descriptor after unloading ES.
    emit("66 8b 35 02 20 00 00 66 8b 3d 00 20 00 00")
    service(0x502)  # Free allocation by SI:DI.
    output(SUCCESS)
    imm(0, 0x4c00)
    emit("cd 21 0f 0b")
    label("failed")
    output(FAILURE)
    imm(0, 0x4c01)
    emit("cd 21 0f 0b")
    for position, target in fixes:
        struct.pack_into("<i", code, position, labels[target] - position - 4)
    text = bytes(code).ljust((len(code) + 511) & ~511, b"\x90")
    if len(text) > 0x1000:
        raise ValueError("Probe code overlaps data")
    data = bytes(512)
    header = struct.pack("<HHIIIHH", 0x14c, 3, 0, 0, 0, 28, 0x10f)
    header += struct.pack("<HHIIIIII", 0x10b, 0, len(text), len(data), 0x10000,
                          0x1000, 0x1000, 0x2000)
    for name, address, size, offset, flags in (
        (b".text", 0x1000, len(text), 512, 0x20),
        (b".data", 0x2000, len(data), 512 + len(text), 0x40),
        (b".bss", 0x3000, 0x10000, 0, 0x80),
    ):
        header += struct.pack("<8sIIIIIIHHI", name, address, address, size, offset, 0, 0, 0, 0, flags)
    return header.ljust(512, b"\0") + text + data


def create_executable(stub):
    if hashlib.sha256(stub).hexdigest() != STUB_SHA256:
        raise ValueError("Expected unmodified CWSDSTUB.EXE release 7")
    return stub + coff_image()
