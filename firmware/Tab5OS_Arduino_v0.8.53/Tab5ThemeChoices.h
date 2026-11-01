#pragma once
#include <lvgl.h>
#include <string>
namespace Tab5ThemeChoices {
// Both selectors use the same validated OS catalogue; SD is never a condition
// for showing the built-in choices. Called on READY before LVGL sizes its list.
template<class Name,class Translate>
inline void populate(lv_obj_t* dropdown,size_t count,Name name,Translate translate,const char* current=nullptr){
 if(!dropdown)return;std::string options=current?translate(current):"";
 for(size_t i=0;i<count;++i){if(!options.empty())options+='\n';options+=translate(name(i));}
 lv_dropdown_set_options(dropdown,options.c_str());
}
}
