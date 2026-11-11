/* Original Tab5OS regression tests. Include the core to exercise fault paths
 * without an installed guest exception handler. Normal instruction tests use
 * cpui386_step, including the real fast/predecoded dispatch when enabled. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef USE_FAST
#include "fast/fast.c"
#else
#include "i386.c"
#endif

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
void *bigmalloc(size_t n) { return calloc(1,n); }

static u8 mmio[0x20000];
static u8 io8(void *o,uword a) { (void)o; CHECK(a>=0xa0000 && a<0xc0000); return mmio[a-0xa0000]; }
static void out8(void *o,uword a,u8 v) { (void)o; CHECK(a>=0xa0000 && a<0xc0000); mmio[a-0xa0000]=v; }
static u16 io16(void *o,uword a) { return (u16)(io8(o,a)|((u16)io8(o,a+1)<<8)); }
static u32 io32(void *o,uword a) { return io16(o,a)|((u32)io16(o,a+2)<<16); }
static void out16(void *o,uword a,u16 v) { out8(o,a,(u8)v);out8(o,a+1,(u8)(v>>8)); }
static void out32(void *o,uword a,u32 v) { out16(o,a,(u16)v);out16(o,a+2,(u16)(v>>16)); }

static void reset(CPUI386 *cpu) { cpui386_reset_pm(cpu,0x4000); cpu->ip=cpu->next_ip; }

static void boundaries(CPUI386 *cpu,u8 *ram,size_t bytes)
{
 reset(cpu); memset(ram+bytes,0xa5,32);
 for(unsigned width=2;width<=4;width+=2) for(int off=-4;off<=2;off++) {
  uword a=(uword)(bytes+off); memset(ram+bytes-8,0x31,8);
  OptAddr p={ADDR_OK1,a,0};u32 expected=0;
  for(unsigned i=0;i<width;i++) if((uint64_t)a+i<bytes) expected|=0x31u<<(8*i);
  CHECK((width==2?load16(cpu,&p):load32(cpu,&p))==expected);
  if(width==2)store16(cpu,&p,0x7698);else store32(cpu,&p,0x32547698);
  for(unsigned i=0;i<width && (uint64_t)a+i<bytes;i++) CHECK(ram[a+i]==(u8)(0x32547698u>>(8*i)));
  for(unsigned i=0;i<32;i++) CHECK(ram[bytes+i]==0xa5);
 }
 /* Non-contiguous translated operands must validate both mapped pages. */
 OptAddr split={ADDR_OK2,0x3fff,(uword)bytes+4096};ram[0x3fff]=0x12;
 CHECK(load32(cpu,&split)==0x12);store32(cpu,&split,0xabcdef76);CHECK(ram[0x3fff]==0x76);
 split.addr1=(uword)bytes-4; /* Force a genuine page-end first address. */
 split.addr1=((uword)bytes&~0xfffu)-1;split.addr2=0x5000;
 ram[split.addr1]=1;ram[0x5000]=2;ram[0x5001]=3;ram[0x5002]=4;
 CHECK(load32(cpu,&split)==0x04030201);
 store32(cpu,&split,0x78563412);CHECK(ram[split.addr1]==0x12 && ram[0x5002]==0x78);
 /* A 32-bit linear wrap may reach low memory; it cannot reach host memory. */
 OptAddr wrap={ADDR_OK1,UINT32_MAX,0};ram[0]=0x12;ram[1]=0x34;ram[2]=0x56;
 CHECK(load32(cpu,&wrap)==0x56341200);
}

