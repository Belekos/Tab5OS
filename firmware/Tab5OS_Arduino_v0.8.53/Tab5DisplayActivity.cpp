#include "Tab5DisplayActivity.h"
#include <Arduino.h>
#include <lvgl.h>

namespace Tab5DisplayActivity {
namespace {
Stats totals;
Source current = Source::Other;
lv_display_t* watched = nullptr;
uint64_t framePixels = 0;
uint32_t displayPixels(){
  if(!watched)return 0;
  const int32_t w=lv_display_get_horizontal_resolution(watched),h=lv_display_get_vertical_resolution(watched);
  return w>0&&h>0?uint32_t(w)*uint32_t(h):0;
}
void event(lv_event_t* e) {
  const auto code = lv_event_get_code(e);
  if (code == LV_EVENT_DELETE) {
    watched = nullptr;
    totals.attached = false;
    return;
  }
  if (code != LV_EVENT_INVALIDATE_AREA) return;
  ++totals.invalidations[static_cast<unsigned>(current)];
  totals.lastInvalidationMs = millis();
  totals.lastSource = current;
  const auto* area=static_cast<const lv_area_t*>(lv_event_get_param(e));
  if(area){
    const int32_t width=lv_area_get_width(area),height=lv_area_get_height(area);
    const uint32_t screen=displayPixels();
    const uint64_t pixels=width>0&&height>0?uint64_t(uint32_t(width))*uint32_t(height):0;
    if(screen&&pixels>=screen/2){
      ++totals.largeInvalidations;totals.lastLargeInvalidationMs=totals.lastInvalidationMs;
      totals.lastLargeSource=current;totals.lastLargeInvalidationPixels=pixels>UINT32_MAX?UINT32_MAX:uint32_t(pixels);
    }
  }
}
}

bool begin(lv_display_t* display) {
  if (!display) return false;
  if (watched) return watched == display;
  if (!lv_display_add_event_cb(display, event, LV_EVENT_ALL, nullptr)) return false;
  watched = display;
  totals.attached = true;
  return true;
}
Stats snapshot() { return totals; }
Stats difference(const Stats& after, const Stats& before) {
  auto result = after;
  for (unsigned i = 0; i < sourceCount; ++i)
    result.invalidations[i] = after.invalidations[i] - before.invalidations[i];
  result.flushedPixels -= before.flushedPixels;
  result.flushAreas -= before.flushAreas;
  result.frames -= before.frames;
  result.largeInvalidations -= before.largeInvalidations;
  result.largeFrames -= before.largeFrames;
  return result;
}
void flushed(int32_t width, int32_t height, bool last, uint32_t now) {
  if (width <= 0 || height <= 0) return;
  const uint64_t pixels=uint64_t(uint32_t(width))*uint32_t(height);
  totals.flushedPixels += pixels;framePixels += pixels;
  ++totals.flushAreas;
  if (last){
    ++totals.frames;
    if(framePixels>totals.maxFramePixels)totals.maxFramePixels=framePixels>UINT32_MAX?UINT32_MAX:uint32_t(framePixels);
    const uint32_t screen=displayPixels();
    if(screen&&framePixels>=screen/2){++totals.largeFrames;totals.lastLargeFrameMs=now;}
    framePixels=0;
  }
  totals.lastFlushMs = now;
}
Scope::Scope(Source source) : previous_(current) {
  current = static_cast<unsigned>(source) < sourceCount ? source : Source::Other;
}
Scope::~Scope() { current = previous_; }
const char* sourceName(Source source) {
  switch (source) {
    case Source::Timers: return "Timers LVGL";
    case Source::ClockPower: return "Horloge et batterie";
    case Source::Audio: return "Audio";
    case Source::Network: return "Réseau";
    case Source::Map: return "Cartes";
    case Source::Files: return "Fichiers";
    case Source::Writing: return "Écriture";
    case Source::Python: return "Python";
    case Source::Tools: return "Outils";
    case Source::Video: return "Vidéo";
    case Source::Home: return "Accueil";
    case Source::System: return "Système";
    default: return "Autres";
  }
}
}
