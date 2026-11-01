"""Native, interactive theme sandbox. Palette updates never recreate its objects."""
import struct
from _tab5 import call
import tab5ui as ui

# Protocol order matches the theme JSON and native ThemePreview::Role enum.
KEYS = ('bg', 'surface', 'raised', 'border', 'text', 'muted', 'active', 'cursor', 'selected')

def preview(theme, x=16, y=242, width=1064, height=258):
    widget = ui.Widget(7, '', x, y, width, height)
    colors(widget, theme)
    return widget

def colors(widget, theme):
    values = [int(theme[key].lstrip('#'), 16) for key in KEYS]
    if any(value < 0 or value > 0xffffff for value in values):
        raise ValueError('Couleur invalide')
    call(96, widget.id, 0, 0, struct.pack('<9I', *values))
