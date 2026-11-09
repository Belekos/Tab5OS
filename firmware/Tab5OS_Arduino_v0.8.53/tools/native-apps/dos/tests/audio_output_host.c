#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int output(const int16_t*,size_t,uint32_t);
static const struct {int (*audio_pcm)(const int16_t*,size_t,uint32_t);} api={output},*tab5_api=&api;
#include "../app_audio.h"
static int calls,remaining,enabled,reject,silence;static int16_t received[4];
/* Same deferred full-block submission as the OS driver: its partial block
 * must receive silence after a short sound, or that sound never reaches I2S. */
static int queued,queued_nonzero,submitted_nonzero;
void pc_audio_enable(bool on){enabled=on;}
int pc_audio_pending(void){return remaining;}
int pc_audio_render(int16_t *dst,int frames){(void)frames;if(!remaining)return 0;remaining--;dst[0]=silence?0:32767;dst[1]=silence?0:-32768;dst[2]=silence?0:10000;dst[3]=silence?0:-10000;return 2;}
static int output(const int16_t *pcm,size_t frames,uint32_t rate){
 if(frames!=2||rate!=PC_AUDIO_HZ)return -1;calls++;memcpy(received,pcm,sizeof(received));if(reject)return -1;
 for(size_t i=0;i<frames;i++){
  if(queued==1024){submitted_nonzero+=queued_nonzero;queued=queued_nonzero=0;}
  queued++;queued_nonzero+=pcm[i*2]!=0||pcm[i*2+1]!=0;
 }
 return 0;
}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 dos_audio_start();CHECK(enabled&&dos_audio_volume()==80);
 silence=1;remaining=4;CHECK(!dos_audio_pump()&&!calls&&!remaining);silence=0;
 remaining=1;CHECK(!dos_audio_pump());CHECK(calls==1&&received[0]==26213&&received[1]==-26214);
 CHECK(submitted_nonzero==0);silence=1;remaining=1200;CHECK(!dos_audio_pump()&&!remaining&&submitted_nonzero==2&&!queued_nonzero);
 int before=calls;remaining=4;CHECK(!dos_audio_pump()&&calls==before);silence=0;
 dos_audio_change(100);CHECK(dos_audio_volume()==100);remaining=1;dos_audio_pump();CHECK(received[0]==32767&&received[1]==-32768);
 dos_audio_change(-50);dos_audio_mute();CHECK(!dos_audio_volume());remaining=1200;CHECK(!dos_audio_pump()&&!remaining&&!received[0]&&!received[1]&&!queued_nonzero);
 before=calls;remaining=4;CHECK(!dos_audio_pump()&&calls==before);
 dos_audio_mute();CHECK(dos_audio_volume()==50);remaining=1;dos_audio_pump();CHECK(received[2]==5000&&received[3]==-5000);
 dos_audio_change(-100);CHECK(!dos_audio_volume());dos_audio_change(10);CHECK(dos_audio_volume()==10);
 silence=1;remaining=1200;CHECK(!dos_audio_pump()&&!remaining&&!queued_nonzero);
 remaining=4;before=calls;CHECK(!dos_audio_pump()&&calls==before&&!remaining);silence=0;
 reject=1;remaining=4;CHECK(dos_audio_pump()==-1&&dos_audio_failed()&&!remaining);CHECK(calls==before+1);
 remaining=3;CHECK(!dos_audio_pump()&&!remaining&&calls==before+1);reject=0;dos_audio_change(10);remaining=1;CHECK(!dos_audio_pump()&&!dos_audio_failed()&&calls==before+2);
 puts("AUDIO OUTPUT PASS: volume clipping/mute/restore/buffered tail/idle silence/failure/retry");return 0;
}
