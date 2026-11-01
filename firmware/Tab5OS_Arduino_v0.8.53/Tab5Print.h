#pragma once
#include <Arduino.h>
namespace Tab5Print {
struct Options {String uri,path;uint32_t textStart=0,textEnd=0,textSourceSize=0;unsigned dpi=300;unsigned paper=0;unsigned scalePercent=100;bool landscape=false;bool temporarySource=false;};
bool start(const Options& options,bool probeOnly=false);
bool estimate(const Options& options);
unsigned pages();bool estimating();
bool certificatePending();bool trustCertificate();
bool busy();void cancel();const char* status();String detail();unsigned progress();
}
