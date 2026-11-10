/* Exercise the actual PC bus and every real peripheral decoder. The reference
 * is the original serial dispatcher; no controller is replaced by a stub. */
#include "pc_internal.h"
#include "video.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned checks, printed;
static uint32_t print_hash;
static bool printer(uint8_t value) { ++printed; print_hash=(print_hash^value)*16777619u; return true; }
static uint8_t legacy_in(uint16_t port) {
    uint8_t value; port&=0x3ff; assert(port>=0x100);
    if(hdc_in(port,&value)||fdc_in(port,&value)||uart_in(port,&value)||
       lpt_in(port,&value)||sound_in(port,&value)||video_in(port,&value)) return value;
    return 0xff;
}
static void legacy_out(uint16_t port,uint8_t value) {
    port&=0x3ff; assert(port>=0x100);
    if(!hdc_out(port,value)&&!fdc_out(port,value)&&!uart_out(port,value)&&
       !lpt_out(port,value)&&!sound_out(port,value)) video_out(port,value);
}
static bool old_path;
static uint8_t input(uint16_t port) { return old_path?legacy_in(port):pc.cpu.bus.in(NULL,port); }
static void output(uint16_t port,uint8_t value) {
    if(old_path) legacy_out(port,value); else pc.cpu.bus.out(NULL,port,value);
}
static uint32_t hash;
static void remember(uint32_t value) { hash=(hash^value)*16777619u; }
static void observe(void) {
    /* Include shared attribute/retrace flip-flop, palette auto-increment,
     * all four boundary ports, the monochrome block and real peripherals. */
    static const uint16_t ports[]={0x3bf,0x3c0,0x3c1,0x3c2,0x3c4,0x3c5,
        0x3c6,0x3c7,0x3c8,0x3c9,0x3cc,0x3ce,0x3cf,0x3d4,0x3d5,
        0x3da,0x3df,0x3e0,0x3bc,0x378,0x379,0x37a,0x3f2,0x3f4,
        0x3f7,0x3f8,0x3fb,0x3fc,0x3fd,0x3ff,0x388,0x389,0x22e,0x1f7};
    for(unsigned i=0;i<sizeof(ports)/sizeof(*ports);i++) remember(input(ports[i]));
    remember(printed); remember(print_hash);
}
static void write_read(uint16_t port,uint8_t value) { output(port,value);observe(); }
static uint32_t trace(bool legacy) {
    old_path=legacy; pc.cpu.cycles=0; pc.cpu.clk=0; pc_reset();
    printed=0; print_hash=hash=2166136261u;
    /* Set up printer data, serial scratch/divisors, floppy control, and DSP
     * acknowledgement before video accesses, detecting collateral changes. */
    output(0x378,0x5a);output(0x37a,0);output(0x37a,1);
    output(0x3ff,0x5a);output(0x3fb,0x80);output(0x3f8,0x0c);output(0x3f9,0);
    output(0x3f2,0x0c);output(0x226,1);output(0x226,0);observe();
    for(unsigned alias=0;alias<64;alias++) {
        unsigned high=alias<<10;
        for(unsigned port=0x3bf;port<=0x3e0;port++) {
            /* Reads through every 16-bit ISA alias must remain identical. */
            remember(input((uint16_t)(high|port)));
        }
        write_read((uint16_t)(high|0x3bc),(uint8_t)alias);
        write_read((uint16_t)(high|0x3c4),2);write_read((uint16_t)(high|0x3c5),(uint8_t)(alias&15));
        write_read((uint16_t)(high|0x3ce),4);write_read((uint16_t)(high|0x3cf),(uint8_t)(alias&3));
        write_read((uint16_t)(high|0x3c8),(uint8_t)alias);
        for(unsigned component=0;component<3;component++)write_read((uint16_t)(high|0x3c9),(uint8_t)((alias+component)&63));
        output((uint16_t)(high|0x3d4),14);write_read((uint16_t)(high|0x3d5),(uint8_t)alias);
        input((uint16_t)(high|0x3da));output((uint16_t)(high|0x3c0),0x20|(alias&15));
        write_read((uint16_t)(high|0x3c0),(uint8_t)(alias&63));
        write_read((uint16_t)(high|0x3bf),0);write_read((uint16_t)(high|0x3df),0);write_read((uint16_t)(high|0x3e0),0);
    }
    /* Exercise index/data word writes through the production word callback. */
    for(unsigned i=0;i<32;i++) {
        uint16_t port=(uint16_t)(0x3ce|(i<<10)),value=(uint16_t)(4|((i&3)<<8));
        if(legacy){legacy_out(port,(uint8_t)value);legacy_out((uint16_t)(port+1),(uint8_t)(value>>8));}
        else pc.cpu.bus.outw(NULL,port,value);
        uint16_t word=legacy?(uint16_t)(legacy_in(port)|(legacy_in((uint16_t)(port+1))<<8)):pc.cpu.bus.inw(NULL,port);
        remember(word);observe();
    }
    return hash;
}
int main(void) {
    pc_config_t cfg={0};cfg.board=PC_AT;cfg.cpu=X86_80286;cfg.cpu_hz=6000000;
    cfg.video=PC_VGA;cfg.conv_kb=640;cfg.ext_kb=1024;cfg.printer=printer;
    assert(!posix_memalign((void**)&cfg.ram,4096,PC_RAM_BYTES(cfg.ext_kb)));
    cfg.page_table=calloc(X86_PAGES,sizeof(uintptr_t));assert(cfg.page_table&&pc_init(&cfg));
    uint32_t hashes[8];unsigned count=0;
    for(unsigned board=PC_XT;board<=PC_AT;board++)for(unsigned video=PC_CGA;video<=PC_VGA;video++) {
        pc.cfg.board=(pc_board_t)board;pc.cfg.video=(pc_video_t)video;pc_reset();
        /* These assertions also catch a future peripheral decoder growing
         * into the fast-path block: then the optimization must be revisited. */
        for(unsigned port=0x3c0;port<=0x3df;port++)for(unsigned value=0;value<=255;value++) {
            uint8_t read=0;
            assert(!hdc_in(port,&read)&&!fdc_in(port,&read)&&!uart_in(port,&read)&&!lpt_in(port,&read)&&!sound_in(port,&read));
            assert(!hdc_out(port,(uint8_t)value)&&!fdc_out(port,(uint8_t)value)&&!uart_out(port,(uint8_t)value)&&!lpt_out(port,(uint8_t)value)&&!sound_out(port,(uint8_t)value));
            checks+=10;
        }
        uint32_t expected=trace(true),actual=trace(false);assert(expected==actual);++checks;hashes[count++]=actual;
    }
    printf("{\"passed\":true,\"checks\":%u,\"configurations\":8,\"aliasBits\":6,\"aliasCount\":64,\"traceHashes\":[",checks);
    for(unsigned i=0;i<count;i++)printf("%s%u",i?",":"",hashes[i]);
    puts("],\"realControllerDecoders\":true,\"referenceDispatchEquivalent\":true}");return 0;
}
