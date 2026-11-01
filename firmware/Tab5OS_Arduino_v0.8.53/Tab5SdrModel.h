#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace Tab5Sdr {
constexpr unsigned BinCount=256, MaxDetections=2048, MaxScanSteps=20000;
enum Mode : unsigned { Spectrum=0, Wfm=1, Am=2, Nfm=3 };
struct Settings { uint32_t hz=100000000; unsigned mode=Wfm,direct=0; int ppm=0; unsigned deemphasis=50; };
inline bool valid(const Settings& s){return s.hz>0&&s.mode<=Nfm&&s.direct<=2&&s.ppm>=-200&&s.ppm<=200&&(s.deemphasis==0||s.deemphasis==50||s.deemphasis==75);}
inline bool frequency(const char* text,uint32_t multiplier,uint32_t& hz){
 if(!text||!*text||!multiplier)return false;
 unsigned dots=0,digits=0;for(const char* p=text;*p;++p){if(*p=='.'){if(++dots>1)return false;}else if(*p>='0'&&*p<='9')++digits;else return false;}
 if(!digits)return false;char* end=nullptr;double value=strtod(text,&end)*multiplier;
 if(!end||*end||!std::isfinite(value)||value<1||value>UINT32_MAX)return false;
 hz=uint32_t(value+.5);return hz>0;
}
struct Scan {uint32_t first=87500000,last=108000000,step=100000,dwell=400;int threshold=-45;bool hold=false;};
inline bool valid(const Scan& s){return s.first&&s.last>=s.first&&s.step&&s.dwell>=150&&s.dwell<=10000&&s.threshold>=-120&&s.threshold<=0&&uint64_t(s.last-s.first)/s.step+1<=MaxScanSteps;}
inline bool nextFrequency(const Scan& s,uint32_t current,uint32_t& next){
 if(!valid(s)||current<s.first||current>s.last||uint64_t(current)+s.step>s.last)return false;
 next=current+s.step;return true;
}
struct Detection {uint32_t hz=0;uint8_t mode=0,direct=0;int16_t db=-120;};
struct DetectionList {
 Detection values[MaxDetections]{};unsigned count=0;bool full=false;
 bool add(const Detection& d){
  if(!valid(Settings{d.hz,d.mode,d.direct})||d.db< -120||d.db>0)return false;
  for(unsigned i=0;i<count;++i)if(values[i].hz==d.hz&&values[i].mode==d.mode&&values[i].direct==d.direct){values[i].db=std::max(values[i].db,d.db);return true;}
  if(count>=MaxDetections){full=true;return false;}values[count++]=d;return true;
 }
 void erase(unsigned i){if(i>=count)return;for(unsigned j=i+1;j<count;++j)values[j-1]=values[j];--count;full=false;}
 size_t serialize(char* output,size_t capacity)const{
  if(!output||capacity<17)return 0;int first=snprintf(output,capacity,"TAB5-SDR,1\n");if(first<0||size_t(first)>=capacity)return 0;size_t used=size_t(first);
  for(unsigned i=0;i<count;++i){const auto& d=values[i];int n=snprintf(output+used,capacity-used,"%lu,%u,%u,%d\n",(unsigned long)d.hz,d.mode,d.direct,d.db);if(n<0||size_t(n)>=capacity-used)return 0;used+=size_t(n);}
  return used;
 }
 bool parse(const char* data,size_t length){
  static constexpr char Header[]="TAB5-SDR,1\n";if(!data||length<sizeof(Header)-1||memcmp(data,Header,sizeof(Header)-1))return false;
  count=0;full=false;size_t at=sizeof(Header)-1;
  while(at<length){size_t end=at;while(end<length&&data[end]!='\n')++end;if(end==length||end-at>47||end==at||count==MaxDetections)return false;
   char line[48];memcpy(line,data+at,end-at);line[end-at]=0;unsigned long hz=0;unsigned mode=0,direct=0;int db=0,consumed=0;
   if(sscanf(line,"%lu,%u,%u,%d%n",&hz,&mode,&direct,&db,&consumed)!=4||line[consumed]||hz>UINT32_MAX||!valid(Settings{uint32_t(hz),mode,direct})||db< -120||db>0)return false;
   if(!add(Detection{uint32_t(hz),uint8_t(mode),uint8_t(direct),int16_t(db)}))return false;at=end+1;
  }return true;
 }
};
struct Preset {const char* name;uint32_t first,last,step;unsigned mode;};
// Receive suggestions, never regulatory/allocation claims or tuner limits.
constexpr Preset Presets[]={
 {"Libre",87500000,108000000,100000,Wfm},
 {"Ondes longues",150000,285000,9000,Am},
 {"Ondes moyennes",520000,1710000,9000,Am},
 {"Ondes courtes",3000000,30000000,5000,Am},
 {"FM 64–108 MHz",64000000,108000000,100000,Wfm},
 {"FM 87,5–108 MHz",87500000,108000000,100000,Wfm},
 {"VHF aviation",118000000,136975000,25000,Am},
 {"VHF 144–148 MHz",144000000,148000000,12500,Nfm},
 {"VHF marine",156000000,162025000,25000,Nfm},
 {"UHF 430–440 MHz",430000000,440000000,12500,Nfm},
 {"UHF 446 MHz",446000000,446200000,6250,Nfm},
 {"UHF 863–870 MHz",863000000,870000000,25000,Spectrum}
};
}
