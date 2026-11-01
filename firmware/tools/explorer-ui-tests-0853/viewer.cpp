#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include "tools/native-apps/image-editor/image_core.h"
#include "Tab5ImageDecode.h"
#include "Tab5ImageFormats.h"
using String=std::string;using std::max;
static unsigned checks=0;void check(bool b){++checks;if(!b){fprintf(stderr,"viewer check %u failed\n",checks);abort();}}
const char* T(const char* s){return s;}
uint32_t millis(){static uint32_t now=0;return ++now;}void delay(unsigned){}
constexpr size_t MAX_IMAGE_CANVAS_BYTES=8U*1024U*1024U;
constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
static size_t heapCalls=0,heapFail=0,liveBytes=0,peakBytes=0;
static std::unordered_map<void*,size_t> allocations;
void* heap_caps_malloc(size_t bytes,unsigned){if(++heapCalls==heapFail)return nullptr;void* p=malloc(bytes);if(p){allocations[p]=bytes;liveBytes+=bytes;peakBytes=std::max(liveBytes,peakBytes);}return p;}
void heap_caps_free(void* p){if(!p)return;auto it=allocations.find(p);check(it!=allocations.end());liveBytes-=it->second;allocations.erase(it);free(p);}
static unsigned reservationCalls=0,reservationFail=0,reservationLive=0;
namespace Tab5Resources{enum class Owner{Files};struct Budget{size_t a,b,c,psram,block;};struct Reservation{bool valid;explicit Reservation(bool v):valid(v){if(v)++reservationLive;}Reservation(const Reservation&)=delete;~Reservation(){release();}explicit operator bool()const{return valid;}void allocated(size_t,size_t){}void release(){if(valid){--reservationLive;valid=false;}}};}
Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner,const Tab5Resources::Budget&,const char*){return Tab5Resources::Reservation(++reservationCalls!=reservationFail);}
static unsigned fileCloses=0;
struct File{std::vector<uint8_t> data;size_t offset=0;size_t read(uint8_t* out,size_t bytes){size_t got=std::min(bytes,data.size()-offset);memcpy(out,data.data()+offset,got);offset+=got;return got;}bool seek(size_t pos){if(pos>data.size())return false;offset=pos;return true;}size_t size()const{return data.size();}explicit operator bool()const{return !data.empty();}bool isDirectory()const{return false;}void close(){++fileCloses;}};
static std::vector<uint8_t> sourceBytes;
constexpr int FILE_READ=0;
struct {File open(const char*,int){return File{sourceBytes};}}tab5FS;
struct {int Display=0;}M5;
struct M5Canvas{void* buffer=nullptr;M5Canvas(int*){}~M5Canvas(){deleteSprite();}void setPsram(bool){}void setColorDepth(int){}void* createSprite(int w,int h){buffer=heap_caps_malloc(size_t(w)*h*2,3);return buffer;}void* getBuffer(){return buffer;}void deleteSprite(){heap_caps_free(buffer);buffer=nullptr;}};
struct lv_image_dsc_t{struct{unsigned magic=0,cf=0,w=0,h=0,stride=0;}header;size_t data_size=0;const uint8_t* data=nullptr;};
constexpr int LV_IMAGE_HEADER_MAGIC=0x19,LV_COLOR_FORMAT_RGB565=1;
namespace Tab5ImageDecode{Image::~Image(){clear();}void Image::clear(){heap_caps_free(pixels);pixels=nullptr;width=height=sourceWidth=sourceHeight=0;}}
unsigned webpCalls=0;
namespace Tab5WebP{bool load(const Tab5ImageDecode::Source& source,Tab5ImageDecode::Image& image,const char*& error,Tab5ImageDecode::Callbacks callbacks,size_t limit){
 ++webpCalls;check(source.size==24);check(limit==1024U*1024U);image.width=256;image.height=64;image.sourceWidth=2048;image.sourceHeight=512;
 image.pixels=static_cast<uint16_t*>(heap_caps_malloc(256*64*2,3));if(!image.pixels){error="injected WebP failure";return false;}
 for(size_t i=0;i<256*64;++i)image.pixels[i]=0x001f;
 if(callbacks.allocated)callbacks.allocated(callbacks.allocationContext,256*64*2);
 return true;
}}
#include "Tab5ExplorerImages.inc"
#include "viewer-dispatch.inc"
size_t save(void* p,const void* data,size_t n){auto& v=*static_cast<std::vector<uint8_t>*>(p);const auto* s=static_cast<const uint8_t*>(data);v.insert(v.end(),s,s+n);return n;}
std::vector<uint8_t> fixture(){IEDoc source{};check(ie_init(&source,[](void*,size_t n)->void*{return malloc(n);},[](void*,void* p){free(p);},nullptr,32,20));
 for(int i=0;i<32*20;++i)source.current.layers[0].pixels[i]=IE_RGBA(200,10,30,255);
 check(ie_layer_add(&source,"top"));source.current.layers[1].pixels[5*32+5]=IE_RGBA(0,255,0,255);
 std::vector<uint8_t> bytes;check(ie_save_project(&source,save,&bytes));ie_destroy(&source);return bytes;}
