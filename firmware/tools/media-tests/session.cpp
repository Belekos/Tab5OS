#include "Tab5MediaSession.h"
#include "Preferences.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
static unsigned checks=0;
static void check(bool result,const char* message){if(!result){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}++checks;}
int main(){
 namespace S=Tab5MediaSession;
 check(S::begin(77)&&S::volume()==77,"first boot migrates SD volume without playing");
 S::browser(1,2,"/Audio/Classical","FR/news",100);
 S::select(1,"https://radio.example/live","Example",100);S::search(1,"Example",100);S::scroll(1,2442,100);
 for(unsigned i=100;i<1300;i+=30)S::service(i);
 check(Preferences::writes==1,"browser coalesces changes; legacy volume migrates once");
 S::service(1300);check(Preferences::writes==2,"one browser blob after quiet period");
 check(S::begin(77)&&S::state().view==1&&S::state().favorites==2,"restores radio favorite page");
 check(!std::strcmp(S::state().selected[1],"https://radio.example/live")&&!std::strcmp(S::state().folder,"/Audio/Classical")&&S::state().scroll[1]==2442,"restores selected station folders and scroll");
 check(!std::strcmp(S::state().search[1],"Example"),"restores filter");
 const auto saved=Preferences::writes;
 for(unsigned n=0;n<4000;++n){S::browser(1,2,"/Audio/Classical","FR/news",2000+n);S::select(1,"https://radio.example/live","Example",2000+n);S::scroll(1,2442,2000+n);S::service(2000+n);}
 check(Preferences::writes==saved,"4000 unchanged redraws produce no NVS writes");
 for(unsigned n=0;n<255;++n){S::volume(uint8_t(n),10000+n);S::service(10000+n);}
 check(Preferences::writes==saved,"slider movement never writes each sample");
 S::volume(0,11000);check(S::flush(11001),"release flushes mute without delay");
 check(S::begin(150)&&S::volume()==0,"zero persists after reboot and overrides legacy SD settings");
 S::volume(64,12000);Preferences::writable=false;
 check(!S::flush(12001),"write failure reported");const auto attempts=Preferences::attempts;
 for(unsigned i=12002;i<17001;i+=10)S::service(i);
 check(Preferences::attempts==attempts,"failed flash write does not retry every tick");
 Preferences::writable=true;S::service(17001);check(S::begin(0)&&S::volume()==64,"retry preserves pending volume");
 const auto unchanged=Preferences::writes;S::browser(0,0,"/Audio","",20000);S::browser(1,2,"/Audio/Classical","FR/news",20100);S::service(21500);
 check(Preferences::writes==unchanged,"returning to original state avoids a redundant write");
 S::select(1,std::string(700,'a').c_str(),"too long",22000);check(!S::state().selected[1][0],"overlong URL is not saved as a misleading truncated identity");
 S::scroll(1,-50,22001);check(S::state().scroll[1]==0,"invalid negative scroll is bounded");
 S::flush(22002);auto& record=Preferences::values["browser"];record.back()^=1;
 check(S::begin(0)&&S::state().view==0&&!S::state().selected[1][0]&&S::volume()==64,"corrupt browser blob rejected without losing independent volume");
 S::volume(81,0xfffffff0U);S::service(0x10U);check(S::volume()==81,"wrapped millis retains pending volume");const auto before=Preferences::writes;
 S::service(0x4a0U);check(Preferences::writes==before+1,"debounce works across millis wrap");
 check(S::begin(100)&&S::volume()==81,"wrapped timer change survives reboot");
 Preferences::values.clear();Preferences::writes=0;
 S::begin(150,false);S::service(5000);check(Preferences::writes==0,"unavailable old SD does not persist default volume");
 check(S::seedLegacyVolume(37,5100)&&S::volume()==37,"late successful migration can seed old volume");S::service(6400);
 check(S::begin(150,false)&&S::volume()==37,"late seeded volume survives reboot without SD");
 check(!S::seedLegacyVolume(99,7000)&&S::volume()==37,"old SD settings cannot override established NVS volume");
 Preferences::values.clear();S::begin(150,false);S::volume(0,10);
 check(!S::seedLegacyVolume(99,11)&&S::volume()==0,"user mute before SD recovery wins over old settings");S::flush(12);
 check(S::begin(150,false)&&S::volume()==0,"cardless volume change survives reboot");
 std::printf("PASS %u checks: production media-session NVS, 4000 redraws, debounce, silence, write failure/retry, corruption, bounded strings, restart and wrap.\n",checks);
}
