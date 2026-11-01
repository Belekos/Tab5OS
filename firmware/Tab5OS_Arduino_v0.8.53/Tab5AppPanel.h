#pragma once
#include "Tab5AppApi.h"
#include <stddef.h>
#include <stdint.h>
// Generic ELF form/console broker. No application-specific analysis or worker.
namespace Tab5AppPanel {
struct Specification {
 char title[129]{},modes[1025]{},targetLabel[129]{},targetHint[257]{};
 char valueLabel[129]{},value[513]{},runLabel[129]{},initialTarget[2049]{};
 char alternateHint[257]{};
 uint32_t valueModes=0,alternateModes=0,modeCount=0;
};
int open(const tab5_app_panel_spec*);
int poll(tab5_app_panel_event*);
int write(const char*,size_t,int reset);
int state(uint32_t);
int network(int argc,const char* const* argv,void (*output)(void*,const char*,size_t),void*,bool (*stop)());
size_t translate(const char*,char*,size_t);
// UI task functions. detach is called only after the ELF worker's last access.
bool takeOpenRequest();
bool active();
bool closing();
bool busy();
bool cancelled();
const Specification* specification();
bool submit(uint32_t mode,const char* target,const char* value);
void stop();
void clear();
void close();
void detach();
uint32_t revision();
const char* snapshot(uint32_t& revision);
const char* completedReport(size_t& length);
const char* status();
}
