#pragma once
#include "Tab5AppCataloguePolicy.h"
#include <lvgl.h>
namespace Tab5AppCatalogue {
enum class Kind : uint8_t { Builtin, Script, Executable };
struct App {
  char id[272]{};
  char title[96]{};
  uint64_t key=0;
  Kind kind=Kind::Builtin;
  uint8_t action=0, symbol=0;
};
// UI API. A one-shot worker owns scan I/O; only the UI publishes a completed table.
// Bounded catalogue storage lives in PSRAM, not the internal heap.
bool reset();
bool addBuiltin(const char* id,const char* title,uint8_t action,uint8_t symbol);
unsigned count();
// Changes only when a complete scan publishes different entries.
uint32_t revision();
const App* at(unsigned index);
bool beginScan(bool mounted);
// Scan into a PSRAM staging table; visible entries survive cancellation/errors.
// 0 scanning, 1 complete, 2 bounded/truncated, -1 storage failure.
int scanStep();
// Cancellation is cooperative; a new request waits for the old I/O owner to retire.
void endScan();
Choices& choices();
bool loadChoices();
bool saveChoices();
bool pinned(unsigned index);
uint8_t icon(unsigned index);
bool pin(unsigned index,bool value);
bool setIcon(unsigned index,uint8_t symbol);
bool move(unsigned index,int direction);
unsigned ordered(unsigned* indices,bool onlyPinned);
// Auto icons: neighbour .ico, app-folder icon.ico, then /Icons/<id>.ico.
// BMP ICO only, 24/32 bit, <=64 px, <=32 KiB; unsupported files use a symbol.
bool attachIcon(lv_obj_t* container,unsigned index);
}
