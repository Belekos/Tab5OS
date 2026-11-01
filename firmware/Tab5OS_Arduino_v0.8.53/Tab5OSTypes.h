#pragma once

#include <Arduino.h>

// Types used in function signatures are kept in a real header so Arduino's
// automatic .ino prototype generator always sees them before generated
// prototypes. This avoids "does not name a type" errors with custom structs.
struct RichFontBlob {
  uint8_t* data = nullptr;
  size_t size = 0;
};

struct RichLoc {
  uint16_t block = 0;
  uint32_t offset = 0;
};

// Kept in this header because it appears in a function signature. Arduino's
// generated prototypes are inserted before declarations written in the .ino.
struct EditorSelection {
  uint32_t startChar = 0;
  uint32_t endChar = 0;
  size_t startByte = 0;
  size_t endByte = 0;
};

enum class HomeAction : uint8_t {
  Home,
  Files,
  Editor,
  Reader,
  Audio,
  Map,
  Settings,
  Calculator,
  Python, Theme, NativeApps, Terminal, Alarm, Sdr = 15, Storage = 16, Pentest = 17, ImageEditor = 18, Meshtastic = 19
};

// This clock type appears in .ino function signatures, so it lives in a
// header that Arduino's automatic prototype generator sees first.
struct TimeZoneDef {
  const char* label;
  const char* posix;
};

struct HomeCardDef {
  HomeAction action;
  const char* title;
};

struct ThemePalette {
  const char* name;
  uint32_t bg;
  uint32_t surface;
  uint32_t raised;
  uint32_t border;
  uint32_t text;
  uint32_t muted;
  uint32_t active;
  uint32_t cursor;
  uint32_t selected;
  uint32_t pointer=0xFFAA20;
  uint32_t pointerOutline=0x101214;
  uint32_t switchOff=UINT32_MAX; // Old themes inherit raised / active.
  uint32_t switchOn=UINT32_MAX;
  uint32_t switchKnobOff=UINT32_MAX; // Legacy knobs inherit text / surface.
  uint32_t switchKnobOn=UINT32_MAX;
};

struct HistorySnapshot {
  char* text = nullptr;
  size_t len = 0;
  int32_t cursor = 0;
};

struct TextStats {
  size_t bytes = 0;
  size_t chars = 0;
  size_t words = 0;
};

