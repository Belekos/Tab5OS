#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include "Tab5MapViewPolicy.h"

// Fixed memory plan for the area visible when the import dialog opens. No
// allocation proportional to the requested zoom or number of downloaded tiles.
namespace Tab5MapImport {
inline bool publicHost(const char* url, const char* domain) {
  if (!url || !domain) return false;
  const char* host = strstr(url, "://"); if (!host) return false; host += 3;
  const char* end = strpbrk(host, "/?#"); if (!end) end = host + strlen(host);
  for(const char* p=host;p<end;++p)if(*p=='@')host=p+1;
  for(const char* p=host;p<end;++p)if(*p==':'){end=p;break;}
  if(end>host && end[-1]=='.')--end;
  const size_t size = size_t(end-host), suffix = strlen(domain);
  if (size < suffix || (size > suffix && host[size-suffix-1] != '.')) return false;
  for (size_t i=0;i<suffix;++i) {
    char c=host[size-suffix+i]; if(c>='A'&&c<='Z')c+=32;
    if(c!=domain[i])return false;
  }
  return true;
}
inline bool allowed(const char* url) { return !publicHost(url,"tile.openstreetmap.org"); }
inline uint16_t effectiveRate(uint16_t tenths, const char* url) {
  if (tenths<1)tenths=1; if(tenths>40)tenths=40;
  return publicHost(url,"tile.opentopomap.org") && tenths>5 ? 5 : tenths;
}
inline uint32_t interval(uint16_t tenths) { return (10000u+tenths-1)/tenths; }
template<class Current,class Clock,class Wait>
bool waitForSlot(bool haveRequest,uint32_t last,uint32_t gap,Current current,Clock now,Wait wait){
  while(haveRequest && uint32_t(now()-last)<gap){if(!current())return false;wait();}
  return current();
}
class Plan {
 public:
  struct Level { int64_t firstX=0,lastX=-1; uint32_t firstY=0,lastY=0; uint8_t z=0; };
  bool prepare(double lat,double lon,uint8_t view,int width,int height,
               uint8_t low,uint8_t high,uint8_t maximum,int8_t shift) {
    count=0;levels=0;reset();
    if(!std::isfinite(lat)||!std::isfinite(lon)||lat < -85.05112878||lat > 85.05112878||
       lon < -180||lon > 180||view>22||high>22||low>high||width<=0||height<=0)return false;
    const double pi=3.14159265358979323846;
    const double cx=(lon+180)/360,cy=(1-std::log(std::tan(pi/4+lat*pi/360))/pi)/2;
    const double world=256.0*double(1u<<view),dx=width/(2*world),dy=height/(2*world);
    const uint8_t z0=Tab5MapViewPolicy::sourceZoom(low,maximum,shift);
    const uint8_t z1=Tab5MapViewPolicy::sourceZoom(high,maximum,shift);
    for(uint8_t z=z0;z<=z1;++z){
      const uint32_t n=1u<<z;auto& r=rows[levels++];r.z=z;
      r.firstX=int64_t(std::floor((cx-dx)*n));r.lastX=int64_t(std::floor(std::nextafter((cx+dx)*n,-INFINITY)));
      if(r.lastX-r.firstX+1>n){r.firstX=0;r.lastX=n-1;}
      const int64_t y0=int64_t(std::floor((cy-dy)*n));
      const int64_t y1=int64_t(std::floor(std::nextafter((cy+dy)*n,-INFINITY)));
      r.firstY=uint32_t(y0<0?0:y0>=n?n-1:y0);r.lastY=uint32_t(y1<0?0:y1>=n?n-1:y1);
      count+=uint64_t(r.lastX-r.firstX+1)*uint64_t(r.lastY-r.firstY+1);
    }
    reset();return levels && count;
  }
  void reset(){level=0;x=0;y=0;started=false;}
  bool next(uint8_t& zoom,uint32_t& outX,uint32_t& outY){
    if(level>=levels)return false;auto& r=rows[level];
    if(!started){x=r.firstX;y=r.firstY;started=true;}
    const int64_t n=int64_t(1u<<r.z);zoom=r.z;outX=uint32_t((x%n+n)%n);outY=y;
    if(++x>r.lastX){x=r.firstX;if(++y>r.lastY){++level;started=false;}}
    return true;
  }
  uint64_t count=0;
 private:
  Level rows[23]{};uint8_t levels=0,level=0;int64_t x=0;uint32_t y=0;bool started=false;
};
}
