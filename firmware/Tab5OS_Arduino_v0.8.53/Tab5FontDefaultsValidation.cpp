#include "Tab5FontDefaults.h"
#include <cstring>
#include <strings.h>
namespace Tab5FontDefaults {
bool validPath(const char* path){
 if(!path||path[0]!='/')return false;
 const size_t n=strnlen(path,PathCapacity);if(n<5||n>=PathCapacity||strcasecmp(path+n-4,".ttf"))return false;
 size_t start=1;
 for(size_t i=1;i<=n;++i){const unsigned char c=static_cast<unsigned char>(path[i]);
  if(i<n&&(c<32||c==127||c=='\\'||c==':'))return false;
  if(i==n||c=='/'){const size_t length=i-start;if(!length||(length==1&&path[start]=='.')||(length==2&&path[start]=='.'&&path[start+1]=='.'))return false;start=i+1;}
 }
 return true;
}
namespace {
uint16_t u16(const uint8_t* p){return uint16_t(uint16_t(p[0])<<8|p[1]);}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
bool range(size_t offset,size_t count,size_t limit){return offset<=limit&&count<=limit-offset;}
struct Table{const uint8_t* p=nullptr;size_t n=0;};
size_t glyphOffset(Table loca,unsigned index,bool longOffsets){return longOffsets?u32(loca.p+index*4):size_t(u16(loca.p+index*2))*2;}
bool glyphValid(Table loca,Table glyf,unsigned glyph,unsigned count,bool longOffsets,uint16_t* parents,unsigned depth,size_t& visits){
 if(depth>=32||++visits>500000)return false;
 for(unsigned i=0;i<depth;++i)if(parents[i]==glyph)return false;
 parents[depth]=uint16_t(glyph);
 const size_t start=glyphOffset(loca,glyph,longOffsets),end=glyphOffset(loca,glyph+1,longOffsets),length=end-start;
 if(!length)return true;if(length<10)return false;const auto* p=glyf.p+start;
 const int contours=int16_t(u16(p));
 if(contours>=0){
  if(!range(10,size_t(contours)*2+2,length))return false;
  unsigned points=0;
  for(int i=0;i<contours;++i){const unsigned next=unsigned(u16(p+10+i*2))+1;if(next<=points||next>32768)return false;points=next;}
  size_t pos=12+size_t(contours)*2+u16(p+10+contours*2);if(pos>length)return false;
  size_t coordinates=0;
  for(unsigned i=0;i<points;){if(pos>=length)return false;const unsigned flag=p[pos++];unsigned repeat=1;
   if(flag&8){if(pos>=length)return false;repeat+=p[pos++];}if(repeat>points-i)return false;
   const size_t x=(flag&2)?1:((flag&16)?0:2),y=(flag&4)?1:((flag&32)?0:2);
   coordinates+=(x+y)*repeat;i+=repeat;
  }
  return range(pos,coordinates,length);
 }
 if(contours!=-1)return false;
 size_t pos=10;unsigned components=0,flags=0;
 do{if(++components>128||!range(pos,4,length))return false;flags=u16(p+pos);const unsigned child=u16(p+pos+2);pos+=4;
  if(child>=count||!(flags&2))return false; // TinyTTF does not support point-matched components.
  unsigned transforms=((flags&8)?1:0)+((flags&64)?1:0)+((flags&128)?1:0);if(transforms>1)return false;
  pos+=(flags&1)?4:2;pos+=(flags&8)?2:(flags&64)?4:(flags&128)?8:0;if(pos>length)return false;
  if(!glyphValid(loca,glyf,child,count,longOffsets,parents,depth+1,visits))return false;
 }while(flags&32);
 if(flags&256){if(!range(pos,2,length))return false;const unsigned instructions=u16(p+pos);pos+=2;if(!range(pos,instructions,length))return false;}
 return true;
}
bool cmapValid(Table cmap,uint16_t glyphs){
 if(cmap.n<4||u16(cmap.p)!=0)return false;
 const size_t count=u16(cmap.p+2);if(!count||!range(4,count*8,cmap.n))return false;
 size_t chosen=0;
 for(size_t i=0;i<count;++i){const auto* r=cmap.p+4+i*8;const auto platform=u16(r),encoding=u16(r+2);const size_t off=u32(r+4);
  if(!range(off,2,cmap.n))return false;
  if(platform==0||(platform==3&&(encoding==1||encoding==10)))chosen=off;
 }
 if(!chosen)return false;
 const auto* p=cmap.p+chosen;const auto format=u16(p);size_t n=0;
 if(format==0||format==4||format==6){if(!range(chosen,4,cmap.n))return false;n=u16(p+2);}
 else if(format==12||format==13){if(!range(chosen,16,cmap.n))return false;n=u32(p+4);}
 else return false;
 if(!n||!range(chosen,n,cmap.n))return false;
 if(format==0){if(n<262)return false;for(size_t i=6;i<262;++i)if(p[i]>=glyphs)return false;return true;}
 if(format==6){if(n<10)return false;const size_t count=u16(p+8);if(!range(10,count*2,n))return false;for(size_t i=0;i<count;++i)if(u16(p+10+i*2)>=glyphs)return false;return true;}
 if(format==12||format==13){const size_t groups=u32(p+12);if(!groups||groups>(n-16)/12)return false;uint32_t previous=0;
  for(size_t i=0;i<groups;++i){const auto* g=p+16+i*12;const uint32_t start=u32(g),end=u32(g+4),first=u32(g+8);if(start>end||end>0x10ffff||(i&&start<=previous)||first>=glyphs)return false;if(format==12&&end-start>=uint32_t(glyphs)-first)return false;previous=end;}
  return true;
 }
 if(n<24)return false;const unsigned segments=u16(p+6)/2;
 if(!segments||(u16(p+6)&1)||!range(16,size_t(segments)*8,n))return false;
 // stb's binary search trusts these fields; validate their exact derivation.
 unsigned power=1,selector=0;while(power*2<=segments){power*=2;++selector;}
 if(u16(p+8)!=power*2||u16(p+10)!=selector||u16(p+12)!=segments*2-power*2)return false;
 uint16_t previous=0;
 for(unsigned i=0;i<segments;++i){const uint16_t end=u16(p+14+i*2),start=u16(p+16+segments*2+i*2),delta=u16(p+16+segments*4+i*2),offset=u16(p+16+segments*6+i*2);
  if(start>end||(i&&start<=previous))return false;previous=end;
  const size_t address=16+segments*6+i*2;
  if(offset&&(offset&1||!range(address+offset,size_t(end-start+1)*2,n)))return false;
  for(uint32_t c=start;c<=end;++c){const uint16_t glyph=offset?u16(p+address+offset+(c-start)*2):uint16_t(c+delta);if(glyph>=glyphs)return false;}
 }
 return previous==0xffff;
}
}
bool validate(const uint8_t* bytes,size_t length){
 if(!bytes||length<4096||length>MaxFontBytes)return false;
 if(u32(bytes)!=0x00010000&&u32(bytes)!=0x74727565)return false;
 const size_t count=u16(bytes+4);if(!count||count>128||!range(12,count*16,length))return false;
 Table head,maxp,hhea,hmtx,loca,glyf,cmap;
 for(size_t i=0;i<count;++i){const auto* r=bytes+12+i*16;const size_t offset=u32(r+8),size=u32(r+12);if(!size||offset<12+count*16||!range(offset,size,length))return false;
  // Duplicate tags are ambiguous to readers and complicate validation.
  for(size_t j=0;j<i;++j)if(!memcmp(r,bytes+12+j*16,4))return false;
  Table t{bytes+offset,size};
  if(!memcmp(r,"head",4))head=t;else if(!memcmp(r,"maxp",4))maxp=t;else if(!memcmp(r,"hhea",4))hhea=t;else if(!memcmp(r,"hmtx",4))hmtx=t;else if(!memcmp(r,"loca",4))loca=t;else if(!memcmp(r,"glyf",4))glyf=t;else if(!memcmp(r,"cmap",4))cmap=t;
  else if(!memcmp(r,"fvar",4)||!memcmp(r,"CFF ",4)||!memcmp(r,"CFF2",4))return false;
 }
 if(head.n<54||maxp.n<6||hhea.n<36||!hmtx.p||!loca.p||!glyf.p||!cmap.p)return false;
 if(u32(head.p+12)!=0x5f0f3cf5)return false;const unsigned units=u16(head.p+18);if(units<16||units>16384)return false;
 const unsigned glyphs=u16(maxp.p+4),metrics=u16(hhea.p+34),format=u16(head.p+50);
 if(!glyphs||!metrics||metrics>glyphs||format>1||hmtx.n<size_t(metrics)*4+size_t(glyphs-metrics)*2)return false;
 if(loca.n<size_t(glyphs+1)*(format?4:2))return false;
 size_t previous=0;
 for(unsigned i=0;i<=glyphs;++i){const size_t next=format?u32(loca.p+i*4):size_t(u16(loca.p+i*2))*2;if(next<previous||next>glyf.n)return false;
  if(i&&next>previous){if(next-previous<10)return false;const auto* g=glyf.p+previous;const int contours=int16_t(u16(g));if(contours>=0){if(!range(10,size_t(contours)*2+2,next-previous))return false;const size_t instructions=u16(g+10+contours*2);if(!range(12+size_t(contours)*2,instructions,next-previous))return false;}else if(contours!=-1)return false;}
  previous=next;
 }
 uint16_t parents[32]{};size_t visits=0;
 for(unsigned glyph=0;glyph<glyphs;++glyph)if(!glyphValid(loca,glyf,glyph,glyphs,format!=0,parents,0,visits))return false;
 return cmapValid(cmap,uint16_t(glyphs));
}
}
