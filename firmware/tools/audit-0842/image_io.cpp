#include "Platform.h"
#include "Tab5AtomicFile.h"
#include "Tab5ImageWrite.h"
#include "src/jpeg/jpge.h"
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <unordered_set>
#include <cstdio>
#include <new>
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
std::unordered_set<void*> allocations;
int failAllocation=-1;
void* heap_caps_malloc(size_t n,int){if(failAllocation==0)return nullptr;if(failAllocation>0)--failAllocation;void* p=std::malloc(n);if(p)allocations.insert(p);return p;}
void heap_caps_free(void* p){if(p){assert(allocations.erase(p)==1);std::free(p);}}
void delay(unsigned){}
const char* tab5StorageErrorText(){return "Injected I/O error";}
#include "production.inc"
void put(const char* path,const char* text){auto n=std::make_shared<Node>();n->bytes=text;files[path]=n;}
std::string get(const char* path){auto at=files.find(path);return at==files.end()?std::string():at->second->bytes;}
void reset(){assert(allocations.empty());files.clear();failOperation=-1;operationCount=0;statErrorPath.clear();openErrorPath.clear();statErrorAfterOpen=openFailed=flushFault=closeFault=false;failAllocation=-1;jpegCancelled=false;}
int main(int argc,char** argv){
 bool baseline=argc>1&&!std::strcmp(argv[1],"baseline");uint16_t pixels[16*16];std::fill(pixels,pixels+256,0x39ef);String error;
 const char* path="/Picture/export.jpg";const char* temp="/Picture/export.jpg.tmp";
 reset();flushFault=true;bool ok=encode(pixels,nullptr,16,16,path,error);
 if(baseline){assert(ok&&!get(path).empty());reset();closeFault=true;assert(encode(pixels,nullptr,16,16,path,error));using Writer=Tab5ImageWrite<decltype(tab5FS),File>;for(bool closing:{false,true}){reset();Writer w;put("/Picture/a.bmp","ORIGINAL");int h=w.begin(tab5FS,"/Picture/a.bmp","w");assert(h>0&&w.chunk(h,"NEW",3)==3);flushFault=!closing;closeFault=closing;assert(w.finish(tab5FS,h,true)==0&&get("/Picture/a.bmp")=="NEW");}std::puts("baseline confirmed: JPEG export and ImageWrite both promote on errno-only flush/close EIO");return 0;}
 assert(!ok&&get(path).empty()&&get(temp).empty()&&allocations.empty());
 reset();closeFault=true;assert(!encode(pixels,nullptr,16,16,path,error)&&get(path).empty()&&get(temp).empty());
 reset();put(path,"ORIGINAL");statErrorPath=path;assert(!encode(pixels,nullptr,16,16,path,error)&&get(path)=="ORIGINAL");
 reset();put(temp,"RECOVERY");statErrorPath=temp;assert(!encode(pixels,nullptr,16,16,path,error)&&get(temp)=="RECOVERY");
 reset();statErrorPath=path;statErrorAfterOpen=true;assert(!encode(pixels,nullptr,16,16,path,error)&&get(path).empty());
 for(int allocation=0;allocation<2;++allocation){reset();failAllocation=allocation;assert(!encode(pixels,nullptr,16,16,path,error)&&allocations.empty());}
 for(int failure=0;failure<30;++failure){reset();failOperation=failure;ok=encode(pixels,nullptr,16,16,path,error);assert(allocations.empty());if(ok){auto bytes=get(path);assert(bytes.size()>4&&uint8_t(bytes[0])==255&&uint8_t(bytes[1])==216&&uint8_t(bytes.back())==217);}else assert(get(path).empty());}
 reset();assert(encode(pixels,nullptr,16,16,path,error));assert(!get(path).empty());
 using Writer=Tab5ImageWrite<decltype(tab5FS),File,true>;
 const char* image="/Picture/a.jpg";const char* backup="/Picture/a.jpg.tab5old";const char* temporary="/Picture/a.jpg.tab5tmp";
 for(const char* unavailable:{"/Picture",image,backup,temporary}){
  reset();put("/Picture","");put(backup,"ORIGINAL");statErrorPath=unavailable;Writer w;assert(w.begin(tab5FS,image,"w")<0);assert(get(image)=="ORIGINAL"||get(backup)=="ORIGINAL");
 }
 reset();put(image,"ORIGINAL");put(backup,"OLDER");Writer w;int h=w.begin(tab5FS,image,"w");assert(h>0&&w.chunk(h,"NEW",3)==3);flushFault=true;assert(w.finish(tab5FS,h,true)<0&&get(image)=="ORIGINAL"&&get(backup)=="OLDER");
 reset();put(image,"ORIGINAL");put(backup,"OLDER");Writer closing;h=closing.begin(tab5FS,image,"w");assert(h>0&&closing.chunk(h,"NEW",3)==3);closeFault=true;assert(closing.finish(tab5FS,h,true)<0&&get(image)=="ORIGINAL"&&get(backup)=="OLDER");
 for(const char* unavailable:{image,backup}){reset();put(image,"ORIGINAL");put(backup,"OLDER");Writer writer;int handle=writer.begin(tab5FS,image,"w");assert(handle>0&&writer.chunk(handle,"NEW",3)==3);statErrorPath=unavailable;assert(writer.finish(tab5FS,handle,true)<0&&get(image)=="ORIGINAL"&&get(backup)=="OLDER");}
 for(int failure=0;failure<35;++failure){reset();put(image,"ORIGINAL");Writer writer;failOperation=failure;int handle=writer.begin(tab5FS,image,"w");if(handle>0){writer.chunk(handle,"NEW",3);int result=writer.finish(tab5FS,handle,true);if(result==0)assert(get(image)=="NEW");else assert(get(image)=="ORIGINAL"||get(backup)=="ORIGINAL");}else assert(get(image)=="ORIGINAL"||get(backup)=="ORIGINAL");}
 assert(allocations.empty());std::puts("PASS: real JPEG encoding + marker/length readback, 30 I/O fault points + allocation/probe/flush/close failures; ImageWrite strict probes, errno-only flush/close rejection and 35 transaction fault points preserve original/backup");
}
