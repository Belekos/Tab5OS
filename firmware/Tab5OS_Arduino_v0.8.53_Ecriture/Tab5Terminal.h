#pragma once
#include <Arduino.h>
namespace Tab5Terminal {
bool open();
bool submit(const char* command);
// Starts an Explorer .sh in its parent directory, safely quoted.
bool runScript(const char* logicalPath);
bool busy();
void stop();
void clearOutput();
void close();
void service();
uint32_t revision();
// UI-thread snapshot; valid until next snapshot or close/service on the UI thread.
const char* snapshot(uint32_t& revision);
String output();
String directory();
String history(int direction,const char* current);
String complete(const char* current);
}
