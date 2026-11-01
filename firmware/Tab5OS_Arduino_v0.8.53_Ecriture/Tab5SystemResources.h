#pragma once
#include <stddef.h>

// Only missing official support files are restored. Documents, settings and
// customisations already present on the card are never replaced.
namespace Tab5SystemResources {
bool start(bool sdMounted);
void service(); // UI task, one bounded 4 KiB decode/I/O operation per invocation
void cancel();
bool busy();
const char* status();
size_t created();
size_t checked();
size_t total();
}
