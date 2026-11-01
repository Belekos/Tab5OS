#pragma once
#include <Arduino.h>
namespace Tab5Firmware {
void clearReview(); bool inspect(const char* path);
// A selected file can be installed directly. The optional preliminary review
// pins that file's size/hash, but is not a prerequisite for installation.
// With no path, retain the original reviewed-file-only calling convention.
bool install(const char* path=nullptr); void cancel(); bool busy();
bool ready(); bool installed(); unsigned progress(); const char* status();
String description(); void restart();
}
