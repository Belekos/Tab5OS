#include "Tab5AppApi.h"
/* Non-mutating hardware availability example. Build with tools/native-apps/build.py. */
__attribute__((visibility("default"))) int tab5_app_main(int argc,char** argv){
 (void)argc;(void)argv;const struct tab5_app_api3* api=tab5_app_api_v3();
 if(!api||api->base.base.version!=3||api->base.base.size<sizeof(*api))return 1;
 static const int pins[]={2,3,4,5,6,7,16,17,18,19,20,21,34,35,37,38,45,47,48,51,52,53,54};
 for(unsigned i=0;i<sizeof(pins)/sizeof(*pins);++i){
  int p=pins[i],status=api->hardware(36,p,0,0,0,0,0,0);char line[]={ 'G',(char)('0'+p/10),(char)('0'+p%10),':',' ',(char)('0'+status),'\n'};
  api->base.base.log(line,sizeof(line));
 }
 return 0;
}
