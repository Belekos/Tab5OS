#include "Tab5WebPPort.h"
#include "Tab5ImageCodec.h"
#include "Tab5Cooperate.h"
#include "src/tab5webp/src/webp/decode.h"
#include "src/tab5webp/src/webp/demux.h"
#include "src/tab5webp/tab5_port.h"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#ifdef ARDUINO_ARCH_ESP32
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
namespace {
constexpr size_t Budget=12U*1024U*1024U,MaxEncoded=2U*1024U*1024U,MaxSourcePixels=4U*1024U*1024U,MaxOutput=1024U*1024U;
std::atomic_flag busy=ATOMIC_FLAG_INIT;
struct Session {Tab5ImageDecode::Callbacks cb;Tab5TimeSlice slice;size_t used=0,peak=0,allocations=0;bool stopped=false,oom=false;const IEDoc* document=nullptr;Tab5ImageCodec::Options options{};};
Session* active=nullptr;
#ifdef TAB5_WEBP_TEST
size_t failAt=0,lastAllocations=0,lastLive=0,lastPeak=0;
#endif
union Header {struct {size_t bytes;} data;std::max_align_t align;};
void* rawAllocate(size_t n){
 if(active&&active->document)return active->document->alloc(active->document->memory_context,n);
#ifdef ARDUINO_ARCH_ESP32
 return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
 return std::malloc(n);
#endif
}
void rawFree(void* p){
 if(active&&active->document){active->document->release(active->document->memory_context,p);return;}
#ifdef ARDUINO_ARCH_ESP32
 heap_caps_free(p);
#else
 std::free(p);
#endif
}
bool reserve(size_t n){if(!active)return false;++active->allocations;
#ifdef TAB5_WEBP_TEST
 if(failAt&&active->allocations==failAt){active->oom=true;return false;}
#endif
 if(n>Budget-active->used){active->oom=true;return false;}active->used+=n;active->peak=std::max(active->peak,active->used);return true;}
struct Bytes {void* p;explicit Bytes(size_t n):p(Tab5WebPMalloc(n)){}~Bytes(){Tab5WebPFree(p);}};
struct Demux {WebPDemuxer* p=nullptr;~Demux(){WebPDemuxDelete(p);}};
struct Iterator {WebPIterator v{};~Iterator(){WebPDemuxReleaseIterator(&v);}};
struct Gate {Session& s;Gate(Session& value):s(value){active=&s;}~Gate(){
#ifdef TAB5_WEBP_TEST
 lastAllocations=s.allocations;lastLive=s.used;lastPeak=s.peak;
#endif
 active=nullptr;busy.clear(std::memory_order_release);}};
bool validDimensions(unsigned w,unsigned h){return w&&h&&w<=4096&&h<=4096&&size_t(w)<=MaxSourcePixels/h;}
uint16_t pixel(const uint8_t* p){const unsigned a=p[3],z=255-a;unsigned r=(p[0]*a+255*z+127)/255,g=(p[1]*a+255*z+127)/255,b=(p[2]*a+255*z+127)/255;return uint16_t((r&248)<<8|(g&252)<<3|b>>3);}
}
extern "C" int Tab5WebPCheckpoint(void){
 if(!active)return 0;if(active->stopped)return 0;
 if(active->document){if(active->document->yield)active->document->yield(active->document->yield_context);if(active->options.cancelled&&active->options.cancelled(active->options.context))active->stopped=true;}
 else if(active->cb.cooperate)active->cb.cooperate();
#ifdef ARDUINO_ARCH_ESP32
 else if(active->slice.due(millis()))vTaskDelay(1);
#endif
 if(active->cb.cancelled&&active->cb.cancelled())active->stopped=true;return !active->stopped;
}
extern "C" void* Tab5WebPMalloc(size_t n){if(!n)n=1;if(n>SIZE_MAX-sizeof(Header)||!reserve(n+sizeof(Header)))return nullptr;
 auto* h=static_cast<Header*>(rawAllocate(n+sizeof(Header)));if(!h){active->used-=n+sizeof(Header);active->oom=true;return nullptr;}h->data.bytes=n+sizeof(Header);return h+1;}
