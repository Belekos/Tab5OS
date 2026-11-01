#pragma once
#include "Tab5SerialCore.h"
#include <functional>
#include <string>
#include <vector>
namespace Tab5Serial {
bool begin();
Config config();
// Applies and persists only an actual change. Refuses during a pending send.
bool configure(const Config&,bool persist=true);
bool requestConfigure(const Config&,bool persist=true); // Queue only; UI never waits for setup/NVS.
// UI-only nonblocking access; false means the existing worker owns the state.
bool tryRequestConfigure(const Config&,bool persist=true);
bool tryStats(Stats&);
bool tryClear();
Stats stats();
size_t write(const void*,size_t); // Raw, non-blocking, possibly partial.
size_t read(void*,size_t,Client owner=Client::None); // Raw; one explicit reader owns a session.
bool claimReceive(Client);
void releaseReceive(Client);
bool sendText(const char*,size_t length=SIZE_MAX); // Copies up to 512 KiB to bounded PSRAM; asynchronous.
bool sendSnapshot(size_t,const std::function<void(char*,size_t)>& fill,Client owner); // One PSRAM snapshot, no extra full-document copy.
void cancelSend(Client owner=Client::None); // TX only; never destroys received data.
void clear();                    // Cancels queued transmit and empties receive.
void debugWrite(const char*,size_t); // Non-blocking mirror; never used in data mode.
int debugAvailable();int debugRead(); // Diagnostic input only; never consumes data RX.
using Output=std::function<void(const char*,size_t)>;
using Cancel=std::function<bool()>;
using Translate=std::function<std::string(const char*)>;
using Input=std::function<size_t(char*,size_t)>;
int command(const std::vector<std::string>&,Output,Cancel,Translate,Input={});
}
#ifdef ARDUINO
#include <Arduino.h>
// Existing UART0 diagnostics are retained. The selected debug port receives a
// best-effort bounded mirror; data traffic never uses this object.
class Tab5SerialDebug:public Print {
public:
 using Print::write;
 void begin(unsigned long baud){Serial0.begin(baud);Tab5Serial::begin();}
 void flush(){Serial0.flush();}
 int available(){int uart=Serial0.available();return uart?uart:Tab5Serial::debugAvailable();}
 int read(){return Serial0.available()?Serial0.read():Tab5Serial::debugRead();}
 size_t write(uint8_t byte)override{return write(&byte,1);}
 size_t write(const uint8_t* p,size_t n)override {size_t result=Serial0.write(p,n);Tab5Serial::debugWrite(reinterpret_cast<const char*>(p),n);return result;}
};
extern Tab5SerialDebug Tab5DebugSerial;
#endif
