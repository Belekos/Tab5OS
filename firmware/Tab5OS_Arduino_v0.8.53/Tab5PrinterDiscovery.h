#pragma once
#include <Arduino.h>
#include <vector>
namespace Tab5Printers {
struct Printer {String name,uri;bool secure=false;};
bool search();bool busy();const char* status();
// UI-thread API. Loading or reading the saved list never starts discovery.
void service();
String selected();
bool remember(const String& uri);
// Last completed successful scan, retained while scanning and across reboots.
// Maximum 24 endpoints. Failed scans leave this snapshot intact.
std::vector<Printer> results();
}
