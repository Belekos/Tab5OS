#include "Tab5KeyboardState.h"
#include "Tab5AppDirectory.h"
#include <assert.h>
#include <stdio.h>

static_assert(sizeof(tab5_app_keyboard_state)==44,"Keyboard ABI must be fixed width");
static_assert(sizeof(tab5_app_directory_entry)==268,"Directory ABI must be fixed width");
static_assert(offsetof(tab5_app_api7,base)==0,"ABI6 prefix");
static_assert(offsetof(tab5_app_api7,keyboard_state)==sizeof(tab5_app_api6),"Append only ABI7");
static_assert(sizeof(tab5_app_api)==8+11*sizeof(void*),"ABI1 layout retained");
static_assert(sizeof(tab5_app_api2)==sizeof(tab5_app_api)+6*sizeof(void*),"ABI2 layout retained");
static_assert(sizeof(tab5_app_api3)==sizeof(tab5_app_api2)+sizeof(void*),"ABI3 layout retained");
static_assert(sizeof(tab5_app_api4)==sizeof(tab5_app_api3)+3*sizeof(void*),"ABI4 layout retained");
static_assert(sizeof(tab5_app_api5)==sizeof(tab5_app_api4)+6*sizeof(void*),"ABI5 layout retained");
static_assert(sizeof(tab5_app_api6)==sizeof(tab5_app_api5)+5*sizeof(void*),"ABI6 layout retained");
static_assert(sizeof(tab5_app_api7)==sizeof(tab5_app_api6)+3*sizeof(void*),"ABI7 layout");

void keyboardTests(){
 Tab5KeyboardState<8> keyboard;uint8_t keys[32],a[32]{},b[32]{},modifiers=255;
 assert(!keyboard.snapshot(true,keys,modifiers)&&!modifiers);
 for(auto key:keys)assert(!key);
 // Escape, F1, F12, arrows, Ctrl/Alt/Shift, and usages past the old decoder.
 const unsigned usages[]={41,58,69,79,224,255};
 for(auto usage:usages)a[usage/8]|=uint8_t(1u<<(usage%8));
 keyboard.update(0,a,0x47,true);assert(keyboard.snapshot(true,keys,modifiers));
 assert(modifiers==0x47&&!memcmp(keys,a,32));
 // State is non-consuming and overlap survives one keyboard's release.
 b[41/8]|=1u<<(41%8);b[4/8]|=1u<<(4%8);keyboard.update(7,b,0x80,true);
 assert(keyboard.snapshot(true,keys,modifiers)&&modifiers==0xc7);
 assert(keys[4/8]&(1u<<(4%8)));assert(keys[255/8]&0x80);
 keyboard.update(0,nullptr,255,false);
 assert(keyboard.snapshot(true,keys,modifiers)&&modifiers==0x80&&!memcmp(keys,b,32));
 keyboard.update(8,a,0xff,true); // Invalid slot does not corrupt another device.
 assert(keyboard.snapshot(true,keys,modifiers)&&modifiers==0x80&&!memcmp(keys,b,32));
 assert(!keyboard.snapshot(false,keys,modifiers)&&!modifiers);
 for(auto key:keys)assert(!key);
 assert(keyboard.snapshot(true,keys,modifiers)&&modifiers==0x80&&!memcmp(keys,b,32));
 keyboard.update(7,nullptr,0,false);assert(!keyboard.snapshot(true,keys,modifiers));
 for(auto key:keys)assert(!key);
 // Release/reconnect cycles cannot resurrect the previous device's report.
 for(unsigned i=0;i<10000;++i){
  keyboard.update(3,a,0x22,true);assert(keyboard.snapshot(true,keys,modifiers)&&modifiers==0x22);
  keyboard.update(3,nullptr,0,false);assert(!keyboard.snapshot(true,keys,modifiers)&&!modifiers);
 }
}
void directoryPathTests(){
 using namespace Tab5AppDirectory;char scope[MaxPath+1]{};
 assert(root("",false,scope)&&!strcmp(scope,"/DOS"));
 assert(contains(scope,"/DOS")&&contains(scope,"/DOS/GAME/DATA.DAT"));
 assert(!contains(scope,"/DOS2")&&!contains(scope,"/Saves/DOS"));
 assert(root("/Games/Example/PLAY.EXE",false,scope)&&!strcmp(scope,"/Games/Example"));
 assert(contains(scope,"/Games/Example/.secret")&&!contains(scope,"/Games/Example2"));
 assert(root("/Games/Example",true,scope)&&!strcmp(scope,"/Games/Example"));
 assert(root("/PLAY.COM",false,scope)&&!strcmp(scope,"/")&&contains(scope,"/Data"));
 const char* bad[]={"","relative","//DOS","/DOS/","/DOS/.","/DOS/..","/DOS/a/../b","/DOS/a//b","/DOS/a.","/DOS/a ","/DOS/a\\b","/DOS/a:b","/DOS/a\nb"};
 for(auto path:bad)assert(!validPath(path));
 char longPath[MaxPath+3];memset(longPath,'a',sizeof(longPath));longPath[0]='/';longPath[MaxPath]=0;
 assert(validPath(longPath));longPath[MaxPath]='a';longPath[MaxPath+1]=0;assert(!validPath(longPath));
 tab5_app_directory_entry item{};item.size=sizeof(item);
 assert(entry("/Games/Example/PLAY.EXE",false,19,item)&&!strcmp(item.name,"PLAY.EXE")&&item.file_size==19&&!item.flags);
 assert(entry(".hidden",true,123,item)&&item.flags==TAB5_DIRECTORY_ENTRY_DIRECTORY&&!item.file_size);
 assert(entry("donn\xc3\xa9" "es.dat",false,UINT32_MAX,item)&&item.file_size==UINT32_MAX);
 assert(!entry("huge.dat",false,uint64_t(UINT32_MAX)+1,item));
 assert(!entry(".",true,0,item)&&!entry("..",true,0,item)&&!entry("",true,0,item));
 char name[257];memset(name,'x',sizeof(name));name[255]=0;assert(entry(name,false,1,item));
 name[255]='x';name[256]=0;assert(!entry(name,false,1,item));
}
int main(){keyboardTests();directoryPathTests();puts("ABI7 keyboard aggregation, disconnect/focus release, old ABI layouts and directory scope tests passed.");}
