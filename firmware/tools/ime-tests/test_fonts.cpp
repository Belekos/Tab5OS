#include "Tab5CjkFonts.h"
#include "Tab5VolumeFS.h"
#include <cassert>
#include <iostream>
#include <map>
#include <set>
std::string testSdRoot,testOverride;size_t testReads=0,testReadBytes=0;TestVolume tab5FS;
const lv_font_t lv_font_montserrat_20{nullptr,20},lv_font_montserrat_24{nullptr,24},lv_font_montserrat_28{nullptr,28},lv_font_montserrat_32{nullptr,32};
static std::map<void*,size_t> blocks;static std::set<lv_font_t*> faces;static bool failAllocation=false,failFont=false;
void* heap_caps_malloc(size_t n,int){if(failAllocation)return nullptr;void* p=malloc(n);if(p)blocks[p]=n;return p;}
void heap_caps_free(void* p){if(!p)return;assert(blocks.erase(p)==1);free(p);}
lv_font_t* lv_tiny_ttf_create_data_ex(const void* data,size_t n,unsigned px,int,unsigned cache){assert(data&&n>4096&&cache==96);if(failFont)return nullptr;auto* f=new lv_font_t{nullptr,px,data};faces.insert(f);return f;}
void lv_tiny_ttf_destroy(lv_font_t* p){assert(faces.erase(p)==1);delete p;}
static void valid(const lv_font_t* p){unsigned depth=0;for(p=p->fallback;p;p=p->fallback){assert(++depth<5);if(p->data){assert(faces.count(const_cast<lv_font_t*>(p)));assert(blocks.count(const_cast<void*>(p->data)));}}}
int main(int argc,char** argv){assert(argc==2);testSdRoot=argv[1];lv_font_t ui,key,italic;
  Tab5Cjk.attach(&ui,20);Tab5Cjk.attach(&key,34);assert(key.fallback==&lv_font_montserrat_32);
  Tab5Cjk.attach(&italic,20,&ui);assert(italic.fallback==&ui);valid(&italic);
  assert(Tab5Cjk.select("zh-CN","zh-CN"));assert(Tab5Cjk.availableFor("zh-CN"));assert(blocks.size()==1&&Tab5Cjk.residentBytes()<6*1024*1024);valid(&ui);valid(&key);
  assert(Tab5Cjk.select("zh-CN","ko-KR"));assert(Tab5Cjk.availableFor("ko-KR")&&blocks.size()==2);assert(ui.fallback->fallback->data);valid(&ui);valid(&key);
  assert(italic.fallback==&ui);valid(&italic);
  assert(Tab5Cjk.select("ja-JP","ja-JP"));assert(blocks.size()==1&&Tab5Cjk.availableFor("ja-JP")&&!Tab5Cjk.availableFor("zh-CN"));valid(&ui);valid(&key);
  auto* preserved=ui.fallback;failAllocation=true;assert(!Tab5Cjk.select("zh-CN","zh-CN"));failAllocation=false;assert(ui.fallback==preserved&&blocks.size()==1);valid(&ui);
  failFont=true;assert(!Tab5Cjk.select("zh-CN","zh-CN"));failFont=false;assert(ui.fallback==preserved&&blocks.size()==1);valid(&ui);
  testOverride="missing-font.ttf";assert(!Tab5Cjk.select("ko-KR","ko-KR"));testOverride.clear();assert(ui.fallback==preserved);valid(&ui);
  Tab5Cjk.attach(&key,28);assert(key.fallback->pixels==28);valid(&key);
  Tab5Cjk.detach(&key);assert(key.fallback==&lv_font_montserrat_28);
  assert(Tab5Cjk.select("fr-FR","en-US"));assert(blocks.empty()&&faces.empty()&&ui.fallback==&lv_font_montserrat_20);assert(italic.fallback==&ui);valid(&italic);Tab5Cjk.detach(&italic);Tab5Cjk.detach(&ui);
  std::cout<<"PASS: actual CJK font lifecycle, one/two repertoire residency, regular-face fallback for styled Arabic, glyph cache cap, resize/detach, atomic allocation/font/missing-file failure, no dangling registered parents or leaks.\n";
}
