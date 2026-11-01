#pragma once
#include <Arduino.h>
#include <vector>
namespace Tab5KeyStore {
constexpr unsigned MaxKeys=8;
struct Info {unsigned slot;String name,id;};
struct Record {uint8_t version=1,id[16]{},key[32]{};char name[49]{};};
std::vector<Info> list();
bool read(unsigned slot,Record& record);
int create(const char* name);
int import(const Record& record);
int find(const uint8_t id[16]);
const char* status();
bool configured();
bool deviceConfigured();
bool legacyAvailable();
// Nonblocking state hints: return false while the state mutex is occupied.
// Data access always checks the actual authorization again under its guard.
bool usingDevice();
bool credentialsAccessible();
// Explicitly create/open local mode; migrate copies unlocked legacy keys, never deletes them.
bool openDevice(bool migrate=false);
// PIN gates device access; device NVS still holds the master. It is not
// resistance against an attacker extracting the tablet's internal flash.
bool pinConfigured();
bool unlockDevice(const char* pin,bool migrate=false);
// First use must supply a PIN. Existing stores without a PIN can be upgraded.
bool initializeDevice(const char* pin,bool migrate=false);
// Every change/removal verifies the current PIN, even in an unlocked session.
bool configurePin(const char* pin,const char* currentPin=nullptr);
// UI operations run on an internal-stack worker, never in LVGL callbacks.
bool beginDeviceUnlock(const char* pin,bool initialize=false,bool migrate=false,bool uiContinuation=false);
bool beginPinChange(const char* currentPin,const char* nextPin,bool uiContinuation=false);
bool beginReset(bool uiContinuation=false);
void finishUi();
bool resetPending();
bool operationSucceeded();
unsigned pinRetrySeconds();
bool erase(unsigned slot);
// Internal encrypted-store interface; never expose this material to scripts.
bool deviceKey(uint8_t key[32],uint8_t id[16],uint32_t* revision=nullptr);
bool sessionCurrent(uint32_t revision);
bool sessionToken(uint32_t& revision);
// Nonblocking UI state hint, also false during a worker or lock contention.
bool unlocked();
bool busy();
bool unlock(const char* password,bool changePassword=false);
void lock();
void service(bool screenOff);
}
