#pragma once
#include "Tab5SdrModel.h"
namespace Tab5Sdr {
struct Frame {int8_t levels[BinCount]{};uint32_t hz=0,sampleRate=240000;};
bool start(uint32_t hz,unsigned mode);
bool start(const Settings&);
bool configure(const Settings&);
bool scan(const Scan&,const Settings&);
void pauseScan(bool pause);
bool scanning();bool scanPaused();
Settings current();
void stop();bool busy();bool stopping();const char* status();
bool spectrum(Frame&);
bool spectrum(uint8_t*);
bool takeDetection(Detection&);
bool detectionOverflow();
void volume(unsigned percent);
}
