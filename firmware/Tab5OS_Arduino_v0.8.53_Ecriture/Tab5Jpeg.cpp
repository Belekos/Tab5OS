#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5AtomicFile.h"
#include "Tab5Task.h"
#include "Tab5Jpeg.h"
#include <FS.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <new>
#include <atomic>
#include "src/jpeg/jpge.h"
namespace {
std::atomic<bool> jpegCancelled{false};
std::atomic<unsigned> jpegProgress{0};
class JpegFile:public jpge::output_stream {
public: File& file;size_t bytes=0;explicit JpegFile(File& f):file(f){}
 bool put_buf(const void* p,int n)override{if(n<0)return false;size_t done=file.write((const uint8_t*)p,n);bytes+=done;return done==size_t(n);}
};
}
static bool encode(const uint16_t* pixels, File* raw, unsigned width, unsigned height,
                  const String& path, String& error) {
 bool present=false;
 if((!pixels&&!raw)||!width||!height||width>4096||height>4096||!tab5AtomicProbe(path.c_str(),present)||present){error="Image ou destination invalide";return false;}
 String temporary=path+".tmp";
 if(!tab5AtomicProbe(temporary.c_str(),present)||present){error="Fichier temporaire déjà présent";return false;}
 // A few scanlines only: no full RGB canvas, no whole JPEG buffer, no JPEG DMA.
 auto* row=(uint8_t*)heap_caps_malloc(size_t(width)*5,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 void* object=heap_caps_malloc(sizeof(jpge::jpeg_encoder),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!row||!object){heap_caps_free(row);heap_caps_free(object);error="Mémoire insuffisante pour exporter cette image";return false;}
 auto* encoder=new(object)jpge::jpeg_encoder;
 errno=0;File output=tab5FS.open(temporary,FILE_WRITE);JpegFile stream(output);
 jpge::params params;params.m_quality=92;params.m_subsampling=jpge::H1V1;params.m_two_pass_flag=false;
 bool ok=output&&encoder->init(&stream,width,height,3,params);
 for(unsigned y=0;ok&&y<height;++y){
  if(jpegCancelled){ok=false;break;}
  const uint8_t* source=nullptr;
  if(raw){ok=raw->read(row+width*3,width*2)==width*2;source=row+width*3;}
  for(unsigned x=0;ok&&x<width;++x){uint16_t c=raw?uint16_t(source[x*2])|(uint16_t(source[x*2+1])<<8):pixels[size_t(y)*width+x];
   unsigned r=(c>>11)&31,g=(c>>5)&63,b=c&31;
   row[x*3]=(r<<3)|(r>>2);row[x*3+1]=(g<<2)|(g>>4);row[x*3+2]=(b<<3)|(b>>2);
  }
  if(ok)ok=encoder->process_scanline(row);
  jpegProgress=(y+1)*99/height;
  delay(1); // Give audio, network and the idle watchdog time after each scanline.
 }
 if(ok)ok=!jpegCancelled&&encoder->process_scanline(nullptr);
 encoder->~jpeg_encoder();heap_caps_free(object);heap_caps_free(row);
 if(output){errno=0;output.flush();const bool synced=!errno;ok=ok&&synced&&!output.getWriteError();errno=0;output.close();ok=ok&&!errno;}
 if(ok){File check=tab5FS.open(temporary);uint8_t a[2],b[2];ok=check&&check.size()==stream.bytes&&stream.bytes>4&&check.read(a,2)==2&&check.seek(stream.bytes-2)&&check.read(b,2)==2&&a[0]==255&&a[1]==216&&b[0]==255&&b[1]==217;check.close();}
 if(ok)ok=!jpegCancelled&&tab5AtomicProbe(path.c_str(),present)&&!present&&tab5FS.rename(temporary,path);
 if(!ok){if(!jpegCancelled)tab5StorageFault("export JPEG",temporary.c_str());tab5FS.remove(temporary);error=jpegCancelled?"Export JPEG annulé":tab5StorageErrorText();}
 else jpegProgress=100;
 return ok;
}

bool tab5SaveJpeg(const uint16_t* pixels,unsigned width,unsigned height,const String& path,String& error){jpegCancelled=false;jpegProgress=0;return encode(pixels,nullptr,width,height,path,error);}
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace {
std::atomic<bool> encoding{false};String rawPath,jpegPath,jpegError,jpegResult;
void runJpeg(){
 Tab5StorageUse storage{true};if(!storage){jpegError="Reconnexion SD en cours — réessayez";return;}
 File in=tab5FS.open(rawPath);uint8_t h[16];bool ok=in&&in.read(h,16)==16&&!memcmp(h,"T5RGB565",8);
 auto get=[](const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;};
 if(ok){unsigned w=get(h+8),height=get(h+12);ok=w&&height&&w<=4096&&height<=4096&&in.size()==16+size_t(w)*height*2;
  if(ok)ok=encode(nullptr,&in,w,height,jpegPath,jpegError);
 }if(in)in.close();tab5FS.remove(rawPath);
 if(ok)jpegResult=jpegPath;else if(!jpegError.length())jpegError="Image source invalide";
}
void jpegTask(void*){runJpeg();encoding=false;tab5TaskCheckpoint("jpeg");vTaskDelete(nullptr);}
}
namespace Tab5Jpeg {
bool start(const String& raw,const String& destination){if(encoding)return false;rawPath=raw;jpegPath=destination;jpegError="";jpegResult="";jpegCancelled=false;jpegProgress=0;encoding=true;
 if(tab5CreateTask(jpegTask,"tab5_jpeg",16384,nullptr,1,nullptr,0)!=pdPASS){encoding=false;jpegError="Mémoire insuffisante";return false;}return true;}
bool busy(){return encoding.load();}String result(){return encoding?String(""):jpegResult;}String error(){return encoding?String(""):jpegError;}
void cancel(){jpegCancelled=true;}
unsigned progress(){return jpegProgress.load();}
}