extern "C" void* Tab5WebPCalloc(size_t n,size_t size){if(size&&n>SIZE_MAX/size)return nullptr;auto* p=Tab5WebPMalloc(n*size);if(p)std::memset(p,0,n*size);return p;}
extern "C" void Tab5WebPFree(void* p){if(!p)return;auto* h=static_cast<Header*>(p)-1;if(active)active->used-=h->data.bytes;rawFree(h);}
namespace Tab5WebP {
#ifdef TAB5_WEBP_TEST
void failAllocationAt(size_t n){failAt=n;}size_t allocationCount(){return lastAllocations;}size_t liveBytes(){return lastLive;}size_t peakBytes(){return lastPeak;}
#endif
bool load(const Tab5ImageDecode::Source& source,Tab5ImageDecode::Image& image,const char*& error,Tab5ImageDecode::Callbacks cb,size_t maxPixels){
 image.clear();error=nullptr;
 if(!source.read||!source.seek||source.size<20||source.size>MaxEncoded||!maxPixels){error="WebP trop volumineux ou incomplet";return false;}
 if(busy.test_and_set(std::memory_order_acquire)){error="Décodeur WebP occupé";return false;}
 Session session{cb};Gate gate(session);bool ok=false;size_t outputBytes=0;
 {
 Bytes encoded(source.size);Demux demux;Iterator frame;
 auto decode=[&]()->bool{
  if(!encoded.p){error="PSRAM insuffisante pour WebP";return false;}
  if(!source.seek(source.context,0)){error="Lecture WebP impossible";return false;}
  auto* data=static_cast<uint8_t*>(encoded.p);
  for(size_t at=0;at<source.size;){if(!Tab5WebPCheckpoint())return false;size_t n=std::min<size_t>(16384,source.size-at),got=source.read(source.context,data+at,n);if(!got||got>n){error="WebP incomplet";return false;}at+=got;}
  if(std::memcmp(data,"RIFF",4)||std::memcmp(data+8,"WEBP",4)){error="En-tête WebP invalide";return false;}
  uint32_t riff=uint32_t(data[4])|uint32_t(data[5])<<8|uint32_t(data[6])<<16|uint32_t(data[7])<<24;
  if(riff<12||uint64_t(riff)+8!=source.size){error="Conteneur WebP incomplet ou invalide";return false;}
  WebPData input{data,source.size};demux.p=WebPDemux(&input);
  if(!demux.p||!WebPDemuxGetFrame(demux.p,1,&frame.v)||!frame.v.complete){error="Image WebP incomplète";return false;}
  unsigned sw=WebPDemuxGetI(demux.p,WEBP_FF_CANVAS_WIDTH),sh=WebPDemuxGetI(demux.p,WEBP_FF_CANVAS_HEIGHT);
  if(!validDimensions(sw,sh)||!validDimensions(frame.v.width,frame.v.height)||unsigned(frame.v.width)>sw||unsigned(frame.v.height)>sh||frame.v.x_offset<0||frame.v.y_offset<0||unsigned(frame.v.x_offset)>sw-unsigned(frame.v.width)||unsigned(frame.v.y_offset)>sh-unsigned(frame.v.height)){error="Dimensions WebP hors limites";return false;}
  const size_t limit=std::min(maxPixels,MaxOutput);unsigned shift=0,w=sw,h=sh;
  while(size_t(w)>limit/h){++shift;w=(sw+(1U<<shift)-1)>>shift;h=(sh+(1U<<shift)-1)>>shift;}
  unsigned scale=1U<<shift,x=unsigned(frame.v.x_offset)>>shift,y=unsigned(frame.v.y_offset)>>shift;
  unsigned fw=(unsigned(frame.v.x_offset+frame.v.width)+scale-1)/scale-x,fh=(unsigned(frame.v.y_offset+frame.v.height)+scale-1)/scale-y;
  if(!fw||!fh||x+fw>w||y+fh>h){error="Cadre WebP invalide";return false;}
  outputBytes=size_t(w)*h*2;if(!reserve(outputBytes)){outputBytes=0;return false;}
  image.pixels=static_cast<uint16_t*>(rawAllocate(outputBytes));if(!image.pixels){session.used-=outputBytes;outputBytes=0;session.oom=true;return false;}
  image.width=w;image.height=h;image.sourceWidth=sw;image.sourceHeight=sh;
  if(cb.allocated)cb.allocated(cb.allocationContext,outputBytes);
  std::memset(image.pixels,255,outputBytes);
  Bytes rgba(size_t(fw)*fh*4);if(!rgba.p)return false;
  WebPDecoderConfig config{};if(!WebPInitDecoderConfig(&config)){error="Décodeur WebP indisponible";return false;}
  config.options.use_threads=0;config.options.use_scaling=fw!=unsigned(frame.v.width)||fh!=unsigned(frame.v.height);config.options.scaled_width=fw;config.options.scaled_height=fh;
  config.output.colorspace=MODE_RGBA;config.output.is_external_memory=1;config.output.u.RGBA.rgba=static_cast<uint8_t*>(rgba.p);config.output.u.RGBA.stride=fw*4;config.output.u.RGBA.size=size_t(fw)*fh*4;
  VP8StatusCode status=WebPDecode(frame.v.fragment.bytes,frame.v.fragment.size,&config);WebPFreeDecBuffer(&config.output);
  if(status!=VP8_STATUS_OK){error=status==VP8_STATUS_OUT_OF_MEMORY?"PSRAM insuffisante pour WebP":"Décodage WebP interrompu ou invalide";return false;}
  for(unsigned row=0;row<fh;++row){if(!Tab5WebPCheckpoint())return false;auto* out=image.pixels+size_t(row+y)*w+x;const auto* in=static_cast<uint8_t*>(rgba.p)+size_t(row)*fw*4;for(unsigned col=0;col<fw;++col)out[col]=pixel(in+col*4);if(cb.progress)cb.progress(unsigned(uint64_t(row+1)*99/fh));}
  return true;
 };
 ok=decode();
 }
 if(!ok){image.clear();if(session.stopped)error="Préparation annulée";else if(session.oom)error="PSRAM insuffisante pour WebP";else if(!error)error="WebP invalide";}
 // Output ownership passes to Image; decoder workspace has all been released.
 if(outputBytes)session.used-=outputBytes;
 if(ok&&cb.progress)cb.progress(100);return ok;
}
}


