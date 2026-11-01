#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "tuner_fc0013.h"
typedef struct {uint8_t value,reg;int writes,reads,failWrite,failRead;uint64_t hash;} Device;
static void byte(Device* d,uint8_t b){d->hash=(d->hash^b)*UINT64_C(1099511628211);}
int rtlsdr_i2c_write_fn(void* p,uint8_t addr,uint8_t* buf,int len){
 Device* d=p;++d->writes;byte(d,1);byte(d,addr);byte(d,len);for(int i=0;i<len;++i)byte(d,buf[i]);
 if(d->writes==d->failWrite)return -1;
 if(len==1)d->reg=buf[0];else if(len==2){assert(buf[0]==0x14);d->value=buf[1];}else assert(0);
 return len;
}
int rtlsdr_i2c_read_fn(void* p,uint8_t addr,uint8_t* buf,int len){
 Device* d=p;++d->reads;byte(d,2);byte(d,addr);byte(d,len);assert(len==1);
 if(d->reads==d->failRead)return -1;
 assert(d->reg==0x14);*buf=d->value;return len;
}
int main(void){
 unsigned cases=0;uint64_t total=UINT64_C(1469598103934665603);
 for(int upper=0;upper<256;upper+=32)for(int gain=-1000;gain<=1000;++gain)for(int failure=0;failure<4;++failure){
  Device d={(uint8_t)(upper|31),0,0,0,failure==1?1:failure==3?2:0,failure==2?1:0,UINT64_C(1469598103934665603)};
  int result=fc0013_set_lna_gain(&d,gain);
  assert((result==0)==(failure==0));assert(d.writes==2);assert(d.reads==(failure==1?0:1));
  if(!failure)assert((d.value&0xe0)==upper);
  total=(total^d.hash)*UINT64_C(1099511628211);total=(total^(uint32_t)result)*UINT64_C(1099511628211);total=(total^d.value)*UINT64_C(1099511628211);++cases;
 }
 printf("{\"passed\":true,\"cases\":%u,\"traceHash\":\"%016llx\"}\n",cases,(unsigned long long)total);
}
