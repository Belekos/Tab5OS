"""Small tactile UI for Tab5OS. The OS owns drawing; the script polls events."""
from _tab5 import call
from builtins import open as _file_open
import struct

_next_id = 1
WIDTH, HEIGHT = 1100, 500

def language():
    return call(57, 0, 0, 0, None, 24).decode()

def open(title):
    global _next_id
    call(50, 0, 0, 0, title.encode())
    _next_id = 1

class Widget:
    def __init__(self, kind, text, x, y, width, height, maximum=100):
        global _next_id
        if x < 0 or y < 0 or width < 24 or height < 24 or x + width > WIDTH or y + height > HEIGHT:
            raise ValueError('Widget outside UI bounds: (%s, %s, %s, %s)' % (x, y, width, height))
        if _next_id > 24:
            raise ValueError('Too many widgets on this page')
        self.id = _next_id
        data = struct.pack('<HHHH', x, y, width, height) + text.encode()
        call(51, self.id, kind, maximum, data)
        _next_id += 1

    def text(self, value=None):
        if value is None:
            return call(58, self.id, 0, 0, None, 512).decode()
        call(52, self.id, 0, 0, str(value).encode())

    def value(self, value):
        call(53, self.id, int(value))

def label(text, x=20, y=20, width=1000, height=40):
    return Widget(1, text, x, y, width, height)

def button(text, x=20, y=80, width=240, height=64):
    return Widget(2, text, x, y, width, height)

def slider(x=20, y=200, width=800, height=40, maximum=100, value=0):
    widget = Widget(3, '', x, y, width, height, maximum)
    widget.value(value)
    return widget

def poll():
    """Return (widget_id, value), or None. Add sleep_ms(20) to your event loop."""
    event = call(54, 0, 0, 0, None, 8)
    return struct.unpack('<II', event) if event else None

def is_open():
    return bool(call(56))

def close():
    call(55)


def entry(text='', x=20, y=80, width=900, height=48, submit=False):
    """Editable input; submit keeps editing after Enter and emits (id, 1)."""
    widget = Widget(4, str(text), x, y, width, height)
    if submit:
        widget.value(1)
    return widget

def dropdown(options, x=20, y=80, width=900, height=48, selected=0):
    widget = Widget(5, '\n'.join(options), x, y, width, height, max(1, len(options)-1))
    widget.value(selected)
    return widget

def preview(text, background, foreground, x=20, y=320, width=900, height=80):
    widget = Widget(6, text, x, y, width, height)
    colors(widget, background, foreground)
    return widget

def colors(widget, background, foreground):
    call(59, widget.id, int(background.lstrip('#'), 16), int(foreground.lstrip('#'), 16))

def tr(source):
    """Use the native OS dictionary; no duplicate JSON load in the Python VM."""
    try:
        return call(81, 0, 0, 0, source.encode() + b'\0', 4096).decode()
    except OSError as error:
        # A new SD pack still opens on an older firmware (operation missing).
        # Native French strings are also a safe fallback for an oversized label.
        if error.args[0] in (38, 28, 22):
            return source
        raise


def shortcut(widget, key):
    """Bind a button to an OS navigation key; text editing retains its keys."""
    keys = {'back': 1, 'previous': 2, 'next': 3, 'enter': 4, 'delete': 5, 'up': 6, 'down': 7}
    try:
        call(105, widget.id, keys[key])
    except OSError as error:
        if error.args[0] not in (38, 22):
            raise
    return widget
