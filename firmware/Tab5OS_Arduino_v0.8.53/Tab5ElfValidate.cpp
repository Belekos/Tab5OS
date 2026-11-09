#include "Tab5ElfValidate.h"
#include <string.h>
#include <algorithm>
namespace Tab5ElfValidate {
namespace {
struct Header { uint8_t ident[16];uint16_t type,machine;uint32_t version,entry,phoff,shoff,flags;uint16_t ehsize,phentsize,phnum,shentsize,shnum,shstrndx; };
struct Program { uint32_t type,offset,vaddr,paddr,filesz,memsz,flags,align; };
struct Section { uint32_t name,type,flags,addr,offset,size,link,info,align,entsize; };
struct Symbol { uint32_t name,value,size;uint8_t info,other;uint16_t section; };
struct Rela { uint32_t offset,info;int32_t addend; };
static_assert(sizeof(Header)==52&&sizeof(Program)==32&&sizeof(Section)==40&&sizeof(Symbol)==16&&sizeof(Rela)==12,"ELF32 layout");
bool fits(uint32_t offset,uint32_t count,size_t length){return offset<=length&&count<=length-offset;}
bool power(uint32_t n){return n==0||(n&(n-1))==0;}
template<class T>T read(const uint8_t* p){T t;memcpy(&t,p,sizeof(t));return t;}
struct Parser {
 const uint8_t* p;size_t n;Header h;const char** reason;bool (*yield)();unsigned ticks=0;
 bool fail(const char* why){if(reason)*reason=why;return false;}
 bool step(){return (++ticks%64)!=0||!yield||yield();}
 Section section(unsigned i)const{return read<Section>(p+h.shoff+i*sizeof(Section));}
 Program program(unsigned i)const{return read<Program>(p+h.phoff+i*sizeof(Program));}
 const char* string(const Section& s,uint32_t offset)const{
  if(offset>=s.size)return nullptr;
  const char* value=reinterpret_cast<const char*>(p+s.offset+offset);
  return memchr(value,0,std::min<size_t>(s.size-offset,129))?value:nullptr;
 }
 bool mapped(uint32_t addr,uint32_t size,uint32_t flags=0,bool initialized=false)const{
  for(unsigned i=0;i<h.phnum;++i){auto s=program(i);uint32_t count=initialized?s.filesz:s.memsz;
   if(s.type==1&&(s.flags&flags)==flags&&addr>=s.vaddr&&addr-s.vaddr<=count&&size<=count-(addr-s.vaddr))return true;
  }return false;
 }
 bool run(Image& image){
  if(memcmp(h.ident,"\177ELF",4)||h.ident[4]!=1||h.ident[5]!=1||h.ident[6]!=1||h.ident[7]!=0||h.type!=3||h.machine!=243||h.version!=1)
   return fail("ELF32 RISC-V little-endian ET_DYN required");
  // ESP32-P4 uses the soft-float ILP32 ABI; RVC is permitted, RVE/float/TSO are not.
  if(h.flags&~1u)return fail("RISC-V ABI flags unsupported");
  if(h.ehsize!=52||h.phentsize!=32||h.shentsize!=40||!h.phnum||h.phnum>16||h.shnum<2||h.shnum>128||h.shstrndx>=h.shnum||!h.shstrndx||h.phoff%4||h.shoff%4||!fits(h.phoff,h.phnum*32,n)||!fits(h.shoff,h.shnum*40,n))return fail("Invalid ELF header tables");
  uint32_t start=0,end=0;bool seen=false;
  for(unsigned i=0;i<h.phnum;++i){auto s=program(i);
   if(s.type==3||s.type==7)return fail("Interpreters and TLS unsupported");
   if(!fits(s.offset,s.filesz,n)||!power(s.align)||s.align>65536)return fail("Invalid program range");
   if(s.type!=1)continue;
   if(s.memsz<s.filesz||!s.memsz||s.memsz>MaxImage||s.vaddr>UINT32_MAX-s.memsz||(seen&&s.vaddr<end)||!(s.flags&4)||s.flags&~7u||((s.flags&3)==3))return fail("Invalid load segment");
   if(s.align>1&&(s.vaddr%s.align)!=(s.offset%s.align))return fail("Invalid segment alignment");
   if(!seen){start=s.vaddr;seen=true;}end=s.vaddr+s.memsz;
  }
  if(!seen||start%64||end-start>MaxImage||!mapped(h.entry,2,1,true)||h.entry%2)return fail("Invalid entry or image span");
  auto names=section(h.shstrndx);if(names.type!=3||!fits(names.offset,names.size,n)||!names.size||names.size>65536)return fail("Invalid section names");
  size_t totalSymbols=0,totalRelocations=0,totalStrings=0,totalTables=0;bool api=false;uint32_t stack=16384;bool stackSeen=false;
  for(unsigned i=0;i<h.shnum;++i){auto s=section(i);
   if(!step())return fail("Cancelled");
   if(!power(s.align)||s.align>65536||!string(names,s.name)||s.type==9||(s.flags&0x400))return fail("Unsupported section");
   if(s.type!=8&&!fits(s.offset,s.size,n))return fail("Section outside file");
   if(!strcmp(string(names,s.name),".tab5.stack")){
    if(stackSeen||s.type!=1||s.size!=4||(s.flags&7)!=2)return fail("Invalid stack request");
    stackSeen=true;stack=read<uint32_t>(p+s.offset);
    if(stack<16384||stack>262144||stack%4096)return fail("Stack request outside budget");
   }
   if(s.type==0){if(i!=0||s.size)return fail("Invalid null section");continue;}
   if((s.flags&2)&&(s.align>64||(s.align>1&&s.addr%s.align)))return fail("Section alignment exceeds native mapping");
   if((s.flags&2)&&s.size&&!mapped(s.addr,s.size,0,s.type!=8))return fail("Allocated section outside image");
   if((s.flags&2)&&s.type!=8&&s.size){bool same=false;
    for(unsigned k=0;k<h.phnum;++k){auto seg=program(k);if(seg.type==1&&s.addr>=seg.vaddr&&s.addr-seg.vaddr<=seg.filesz&&s.size<=seg.filesz-(s.addr-seg.vaddr)&&s.offset==seg.offset+(s.addr-seg.vaddr))same=true;}
    if(!same)return fail("Section/segment mapping differs");
   }
   if(s.type==3){totalStrings+=s.size;if(totalStrings>65536||!s.size||p[s.offset]!=0||p[s.offset+s.size-1]!=0)return fail("String table budget or terminator");}
   if(s.type==2||s.type==11){
    totalSymbols+=s.size/16;totalTables+=s.size;
    if(s.offset%4||s.entsize!=16||s.size%16||!s.size||s.link>=h.shnum||totalSymbols>4096||totalTables>1048576)return fail("Invalid symbol table budget");
    auto strings=section(s.link);if(strings.type!=3||!fits(strings.offset,strings.size,n)||strings.size>65536)return fail("Invalid symbol strings");
    for(unsigned j=0;j<s.size/16;++j){if(!step())return fail("Cancelled");auto sym=read<Symbol>(p+s.offset+j*16);const char* name=string(strings,sym.name);
     if(!name)return fail("Invalid symbol name");
     if((sym.info&15)==6||sym.section==0xfff2)return fail("TLS/common symbols unsupported");
     if(sym.section==0){
      if(!j&&!sym.name&&!sym.value&&!sym.size&&!sym.info)continue;
      if((strcmp(name,"tab5_app_api_v1")&&strcmp(name,"tab5_app_api_v2")&&strcmp(name,"tab5_app_api_v3")&&strcmp(name,"tab5_app_api_v4")&&strcmp(name,"tab5_app_api_v5")&&strcmp(name,"tab5_app_api_v6")&&strcmp(name,"tab5_app_api_v7"))||sym.value||sym.size)return fail("Only Tab5 ABI entry points may be imported");
      if(s.type==11)api=true;
     }else if(sym.section<0xff00){
      if(sym.section>=h.shnum)return fail("Invalid symbol section");
      auto owner=section(sym.section);
      if((owner.flags&2)&&!mapped(sym.value,sym.size?sym.size:1))return fail("Symbol outside image");
     }else if(sym.section!=0xfff1)return fail("Unsupported special symbol");
    }
   }
   if(s.type==4){
    totalRelocations+=s.size/12;totalTables+=s.size;
    if(s.offset%4||s.entsize!=12||s.size%12||s.link>=h.shnum||s.info>=h.shnum||totalRelocations>65536||totalTables>1048576)return fail("Invalid relocation table budget");
    auto symbols=section(s.link);
    if((symbols.type!=11&&symbols.type!=2)||symbols.entsize!=16||symbols.size%16||!fits(symbols.offset,symbols.size,n))return fail("Invalid relocation symbols");
    for(unsigned j=0;j<s.size/12;++j){if(!step())return fail("Cancelled");auto r=read<Rela>(p+s.offset+j*12);unsigned type=r.info&255,index=r.info>>8;
     if(index>=symbols.size/16||(type!=0&&type!=1&&type!=3&&type!=5))return fail("Unsupported relocation");
     if(type==0)continue;
     if(r.offset%4||!mapped(r.offset,4,2))return fail("Relocation target outside writable segment");
     if(type==3){if(index||r.addend<0||!mapped(uint32_t(r.addend),1))return fail("Invalid relative relocation");}
     else {auto sym=read<Symbol>(p+symbols.offset+index*16);
      if(!sym.section){if(type==1&&r.addend)return fail("Imported symbol addend unsupported");}
      else {int64_t address=int64_t(sym.value)+(type==1?r.addend:0);if(sym.section>=h.shnum||address<0||address>UINT32_MAX||!mapped(uint32_t(address),1))return fail("Relocated symbol outside image");}
     }
    }
   }
   if(s.type==6){
    if(s.entsize!=8||s.size%8||s.size>4096)return fail("Invalid dynamic table");
    bool terminated=false;
    for(unsigned j=0;j<s.size;j+=8){auto tag=read<uint32_t>(p+s.offset+j);auto value=read<uint32_t>(p+s.offset+j+4);if(!tag){terminated=true;break;}
     if(tag==1||tag==12||tag==13||tag==15||tag==17||tag==18||tag==19||tag==22||tag==25||tag==26||tag==27||tag==28||tag==29||tag==32||tag==33||tag==35||tag==36||tag==37||(tag==30&&(value&4)))return fail("Dynamic dependencies/initializers unsupported");
    }if(!terminated)return fail("Unterminated dynamic table");
   }
   if(s.type==14||s.type==15||s.type==16)return fail("Initializers unsupported");
  }
  if(!api)return fail("Application ABI import missing");
  image={start,end-start,h.entry,stack};return true;
 }
};
}
bool validate(const uint8_t* file,size_t length,Image& image,const char** reason,bool (*cooperate)()){
 image={};if(reason)*reason=nullptr;
 if(!file||length<sizeof(Header)||length>MaxFile){if(reason)*reason="Invalid ELF size";return false;}
 Parser parser{file,length,read<Header>(file),reason,cooperate};return parser.run(image);
}
}
