#pragma once
#include <stdint.h>

struct _lv_display_t;
namespace Tab5DisplayActivity {
enum class Source : uint8_t {
  Other, Timers, ClockPower, Audio, Network, Map, Files, Writing, Python,
  Tools, Video, Home, System, Count
};
constexpr unsigned sourceCount = static_cast<unsigned>(Source::Count);
struct Stats {
  uint32_t invalidations[sourceCount]{};
  uint64_t flushedPixels = 0;
  uint32_t flushAreas = 0;
  uint32_t frames = 0;
  uint32_t lastFlushMs = 0;
  uint32_t lastInvalidationMs = 0;
  Source lastSource = Source::Other;
  uint32_t largeInvalidations = 0;
  uint32_t lastLargeInvalidationMs = 0;
  uint32_t lastLargeInvalidationPixels = 0;
  Source lastLargeSource = Source::Other;
  uint32_t largeFrames = 0;
  uint32_t lastLargeFrameMs = 0;
  uint32_t maxFramePixels = 0;
  bool attached = false;
};

// All calls remain on the LVGL/UI task. The observer owns no timer, task, file,
// framebuffer or recurring allocation. Requests may overlap; they are not pixels.
bool begin(_lv_display_t* display);
Stats snapshot();
Stats difference(const Stats& after, const Stats& before);
void flushed(int32_t width, int32_t height, bool last, uint32_t now);
const char* sourceName(Source source);
class Scope {
 public:
  explicit Scope(Source source);
  ~Scope();
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;
 private:
  Source previous_;
};
}
