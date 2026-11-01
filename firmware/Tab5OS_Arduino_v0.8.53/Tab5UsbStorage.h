#pragma once
#include <stdint.h>
#include "Tab5UsbFormatPolicy.h"
namespace Tab5UsbStorage {
using Format=Tab5UsbFormatPolicy::Format;
enum class FormatState:uint8_t {Idle,Waiting,Running,Succeeded,Failed,Cancelled};
bool begin();
void serviceWorker(); // existing USB device worker only
bool mounted();
bool attached();
bool pending();
bool ejecting();
const char* status();
uint32_t revision();
uint64_t capacityBytes(); // physical capacity, not free filesystem space
uint32_t connectionToken(); // changes on disconnect; confirmation must retain it
bool supported(Format format); // geometry and this firmware's actual formats
bool requestFormat(Format format,uint32_t expectedToken); // explicit confirmation only
FormatState formatState();
bool cancelFormat(); // waiting only; cannot safely cancel after first write
void mount();
void eject();
void fault(int error);
void automatic(bool enabled);
bool automatic();
bool path(const char* path);
bool retain(uint32_t& generation);
void release();
bool valid(uint32_t generation);
}
