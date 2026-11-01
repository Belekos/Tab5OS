#include <cstdio>
#include <cstdlib>
#include "Tab5ShellLayout.h"
static unsigned checks=0;
static void check(bool ok,const char* message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void resetCase(){preferences_test::reset();Tab5Shell::choices=Tab5Shell::Choices{};}
int main(){
 using namespace Tab5Shell;using namespace preferences_test;
 resetCase();check(load(),"empty settings load");check(writes==0&&blobs.empty(),"empty load never writes");
 check(choices.valid()&&choices.count==7,"default valid");
 LegacyChoices old;old.enabled=0;old.visible=(1u<<5)|(1u<<1)|(1u<<0);const uint8_t order[]={5,4,3,2,1,0,6};std::copy(order,order+7,old.order);
 set("choices",old);const auto original=blobs["choices"];
 check(load(),"legacy migration load");check(writes==0&&blobs.size()==1&&blobs["choices"]==original,"migration never mutates NVS");
 check(choices.valid()&&choices.enabled==0&&choices.count==3,"legacy enabled and visibility");
 check(choices.keys[0]==legacyKey(5)&&choices.keys[1]==legacyKey(1)&&choices.keys[2]==homeKey(),"legacy order preserved");
 check(save(),"explicit v2 save");check(writes==1&&blobs.count("apps-v2")==1&&blobs["choices"]==original,"v1 remains byte-identical after v2 save");
 auto saved=choices;choices=Choices{};check(load(),"v2 load");check(!memcmp(&saved,&choices,sizeof(saved)),"v2 round trip identical");check(writes==1,"v2 load never writes");
 check(blobs["apps-v2"].size()==88,"v2 exact on-disk length");
 for(unsigned mode=0;mode<7;++mode){
  resetCase();set("choices",old);Choices invalid;
  if(mode==0)invalid.version=3;
  if(mode==1)invalid.enabled=2;
  if(mode==2)invalid.count=DockCount+1;
  if(mode==3)invalid.reserved=1;
  if(mode==4)invalid.keys[1]=invalid.keys[0];
  if(mode==5)invalid.keys[0]=0;
  if(mode==6)invalid.keys[DockCount-1]=42;
  set("apps-v2",invalid);const auto before=blobs;const Choices current=choices;
  check(!load(),"invalid v2 rejected");check(blobs==before&&writes==0,"invalid v2 never overwrites v1/v2");
  check(!memcmp(&current,&choices,sizeof(current)),"invalid v2 never partially applies");
 }
 resetCase();set("choices",old);blobs["choices"].pop_back();auto before=blobs;
 check(!load()&&blobs==before&&writes==0,"truncated legacy rejected without write");
 resetCase();set("apps-v2",saved);blobs["apps-v2"].pop_back();before=blobs;
 check(!load()&&blobs==before&&writes==0,"truncated v2 rejected without write");
 resetCase();set("apps-v2",saved);shortRead=true;before=blobs;Choices current=choices;
 check(!load()&&writes==0&&blobs==before&&!memcmp(&current,&choices,sizeof(current)),"short read rejected atomically");
 resetCase();failReadOpen=true;check(load()&&writes==0&&choices.valid(),"missing namespace accepted read-only");
 resetCase();failWriteOpen=true;check(!save()&&writes==0,"write open failure propagated");
 resetCase();shortWrite=true;check(!save()&&writes==1&&blobs.empty(),"short write propagated");
 resetCase();choices.keys[1]=choices.keys[0];check(!save()&&begins==0&&writes==0,"invalid RAM choices never open NVS for write");
 resetCase();old.enabled=1;old.visible=0;set("choices",old);check(load()&&choices.count==0&&choices.dockWidth()==0,"all-hidden migration keeps no dock");
 check(save(),"empty dock can persist");choices=Choices{};check(load()&&choices.count==0&&choices.dockWidth()==0,"empty dock survives restart");
 resetCase();old.enabled=1;old.visible=0x7f;old.order[1]=old.order[0];set("choices",old);before=blobs;
 check(!load()&&blobs==before&&writes==0,"duplicate legacy order rejected without write");
 std::printf("{\"checks\":%u,\"passed\":true,\"boundary\":\"Preferences transport simulated; production load/save executed\"}\n",checks);
}