namespace Tab5ImageCodec {
int loadWebp(IEDoc* doc, IERead reader, void* input, Options options) {
 if(options.imported)*options.imported=ImportInfo{};
 auto fail=[&](const char* error){if(doc)doc->error=error;return 0;};
 if(!doc||!doc->alloc||!doc->release||!reader)return fail("Lecture WebP impossible");
 if(busy.test_and_set(std::memory_order_acquire))return fail("Décodeur WebP occupé");
 Session session{};session.document=doc;session.options=options;Gate gate(session);
 IEDoc temporary{};const char* error="WebP incomplet ou invalide; image conservee.";
 auto take=[&](uint8_t* out,size_t count)->bool{
  while(count){if(!Tab5WebPCheckpoint())return false;const size_t want=std::min<size_t>(16384,count),got=reader(input,out,want);if(!got||got>want)return false;out+=got;count-=got;}return true;
 };
 auto decode=[&]()->bool{
  uint8_t header[12];if(!take(header,sizeof(header))||std::memcmp(header,"RIFF",4)||std::memcmp(header+8,"WEBP",4))return false;
  const uint32_t riff=uint32_t(header[4])|uint32_t(header[5])<<8|uint32_t(header[6])<<16|uint32_t(header[7])<<24;
  if(riff<12||uint64_t(riff)+8>MaxEncoded){error="WebP trop volumineux ou incomplet";return false;}
  const size_t length=size_t(riff)+8;Bytes encoded(length);Demux demux;Iterator frame;
  if(!encoded.p)return false;
  auto* data=static_cast<uint8_t*>(encoded.p);std::memcpy(data,header,sizeof(header));
  if(!take(data+sizeof(header),length-sizeof(header)))return false;
  uint8_t extra;if(reader(input,&extra,1))return false;
  WebPData source{data,length};demux.p=WebPDemux(&source);
  if(!demux.p||!WebPDemuxGetFrame(demux.p,1,&frame.v)||!frame.v.complete)return false;
  const unsigned w=WebPDemuxGetI(demux.p,WEBP_FF_CANVAS_WIDTH),h=WebPDemuxGetI(demux.p,WEBP_FF_CANVAS_HEIGHT);
  if(!w||!h||w>IE_MAX_DIMENSION||h>IE_MAX_DIMENSION||size_t(w)>IE_DOCUMENT_MAX_BYTES/4/h){
    error="Dimensions/calques hors limite (8192, moins de 8 Mio de pixels).";return false;
  }
  if(frame.v.width<1||frame.v.height<1||unsigned(frame.v.width)>w||unsigned(frame.v.height)>h||
     frame.v.x_offset<0||frame.v.y_offset<0||unsigned(frame.v.x_offset)>w-unsigned(frame.v.width)||unsigned(frame.v.y_offset)>h-unsigned(frame.v.height))return false;
  if(!ie_init(&temporary,doc->alloc,doc->release,doc->memory_context,int(w),int(h))){session.oom=true;return false;}
  ie_set_yield(&temporary,doc->yield,doc->yield_context);
  auto* rgba=reinterpret_cast<uint8_t*>(temporary.current.layers[0].pixels);
  // The editable canvas uses straight alpha and original dimensions. No RGB565
  // intermediate, decoder scaling or extra full-frame buffer is involved.
  std::memset(rgba,0,size_t(w)*h*4);
  WebPDecoderConfig config{};if(!WebPInitDecoderConfig(&config))return false;
  config.options.use_threads=0;config.options.use_scaling=0;
  config.output.colorspace=MODE_RGBA;config.output.is_external_memory=1;
  config.output.u.RGBA.rgba=rgba+(size_t(frame.v.y_offset)*w+frame.v.x_offset)*4;
  config.output.u.RGBA.stride=w*4;
  config.output.u.RGBA.size=(size_t(frame.v.height-1)*w+frame.v.width)*4;
  const VP8StatusCode status=WebPDecode(frame.v.fragment.bytes,frame.v.fragment.size,&config);WebPFreeDecBuffer(&config.output);
  if(status==VP8_STATUS_OUT_OF_MEMORY)session.oom=true;
  return status==VP8_STATUS_OK&&Tab5WebPCheckpoint();
 };
 if(!decode()){
  ie_destroy(&temporary);
  return fail(session.stopped?"Import WebP annule; image conservee.":session.oom?"Memoire insuffisante; image conservee.":error);
 }
 std::strcpy(temporary.current.layers[0].name,"Image WebP");temporary.dirty=0;temporary.error=nullptr;
 if(!ie_clipboard_transfer(&temporary,doc)){
  ie_destroy(&temporary);doc->error="Import impossible; image conservee.";return 0;
 }
 ie_destroy(doc);*doc=temporary;
 if(options.imported)*options.imported={unsigned(doc->current.width),unsigned(doc->current.height),unsigned(doc->current.width),unsigned(doc->current.height),1};
 return 1;
}
}
