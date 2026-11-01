#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5TileRaster.h"
#include "Tab5MapPrint.h"
#include "Tab5Memory.h"
#include "Tab5MapPrintPlan.h"
#include "Tab5Map.h"
#include <M5Unified.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cmath>
namespace {
tab5mapprint::Plan plan;M5Canvas strip(&M5.Display);File output;String destination,attribution;
bool active=false,ready=false,waiting=false,tileFailed=false,badCache=false;
const uint16_t* capturePixels=nullptr;
struct FallbackTile {int64_t x,y;};Tab5PsramTable<FallbackTile,256> fallbackTiles;unsigned fallbackCount=0;int lastHttp=0;uint32_t gen=0,waitSince=0;unsigned style=0,row=0,rows=0;int64_t tx=0,ty=0,lastY=0;
Tab5TileValidation::Result drawResult=Tab5TileValidation::Result::Valid;
const char* message="Prêt";
void finish(const char* error){if(error&&strcmp(error,"Écriture de la carte impossible")==0)tab5StorageFault("capture carte",destination.c_str());if(output)output.close();strip.deleteSprite();fallbackTiles.clear();capturePixels=nullptr;active=false;waiting=false;message=error;if(destination.length())tab5FS.remove(destination);destination="";ready=false;}
bool knownFallback(){for(unsigned i=0;i<fallbackCount;++i)if(fallbackTiles[i].x==tx&&fallbackTiles[i].y==ty)return true;return false;}
void band(){rows=std::min(128u,plan.height+38-row);strip.fillScreen(TFT_WHITE);tx=plan.firstX;ty=std::max(plan.firstY,int64_t(std::floor((plan.top+row/plan.scale)/256)));lastY=std::min(plan.lastY,int64_t(std::floor((plan.top+std::min(row+rows,plan.height)/plan.scale-1)/256)));}
void put32(uint8_t* p,uint32_t n){for(int i=0;i<4;++i)p[i]=n>>(i*8);}
}
bool startPlan(const tab5mapprint::Plan& selected,uint32_t generation,const String& credit){
 if(active)return false;if(ready)Tab5MapPrint::cancel();capturePixels=nullptr;plan=selected;if(!plan.width){message="Dimensions d’impression invalides";return false;}
 strip.setPsram(true);strip.setColorDepth(16);strip.setSwapBytes(false);if(!strip.createSprite(plan.width,128)){message="Mémoire insuffisante";return false;}
 tab5FS.mkdir("/System");destination=String("/System/print-map-")+millis()+".t5r";
 if(tab5FS.exists(destination)){strip.deleteSprite();fallbackTiles.clear();message="Fichier temporaire déjà présent";return false;}
 output=tab5FS.open(destination,FILE_WRITE);uint8_t header[16];memcpy(header,"T5RGB565",8);put32(header+8,plan.width);put32(header+12,plan.height+38);
 if(!output||output.write(header,16)!=16){finish("Écriture de la carte impossible");return false;}
 gen=generation;style=Tab5Map.selectedStyle();attribution=credit;attribution.replace("©","(c)");row=0;fallbackCount=0;lastHttp=0;tileFailed=badCache=false;ready=false;active=true;waiting=false;message="Préparation de la carte pour le papier…";band();return true;
}
namespace Tab5MapPrint {
bool beginCapture(const uint16_t* pixels,unsigned width,unsigned height,unsigned zoom,uint32_t generation,const String& credit){
 if(!pixels||!width||!height||width>2048||height>2048)return false;
 tab5mapprint::Plan capture;capture.width=width;capture.height=height;capture.zoom=zoom;capture.scale=1;
 if(!startPlan(capture,generation,credit))return false;
 capturePixels=pixels;message="Capture de la carte affichée…";return true;
}
bool begin(double x,double y,unsigned viewZoom,unsigned maxZoom,uint32_t generation,unsigned paper,unsigned dpi,bool landscape,const String& credit,unsigned percent){
 double w=paper==1?215.9:paper==2?148:210,h=paper==1?279.4:paper==2?210:297;if(landscape)std::swap(w,h);
 if(percent<25||percent>200)return false;w=16+(w-16)*percent/100.0;h=16+(h-16)*percent/100.0;
 return startPlan(tab5mapprint::plan(x,y,viewZoom,maxZoom,w,h,dpi),generation,credit);
}
bool beginJpeg(double x,double y,unsigned viewZoom,unsigned maxZoom,uint32_t generation,const String& credit){
 bool ok=startPlan(tab5mapprint::jpegPlan(x,y,viewZoom,maxZoom),generation,credit);if(ok)message="Préparation du JPEG haute définition…";return ok;
}
void tileResult(const Tab5MapTileResult& r){
 if(!active||!waiting||r.generation!=gen||r.style!=style||r.zoom!=plan.zoom)return;
 uint32_t world=1u<<plan.zoom,xx=uint32_t((tx%int64_t(world)+world)%world);
 if(r.x!=xx||r.y!=ty)return;
 lastHttp=r.httpCode;tileFailed=!r.success;if(r.success)badCache=false;
}
static bool drawTile(const uint8_t* data,size_t n,int64_t rawX,int64_t rawY,unsigned lower){
 drawResult=Tab5TileValidation::Result::Valid;
 double factor=double(1u<<lower);
 int x=int(std::lround((rawX*256*factor-plan.left)*plan.scale));
 int y=int(std::lround((rawY*256*factor-plan.top)*plan.scale))-int(row);
 int cellX=int(std::lround((tx*256-plan.left)*plan.scale));
 int cellY=int(std::lround((ty*256-plan.top)*plan.scale))-int(row);
 int edgeX=int(std::lround(((tx+1)*256-plan.left)*plan.scale));
 int edgeY=int(std::lround(((ty+1)*256-plan.top)*plan.scale))-int(row);
 int left=std::max(0,cellX),top=std::max(0,cellY),right=std::min(int(plan.width),edgeX),bottom=std::min(int(rows),edgeY);
 if(right<=left||bottom<=top)return true;
 return Tab5TileRaster::draw(static_cast<uint16_t*>(strip.getBuffer()),plan.width,128,
     data,n,x,y,std::max(1,int(std::lround(256*plan.scale*factor))),left,top,right-left,bottom-top,&drawResult);
}
static bool fallback(){
 const int64_t world=1LL<<plan.zoom;
 for(unsigned lower=1;lower<=3&&lower<=plan.zoom;++lower){
  const int64_t parentX=int64_t(std::floor(double(tx)/(1u<<lower))),parentY=ty>>lower;
  const int64_t parentWorld=world>>lower;uint8_t* data=nullptr;size_t n=0;
  uint32_t x=uint32_t((parentX%parentWorld+parentWorld)%parentWorld);
  if(!Tab5Map.readCachedTile(plan.zoom-lower,x,parentY,data,n))continue;
  bool ok=drawTile(data,n,parentX,parentY,lower);if(!ok)Tab5Map.rejectCachedTile(style,plan.zoom-lower,x,parentY,data,n);heap_caps_free(data);
  if(drawResult==Tab5TileValidation::Result::NoMemory||drawResult==Tab5TileValidation::Result::Unavailable)return false;
  if(ok){if(!knownFallback()&&fallbackCount<256&&fallbackTiles.ensure(fallbackCount+1))fallbackTiles[fallbackCount++]={tx,ty};return true;}
 }return false;
}
void service(uint32_t generation,bool mapActive){
 if(!active)return;if(!mapActive||generation!=gen||style!=Tab5Map.selectedStyle()){finish("Préparation de la carte annulée");return;}
 if(capturePixels){
  auto* data=static_cast<uint16_t*>(strip.getBuffer());
  if(row<plan.height){
   const unsigned n=std::min(32u,plan.height-row);
   for(size_t i=0;i<size_t(n)*plan.width;++i){uint16_t pixel=capturePixels[size_t(row)*plan.width+i];data[i]=uint16_t(pixel>>8|pixel<<8);}
   size_t bytes=size_t(n)*plan.width*2;
   if(output.write(reinterpret_cast<uint8_t*>(data),bytes)!=bytes){finish("Écriture de la carte impossible");return;}
   row+=n;return;
  }
  strip.fillScreen(TFT_BLACK);strip.setTextFont(2);strip.setTextColor(TFT_WHITE,TFT_BLACK);
  strip.setCursor(6,2);strip.printf("z%u | %u x %u px",plan.zoom,plan.width,plan.height);
  strip.setCursor(6,20);strip.print(attribution);
  for(size_t i=0;i<size_t(38)*plan.width;++i)data[i]=uint16_t(data[i]>>8|data[i]<<8);
  size_t bytes=size_t(38)*plan.width*2;
  if(output.write(reinterpret_cast<uint8_t*>(data),bytes)!=bytes){finish("Écriture de la carte impossible");return;}
  row+=38;output.flush();output.close();strip.deleteSprite();fallbackTiles.clear();capturePixels=nullptr;active=false;ready=true;message="Carte prête";return;
 }
 if(ty<=lastY&&row<plan.height){
  const uint32_t world=1u<<plan.zoom;uint32_t xx=uint32_t((tx%int64_t(world)+world)%world);uint8_t* bytes=nullptr;size_t size=0;
  bool done=false;drawResult=Tab5TileValidation::Result::Valid;
  if(knownFallback())done=fallback();
  else if(!badCache&&Tab5Map.readCachedTile(plan.zoom,xx,ty,bytes,size)){
   done=drawTile(bytes,size,tx,ty,0);if(!done)Tab5Map.rejectCachedTile(style,plan.zoom,xx,ty,bytes,size);heap_caps_free(bytes);
   if(!done)badCache=true;
  }
  if(!done){
   if(drawResult==Tab5TileValidation::Result::NoMemory||drawResult==Tab5TileValidation::Result::Unavailable){finish("Mémoire ou décodeur temporairement indisponible");return;}
   if(tileFailed||(waiting&&millis()-waitSince>120000)){
    if(!fallback()){finish("Tuile indisponible après plusieurs tentatives");return;}
   }else {
    if(!waiting){
     // A full queue is temporary, never an immediate export failure.
     if(!waitSince)waitSince=millis();
     if(Tab5Map.requestTile(plan.zoom,xx,ty,gen)){waiting=true;waitSince=millis();}
     else if(millis()-waitSince>10000)finish("Chargement de tuile impossible");
    }
    return;
   }
  }
  waiting=tileFailed=badCache=false;waitSince=0;
  if(++tx>plan.lastX){tx=plan.firstX;++ty;}return;
 }
 if(row+rows>plan.height){int start=int(plan.height)-int(row);strip.fillRect(0,start,plan.width,38,TFT_BLACK);strip.setTextFont(2);strip.setTextColor(TFT_WHITE,TFT_BLACK);strip.setCursor(6,start+2);strip.printf("z%u | %u x %u px",plan.zoom,plan.width,plan.height);strip.setCursor(6,start+20);strip.print(attribution);}
 // M5GFX 16-bit sprites store swap565_t; convert to little-endian RGB565.
 auto* line=static_cast<uint16_t*>(strip.getBuffer());
 for(size_t i=0;i<size_t(rows)*plan.width;++i)line[i]=uint16_t((line[i]>>8)|(line[i]<<8));
 if(output.write(reinterpret_cast<uint8_t*>(line),size_t(plan.width)*rows*2)!=size_t(plan.width)*rows*2){finish("Écriture de la carte impossible");return;}
 row+=rows;if(row>=plan.height+38){output.flush();output.close();strip.deleteSprite();fallbackTiles.clear();active=false;ready=true;message=fallbackCount?"Carte prête : certaines tuiles sont moins détaillées":"Carte prête pour l’impression";}else band();
}
bool busy(){return active;}const char* status(){return message;}
String detail(){String text=String(plan.width)+" × "+plan.height+" px · "+(plan.height?row*100/(plan.height+38):0)+" %";
 if(active&&waiting)text+=" · z"+String(plan.zoom)+"/"+String((long)tx)+"/"+String((long)ty);
 if(fallbackCount)text+=" · ~"+String(fallbackCount);
 if(lastHttp&&lastHttp!=200)text+=" · HTTP "+String(lastHttp);return text;}
unsigned fallbackTileCount(){return fallbackCount;}
unsigned progress(){return plan.height?std::min(100u,row*100/(plan.height+38)):0;}
String takePath(){if(!ready)return "";String result=destination;destination="";ready=false;return result;}
void cancel(){finish("Préparation de la carte annulée");}
}
