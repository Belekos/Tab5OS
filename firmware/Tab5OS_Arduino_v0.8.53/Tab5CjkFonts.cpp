#include "Tab5CjkFonts.h"
#include "Tab5VolumeFS.h"
#include <esp_heap_caps.h>
Tab5CjkFonts Tab5Cjk;
namespace {const lv_font_t* bitmap(uint16_t pixels){return pixels>=30?&lv_font_montserrat_32:pixels>=26?&lv_font_montserrat_28:pixels>=22?&lv_font_montserrat_24:&lv_font_montserrat_20;}}
const char* Tab5CjkFonts::repertoire(const String& id){
  if(id.startsWith("zh"))return "zh";if(id.startsWith("ja"))return "ja";if(id.startsWith("ko"))return "ko";return "";
}
bool Tab5CjkFonts::availableFor(const String& id)const{const char* wanted=repertoire(id);return !wanted[0]||(_data[0]&&_id[0]==wanted)||(_data[1]&&_id[1]==wanted);}
lv_font_t* Tab5CjkFonts::face(uint16_t pixels,uint8_t bank){
#if LV_USE_TINY_TTF
  if(bank>1||!_data[bank])return nullptr;
  for(size_t i=0;i<_faceCount;++i)if(_faces[i].pixels==pixels&&_faces[i].bank==bank)return _faces[i].font;
  if(_faceCount>=64)return nullptr;
  auto* font=lv_tiny_ttf_create_data_ex(_data[bank],_bytes[bank],pixels,LV_FONT_KERNING_NONE,96);
  if(font){_faces[_faceCount++]={font,pixels,bank};font->fallback=bank==0?face(pixels,1):nullptr;if(!font->fallback)font->fallback=bitmap(pixels);}return font;
#else
  return nullptr;
#endif
}
void Tab5CjkFonts::detach(lv_font_t* primary){for(size_t i=0;i<_parentCount;++i)if(_parents[i].font==primary){primary->fallback=bitmap(_parents[i].pixels);_parents[i]=_parents[--_parentCount];return;}}
void Tab5CjkFonts::attach(lv_font_t* primary,uint16_t pixels,lv_font_t* regular){
  if(!primary)return;
  if(regular==primary)regular=nullptr;
  for(size_t i=0;i<_parentCount;++i)if(_parents[i].font==primary){_parents[i].pixels=pixels;_parents[i].regular=regular;primary->fallback=regular?regular:face(pixels);if(!primary->fallback)primary->fallback=bitmap(pixels);return;}
  if(_parentCount<80)_parents[_parentCount++]={primary,pixels,regular};
  primary->fallback=regular?regular:face(pixels);if(!primary->fallback)primary->fallback=bitmap(pixels);
}
void Tab5CjkFonts::reattach(){for(size_t i=0;i<_parentCount;++i){auto& p=_parents[i];p.font->fallback=p.regular?p.regular:face(p.pixels);if(!p.font->fallback)p.font->fallback=bitmap(p.pixels);}}
bool Tab5CjkFonts::select(const String& language,const String& keyboard){
#if LV_USE_TINY_TTF
  String wanted[2]={repertoire(language),""};String key=repertoire(keyboard);
  if(!wanted[0].length())wanted[0]=key;else if(key.length()&&key!=wanted[0])wanted[1]=key;
  if(_id[0]==wanted[0]&&_id[1]==wanted[1])return true;
  uint8_t* data[2]={};size_t bytes[2]={};bool fresh[2]={};lv_font_t* initial[2]={};
  auto fail=[&](){for(unsigned n=0;n<2;++n){if(initial[n])lv_tiny_ttf_destroy(initial[n]);if(fresh[n])heap_caps_free(data[n]);}return false;};
  for(unsigned n=0;n<2;++n){if(!wanted[n].length())continue;
    for(unsigned old=0;old<2;++old)if(_id[old]==wanted[n]){data[n]=_data[old];bytes[n]=_bytes[old];break;}
    if(data[n])continue;
    const String path=String("/System/fonts/cjk-")+wanted[n]+".ttf";
    File f=tab5FS.open(path,FILE_READ);if(!f)return fail();bytes[n]=f.size();
    if(bytes[n]<4096||bytes[n]>6U*1024U*1024U)return fail();
    data[n]=static_cast<uint8_t*>(heap_caps_malloc(bytes[n],MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));fresh[n]=true;if(!data[n])return fail();
    const size_t got=f.read(data[n],bytes[n]);f.close();if(got!=bytes[n])return fail();
    initial[n]=lv_tiny_ttf_create_data_ex(data[n],bytes[n],20,LV_FONT_KERNING_NONE,96);if(!initial[n])return fail();
  }
  // Called on the UI task between LVGL frames. Every registered primary font
  // is repointed before rendering can observe a retired fallback face.
  for(size_t i=0;i<_parentCount;++i)_parents[i].font->fallback=bitmap(_parents[i].pixels);
  for(size_t i=0;i<_faceCount;++i)lv_tiny_ttf_destroy(_faces[i].font);
  _faceCount=0;
  for(unsigned old=0;old<2;++old)if(_data[old]&&_data[old]!=data[0]&&_data[old]!=data[1])heap_caps_free(_data[old]);
  for(unsigned n=0;n<2;++n){_data[n]=data[n];_bytes[n]=bytes[n];_id[n]=wanted[n];if(initial[n]){initial[n]->fallback=&lv_font_montserrat_20;_faces[_faceCount++]={initial[n],20,uint8_t(n)};}}
  for(size_t i=0;i<_faceCount;++i)if(_faces[i].bank==0){auto* fallback=face(_faces[i].pixels,1);if(fallback)_faces[i].font->fallback=fallback;}
  reattach();return true;
#else
  (void)language;(void)keyboard;return false;
#endif
}
