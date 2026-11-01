#pragma once
#include <lvgl.h>
#include <cstring>
#include "Tab5KeyboardLayouts.h"
namespace Tab5NumericKeyboard {
// Keep the decimal point literal even with a CJK input method selected.
inline unsigned mode(lv_obj_t* target) {
 if(!target||!lv_obj_is_valid(target)||!lv_obj_check_type(target,&lv_textarea_class))return 0;
 const char* accepted=lv_textarea_get_accepted_chars(target);
 if(!accepted||!*accepted)return 0;
 for(const char* p=accepted;*p;++p)if(!strchr("0123456789.-",*p))return 0;
 for(const char* p="0123456789";*p;++p)if(!strchr(accepted,*p))return 0;
 return strchr(accepted,'-')?2:1;
}
inline const std::vector<Tab5KeyDef>& row(unsigned mode,unsigned index) {
 static const std::vector<Tab5KeyDef> rows[4]={
  {{"7","7"},{"8","8"},{"9","9"},{"","<BKSP>"}},
  {{"4","4"},{"5","5"},{"6","6"},{"","<LEFT>"}},
  {{"1","1"},{"2","2"},{"3","3"},{"","<RIGHT>"}},
  {{"0","0"},{".","."},{"","<ENTER>","",2}}
 };
 static const std::vector<Tab5KeyDef> signedLast={{"0","0"},{".","."},{"-","-"},{"","<ENTER>"}};
 return mode==2&&index==3?signedLast:rows[index%4];
}
}
