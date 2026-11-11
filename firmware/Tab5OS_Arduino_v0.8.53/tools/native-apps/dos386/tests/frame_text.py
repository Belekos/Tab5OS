"""Read ASCII test markers from the DOS application's rendered RGB565 canvas.

The keyboard must be hidden and VGA must use 80x25 text mode: app.c places the
unscaled 720x400 guest screen at (40, 0) in its 800x480 canvas. This checks pixels
only; it never reads guest memory. Exact glyph matches on a black background are
required, so unknown glyphs, graphics, and visible cursor cells become '?'.
"""
from functools import lru_cache
from pathlib import Path
import re


FONT_FILE = Path(__file__).resolve().parents[1] / "core/vgafont.c"
WIDTH, HEIGHT = 800, 480
LEFT, TOP = 40, 0
COLUMNS, ROWS = 80, 25
CELL_WIDTH, CELL_HEIGHT = 9, 16
FRAME_BYTES = WIDTH * HEIGHT * 2


def _parse_font(source: str) -> bytes:
    # Comments contain hexadecimal glyph numbers; remove them before parsing.
    source = re.sub(r"/\*.*?\*/|//[^\r\n]*", "", source, flags=re.DOTALL)
    match = re.search(r"\bvgafont16\s*\[\s*256\s*\*\s*16\s*\]\s*=\s*\{(.*?)\}\s*;",
                      source, flags=re.DOTALL)
    if match is None:
        raise ValueError("Cannot find the 4096-byte vgafont16 initializer")
    tokens = match.group(1).strip().rstrip(",").split(",")
    values = []
    for token in tokens:
        token = token.strip()
        if not re.fullmatch(r"(?:0[xX][0-9a-fA-F]+|[0-9]+)", token):
            raise ValueError("Unexpected token in vgafont16 initializer")
        value = int(token, 16 if token.lower().startswith("0x") else 10)
        if not 0 <= value <= 255:
            raise ValueError("vgafont16 contains a value outside byte range")
        values.append(value)
    if len(values) != 4096:
        raise ValueError("vgafont16 must contain exactly 4096 bytes")
    return bytes(values)


@lru_cache(maxsize=1)
def _font() -> bytes:
    return _parse_font(FONT_FILE.read_text(encoding="utf8"))


@lru_cache(maxsize=1)
def _glyphs() -> dict[bytes, str]:
    font = _font()
    result = {}
    for code in range(32, 127):
        pattern = font[code * CELL_HEIGHT:(code + 1) * CELL_HEIGHT]
        # Never guess if a changed source font makes two ASCII glyphs identical.
        result[pattern] = chr(code) if pattern not in result else "?"
    return result


def decode(frame: bytes) -> str:
    """Return 25 newline-separated rows of 80 characters from an exact canvas.

    Input is the complete, tightly packed 800x480 RGB565 canvas (768000 bytes).
    Any nonzero 16-bit pixel is foreground; its hue and host byte order do not
    affect the match. Trailing spaces and unknown cells are preserved.
    """
    if len(frame) != FRAME_BYTES:
        raise ValueError(f"Expected {FRAME_BYTES} RGB565 bytes, got {len(frame)}")
    pixels = memoryview(frame).cast("H")
    glyphs = _glyphs()
    rows = []
    for row in range(ROWS):
        text = []
        for column in range(COLUMNS):
            origin = (TOP + row * CELL_HEIGHT) * WIDTH + LEFT + column * CELL_WIDTH
            pattern = bytearray(CELL_HEIGHT)
            spacing_ink = False
            for y in range(CELL_HEIGHT):
                start = origin + y * WIDTH
                bits = 0
                for x in range(8):
                    bits = (bits << 1) | bool(pixels[start + x])
                pattern[y] = bits
                spacing_ink |= bool(pixels[start + 8])
            # ASCII leaves column nine black. The VGA cursor fills all nine
            # pixels, so even an underline-shaped cursor cannot count as text.
            text.append("?" if spacing_ink else glyphs.get(bytes(pattern), "?"))
        rows.append("".join(text))
    return "\n".join(rows)


def _self_test() -> None:
    """Synthetic crop/recognition checks; these do not execute DOS or its ELF."""
    frame = bytearray(b"\xff" * FRAME_BYTES)  # Nonblack app chrome is ignored.
    for y in range(TOP, TOP + ROWS * CELL_HEIGHT):
        start = (y * WIDTH + LEFT) * 2
        frame[start:start + COLUMNS * CELL_WIDTH * 2] = bytes(COLUMNS * CELL_WIDTH * 2)

    def pixel(x, y, color):
        start = (y * WIDTH + x) * 2
        frame[start:start + 2] = color.to_bytes(2, "little")

    def put(column, row, text, color):
        font = _font()
        for index, char in enumerate(text):
            glyph = font[ord(char) * CELL_HEIGHT:(ord(char) + 1) * CELL_HEIGHT]
            for y, bits in enumerate(glyph):
                for x in range(8):
                    if bits & (128 >> x):
                        pixel(LEFT + (column + index) * CELL_WIDTH + x,
                              TOP + row * CELL_HEIGHT + y, color)

    expected = [" " * COLUMNS for _ in range(ROWS)]
    for column, row, text, color in ((0, 0, "TAB5 COM OK", 0x0001),
                                    (1, 1, "TAB5 MZ EXE OK", 0x0100),
                                    (0, 2, "TAB5 ADJACENT DATA OK", 0xF800),
                                    (79, 24, "Z", 0xFFFF)):
        put(column, row, text, color)
        expected[row] = expected[row][:column] + text + expected[row][column + len(text):]
    # A damaged glyph and a visible nine-pixel cursor must remain unknown.
    pixel(LEFT, TOP + 3 * CELL_HEIGHT, 1)
    for x in range(CELL_WIDTH):
        pixel(LEFT + CELL_WIDTH + x, TOP + 4 * CELL_HEIGHT - 1, 0xFFFF)
    expected[3] = "??" + expected[3][2:]
    assert decode(bytes(frame)) == "\n".join(expected)
    try:
        decode(bytes(frame[:-2]))
    except ValueError:
        pass
    else:
        raise AssertionError("A truncated RGB565 canvas was accepted")
    source = FONT_FILE.read_text(encoding="utf8")
    assert _parse_font("/* 0xff 123 */\n" + source + "\n// 0x01") == _font()


if __name__ == "__main__":
    _self_test()
    print("PASS: synthetic RGB565 crop, marker, color, cursor, and size checks")