void reset(){check(liveBytes==0);check(reservationLive==0);heapCalls=0;heapFail=0;reservationCalls=0;reservationFail=0;}
int main(){const auto data=fixture();sourceBytes=data;File file{data};M5Canvas* canvas=nullptr;lv_image_dsc_t dsc{};int w=0,h=0;String error;
 check(loadSdImageCanvas("/Pictures/drawing.t5i",4096,4096,canvas,dsc,w,h,error));check(fileCloses==1);check(w==32&&h==20);check(dsc.header.w==32&&dsc.header.h==20);check(dsc.header.cf==LV_COLOR_FORMAT_RGB565);check(dsc.header.stride==64);check(liveBytes==32*20*2);
 auto* pixels=reinterpret_cast<const uint16_t*>(dsc.data);check(pixels[5*32+5]==0x07e0);check(pixels[0]==uint16_t(((200&248)<<8)|((10&252)<<3)|(30>>3)));const size_t baseAllocations=heapCalls;const unsigned baseReservations=reservationCalls;
 delete canvas;canvas=nullptr;reset();file.offset=0;
 check(loadProjectImageCanvas(file,10,10,canvas,dsc,w,h,error));check(w==32&&h==20);check(dsc.header.w==10&&dsc.header.h==6);delete canvas;canvas=nullptr;reset();
 for(size_t failure=1;failure<=baseAllocations;++failure){heapFail=failure;file.offset=0;error.clear();check(!loadProjectImageCanvas(file,4096,4096,canvas,dsc,w,h,error));check(!canvas);check(!error.empty());reset();}
 for(unsigned failure=1;failure<=baseReservations;++failure){reservationFail=failure;file.offset=0;error.clear();check(!loadProjectImageCanvas(file,4096,4096,canvas,dsc,w,h,error));check(!canvas);check(!error.empty());reset();}
 file.data[28]^=1;file.offset=0;check(!loadProjectImageCanvas(file,4096,4096,canvas,dsc,w,h,error));check(!canvas);reset();file.data=data;
 for(unsigned i=0;i<100;++i){file.offset=0;check(loadProjectImageCanvas(file,64,64,canvas,dsc,w,h,error));delete canvas;canvas=nullptr;reset();}
 // Viewer WebP routes to its bounded decoder, not the editable document import.
 File webp{std::vector<uint8_t>(24)};check(loadWebpImageCanvas(webp,100,100,canvas,dsc,w,h,error));check(webpCalls==1);check(w==2048&&h==512);check(dsc.header.w==100&&dsc.header.h==25);check(liveBytes==100*25*2);check(reinterpret_cast<const uint16_t*>(dsc.data)[2499]==0x001f);delete canvas;canvas=nullptr;reset();
 for(size_t failure=1;failure<=2;++failure){heapFail=failure;check(!loadWebpImageCanvas(webp,100,100,canvas,dsc,w,h,error));check(!canvas);reset();}
 for(unsigned failure=1;failure<=2;++failure){reservationFail=failure;check(!loadWebpImageCanvas(webp,100,100,canvas,dsc,w,h,error));check(!canvas);reset();}
 printf("{\"checks\":%u,\"projectCycles\":100,\"allocationFailureScenarios\":%zu,\"reservationFailureScenarios\":%u,\"peakBytes\":%zu,\"liveBytes\":%zu,\"passed\":true}\n",checks,baseAllocations+2,baseReservations+2,peakBytes,liveBytes);
}
