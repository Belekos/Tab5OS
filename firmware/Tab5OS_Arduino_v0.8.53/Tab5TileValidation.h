#pragma once
#include <cstddef>
#include <cstdint>
namespace Tab5TileValidation {
// Invalid is proof about a completely read image. Temporary resource or I/O
// failure must never delete a cache or quarantine an archive entry.
enum class Result : uint8_t { Invalid, Valid, Unavailable, NoMemory };
struct Source {
 void* context;
 size_t size;
 size_t (*read)(void*,uint8_t*,size_t);
};
// A short/failed source read is unavailable, never decoder-proven corruption.
Result validateSource(const Source& source, bool (*cancelled)() = nullptr);
Result validate(const uint8_t* data, size_t size, bool (*cancelled)() = nullptr);
// Optional 256×256 native RGB565 destination. No display state or DMA involved.
Result decode(const uint8_t* data, size_t size, uint16_t* pixels,
              bool (*cancelled)() = nullptr);
}
