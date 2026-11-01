#pragma once
#include <stdint.h>
#include <stddef.h>

namespace Tab5AlarmTone {
constexpr uint8_t Count=4;
constexpr uint32_t SampleRate=24000;
constexpr uint32_t Period=48000;
inline uint8_t normalize(uint8_t value){return value<Count?value:0;}
struct Note {uint16_t startMs,endMs,hz;};
inline int16_t sample(uint8_t tone,uint32_t sampleIndex){
 static constexpr Note notes[Count][4]={
  {{0,250,750},{420,670,750},{0,0,0},{0,0,0}},
  {{0,280,523},{320,600,659},{640,1000,784},{0,0,0}},
  {{0,140,880},{220,360,880},{440,580,880},{660,800,880}},
  {{0,650,440},{900,1550,554},{0,0,0},{0,0,0}}
 };
 static constexpr int16_t sine[]={0,2146,4209,6111,7778,9146,10163,10789,11000,10789,10163,9146,7778,6111,4209,2146,0,-2146,-4209,-6111,-7778,-9146,-10163,-10789,-11000,-10789,-10163,-9146,-7778,-6111,-4209,-2146};
 const uint32_t position=sampleIndex%Period;
 for(const auto& note:notes[normalize(tone)]){
  const uint32_t begin=uint32_t(note.startMs)*24,end=uint32_t(note.endMs)*24;
  if(!note.hz||position<begin||position>=end)continue;
  const uint32_t elapsed=position-begin,remaining=end-position-1;
  uint32_t envelope=elapsed<240?elapsed:240;
  if(remaining<envelope)envelope=remaining;
  // Note-local phase and a 10 ms envelope make the signal independent of
  // buffer boundaries and keep every transition to silence at zero.
  const uint32_t phase=(elapsed*note.hz)%SampleRate;
  const int32_t amplitude=normalize(tone)==3?7000:11000;
  return int16_t(int32_t(sine[phase*32/SampleRate])*int32_t(envelope)/240*amplitude/11000);
 }
 return 0;
}
inline void render(uint8_t tone,uint32_t& sampleIndex,int16_t* output,size_t count){
 for(size_t i=0;i<count;++i){output[i]=sample(tone,sampleIndex);sampleIndex=(sampleIndex+1)%Period;}
}
}
