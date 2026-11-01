#include "Tab5LaunchProfile.h"
#include "Tab5PythonUI.h"
#include "Preferences.h"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <string>
int main(){
 char out[270];
 struct Case{const char* input;const char* result;};
 const Case cases[]={
  {"sd:/Applications/test.elf","/Applications/test.elf"},{"usb:/Jeux/jeu.gba","/USB/Jeux/jeu.gba"},
  {"USB:/a.elf","/USB/a.elf"},{"/usb/a.elf","/USB/a.elf"},{"/Tests/écriture.elf","/Tests/écriture.elf"},
  {"sd:/USB/a.elf",nullptr},{"sd:/usb/a.elf",nullptr},{"sd:/a/../b",nullptr},{"usb:/../sd/a",nullptr},
  {"/a//b",nullptr},{"/a/./b",nullptr},{"/a/b.",nullptr},{"/a/b ",nullptr},{"/a~1/b",nullptr},
  {"/a\\b",nullptr},{"/a:b",nullptr},{"/a\nb",nullptr},{"test.elf",nullptr},{"/a/",nullptr},{"/","/"},{"usb:/","/USB"},{"sd:/","/"}
 };
 for(const auto& c:cases){const bool ok=tab5LaunchPath(c.input,out,sizeof(out));assert(ok==bool(c.result));if(ok)assert(!strcmp(out,c.result));}
 assert(tab5LaunchDisplayPath("/USB/My folder/test.elf",out,sizeof(out))&&!strcmp(out,"usb:/My folder/test.elf"));
 assert(tab5LaunchDisplayPath("/My folder/test.elf",out,sizeof(out))&&!strcmp(out,"sd:/My folder/test.elf"));
 std::string max="/"+std::string(259,'a');assert(tab5LaunchPath(max.c_str(),out,sizeof(out)));max+='b';assert(!tab5LaunchPath(max.c_str(),out,sizeof(out)));
 assert(!tab5LaunchPath("usb:/a",out,5));assert(!tab5LaunchPath(nullptr,out,sizeof(out)));
 Tab5LaunchProfile profile,loaded;assert(!tab5LaunchProfileLoad("/apps/test.elf",loaded));assert(loaded.mode==Tab5LaunchMode::Ask);
 profile.mode=Tab5LaunchMode::None;assert(tab5LaunchProfileSave("sd:/Apps/Test.elf",profile));assert(tab5LaunchProfileLoad("/apps/test.ELF",loaded)&&loaded.mode==Tab5LaunchMode::None);
 profile.mode=Tab5LaunchMode::File;strcpy(profile.argument,"usb:/Jeux/My game.gba");assert(tab5LaunchProfileSave("sd:/Apps/Test.elf",profile));assert(tab5LaunchProfileLoad("/apps/test.elf",loaded)&&!strcmp(loaded.argument,"/USB/Jeux/My game.gba"));
 assert(!tab5LaunchProfileLoad("usb:/Apps/Test.elf",loaded));strcpy(profile.argument,"usb:/../a");assert(!tab5LaunchProfileSave("/apps/test.elf",profile));
 assert(tab5LaunchProfileForget("/apps/test.elf"));assert(!tab5LaunchProfileLoad("/apps/test.elf",loaded));
 profile={};Preferences::writable=false;assert(!tab5LaunchProfileSave("/apps/test.elf",profile));Preferences::writable=true;
 assert(tab5LaunchProfileSave("/apps/test.elf",profile));auto& blob=Preferences::values.begin()->second;blob[0]^=1;assert(!tab5LaunchProfileLoad("/apps/test.elf",loaded));
 // Real Python UI bridge: explicit shortcut binding is per page generation.
 using namespace Tab5PythonUI;reset();const char title[]="Tests";assert(call(50,0,0,0,title,5,nullptr,0)==0);
 uint32_t generation;char name[192];assert(opened(generation,name));
 const unsigned char widget[]={0,0,0,0,100,0,46,0,'B','a','c','k'};
 assert(call(51,1,2,100,widget,sizeof(widget),nullptr,0)==1);assert(call(105,1,1,0,nullptr,0,nullptr,0)==0);
 Widget w;assert(read(1,w,generation)&&w.shortcut==1);assert(call(105,1,8,0,nullptr,0,nullptr,0)==-EINVAL);
 event(1,1,generation);unsigned char data[8];assert(call(54,0,0,0,nullptr,0,data,sizeof(data))==8&&data[0]==1);
 const char article[]="/wiki/article.md\0Article";assert(call(90,15,0,0,article,sizeof(article)-1,nullptr,0)==0);
 ReaderRequest request;assert(readerRequest(request)&&request.rich&&request.illustrated&&!request.canSave&&!request.canUpdate);
 readerFinished(request.revision);assert(call(90,16,0,0,article,sizeof(article)-1,nullptr,0)==-EINVAL);
 assert(call(50,0,0,0,title,5,nullptr,0)==0);assert(call(51,1,2,100,widget,sizeof(widget),nullptr,0)==1);
 assert(!read(1,w,generation));event(1,1,generation);assert(call(54,0,0,0,nullptr,0,data,sizeof(data))==0);
 close();assert(!opened(generation,name));releaseStorage();
 puts("PASS: path validation, SD/USB identity, persistent launch profiles, failure handling, UI shortcut generation, reader action flags");
}
