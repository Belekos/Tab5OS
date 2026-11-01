#pragma once
#include <Arduino.h>
#include <vector>
// Values exist only in unlocked RAM and authenticated encrypted SD files.
// Names follow shell identifier syntax; callers must wipe copied values.
namespace Tab5Secrets {
constexpr unsigned MaxNames=64;
constexpr size_t MaxNameBytes=48,MaxValueBytes=512;
bool available();
// Internal device-unlock worker only. Public reads remain closed while busy.
bool preload();
// Nonblocking UI hint; false while a worker owns the cache mutex.
bool ready();
bool get(const char* name,char* output,size_t capacity);
bool set(const char* name,const char* value);
bool erase(const char* name);
std::vector<String> names();
const char* status();
void clearCache();
// Explicit recovery only: move the entire encrypted directory aside. Never
// delete user files, and never overwrite an earlier recovery directory.
bool retireStore(const char* destination);
}
