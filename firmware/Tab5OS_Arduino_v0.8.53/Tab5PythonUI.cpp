#include "Tab5PythonUI.h"
#include <atomic>
#include <cerrno>
#include <cstring>
#include <new>
#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>
#else
#include <mutex>
#include <cstdlib>
#endif
namespace Tab5PythonUI {
namespace {
Widget* widgets=nullptr;char title[192]{},language[24]="en-US";
uint32_t generation=0;bool visible=false;
char imagePath[272]{};int imageStatus=0;bool imagePending=false;
ReaderRequest reader;int readerStatus=0;char readerTarget[768]{};
struct Event {uint32_t id,value;};Event events[16];unsigned first=0,count=0;
#ifdef ARDUINO
StaticSemaphore_t mutexStorage;
SemaphoreHandle_t mutex(){static auto handle=xSemaphoreCreateMutexStatic(&mutexStorage);return handle;}
struct Lock {Lock(){xSemaphoreTake(mutex(),portMAX_DELAY);}~Lock(){xSemaphoreGive(mutex());}};
void* allocateWidgets(){return heap_caps_calloc(MaxWidgets,sizeof(Widget),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
void freeWidgets(void* p){heap_caps_free(p);}
#else
std::mutex mutex;
struct Lock {Lock(){mutex.lock();}~Lock(){mutex.unlock();}};
void* allocateWidgets(){return std::calloc(MaxWidgets,sizeof(Widget));}
void freeWidgets(void* p){std::free(p);}
#endif
void copy(char* out,size_t cap,const void* data,size_t n){if(n>=cap)n=cap-1;if(n)memcpy(out,data,n);out[n]=0;}
}
void reset(){Lock lock;imagePending=false;imageStatus=0;readerStatus=0;visible=false;++generation;if(widgets)for(unsigned i=0;i<MaxWidgets;++i)widgets[i]=Widget{};first=count=0;}
void releaseStorage(){Lock lock;imagePending=false;imageStatus=0;readerStatus=0;visible=false;++generation;first=count=0;freeWidgets(widgets);widgets=nullptr;}
void close(){Lock lock;imagePending=false;imageStatus=0;readerStatus=0;visible=false;++generation;first=count=0;}
void setLanguage(const char* lang){if(!lang)return;Lock lock;copy(language,sizeof(language),lang,strlen(lang));}
bool imageRequest(char path[272]){Lock lock;if(!imagePending)return false;memcpy(path,imagePath,sizeof(imagePath));imagePending=false;return true;}
void imageFinished(int result){Lock lock;imageStatus=result;}
bool readerRequest(ReaderRequest& request){Lock lock;if(readerStatus!=1)return false;request=reader;return true;}
void readerFinished(uint32_t revision,int result,const char* target){Lock lock;if(reader.revision==revision){readerStatus=result;copy(readerTarget,sizeof(readerTarget),target?target:"",target?strlen(target):0);}}
bool opened(uint32_t& gen,char out[192]){Lock lock;gen=generation;memcpy(out,title,sizeof(title));return visible;}
bool read(unsigned id,Widget& w,uint32_t expectedGeneration){if(id<1||id>MaxWidgets)return false;Lock lock;if(!visible||!widgets||(expectedGeneration&&expectedGeneration!=generation))return false;w=widgets[id-1];return w.type!=0;}
bool consumeEvent(unsigned id){Lock lock;for(unsigned i=0;i<count;++i)if(events[(first+i)%16].id==id){for(unsigned j=i;j+1<count;++j)events[(first+j)%16]=events[(first+j+1)%16];--count;return true;}return false;}
void event(unsigned id,unsigned value,uint32_t expectedGeneration){Lock lock;if((expectedGeneration&&expectedGeneration!=generation)||!visible||!widgets||id<1||id>MaxWidgets)return;auto& w=widgets[id-1];if(w.type!=2&&w.type!=3&&w.type!=4&&w.type!=5&&w.type!=7)return;
 if(w.type==7&&value>=9)return;
 if(w.type==3||w.type==5){w.value=value>w.maximum?w.maximum:value;++w.revision;value=w.value;}
 // Coalesce slider motion; keep the latest value without flooding the VM.
 if(w.type==3||w.type==4||w.type==5)for(unsigned i=0;i<count;++i){auto& e=events[(first+i)%16];if(e.id==id){e.value=value;return;}}
 if(count<16){events[(first+count)%16]={id,value};++count;}
}
void input(unsigned id,const char* text,uint32_t expectedGeneration){
 if(!text)return;Lock lock;if((expectedGeneration&&expectedGeneration!=generation)||!visible||!widgets||id<1||id>MaxWidgets||widgets[id-1].type!=4)return;
 size_t n=strlen(text);if(n>=TextCapacity){n=TextCapacity-1;while(n&&(uint8_t(text[n])&0xc0)==0x80)--n;}auto& w=widgets[id-1];
 if(!strcmp(w.text,text))return;copy(w.text,sizeof(w.text),text,n);++w.revision;
}
int call(int op,int a,int b,int c,const void* data,size_t n,void* out,size_t capacity){
 if((n&&!data)||(capacity&&!out))return -EINVAL;
 Lock lock;
 if(op==94)return imageStatus;
 if(op==95){if(!visible||readerStatus==1||imageStatus==1)return -EBUSY;if(!data||!n||n>=sizeof(imagePath)||memchr(data,0,n)||static_cast<const char*>(data)[0]!='/')return -EINVAL;copy(imagePath,sizeof(imagePath),data,n);imageStatus=1;imagePending=true;return 0;}
 if(op==91)return readerStatus;
 if(op==92){if(readerStatus==1)return -EBUSY;size_t length=strlen(readerTarget);if(capacity<length)return -ENOSPC;if(length)memcpy(out,readerTarget,length);return length;}
 if(op==90){
  if(!visible||!data||n<3||readerStatus==1)return readerStatus==1?-EBUSY:-EINVAL;
  const char* bytes=static_cast<const char*>(data);const char* separator=static_cast<const char*>(memchr(bytes,0,n));
  if(!separator)return -EINVAL;size_t pathSize=separator-bytes,titleSize=n-pathSize-1;
  if(!pathSize||pathSize>=sizeof(reader.path)||titleSize>=sizeof(reader.title)||memchr(separator+1,0,titleSize))return -E2BIG;
  if(a<0||(a&~15))return -EINVAL;reader.illustrated=(a&1)!=0;reader.rich=(a&2)!=0;reader.canSave=!(a&4);reader.canUpdate=!(a&8);readerTarget[0]=0;
  copy(reader.path,sizeof(reader.path),bytes,pathSize);copy(reader.title,sizeof(reader.title),separator+1,titleSize);
  ++reader.revision;if(!reader.revision)++reader.revision;readerStatus=1;return 0;
 }
 if(op==50){if(n>=sizeof(title))return -E2BIG;if(!widgets){widgets=static_cast<Widget*>(allocateWidgets());if(widgets)for(unsigned i=0;i<MaxWidgets;++i)new (&widgets[i]) Widget{};}if(!widgets)return -ENOMEM;readerStatus=0;copy(title,sizeof(title),data,n);if(widgets)for(unsigned i=0;i<MaxWidgets;++i)widgets[i]=Widget{};first=count=0;++generation;visible=true;return 0;}
 if(op==55){readerStatus=0;visible=false;++generation;first=count=0;return 0;}
 if(op==56)return visible;
 if(op==57){size_t size=strlen(language);if(capacity<size)return -ENOSPC;memcpy(out,language,size);return size;}
 if(op==54){if(capacity<8)return -EINVAL;if(!count)return 0;auto e=events[first];first=(first+1)%16;--count;auto* p=(uint8_t*)out;for(unsigned i=0;i<4;++i){p[i]=e.id>>(i*8);p[i+4]=e.value>>(i*8);}return 8;}
 if(!visible||!widgets||a<1||a>int(MaxWidgets))return -EINVAL;auto& w=widgets[a-1];
 if(op==51){if(w.type||b<1||b>7||n<8||n>=TextCapacity+8||c<1||c>10000)return -EINVAL;const auto* p=(const uint8_t*)data;
  auto get=[&](unsigned i){return unsigned(p[i])|unsigned(p[i+1])<<8;};
  unsigned x=get(0),y=get(2),width=get(4),height=get(6);
  if(width<24||height<24||x+width>1100||y+height>500)return -EINVAL;
  w.x=x;w.y=y;w.w=width;w.h=height;w.type=b;w.maximum=c;copy(w.text,sizeof(w.text),p+8,n-8);++w.revision;return a;
 }
 if(!w.type)return -ENOENT;
 if(op==105){if(w.type!=2||b<0||b>7)return -EINVAL;w.shortcut=uint8_t(b);++w.revision;return 0;}
 if(op==58){size_t length=strlen(w.text);if(capacity<length)return -ENOSPC;memcpy(out,w.text,length);return length;}
 if(op==96){
  if(w.type!=7||n!=36)return -EINVAL;uint32_t palette[9];const auto* p=static_cast<const uint8_t*>(data);
  for(unsigned i=0;i<9;++i){const unsigned k=i*4;palette[i]=uint32_t(p[k])|uint32_t(p[k+1])<<8|uint32_t(p[k+2])<<16|uint32_t(p[k+3])<<24;if(palette[i]>0xffffff)return -EINVAL;}
  if(!memcmp(w.palette,palette,sizeof(palette)))return 0;memcpy(w.palette,palette,sizeof(palette));++w.revision;return 0;
 }
 if(op==59){if(w.type!=6||b<0||b>0xffffff||c<0||c>0xffffff)return -EINVAL;if(w.background==unsigned(b)&&w.foreground==unsigned(c))return 0;w.background=b;w.foreground=c;++w.revision;return 0;}
 if(op==52){if(n>=sizeof(w.text))return -E2BIG;if(strlen(w.text)==n&&(!n||!memcmp(w.text,data,n)))return 0;copy(w.text,sizeof(w.text),data,n);++w.revision;return 0;}
 if(op==53){if(b<0||b>w.maximum)return -EINVAL;if(w.value==unsigned(b))return 0;w.value=b;++w.revision;return 0;}
 return -ENOSYS;
}
}
