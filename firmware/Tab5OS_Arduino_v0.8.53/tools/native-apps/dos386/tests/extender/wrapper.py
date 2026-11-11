"""CC0: DOS launcher protecting CWSDPMI startup from read-only disk prompts.

The unmodified CWSDSTUB opens its default swap file before installing its own
critical-error handler. This parent installs INT 24h/Fail before DOS EXEC,
restores the previous handler afterward, and verifies normal exit with code zero.
"""
import struct

RETURN_MARKER = "TAB5 DPMI RETURN OK"


def create_wrapper():
    code = bytearray()
    labels, addresses, branches = {}, [], []

    def emit(raw):
        code.extend(bytes.fromhex(raw))

    def label(name):
        labels[name] = len(code) + 0x100

    def address(name, add=0):
        addresses.append((len(code), name, add))
        code.extend(bytes(2))

    def jump(opcode, name):
        emit(opcode)
        branches.append((len(code), name))
        code.extend(bytes(2))

    def output(name):
        emit("ba"); address(name); emit("b4 09 cd 21")

    # Move stack into the retained 4 KiB and release unused COM memory so DOS
    # can load the child. SS=CS at entry but make that relationship explicit.
    emit("8c c8 8e d0 bc 00 10 8e d8 8e c0 bb 10 01 b4 4a cd 21")
    jump("0f 82", "early_fail")
    # Save the original critical-error vector, then install our Fail handler.
    emit("b8 24 35 cd 21 89 1e"); address("old_vector")
    emit("8c 06"); address("old_vector", 2)
    emit("ba"); address("int24"); emit("b8 24 25 cd 21")
    # EXEC parameter block segments point to our PSP/data segment.
    emit("8c c8 a3"); address("parameters", 4)
    emit("a3"); address("parameters", 8)
    emit("a3"); address("parameters", 12)
    emit("8e c0 bb"); address("parameters")
    emit("ba"); address("program_name")
    emit("b8 00 4b cd 21")
    jump("0f 82", "exec_fail")
    # AH is the termination type; AL is the return code. Require both zero.
    emit("b4 4d cd 21 a3"); address("exit_status")
    jump("e9", "restore")
    label("exec_fail")
    emit("c7 06"); address("exit_status"); emit("7f 00")
    label("restore")
    emit("8b 16"); address("old_vector")
    emit("a1"); address("old_vector", 2)
    emit("8e d8 b8 24 25 cd 21 0e 1f")
    emit("83 3e"); address("exit_status"); emit("00")
    jump("0f 85", "child_fail")
    output("success")
    emit("b8 00 4c cd 21")
    label("early_fail")
    label("child_fail")
    output("failed")
    emit("b8 01 4c cd 21")
    label("int24")
    emit("b0 03 cf")
    label("old_vector"); emit("00 00 00 00")
    label("exit_status"); emit("00 00")
    label("parameters")
    emit("00 00"); address("command_tail"); emit("00 00 5c 00 00 00 6c 00 00 00")
    label("command_tail"); emit("00 0d")
    label("program_name"); code.extend(b"TESTDPMI.EXE\0")
    label("success"); code.extend((RETURN_MARKER + "\r\n$").encode())
    label("failed"); code.extend(b"TAB5 DPMI EXEC FAIL\r\n$")
    for position, target, add in addresses:
        struct.pack_into("<H", code, position, labels[target] + add)
    for position, target in branches:
        struct.pack_into("<h", code, position, labels[target] - (position + 0x102))
    if len(code) + 0x100 >= 0x1000:
        raise ValueError("Launcher overlaps retained stack")
    return bytes(code)
