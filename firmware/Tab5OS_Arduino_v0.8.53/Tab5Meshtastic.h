#pragma once
#include <lvgl.h>
#include "WriterKeys.h"
#include "Tab5MeshModel.h"
namespace Tab5Meshtastic {
using Options=Tab5MeshModel::Options;
struct Position {uint32_t node=0,timestamp=0;double latitude=0,longitude=0;char name[40]{};};
struct Badge {bool active=false,ready=false,pairing=false;uint16_t unread=0;uint32_t revision=0;};
struct Hooks {
 const char* (*translate)(const char*)=nullptr;
 const lv_font_t* (*font)()=nullptr;
 uint32_t (*color)(unsigned)=nullptr;
 // Automatic focus may defer to a physical keyboard; an explicit request may not.
 void (*keyboard)(lv_obj_t*,bool explicitRequest)=nullptr;
 void (*closeKeyboard)()=nullptr;
 void (*openMap)(uint32_t node,double latitude,double longitude)=nullptr;
 bool (*storageAvailable)()=nullptr;
};
void begin(Hooks);
void open();
// Settings can open without a radio/session allocation or Bluetooth startup.
void openSettings(lv_obj_t* returnScope=nullptr);
void closeSettings();
bool settingsVisible();
void close(); // Background option keeps the radio active.
void stop();  // Explicit stop; background is not restarted until resumed/opened.
void service();
void resize(int height);
bool visible();
bool busy();
lv_obj_t* scope();
void key(const writing::KeyEvent&);
Options options();
void setOptions(Options);
Badge badge();
void markRead();
size_t positionCount();
bool position(size_t index,Position& result);
uint32_t positionRevision();
bool showOnMap(uint32_t node);
}
