#pragma once
#include "FS.h"
namespace Tab5UsbStorage {
inline bool path(const char* p){return !strcmp(p,"/USB")||!strncmp(p,"/USB/",5);}
inline bool mounted(){return usbMounted;}
inline uint32_t revision(){return usbGeneration;}
}
