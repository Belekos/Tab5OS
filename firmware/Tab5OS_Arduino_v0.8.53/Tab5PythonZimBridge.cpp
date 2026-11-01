#include "Tab5VolumeFS.h"
#ifndef TAB5_PYTHON_HOST_TEST
#include "Tab5PythonZimBridge.h"
#include "Tab5Zim.h"
#include "Tab5Json.h"
#include "Tab5Storage.h"
#include "Tab5ResourceManager.h"
#include <SD.h>
#include <esp_heap_caps.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
extern "C" int tab5_mp_cancelled();
extern "C" void tab5_mp_yield();
namespace Tab5PythonZimBridge {
namespace {
bool validPath(const char* source) {
 if(!source||source[0]!='/'||strlen(source)>250)return false;
 const char* start=source+1;
 for(const char* p=source;;++p){
  if(*p&&(uint8_t(*p)<32||*p=='\\'||*p==':'))return false;
  if(!*p||*p=='/'){
   if(p>=start){size_t n=p-start;if((n==1&&*start=='.')||(n==2&&start[0]=='.'&&start[1]=='.'))return false;}
   start=p+1;if(!*p)break;
  }
 }
 return true;
}
bool cancelled(void*){return tab5_mp_cancelled();}
void cooperate(void*){tab5_mp_yield();}
struct MemoryFailure{size_t requested=0,free=0,largest=0;};
void* allocate(void* context,size_t size){
 if(!size||tab5_mp_cancelled())return nullptr;
 // Admission happens before consuming the decoder's large blocks. Waiting for
 // malloc to fail first could leave no reserve for the UI or for cancellation.
 auto recovered=tab5ReserveMemory(Tab5Resources::Owner::Python,{0,0,0,size,size},"Wikipedia");
 void* memory=recovered?heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):nullptr;
 if(memory)recovered.allocated(0,size);
 else if(context){auto& f=*static_cast<MemoryFailure*>(context);f.requested=size;f.free=heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);f.largest=heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
 return memory;
}
void release(void*,void* data){heap_caps_free(data);}
int result(JsonDocument& reply,void* output,size_t capacity){
 if(reply.overflowed())return -ENOMEM;
 if(measureJson(reply)>capacity)return -ENOSPC;
 return int(serializeJson(reply,static_cast<char*>(output),capacity));
}
int failure(const char* error,void* output,size_t capacity,const MemoryFailure* memory=nullptr){
 if(tab5_mp_cancelled())return -ECANCELED;
 JsonDocument reply(tab5JsonAllocator());reply["ok"]=false;reply["error"]=error?error:"Invalid ZIM archive";
 if(memory&&memory->requested&&error&&strstr(error,"PSRAM")){
  char detail[224];snprintf(detail,sizeof(detail),"%s (%u KiB requested; %u KiB free; largest block %u KiB)",error,unsigned((memory->requested+1023)/1024),unsigned(memory->free/1024),unsigned(memory->largest/1024));reply["error"]=detail;
  reply["requested"]=memory->requested;reply["free"]=memory->free;reply["largest_block"]=memory->largest;
 }
 return result(reply,output,capacity);
}
void title(const char* input,char* output,size_t limit) {
 size_t n=strlen(input);if(n>limit){n=limit;while(n&&(uint8_t(input[n])&0xc0)==0x80)--n;}
 memcpy(output,input,n);output[n]=0;
}
struct Search {
 JsonArray entries;size_t budget;
 static bool add(void* context,const Tab5Zim::Entry& item){
  auto& state=*static_cast<Search*>(context);auto row=state.entries.add<JsonObject>();
  row["id"]=item.id;char text[301];size_t limit=300;title(item.title[0]?item.title:item.path,text,limit);row["title"]=text;
  while(measureJson(row)>state.budget&&limit>8){limit=limit*3/4;title(item.title[0]?item.title:item.path,text,limit);row["title"]=text;}
  return true;
 }
};
struct Output {
 int fd=-1,error=0;uint64_t bytes=0;
 static bool write(void* context,const uint8_t* data,size_t size){
  auto& out=*static_cast<Output*>(context);
  if(size>8388608-out.bytes){out.error=-EFBIG;return false;}
  if(tab5_mp_cancelled()){out.error=-ECANCELED;return false;}
  ssize_t written=::write(out.fd,data,size);if(written<0){out.error=-errno;return false;}
  out.bytes+=written;if(size_t(written)!=size){out.error=-ENOSPC;return false;}
  tab5_mp_yield();return true;
 }
};
struct ImageRequest { uint64_t offset=0;char target[1030]={}; };
struct RichOutput {
 ImageRequest images[8];unsigned count=0;
 static bool image(void* context,uint64_t offset,const char* target){
  auto& state=*static_cast<RichOutput*>(context);
  if(state.count==8)return true;
  auto& image=state.images[state.count++];image.offset=offset;
  snprintf(image.target,sizeof(image.target),"%s",target);return true;
 }
};
bool beginOutput(Output& output,const char* path){
 if(unlink(path)&&errno!=ENOENT){output.error=-errno;return false;}
 output.fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0666);
 if(output.fd<0){output.error=-errno;return false;}
 return true;
}
bool finishOutput(Output& output,bool ok){
 if(output.fd<0)return false;
 if(ok&&fsync(output.fd)){ok=false;output.error=-errno;}
 if(close(output.fd)&&ok){ok=false;output.error=-errno;}
 output.fd=-1;return ok;
}
}
int call(int op,const void* input,size_t size,void* output,size_t capacity){
 if((op!=83&&op!=84)||!input||size>4096||!output||capacity<512)return -EINVAL;
 JsonDocument request(tab5JsonAllocator());if(deserializeJson(request,input,size))return -EINVAL;
 const char* source=request["path"]|"";if(!validPath(source))return -EINVAL;
 Tab5StorageUse storage;if(!storage)return -ENODEV;
 Tab5Zim::SdSource archive;String error;if(!archive.open(tab5FS,source,error))return failure(error.c_str(),output,capacity);
 MemoryFailure memory;Tab5Zim::Callbacks callbacks;callbacks.context=&memory;callbacks.cancelled=cancelled;callbacks.cooperate=cooperate;callbacks.allocate=allocate;callbacks.release=release;
 Tab5Zim::Reader reader(archive.source(),callbacks);if(!reader.open())return failure(reader.error(),output,capacity,&memory);
 if(op==83){
  const char* prefix=request["prefix"]|"";if(strlen(prefix)>200)return -EINVAL;
  unsigned limit=request["limit"]|8u;uint32_t cursor=request["cursor"]|0u;
  if(!limit||limit>12)return -EINVAL;
  JsonDocument reply(tab5JsonAllocator());reply["ok"]=true;reply["entries_total"]=reader.entryCount();reply["articles_total"]=reader.articleCount();reply["search"]="title-prefix";
  Search search{reply["entries"].to<JsonArray>(),(capacity-200)/limit};unsigned found=0;uint32_t next=0;
  if(!reader.search(prefix,Search::add,&search,limit,&found,cursor,&next))return failure(reader.error(),output,capacity,&memory);
  if(next)reply["next_cursor"]=next;else reply["next_cursor"]=nullptr;
  return result(reply,output,capacity);
 }
 const char* link=request["link"]|"";const char* base=request["base"]|"";
 if(!request["id"].is<uint32_t>()&&!*link)return -EINVAL;
 if(strlen(link)>1026||strlen(base)>1026)return -EINVAL;
 const char* destination=request["destination"]|"";if(!validPath(destination)||!strcasecmp(source,destination))return -EINVAL;
 size_t destinationLength=strlen(destination);
 if(destinationLength<4||strcasecmp(destination+destinationLength-4,".txt"))return -EINVAL;
 char target[256],temporary[272],linksTarget[272],linksPart[280],mediaTarget[272],mediaPart[280];
 snprintf(target,sizeof(target),"/sd%s",destination);snprintf(temporary,sizeof(temporary),"%s.part",target);
 snprintf(linksTarget,sizeof(linksTarget),"%s.links",target);snprintf(linksPart,sizeof(linksPart),"%s.part",linksTarget);
 snprintf(mediaTarget,sizeof(mediaTarget),"%s.media",target);snprintf(mediaPart,sizeof(mediaPart),"%s.part",mediaTarget);
 auto* resolved=static_cast<Tab5Zim::Entry*>(allocate(nullptr,sizeof(Tab5Zim::Entry)));
 auto* richState=static_cast<RichOutput*>(allocate(nullptr,sizeof(RichOutput)));
 if(!resolved||!richState){release(nullptr,resolved);release(nullptr,richState);return -ENOMEM;}
 new(resolved) Tab5Zim::Entry;
 new(richState) RichOutput;
 uint32_t id=request["id"]|0u;
 if(*link&&!reader.resolveLink(link,*resolved,*base?base:nullptr)){
  resolved->~Entry();release(nullptr,resolved);richState->~RichOutput();release(nullptr,richState);
  return failure(reader.error(),output,capacity,&memory);
 }
 if(*link)id=resolved->id;
 Output out,links,media;
 bool ok=beginOutput(out,temporary)&&beginOutput(links,linksPart)&&beginOutput(media,mediaPart);
 Tab5Zim::RichOptions rich;rich.links={&links,Output::write};rich.context=richState;
 if(request["images"]|true)rich.image=RichOutput::image;
 uint64_t written=0;
 if(ok)ok=reader.extractText(id,{&out,Output::write},resolved,&written,&rich);
 const char* extractionError=ok?nullptr:reader.error();
 // The article stream and its decoder have been released before opening images.
 unsigned imageCount=0;uint64_t imageBytes=0;int imageError=0;
 for(unsigned i=0;ok&&i<richState->count;++i){
  if(tab5_mp_cancelled()){ok=false;imageError=-ECANCELED;break;}
  const ImageRequest& image=richState->images[i];
  // Reuse an allocated entry; preserve the article metadata for the response.
  auto* item=static_cast<Tab5Zim::Entry*>(allocate(nullptr,sizeof(Tab5Zim::Entry)));
  if(!item)break;
  new(item) Tab5Zim::Entry;
  bool found=reader.resolveLink(image.target,*item);uint32_t imageId=item->id;
  item->~Entry();release(nullptr,item);
  if(!found)continue;
  char imageTarget[272],imagePart[280],imagePath[264];
  snprintf(imageTarget,sizeof(imageTarget),"%s.img%u",target,imageCount);
  snprintf(imagePart,sizeof(imagePart),"%s.part",imageTarget);
  snprintf(imagePath,sizeof(imagePath),"%s.img%u",destination,imageCount);
  Output imageOut;
  if(!beginOutput(imageOut,imagePart)){ok=false;imageError=imageOut.error;break;}
  uint64_t bytes=0;
  bool extracted=reader.extractImage(imageId,{&imageOut,Output::write},&bytes,131072);
  extracted=finishOutput(imageOut,extracted);
  if(!extracted){unlink(imagePart);if(imageOut.error){ok=false;imageError=imageOut.error;}continue;}
  char row[320];int n=snprintf(row,sizeof(row),"%llu\t%s\n",(unsigned long long)image.offset,imagePath);
  if(!Output::write(&media,reinterpret_cast<const uint8_t*>(row),size_t(n))){ok=false;unlink(imagePart);break;}
  imageBytes+=bytes;++imageCount;
 }
 ok=finishOutput(out,ok)&&ok;
 ok=finishOutput(links,ok)&&ok;
 ok=finishOutput(media,ok)&&ok;
 int ioError=out.error?out.error:(links.error?links.error:(media.error?media.error:imageError));
 if(tab5_mp_cancelled()){ok=false;ioError=-ECANCELED;}
 bool sidecarsChanged=false;
 for(unsigned i=0;ok&&i<imageCount;++i){
  char imageTarget[272],imagePart[280];snprintf(imageTarget,sizeof(imageTarget),"%s.img%u",target,i);snprintf(imagePart,sizeof(imagePart),"%s.part",imageTarget);
  if(rename(imagePart,imageTarget)){ok=false;ioError=-errno;}else sidecarsChanged=true;
 }
 if(ok){if(rename(linksPart,linksTarget)){ok=false;ioError=-errno;}else sidecarsChanged=true;}
 if(ok&&rename(mediaPart,mediaTarget)){ok=false;ioError=-errno;}
 if(ok&&rename(temporary,target)){ok=false;ioError=-errno;}
 JsonDocument reply(tab5JsonAllocator());
 if(ok){
  reply["ok"]=true;reply["path"]=destination;reply["title"]=resolved->title[0]?resolved->title:resolved->path;
  reply["article_path"]=resolved->path;reply["id"]=resolved->id;reply["bytes"]=written;reply["rich"]=true;reply["images"]=imageCount;reply["image_bytes"]=imageBytes;reply["image_failures"]=richState->count-imageCount;
  char canonical[1030];snprintf(canonical,sizeof(canonical),"%c/%s",resolved->nameSpace,resolved->path);reply["article_link"]=canonical;
 }
 resolved->~Entry();release(nullptr,resolved);
 richState->~RichOutput();release(nullptr,richState);
 // Partial files are never advertised. A failed final commit removes sidecars
 // so an older text file cannot accidentally consume newer byte offsets.
 if(!ok){unlink(temporary);unlink(linksPart);unlink(mediaPart);if(sidecarsChanged){unlink(linksTarget);unlink(mediaTarget);}}
 for(unsigned i=0;i<8;++i){
  char imageTarget[272],imagePart[280];snprintf(imageTarget,sizeof(imageTarget),"%s.img%u",target,i);snprintf(imagePart,sizeof(imagePart),"%s.part",imageTarget);
  unlink(imagePart);if(ok&&i>=imageCount)unlink(imageTarget);
 }
 if(!ok)return ioError?ioError:failure(extractionError?extractionError:reader.error(),output,capacity,&memory);
 return result(reply,output,capacity);
}
}
#endif
