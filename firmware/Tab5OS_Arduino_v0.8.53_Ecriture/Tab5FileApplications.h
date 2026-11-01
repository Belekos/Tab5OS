#pragma once
#include <cstring>
#include <strings.h>
// Shared associations for the file browser and application launcher.
inline const char* tab5ApplicationForFile(const char* path){
 if(!path)return nullptr;const char* suffix=strrchr(path,'.');if(!suffix)return nullptr;
 struct Entry{const char* extension;const char* app;};
 static const Entry entries[]={
  {".gb","/Applications/gameboy.app.elf"},{".gbc","/Applications/gameboy_color.app.elf"},
  {".nes","/Applications/NES.app.elf"},{".gg","/Applications/Sega8.app.elf"},{".sms","/Applications/Sega8.app.elf"},
  {".gba","/Applications/GBA.app.elf"},{".sfc","/Applications/SNES.app.elf"},{".smc","/Applications/SNES.app.elf"},
  {".gen","/Applications/MegaDrive.app.elf"},
  {".wad","/Applications/Doom.app.elf"}};
 for(const auto& e:entries)if(!strcasecmp(suffix,e.extension))return e.app;return nullptr;
}
