"""Wired USB HID/XInput snapshots; no device ownership or consuming event queue."""
from _tab5 import call
import struct

RIGHT, LEFT, UP, DOWN = 1, 2, 4, 8
A, B, SELECT, START = 16, 32, 64, 128
X, Y, L, R = 2048, 4096, 8192, 16384

def capture(enabled=True):
    """Suppress OS gamepad navigation only while this Python UI is foreground.

    Use for games reading state(); ordinary tab5ui applications need no capture.
    The OS resets capture for each new script. Guide still opens the OS help.
    """
    call(106, 1, int(bool(enabled)))

def state():
    """Return buttons, raw buttons, four signed axes and reception diagnostics.

    Axis order: X, Y, RX, RY; -32768..32767, positive Y downward.
    axis_mask states which axes exist. Zero buttons also means disconnected;
    consult connected. Multiple readers may call this without losing events.
    """
    b = call(106, 0, 0, 0, None, 64)
    ident, buttons, raw, reports, rejected, errors, restarts = struct.unpack('<7I', b[:28])
    vendor, product = struct.unpack('<2H', b[28:32])
    axes = struct.unpack('<4h', b[32:40])
    return dict(connected=bool(b[43]), identity=ident, buttons=buttons,
                raw_buttons=raw, axes=axes, directions=b[40], axis_mask=b[41],
                protocol='XInput' if b[42] else 'HID', vendor=vendor, product=product,
                reports=reports, rejected=rejected, errors=errors, restarts=restarts,
                interface=b[45], report_length=b[44], raw_report=b[48:48+min(16,b[44])])

def buttons():
    """Return the held normalized button mask, excluding the keyboard."""
    return struct.unpack('<I', call(106, 0, 0, 0, None, 64)[4:8])[0]
