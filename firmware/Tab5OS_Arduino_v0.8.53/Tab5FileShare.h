#pragma once
#include <stdint.h>
// A temporary local browser session. No server, socket or worker exists at boot.
// The effective OS locale is captured when sharing starts; french is retained for old callers.
bool tab5FileShareStart(bool accessPoint = false, bool french = false);
// UI task only, when Audio/Writing storageIdle and no MapPrint job.
// Tries a brief exclusive storage lease; never waits for another SD user.
void tab5FileShareService();
void tab5FileShareStop();
bool tab5FileShareBusy();
bool tab5FileShareActive();
bool tab5FileShareAccessPoint();
uint32_t tab5FileShareCode();
uint32_t tab5FileShareSecondsRemaining();
const char* tab5FileShareUrl();
const char* tab5FileShareSsid();
const char* tab5FileSharePassword();
const char* tab5FileShareError();
