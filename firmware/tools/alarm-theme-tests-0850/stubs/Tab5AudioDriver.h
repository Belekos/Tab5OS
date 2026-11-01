#pragma once
#include <cstdint>
#include <cstddef>
enum class Tab5AudioClient {Alarm};
enum class Tab5PcmFormat {S16};
struct Tab5PcmSpec {unsigned sampleRate=0,channels=0;Tab5PcmFormat format=Tab5PcmFormat::S16;};
extern bool configureOk;
extern unsigned claims,releases,stops,pcmWrites;
extern void(*onPcm)();
struct DriverStub {
 uint8_t v=123;
 uint8_t volume(){return v;}
 void setVolume(uint8_t value){v=value;}
 bool claim(Tab5AudioClient,unsigned){++claims;return true;}
 bool configure(Tab5AudioClient,const Tab5PcmSpec&){return configureOk;}
 bool writePcm(Tab5AudioClient,const int16_t*,size_t,const Tab5PcmSpec&){++pcmWrites;if(onPcm)onPcm();return true;}
 void stop(Tab5AudioClient){++stops;}
 void release(Tab5AudioClient){++releases;}
};
extern DriverStub Tab5AudioDevice;