static void page_tables(CPUI386 *cpu,u8 *ram,size_t bytes)
{
 u32 v;reset(cpu);cpu->cr0|=CR0_PG|CR0_WP;cpu->cr3=0xfffff000;tlb_clear(cpu);
 CHECK(!cpu_load32(cpu,SEG_DS,0x1234,&v));CHECK(cpu->excno==EX_PF && cpu->cr2==0x1234);
 cpu->cr3=0x1000;memset(ram+0x1000,0,0x2000);pstore32(cpu,0x1000,0xfffff007);tlb_clear(cpu);
 CHECK(!cpu_load32(cpu,SEG_DS,0,&v));CHECK(cpu->excno==EX_PF);
 /* RAM deliberately ends three bytes into its final page. */
 pstore32(cpu,0x1000,((uword)bytes&~0xfffu)|7);tlb_clear(cpu);
 CHECK(!cpu_load32(cpu,SEG_DS,0,&v));
 pstore32(cpu,0x1000,0x2007);pstore32(cpu,0x2000,0x3007);pstore32(cpu,0x2004,0x5007);tlb_clear(cpu);
 ram[0x3fff]=0xaa;ram[0x5000]=0xbb;ram[0x5001]=0xcc;ram[0x5002]=0xdd;
 CHECK(cpu_load32(cpu,SEG_DS,0xfff,&v) && v==0xddccbbaa);
 CHECK(cpu_store32(cpu,SEG_DS,0xfff,0x12345678));CHECK(ram[0x3fff]==0x78 && ram[0x5002]==0x12);
 CHECK((pload32(cpu,0x1000)&0x20) && (pload32(cpu,0x2000)&0x60)==0x60 && (pload32(cpu,0x2004)&0x60)==0x60);
 /* Supervisor write protection and user privilege errors are retained. */
 pstore32(cpu,0x2000,0x3005);tlb_clear(cpu);
 CHECK(!cpu_store32(cpu,SEG_DS,0,0x1234));CHECK(cpu->excno==EX_PF && cpu->excerr==3);
 pstore32(cpu,0x2000,0x3003);tlb_clear(cpu);cpu->cpl=3;
 CHECK(!cpu_load32(cpu,SEG_DS,0,&v));CHECK(cpu->excerr==5);cpu->cpl=0;
 /* Top linear page crosses into page zero, not a 1025th PDE. */
 pstore32(cpu,0x1ffc,0x6007);pstore32(cpu,0x6ffc,0x7007);pstore32(cpu,0x2000,0x3007);tlb_clear(cpu);
 ram[0x7fff]=0x11;ram[0x3000]=0x22;ram[0x3001]=0x33;ram[0x3002]=0x44;
 CHECK(cpu_load32(cpu,SEG_DS,UINT32_MAX,&v) && v==0x44332211);
 /* A present PTE mapping outside physical RAM reads open space safely. */
 pstore32(cpu,0x2000,0xd0000007);tlb_clear(cpu);
 CHECK(cpu_load32(cpu,SEG_DS,0,&v) && !v);
 CHECK(cpu_store32(cpu,SEG_DS,0,0x12345678));
}

static void mmio_boundaries(CPUI386 *cpu,u8 *ram)
{
 reset(cpu);cpu->cb.iomem_read8=io8;cpu->cb.iomem_read16=io16;cpu->cb.iomem_read32=io32;
 cpu->cb.iomem_write8=out8;cpu->cb.iomem_write16=out16;cpu->cb.iomem_write32=out32;
 OptAddr p={ADDR_OK1,0x9ffff,0};ram[0x9ffff]=0x11;mmio[0]=0x22;mmio[1]=0x33;mmio[2]=0x44;
 CHECK(load32(cpu,&p)==0x44332211);store32(cpu,&p,0x12345678);
 CHECK(ram[0x9ffff]==0x78 && mmio[0]==0x56 && mmio[2]==0x12);
 p.addr1=0xbffff;mmio[0x1ffff]=0x11;ram[0xc0000]=0x22;ram[0xc0001]=0x33;ram[0xc0002]=0x44;
 CHECK(load32(cpu,&p)==0x44332211);store32(cpu,&p,0x12345678);
 CHECK(mmio[0x1ffff]==0x78 && ram[0xc0000]==0x56 && ram[0xc0002]==0x12);
 memset(&cpu->cb,0,sizeof(cpu->cb));
}

