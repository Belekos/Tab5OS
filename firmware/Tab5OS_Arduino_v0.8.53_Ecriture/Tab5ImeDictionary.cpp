#include "Tab5ImeDictionary.h"
#include "Tab5VolumeFS.h"
#include <cstring>
Tab5ImeDictionary Tab5ImeWords;
namespace {
uint32_t le32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
bool read32(File& f,uint32_t at,uint32_t& out){uint8_t b[4];if(!f.seek(at)||f.read(b,4)!=4)return false;out=le32(b);return true;}
bool offsets(File& f,uint32_t at,uint32_t& first,uint32_t& end){uint8_t b[8];if(!f.seek(at)||f.read(b,8)!=8)return false;first=le32(b);end=le32(b+4);return true;}
bool word(File& f,String& out,uint32_t end){out="";uint32_t start=f.position();if(start>=end)return false;char bytes[128];size_t count=end-start;if(count>sizeof(bytes))count=sizeof(bytes);if(f.read(reinterpret_cast<uint8_t*>(bytes),count)!=count)return false;const auto* zero=static_cast<const char*>(memchr(bytes,0,count));if(!zero)return false;out.concat(bytes,zero-bytes);return f.seek(start+uint32_t(zero-bytes)+1);}
}
Tab5ImeDictionary::Page Tab5ImeDictionary::lookup(tab5ime::Mode mode,const char* key,uint32_t first)const{
  Page page;if(!key||!key[0]||strlen(key)>96)return page;
  const char* path=mode==tab5ime::Mode::Pinyin?"/System/ime/zh-pinyin.dat":mode==tab5ime::Mode::Japanese?"/System/ime/ja-kana.dat":nullptr;
  if(!path)return page;File f=tab5FS.open(path,FILE_READ);if(!f||f.isDirectory())return page;
  const uint32_t size=f.size();uint8_t head[16];
  if(size<16||size>32U*1024U*1024U||f.read(head,16)!=16||memcmp(head,"T5IME01\0",8))return page;
  uint32_t count=le32(head+8),data=le32(head+12);
  if(!count||count>1000000||data!=16U+(count+1U)*4U||data>size)return page;
  page.available=true;uint32_t lo=0,hi=count,at=0,end=0;String found;
  while(lo<hi){uint32_t mid=lo+(hi-lo)/2;
    if(!offsets(f,16+4*mid,at,end)||at<data||end>size||at>=end||!f.seek(at)||!word(f,found,end)){page.available=false;return page;}
    int compared=strcmp(found.c_str(),key);if(compared<0)lo=mid+1;else hi=mid;
  }
  if(lo>=count)return page;
  if(!offsets(f,16+4*lo,at,end)||at<data||end>size||at>=end||!f.seek(at)||!word(f,found,end)){page.available=false;return page;}
  if(found!=key)return page;
  // Each row stores candidate count + candidate offset table. Selecting the
  // last page is O(page size), even for hundreds of homophones.
  uint32_t table=f.position(),total;
  if(!read32(f,table,total)||total>(end-table)/4||table+4U+4U*(total+1U)>end){page.available=false;return page;}
  page.total=total;if(first>=total)return page;
  uint32_t stop=first+PAGE_SIZE;if(stop>total)stop=total;
  for(uint32_t i=first;i<stop;++i){uint32_t a,b;
    if(!offsets(f,table+4+4*i,a,b)||a<table+4+4*(total+1)||b>end||a>=b||!f.seek(a)||!word(f,page.words[page.count],b)){page.count=0;page.available=false;return page;}
    ++page.count;
  }return page;
}
