/* Real core regression: PIT integrals, AdLib detection/synthesis, SB PCM DMA. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pc_internal.h"
#include "emu8950.h"
#include "emu8950_noise.h"
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) {fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;} } while(0)
static void opl(uint8_t reg,uint8_t value){sound_out(0x388,reg);sound_out(0x389,value);}
static void dsp(uint8_t value){sound_out(0x22c,value);}
static uint8_t in(uint16_t port){uint8_t value=0;return sound_in(port,&value)?value:0;}
static uint32_t noise_reference(uint32_t noise,unsigned clocks) {
    while(clocks--){if(noise&1u)noise^=0x800200u;noise>>=1;}
    return noise;
}
static uint32_t reference(uint32_t r,uint64_t a,uint64_t b) {
    uint32_t half=(r+1)/2;
    uint64_t sa=a>1?a-1:0,sb=b>1?b-1:0;
    uint64_t ha=sa/r*half+(sa%r<half?sa%r:half),hb=sb/r*half+(sb%r<half?sb%r:half);
    return (uint32_t)(hb-ha+(a<1?(b<1?b:1)-a:0));
}
int main(void) {
    /* Every possible YM3812 noise state, including the zero state; compare
     * the two intermediate jumps to the original one-clock recurrence. */
    for(uint32_t noise=0;noise<(1u<<23);noise++) {
        CHECK(opl_noise_advance14(noise)==noise_reference(noise,14));
        CHECK(opl_noise_advance2(noise)==noise_reference(noise,2));
    }
    uint32_t noise_random=0x91fa003bu;
    for(unsigned i=0;i<65536;i++) {
        noise_random=noise_random*1664525u+1013904223u;
        CHECK(opl_noise_advance14(noise_random)==noise_reference(noise_random,14));
        CHECK(opl_noise_advance2(noise_random)==noise_reference(noise_random,2));
    }
    puts("OPL NOISE EQUIVALENCE PASS: all 8388608 23-bit states and 65536 full-width states, 14/2 clocks");
    pc_config_t cfg={0};cfg.board=PC_AT;cfg.cpu=X86_80286;cfg.cpu_hz=6000000;cfg.video=PC_VGA;
    cfg.conv_kb=640;cfg.ext_kb=1024;
    CHECK(!posix_memalign((void**)&cfg.ram,4096,PC_RAM_BYTES(cfg.ext_kb)));
    cfg.page_table=calloc(PC_PAGES(PC_AT),sizeof(uintptr_t));CHECK(cfg.page_table&&pc_init(&cfg));
    static const uint32_t periods[]={2,3,4,7,100,1193,65535,65536};
    uint32_t random=17;
    for(unsigned r=0;r<sizeof(periods)/sizeof(*periods);r++) {
        pit_reset();pit_write(3,0xb6);pit_write(2,(uint8_t)periods[r]);pit_write(2,(uint8_t)(periods[r]>>8));pit_gate2(true);
        for(unsigned n=0;n<10000;n++) {
            random=random*1664525u+1013904223u;
            uint64_t a=n<100?n:(uint64_t)random+(n&1?0:UINT64_C(1)<<34);
            uint64_t b=a+(n%3?random%1000:UINT64_C(1)<<33);
            CHECK(pit_high2(a,b)==reference(periods[r],a,b));
        }
    }
    pit_reset();speaker_reset();sound_reset();
    CHECK(in(0x388)==6);
    opl(2,255);opl(4,1);pc.cpu.cycles=cfg.cpu_hz/1000;
    CHECK((in(0x388)&0xc0)==0xc0);opl(4,0x80);CHECK(in(0x388)==6);
    pc.cpu.cycles=0;speaker_reset();sound_reset();
    sound_out(0x226,1);sound_out(0x226,0);CHECK(in(0x22e)&0x80);CHECK(in(0x22a)==0xaa);
    dsp(0xe1);CHECK(in(0x22a)==2);CHECK(in(0x22a)==1);
    dsp(0xd1);dsp(0x10);dsp(255);CHECK(sound_sample()==127*40);
    dsp(0xd3);CHECK(sound_sample()==0);dsp(0xd1);
    pc.ram[0x1000]=0;pc.ram[0x1001]=255;pc.ram[0x1002]=128;
    dma_write(0x0c,0);dma_write(2,0);dma_write(2,0x10);dma_write(3,2);dma_write(3,0);
    dma_write(0x83,0);dma_write(0x0b,0x49);dma_write(0x0a,1);
    dsp(0x41);dsp((uint8_t)(PC_AUDIO_HZ>>8));dsp((uint8_t)PC_AUDIO_HZ);
    dsp(0x14);dsp(2);dsp(0);CHECK(sound_next_event()!=NEVER);
    CHECK(sound_sample()==-128*40);CHECK(sound_sample()==127*40);CHECK(sound_sample()==0);
    CHECK(sound_next_event()==NEVER);CHECK(pic_read(0,0)&0x80);CHECK(dma_read(8)&2);
    /* 1 Hz ADPCM must neither divide by zero nor fire its block immediately. */
    dsp(0x41);dsp(0);dsp(1);dsp(0x16);dsp(0);dsp(0);
    CHECK(sound_next_event()!=NEVER);CHECK(sound_next_event()>cfg.cpu_hz*3u);
    for(unsigned i=0;i<PC_AUDIO_HZ;i++)sound_sample();CHECK(sound_next_event()!=NEVER);
    sound_reset();
    static const uint8_t offset[]={0,1,2,8,9,10,16,17,18};
    for(int ch=0;ch<9;ch++) {
        int op=offset[ch];opl((uint8_t)(0x20+op),1);opl((uint8_t)(0x23+op),1);
        opl((uint8_t)(0x40+op),0x10);opl((uint8_t)(0x43+op),0);
        opl((uint8_t)(0x60+op),0xf0);opl((uint8_t)(0x63+op),0xf0);
        opl((uint8_t)(0x80+op),0x77);opl((uint8_t)(0x83+op),0x77);
        opl((uint8_t)(0xc0+ch),1);opl((uint8_t)(0xa0+ch),(uint8_t)(0x98+ch*3));opl((uint8_t)(0xb0+ch),0x31);
    }
    int16_t pcm[512*2];unsigned nonzero=0;uint32_t hash=2166136261u,at=0;
    clock_t started=clock();
    while(at<PC_AUDIO_HZ*4u) {
        int n=PC_AUDIO_HZ*4u-at<512?(int)(PC_AUDIO_HZ*4u-at):512;memset(pcm,0,sizeof(pcm));sound_mix(pcm,n,at);
        for(int i=0;i<n*2;i++){nonzero+=pcm[i]!=0;hash=(hash^(uint16_t)pcm[i])*16777619u;}
        for(int i=0;i<n;i++)CHECK(pcm[i*2]==pcm[i*2+1]);at+=(uint32_t)n;
    }
    CHECK(nonzero>PC_AUDIO_HZ);
    double synth_ms=1000.0*(clock()-started)/CLOCKS_PER_SEC/4;
    /* The freestanding mode cannot silently enable an expensive converter. */
    CHECK(OPL_new(3579545,22050)==NULL);
    /* Golden hash produced independently by the unmodified emu8950 sources
     * in esp32emu 7d6db01763bfe7980bd439b8a80e6b1f423d22cb. Six seconds of
     * register writes cover rhythm, key scaling, envelopes and timer bits. */
    OPL *trace=OPL_new(3579545,3579545/72);CHECK(trace!=NULL);OPL_setChipType(trace,2);OPL_reset(trace);
    uint32_t trace_hash=2166136261u,rnd=9381;nonzero=0;
    for(unsigned tick=0;tick<3579545u/72*6;tick++) {
        if(!(tick%97)){rnd=rnd*1664525u+1013904223u;OPL_writeReg(trace,(rnd>>16)&255,rnd&255);}
        int16_t sample=OPL_calc(trace);nonzero+=sample!=0;trace_hash=(trace_hash^(uint16_t)sample)*16777619u;
    }
    OPL_delete(trace);CHECK(nonzero>100);CHECK(trace_hash==0x28ad29b6u);
    printf("AUDIO PASS checks=%u opl_hash=%08x synth_ms_per_second=%.2f (host only)\n",checks,hash,synth_ms);
    puts("OPL NATIVE EQUIVALENCE PASS: six-second trace equals pristine upstream");
    return 0;
}
