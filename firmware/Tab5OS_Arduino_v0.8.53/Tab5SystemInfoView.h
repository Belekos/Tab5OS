#pragma once
#include <lvgl.h>
#include <stdint.h>

namespace Tab5SystemInfoView {
enum MeterId : unsigned { Internal, External, Firmware, Count };
struct Meter { uint64_t used=0,total=0; char note[128]{}; };
struct Snapshot {
  char heading[96]{},hardware[224]{},power[160]{},connections[256]{},display[192]{},memoryLayout[768]{};
  Meter meters[Count];
};
struct Hooks {
  const char* (*translate)(const char*)=nullptr;
  const lv_font_t* (*font)()=nullptr;
  void (*refresh)()=nullptr;
  void (*retestSd)()=nullptr;
  void (*closed)()=nullptr;
};
void open(Hooks);
void close();
void update(const Snapshot&);
void details(const char*);
bool wantsDetails();
lv_obj_t* scope();
lv_obj_t* body();
}
