"""Original CC0 8086 probe for real DOS mouse-driver and speaker integration.

Only documented guest interrupts and I/O ports are used. Results are printed
by the guest and recognized from the shipped frontend's rendered pixels.
"""
import struct


def create_mouse_probe():
    code = bytearray()
    labels, addresses, branches = {}, [], []

    def mark(name):
        labels[name] = len(code)

    def emit(raw):
        code.extend(raw)

    def print_message(name):
        emit(b"\xba")
        addresses.append((len(code), name))
        emit(b"\x00\x00\xb4\x09\xcd\x21")

    def branch(opcode, label):
        emit(bytes((opcode, 0)))
        branches.append((len(code) - 1, label))

    # Reset the installed driver, then set a known cursor position.
    emit(b"\x31\xc0\xcd\x33\x83\xf8\xff")
    branch(0x74, "installed")  # JE
    print_message("failed")
    emit(b"\xb8\x01\x4c\xcd\x21")
    mark("installed")
    emit(b"\xb8\x04\x00\xb9\x40\x01\xba\x64\x00\xcd\x33")
    print_message("ready")
    mark("move")
    emit(b"\xb8\x03\x00\xcd\x33\x81\xf9\x40\x01")
    branch(0x76, "move")  # JBE
    print_message("moved")
    mark("left")
    emit(b"\xb8\x03\x00\xcd\x33\xf7\xc3\x01\x00")
    branch(0x74, "left")  # JZ
    print_message("left_ok")
    mark("right")
    emit(b"\xb8\x03\x00\xcd\x33\xf7\xc3\x02\x00")
    branch(0x74, "right")
    print_message("right_ok")
    # PIT channel 2, square wave, divisor 1193 (~1 kHz); enable speaker.
    emit(b"\xb0\xb6\xe6\x43\xb0\xa9\xe6\x42\xb0\x04\xe6\x42")
    emit(b"\xe4\x61\x0c\x03\xe6\x61")
    print_message("audio")
    mark("key")
    emit(b"\xb4\x01\xcd\x16")
    branch(0x74, "key")
    emit(b"\x31\xc0\xcd\x16\xe4\x61\x24\xfc\xe6\x61")
    print_message("complete")
    emit(b"\xb8\x00\x4c\xcd\x21")
    for name, message in (
        ("failed", "TAB5 MOUSE DRIVER MISSING"),
        ("ready", "TAB5 MOUSE READY"),
        ("moved", "TAB5 MOUSE MOVE OK"),
        ("left_ok", "TAB5 MOUSE LEFT OK"),
        ("right_ok", "TAB5 MOUSE RIGHT OK"),
        ("audio", "TAB5 AUDIO READY"),
        ("complete", "TAB5 INPUT AUDIO OK"),
    ):
        mark(name)
        emit(message.encode("ascii") + b"\r\n$")
    for offset, name in addresses:
        struct.pack_into("<H", code, offset, 0x100 + labels[name])
    for offset, name in branches:
        distance = labels[name] - (offset + 1)
        if not -128 <= distance <= 127:
            raise ValueError("Branch out of range: " + name)
        struct.pack_into("b", code, offset, distance)
    return bytes(code)
