#pragma once
#include <Arduino.h>
namespace Tab5Vault {
bool start(const String& source,const String& destination,const char* password,bool decrypt);
bool busy();
void cancel();
const char* status();
unsigned progress();
}
namespace Tab5Vault {
bool startKey(const String& source,const String& destination,unsigned keySlot,bool decrypt);
int keyForFile(const String& source); // -1: missing key, -2: legacy/password or plain file
bool backupKey(unsigned slot,const String& destination,const char* password);
bool restoreKey(const String& source,const char* password);
// Fast backups bound to this device master, independent of individual keys.
bool backupDevice(unsigned slot,const String& destination);
bool restoreDevice(const String& source);
}
