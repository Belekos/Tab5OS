#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
namespace tab5theme {
constexpr unsigned PaletteCount=19;
inline const char* roleKey(unsigned role){
 static const char* keys[]={"bg","surface","raised","border","text","muted","active","cursor","selected","pointer","pointer_outline","switch_off","switch_on","switch_knob_off","switch_knob_on","file_icon","folder_icon","hidden_file_icon","hidden_folder_icon"};
 return role<PaletteCount?keys[role]:"";
}
inline bool parseColor(const char* text,uint32_t& color){
 if(!text||text[0]!='#')return false;
 uint32_t value=0;
 for(unsigned i=1;i<=6;++i){unsigned char c=text[i];unsigned digit;
  if(c>='0'&&c<='9')digit=c-'0';else if(c>='a'&&c<='f')digit=c-'a'+10;
  else if(c>='A'&&c<='F')digit=c-'A'+10;else return false;
  value=(value<<4)|digit;
 }
 if(text[7])return false;color=value;return true;
}
// Old nine/eleven/thirteen-role themes retain their switch appearance. Missing
// tracks inherit raised/active; missing knobs inherit text/surface. Missing icon
// roles retain a sentinel so legacy themes keep hidden/protected row colours.
// The editor resolves inheritance for display only. Black is valid.
inline uint32_t visibleColor(const uint32_t* colors,unsigned role){
 return role>=15&&role<PaletteCount&&colors[role]>0xffffff?colors[role>=17?5:4]:colors[role];
}
template<class Lookup> bool readPalette(Lookup lookup,uint32_t* output){
 uint32_t colors[PaletteCount];
 for(unsigned i=0;i<PaletteCount;++i){const char* value=lookup(i);
  if(i>=15&&(!value||!*value))colors[i]=UINT32_MAX;
  else if(i>=9&&(!value||!*value))colors[i]=i==9?0xffaa20:i==10?0x101214:i==11?colors[2]:i==12?colors[6]:i==13?colors[4]:colors[1];
  else if(!parseColor(value,colors[i]))return false;
 }
 for(unsigned i=0;i<PaletteCount;++i)output[i]=colors[i];return true;
}
template<class Save> void writePalette(const uint32_t* colors,Save save){
 // A null value asks the caller to remove an inherited optional role.
 for(unsigned i=0;i<PaletteCount;++i){if(i>=15&&colors[i]>0xffffff){save(roleKey(i),static_cast<char*>(nullptr));continue;}char hex[8];snprintf(hex,sizeof(hex),"#%06lX",static_cast<unsigned long>(colors[i]&0xffffff));save(roleKey(i),hex);}
}
}
