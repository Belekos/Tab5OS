#pragma once
#include "Tab5GeoFold.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Original bounded GeoNames reader. The SD database is never loaded into RAM.
// Reader supplies size() and read(offset, destination, bytes), cancellation is
// checked between bounded groups. File offsets and UTF-8 are untrusted input.
namespace Tab5GeoIndex {
constexpr size_t MaxKey=159,MaxResults=8;
inline uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline bool validCoordinate(double lat,double lon){return std::isfinite(lat)&&std::isfinite(lon)&&lat>=-85.051129&&lat<=85.051129&&lon>=-180&&lon<=180;}
inline bool next(const char*& p,uint32_t& cp){
 const auto c=uint8_t(*p++);if(c<0x80){cp=c;return c!=0;}
 const unsigned n=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:0;if(!n)return false;
 cp=c&((1u<<(6-n))-1);
 for(unsigned i=0;i<n;++i){const auto b=uint8_t(*p);if(!b||(b&0xc0)!=0x80)return false;++p;cp=(cp<<6)|(b&63);}
 return !(n==1&&cp<0x80)&&!(n==2&&cp<0x800)&&!(n==3&&cp<0x10000)&&cp<=0x10ffff&&!(cp>=0xd800&&cp<=0xdfff);
}
inline bool utf8(const char* p){if(!p)return false;while(*p){uint32_t cp;if(!next(p,cp)||cp<32||cp==127)return false;}return true;}
inline bool normalize(const char* input,char* output,size_t capacity){
 if(!input||!output||!capacity)return false;size_t used=0;bool pendingSpace=false;
 while(*input){const char* start=input;uint32_t cp;if(!next(input,cp)||cp<32||cp==127){output[0]=0;return false;}
  if(cp>=0x300&&cp<=0x36f)continue;
  if((cp<128&&!((cp>='a'&&cp<='z')||(cp>='A'&&cp<='Z')||(cp>='0'&&cp<='9')))||cp==0x85||cp==0xa0||cp==0x1680||cp==0x3000||(cp>=0x2000&&cp<=0x206f)){pendingSpace=used!=0;continue;}
  char ascii=cp>='A'&&cp<='Z'?char(cp+32):char(cp);const char* bytes=cp<128?&ascii:start;size_t n=cp<128?1:size_t(input-start);
  if(cp>=128&&cp<=0xffff){size_t lo=0,hi=sizeof(folds)/sizeof(folds[0]);while(lo<hi){size_t mid=lo+(hi-lo)/2;if(folds[mid].code<cp)lo=mid+1;else hi=mid;}if(lo<sizeof(folds)/sizeof(folds[0])&&folds[lo].code==cp){bytes=folds[lo].text;n=folds[lo].length;}}
  if(!n)continue;
  if(n==1&&bytes[0]==' '){pendingSpace=used!=0;continue;}
  if(used+n+(pendingSpace?1:0)>=capacity){output[0]=0;return false;}
  if(pendingSpace)output[used++]=' ';pendingSpace=false;memcpy(output+used,bytes,n);used+=n;
 }
 output[used]=0;return used!=0;
}
inline void copy(char* out,size_t capacity,const char* input){if(!capacity)return;size_t n=strlen(input);if(n>=capacity){n=capacity-1;while(n&&(uint8_t(input[n])&0xc0)==0x80)--n;}memcpy(out,input,n);out[n]=0;}
struct Header{uint32_t places=0,keys=0,keyIndex=0,keyData=0,placeIndex=0,placeData=0,bytes=0;};
enum class Status:uint8_t{Ok,Invalid,Io,Cancelled};
struct Candidate{uint32_t place=0,population=0;int32_t latitude=0,longitude=0;double distance=0;bool exact=false;};
struct Result{uint32_t id=0,population=0;double latitude=0,longitude=0;char name[96]{},country[3]{},area[32]{};bool exact=false;};
struct Outcome{Status status=Status::Ok;uint8_t count=0;bool truncated=false;uint32_t scanned=0;};
inline double distanceSquared(double a,double b,double lat,double lon){double dx=fabs(lon-b);dx=std::min(dx,360-dx);dx*=std::cos(a*0.017453292519943295);double dy=lat-a;return dx*dx+dy*dy;}
inline bool better(const Candidate& a,const Candidate& b){if(a.exact!=b.exact)return a.exact;const double bucketA=std::floor(std::sqrt(a.distance)*2),bucketB=std::floor(std::sqrt(b.distance)*2);if(bucketA!=bucketB)return bucketA<bucketB;if(a.population!=b.population)return a.population>b.population;const double delta=a.distance-b.distance;if(fabs(delta)>1e-10)return delta<0;return a.place<b.place;}
template<class Reader> bool header(Reader& reader,Header& h){
 uint8_t b[64];if(reader.size()<sizeof(b)||reader.size()>128u*1024u*1024u||!reader.read(0,b,sizeof(b)))return false;
 if(memcmp(b,"T5GEO01\0",8)||u32(b+8)!=1||u32(b+12)!=64||u32(b+44)!=MaxKey)return false;
 h.places=u32(b+16);h.keys=u32(b+20);h.keyIndex=u32(b+24);h.keyData=u32(b+28);h.placeIndex=u32(b+32);h.placeData=u32(b+36);h.bytes=u32(b+40);
 return h.places&&h.places<=1000000&&h.keys&&h.keys<=3000000&&h.keyIndex==64&&
  uint64_t(h.keyIndex)+uint64_t(h.keys)*4==h.keyData&&h.keyData<h.placeIndex&&
  uint64_t(h.placeIndex)+uint64_t(h.places)*4==h.placeData&&h.placeData<h.bytes&&h.bytes==reader.size();
}
template<class Reader> bool offset(Reader& r,const Header& h,uint32_t index,uint32_t& pos){uint8_t b[4];if(index>=h.keys||!r.read(h.keyIndex+index*4,b,4))return false;const uint64_t p=uint64_t(h.keyData)+u32(b);if(p+17>h.placeIndex)return false;pos=uint32_t(p);return true;}
template<class Reader> bool entry(Reader& r,const Header& h,uint32_t pos,Candidate& c,char* key,uint32_t& after){
 uint8_t b[17];if(pos<h.keyData||uint64_t(pos)+17>h.placeIndex||!r.read(pos,b,17))return false;
 c.place=u32(b);c.latitude=int32_t(u32(b+4));c.longitude=int32_t(u32(b+8));c.population=u32(b+12);unsigned n=b[16];
 if(c.place>=h.places||n<2||n>MaxKey||uint64_t(pos)+17+n>h.placeIndex||!validCoordinate(double(c.latitude)/1e6,double(c.longitude)/1e6)||!r.read(pos+17,key,n))return false;
 key[n]=0;if(memchr(key,0,n)||!utf8(key))return false;after=pos+17+n;return true;
}
template<class Reader> bool place(Reader& r,const Header& h,const Candidate& c,Result& out){
 uint8_t b[8];if(c.place>=h.places||!r.read(h.placeIndex+c.place*4,b,4))return false;
 uint64_t pos=uint64_t(h.placeData)+u32(b);if(pos+8>h.bytes||!r.read(uint32_t(pos),b,8))return false;
 unsigned n=b[4],area=b[7];if(!n||n>95||area>31||pos+8+n+area>h.bytes)return false;
 if((b[5]!=' '&&(b[5]<'A'||b[5]>'Z'))||(b[6]!=' '&&(b[6]<'A'||b[6]>'Z')))return false;
 out={};out.id=u32(b);out.country[0]=char(b[5]);out.country[1]=char(b[6]);
 if(!r.read(uint32_t(pos+8),out.name,n)||(area&&!r.read(uint32_t(pos+8+n),out.area,area)))return false;
 if(memchr(out.name,0,n)||memchr(out.area,0,area)||!utf8(out.name)||!utf8(out.area))return false;
 out.latitude=double(c.latitude)/1e6;out.longitude=double(c.longitude)/1e6;out.population=c.population;out.exact=c.exact;return true;
}
template<class Reader,class Cancel> Outcome search(Reader& r,const char* text,double lat,double lon,Result* results,size_t capacity,Cancel cancel){
 Outcome out;Header h;char query[MaxKey+1],key[MaxKey+1];Candidate selected[MaxResults]{};
 if(!results||!capacity||!normalize(text,query,sizeof(query))||!validCoordinate(lat,lon)){out.status=Status::Invalid;return out;}
 if(cancel()){out.status=Status::Cancelled;return out;}
 if(!header(r,h)){out.status=Status::Invalid;return out;}capacity=std::min(capacity,MaxResults);const size_t n=strlen(query);
 uint32_t lo=0,hi=h.keys;
 while(lo<hi){uint32_t mid=lo+(hi-lo)/2,pos,after;Candidate c;
  if(cancel()){out.status=Status::Cancelled;return out;}
  if(!offset(r,h,mid,pos)||!entry(r,h,pos,c,key,after)){out.status=Status::Io;return out;}
  if(strcmp(key,query)<0)lo=mid+1;else hi=mid;
 }
 if(lo==h.keys)return out;uint32_t pos;if(!offset(r,h,lo,pos)){out.status=Status::Io;return out;}
 for(uint32_t index=lo;index<h.keys;++index){
  if((out.scanned%64)==0&&cancel()){out.status=Status::Cancelled;out.count=0;return out;}
  Candidate c;uint32_t after;if(!entry(r,h,pos,c,key,after)){out.status=Status::Io;out.count=0;return out;}
  pos=after;if(strncmp(key,query,n))break;
  if(out.scanned++>=50000){out.truncated=true;break;}
  c.exact=key[n]==0;c.distance=distanceSquared(lat,lon,double(c.latitude)/1e6,double(c.longitude)/1e6);
  size_t duplicate=0;while(duplicate<out.count&&selected[duplicate].place!=c.place)++duplicate;
  if(duplicate<out.count){if(!better(c,selected[duplicate]))continue;for(size_t i=duplicate;i+1<out.count;++i)selected[i]=selected[i+1];--out.count;}
  size_t at=0;while(at<out.count&&!better(c,selected[at]))++at;
  if(at>=capacity)continue;for(size_t i=std::min<size_t>(out.count,capacity-1);i>at;--i)selected[i]=selected[i-1];selected[at]=c;if(out.count<capacity)++out.count;
 }
 for(size_t i=0;i<out.count;++i){if(cancel()){out.status=Status::Cancelled;out.count=0;return out;}if(!place(r,h,selected[i],results[i])){out.status=Status::Io;out.count=0;return out;}}
 return out;
}
}