static void instructions(CPUI386 *cpu,u8 *ram)
{
 reset(cpu);memset(ram+0x4000,0x66,64);CHECK(!cpu_exec1(cpu,1));CHECK(cpu->excno==EX_GP);
#ifdef USE_FAST
 reset(cpu);CHECK(!fast_exec(cpu,512));CHECK(cpu->excno==EX_GP);
#endif
 static const u8 loop[]={0xb8,1,0,0,0,0xa3,0,0x60,0,0,0xeb,0xf4};
 reset(cpu);memcpy(ram+0x4000,loop,sizeof(loop));cpui386_step(cpu,512);CHECK(pload32(cpu,0x6000)==1);
 ram[0x4001]=2;cpui386_step(cpu,512);CHECK(pload32(cpu,0x6000)==2);
 /* A store executed inside a decoded block patches the next iteration. */
 static const u8 self[]={0xb8,1,0,0,0,0xa3,0,0x60,0,0,0xc7,5,1,0x40,0,0,3,0,0,0,0xeb,0xea};
 reset(cpu);memcpy(ram+0x4000,self,sizeof(self));cpui386_step(cpu,512);CHECK(pload32(cpu,0x6000)==3);
 /* Signed division overflow must trap in the guest, never in the host. */
 reset(cpu);ram[0x4000]=0xf7;ram[0x4001]=0xf9;cpu->gprx[0].r32=0;cpu->gprx[2].r32=0x80000000;cpu->gprx[1].r32=UINT32_MAX;
 CHECK(!cpu_exec1(cpu,1));CHECK(cpu->excno==EX_DE);
 reset(cpu);ram[0x4000]=0x66;ram[0x4001]=0xf7;ram[0x4002]=0xf9;cpu->gprx[0].r32=0;cpu->gprx[2].r32=0x8000;cpu->gprx[1].r32=0xffff;
 CHECK(!cpu_exec1(cpu,1));CHECK(cpu->excno==EX_DE);
 /* 486 and i686 register operations, executed through the public scheduler. */
 static const u8 modern[]={0xb8,0x78,0x56,0x34,0x12,0x0f,0xc8,0xbb,2,0,0,0,0x0f,0xc1,0xd8,0x39,0xc0,0xb9,0xef,0xbe,0xad,0xde,0x0f,0x44,0xc1,0xa3,0,0x60,0,0,0xf4};
 reset(cpu);memcpy(ram+0x4000,modern,sizeof(modern));cpui386_step(cpu,512);CHECK(pload32(cpu,0x6000)==0xdeadbeef);
#ifdef I386_ENABLE_FPU
 cpui386_enable_fpu(cpu);
 static const u8 x87[]={0xdb,0xe3,0xd9,0xe8,0xd9,0xe8,0xde,0xc1,0xdb,0x1d,0,0x60,0,0,0xf4};
 reset(cpu);memcpy(ram+0x4000,x87,sizeof(x87));cpui386_step(cpu,512);CHECK(pload32(cpu,0x6000)==2);
#ifdef I386_ENABLE_MMX
 static const u8 mmx[]={0xb8,0x15,0,0,0,0x0f,0x6e,0xc0,0x0f,0xfe,0xc0,0x0f,0x7e,0xc0,0x0f,0x77,0xa3,0,0x60,0,0,0xf4};
 reset(cpu);memcpy(ram+0x4000,mmx,sizeof(mmx));cpui386_step(cpu,512);CHECK(pload32(cpu,0x6000)==42);
#endif
#ifdef I386_ENABLE_SSE2
 static const u8 sse2[]={0xb8,0x15,0,0,0,0x66,0x0f,0x6e,0xc0,0x66,0x0f,0xfe,0xc0,0x66,0x0f,0x7e,0xc0,0xa3,0,0x60,0,0,0xf4};
 reset(cpu);memcpy(ram+0x4000,sse2,sizeof(sse2));cpui386_step(cpu,512);CHECK(pload32(cpu,0x6000)==42);
#endif
#ifdef I386_ENABLE_SSSE3
 static const u8 ssse3[]={0xb8,0xfb,0xff,0xff,0xff,0x66,0x0f,0x6e,0xc0,0x66,0x0f,0x38,0x1e,0xc0,0x66,0x0f,0x7e,0xc0,0xa3,0,0x60,0,0,0xf4};
 reset(cpu);memcpy(ram+0x4000,ssse3,sizeof(ssse3));cpui386_step(cpu,512);CHECK(pload32(cpu,0x6000)==5);
#endif
#endif
}

int main(void)
{
 const size_t bytes=1024*1024+3;u8 *ram=calloc(1,bytes+32);CHECK(ram!=NULL);
 CPUI386 *cpu=cpui386_new(6,(char*)ram,(long)bytes,NULL);CHECK(cpu!=NULL);
 boundaries(cpu,ram,bytes);page_tables(cpu,ram,bytes);mmio_boundaries(cpu,ram);instructions(cpu,ram);
#ifdef USE_FAST
 reset(cpu);ftlb_flush(cpu);CHECK(ftlb_miss(cpu,(uword)bytes-2,1,0)==NULL);
#ifdef FAST_PD
 CHECK(pd_arena!=NULL);CHECK(pd_free>pd_arena); /* prove the tested loop used decoded blocks */
#endif
#endif
 cpui386_delete(cpu);free(ram);printf("PASS DOS x86 CPU: %u checks\n",checks);return 0;
}
