#pragma once
#include <cstddef>
#include <cstdint>

struct Tab5JpegHeader {
  uint16_t width = 0;
  uint16_t height = 0;
  uint8_t mcuWidth = 0;
  uint8_t mcuHeight = 0;
};

// Guard the IDF 5.5 JPEG parser before it writes fixed-size table arrays.
// This is a bounded baseline-header probe, not an entropy-stream decoder.
// Missing Huffman tables are allowed: the native MJPEG decoder supplies them.
inline bool tab5ProbeJpegHeader(const uint8_t* bytes, size_t size,
                               Tab5JpegHeader& result) {
  result = {};
  if (!bytes || size < 4 || bytes[0] != 0xff || bytes[1] != 0xd8)
    return false;
  auto readBigEndian16 = [](const uint8_t* p) -> uint16_t {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
  };
  size_t pos = 2;
  uint8_t components = 0;
  unsigned quantizationTables = 0;
  Tab5JpegHeader parsed;
  while (pos < size) {
    if (bytes[pos++] != 0xff) return false;
    while (pos < size && bytes[pos] == 0xff) ++pos;
    if (pos >= size) return false;
    const uint8_t marker = bytes[pos++];
    if (marker == 0 || marker == 0xd8 || marker == 0xd9 ||
        (marker >= 0xd0 && marker <= 0xd7)) return false;
    if (size - pos < 2) return false;
    const size_t length = readBigEndian16(bytes + pos);
    if (length < 2 || length > size - pos) return false;
    const uint8_t* p = bytes + pos + 2;
    size_t remaining = length - 2;
    pos += length;
    if (marker == 0xc0) {
      if (components || remaining < 6 || p[0] != 8) return false;
      parsed.height = readBigEndian16(p + 1);
      parsed.width = readBigEndian16(p + 3);
      components = p[5];
      if (!parsed.width || !parsed.height ||
          (components != 1 && components != 3) ||
          remaining != static_cast<size_t>(6 + 3 * components)) return false;
      const uint8_t sampling = p[7];
      const uint8_t h = sampling >> 4, v = sampling & 15;
      if (!h || !v || h > 4 || v > 4) return false;
      if (components == 3 && sampling != 0x11 && sampling != 0x21 &&
          sampling != 0x22) return false;
      for (unsigned i = 0; i < components; ++i) {
        const uint8_t factor = p[7 + 3 * i];
        if (p[8 + 3 * i] >= 4 || !(factor >> 4) || !(factor & 15) ||
            (factor >> 4) > 4 || (factor & 15) > 4) return false;
      }
      parsed.mcuWidth = h * 8;
      parsed.mcuHeight = v * 8;
    } else if (marker == 0xdb) {
      while (remaining) {
        const uint8_t descriptor = *p++;
        --remaining;
        const size_t tableBytes = (descriptor >> 4) ? 128 : 64;
        if ((descriptor >> 4) > 1 || (descriptor & 15) >= 4 ||
            ++quantizationTables > 4 || remaining < tableBytes) return false;
        p += tableBytes;
        remaining -= tableBytes;
      }
    } else if (marker == 0xc4) {
      while (remaining) {
        if (remaining < 17 || (p[0] >> 4) >= 2 || (p[0] & 15) >= 2)
          return false;
        size_t symbols = 0;
        for (unsigned i = 1; i <= 16; ++i) symbols += p[i];
        if (symbols > 256 || remaining - 17 < symbols) return false;
        p += 17 + symbols;
        remaining -= 17 + symbols;
      }
    } else if (marker == 0xdd) {
      if (remaining != 2) return false;
    } else if (marker == 0xda) {
      if (!components || remaining < 4 || p[0] == 0 ||
          p[0] > components || remaining != static_cast<size_t>(4 + 2 * p[0]))
        return false;
      // The native hardware validates scan selectors and entropy data.
      result = parsed;
      return true;
    } else if ((marker < 0xe0 || marker > 0xef) && marker != 0xfe) {
      // The native parser only skips APP/COM payloads; other header marker
      // payloads must not be allowed to masquerade as DQT/DHT/SOF markers.
      return false;
    }
  }
  return false;
}
