#pragma once
#include <lvgl.h>
#include <algorithm>
#include <cstring>

namespace Tab5CalculatorUi {
enum class Section { Memory, Numbers, Scientific };
struct Key {const char* label;const char* fallback;const char* insert;int action;Section section;int row,column,span;};
constexpr Key Keys[]={
 {"MS","MS",nullptr,4,Section::Memory,0,0,1},{"MR","MR",nullptr,5,Section::Memory,0,1,1},
 {"M+","M+",nullptr,6,Section::Memory,0,2,1},{"M−","M-",nullptr,7,Section::Memory,0,3,1},{"MC","MC",nullptr,8,Section::Memory,0,4,1},
 {"C","C",nullptr,2,Section::Numbers,0,0,1},{LV_SYMBOL_BACKSPACE,"BS",nullptr,3,Section::Numbers,0,1,1},
 {"(","(","(",120,Section::Numbers,0,2,1},{")",")",")",121,Section::Numbers,0,3,1},
 {"7","7","7",100,Section::Numbers,1,0,1},{"8","8","8",101,Section::Numbers,1,1,1},{"9","9","9",102,Section::Numbers,1,2,1},{"÷","/","/",103,Section::Numbers,1,3,1},
 {"4","4","4",106,Section::Numbers,2,0,1},{"5","5","5",107,Section::Numbers,2,1,1},{"6","6","6",108,Section::Numbers,2,2,1},{"×","*","*",109,Section::Numbers,2,3,1},
 {"1","1","1",112,Section::Numbers,3,0,1},{"2","2","2",113,Section::Numbers,3,1,1},{"3","3","3",114,Section::Numbers,3,2,1},{"−","-","-",115,Section::Numbers,3,3,1},
 {"0","0","0",118,Section::Numbers,4,0,1},{".",".",".",119,Section::Numbers,4,1,1},{"ANS","ANS","ans",125,Section::Numbers,4,2,1},{"+","+","+",122,Section::Numbers,4,3,1},
 {"π","pi","pi",124,Section::Numbers,5,0,1},{"mod","mod","%",129,Section::Numbers,5,1,1},{"=","=",nullptr,1,Section::Numbers,5,2,2},
 {"sin","sin","sin(",104,Section::Scientific,0,0,1},{"asin","asin","asin(",105,Section::Scientific,0,1,1},
 {"cos","cos","cos(",110,Section::Scientific,1,0,1},{"acos","acos","acos(",111,Section::Scientific,1,1,1},
 {"tan","tan","tan(",116,Section::Scientific,2,0,1},{"atan","atan","atan(",117,Section::Scientific,2,1,1},
 {"√","sqrt","sqrt(",126,Section::Scientific,3,0,1},{"^","^","^",123,Section::Scientific,3,1,1},
 {"ln","ln","ln(",127,Section::Scientific,4,0,1},{"log","log","log(",128,Section::Scientific,4,1,1},
 {"exp","exp","exp(",130,Section::Scientific,5,0,1},{"abs","abs","abs(",131,Section::Scientific,5,1,1}
};
constexpr size_t KeyCount=sizeof(Keys)/sizeof(Keys[0]);
struct Widgets {
 lv_obj_t *input=nullptr,*history=nullptr,*angle=nullptr,*status=nullptr,*conversion=nullptr;
 lv_obj_t* keys[KeyCount]{};
};
inline const char* insertion(int action){for(const auto& key:Keys)if(key.action==action)return key.insert;return nullptr;}
inline const char* caption(const Key& key,const lv_font_t* font){
 for(const unsigned char* p=reinterpret_cast<const unsigned char*>(key.label);*p;){
  uint32_t point=*p++;if(point>=0xc0){unsigned more=point<0xe0?1:point<0xf0?2:3;point&=(1U<<(6-more))-1;while(more--&&*p)point=(point<<6)|(*p++&63);}
  lv_font_glyph_dsc_t glyph{};if(!font||!lv_font_get_glyph_dsc(font,&glyph,point,0)||glyph.is_placeholder)return key.fallback;
 }return key.label;
}
inline const char* keyboardToken(const char* text){
 if(!text||!*text)return nullptr;
 if(!std::strcmp(text,"√"))return "sqrt(";if(!std::strcmp(text,"π"))return "pi";
 if(!std::strcmp(text,"×"))return "*";if(!std::strcmp(text,"÷"))return "/";
 if(!std::strcmp(text,"−"))return "-";if(!std::strcmp(text,"²"))return "^2";if(!std::strcmp(text,","))return ".";
 for(const unsigned char* p=reinterpret_cast<const unsigned char*>(text);*p;++p)if(*p>=128||!std::strchr("0123456789+-*/().=^%",*p))return nullptr;return text;
}
inline void place(lv_obj_t* object,int x,int y,int width,int height){if(object){lv_obj_set_pos(object,x,y);lv_obj_set_size(object,width,height);}}
inline void layout(Widgets& ui,int width,int height){
 const int gap=8,history=std::clamp(width*42/100,380,540),right=history+20,keyWidth=width-right;
 const int scientific=(keyWidth-16)/3,numbers=keyWidth-16-scientific,scienceX=right+numbers+16;
 // A raised virtual height keeps the touch targets usable with the software
 // keyboard visible; the ordinary full-height page fits without scrolling.
 const int usable=std::max(height,612),keyHeight=(usable-44-196-5*gap)/6;
 place(ui.input,0,64,width-224,52);place(ui.angle,width-208,64,208,52);
 place(ui.history,0,132,history,usable-176);place(ui.conversion,width-352,0,208,44);
 for(size_t i=0;i<KeyCount;++i){const auto& key=Keys[i];int x=0,y=0,w=0,h=0;
  if(key.section==Section::Memory){w=(keyWidth-4*gap)/5;x=right+key.column*(w+gap);y=132;h=44;}
  else {const bool science=key.section==Section::Scientific;const int cell=((science?scientific:numbers)-(science?1:3)*gap)/(science?2:4);
   x=(science?scienceX:right)+key.column*(cell+gap);y=196+key.row*(keyHeight+gap);w=cell*key.span+gap*(key.span-1);h=keyHeight;}
  place(ui.keys[i],x,y,w,h);
 }
 place(ui.status,0,usable-32,width,32);
}
}
