#ifndef TAB5_PYTHON_HOST_TEST
#include "Tab5PythonArchives.h"
#include "Tab5SystemPath.h"
#include "Tab5HttpRange.h"
#include "Tab5Json.h"
#include "Tab5Storage.h"
#include "Tab5NetworkGate.h"
#include "Tab5ResourceManager.h"
#include "Tab5PythonUI.h"
#include "Tab5CooperativeClient.h"
#include <esp_vfs_fat.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "Tab5SecureClient.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <mbedtls/sha256.h>
extern "C" int tab5_mp_cancelled();
extern "C" void tab5_mp_yield();

namespace Tab5PythonArchives {
namespace {
bool sdPath(const char* source,char* target,size_t capacity,bool prefix=true) {
 if(!source||source[0]!='/')return false;
 size_t n=strlen(source);if(!n||n>250||n+(prefix?4:1)>capacity)return false;
 const char* start=source+1;
 for(const char* p=source;;++p){
  if(*p&&(uint8_t(*p)<32||*p=='\\'||*p==':'))return false;
  if(!*p||*p=='/'){
   if(p>=start){size_t len=p-start;if((len==1&&*start=='.')||(len==2&&start[0]=='.'&&start[1]=='.'))return false;}
   start=p+1;if(!*p)break;
  }
 }
 if(prefix)memcpy(target,"/sd",3);return tab5SystemPath(target+(prefix?3:0),capacity-(prefix?3:0),source,n);
}
int result(JsonDocument& doc,void* output,size_t capacity) {
 if(doc.overflowed())return -ENOMEM;
 size_t n=measureJson(doc);if(n>capacity)return -ENOSPC;
 return int(serializeJson(doc,static_cast<char*>(output),capacity));
}
struct Transfer {
 int pause=0,widget=0;bool paused=false;uint32_t started=millis(),last=0;
 uint64_t offset=0,total=0;
 bool cancelled() {
  if(!paused&&pause>0)paused=Tab5PythonUI::consumeEvent(unsigned(pause));
  return paused||tab5_mp_cancelled()||millis()-started>3600000;
 }
 int error() const {return paused?-5006:tab5_mp_cancelled()?-ECANCELED:-ETIMEDOUT;}
 static bool cancel(void* context){return static_cast<Transfer*>(context)->cancelled();}
};
class VerifiedRangeClient:public Tab5CooperativeClient<Tab5SecureClient> {
 public:explicit VerifiedRangeClient(Transfer& state):Tab5CooperativeClient(Transfer::cancel,&state) {
  
 }
};
class PartSink:public Stream {
 public:
 int fd,error=0;uint32_t count=0,limit;Transfer& transfer;
 PartSink(int f,uint32_t l,Transfer& state):fd(f),limit(l),transfer(state){}
 int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}
 void flush()override{if(!error&&fsync(fd))error=-errno;}
 size_t write(uint8_t value)override{return write(&value,1);}
 size_t write(const uint8_t* data,size_t size)override {
  if(error){setWriteError(-error);return 0;}
  if(transfer.cancelled()){error=transfer.error();setWriteError(-error);return 0;}
  if(!size){error=-ETIMEDOUT;setWriteError(-error);return 0;}
  if(size>limit-count){error=-EFBIG;setWriteError(-error);return 0;}
  ssize_t written=::write(fd,data,size);
  if(written<0){error=-errno;setWriteError(-error);return 0;}
  count+=uint32_t(written);
  if(size_t(written)!=size){error=-ENOSPC;setWriteError(-error);return written;}
  if(transfer.widget>0&&millis()-transfer.last>200){
   char label[96];snprintf(label,sizeof(label),"%llu / %llu KiB",(unsigned long long)((transfer.offset+count)/1024),(unsigned long long)(transfer.total/1024));
   Tab5PythonUI::call(52,transfer.widget,0,0,label,strlen(label),nullptr,0);transfer.last=millis();
  }
  tab5_mp_yield();return size;
 }
};
int download(int widget,int pause,const void* input,size_t size,void* output,size_t capacity) {
 if(!input||size>4096||capacity<1024)return -EINVAL;
 JsonDocument request(tab5JsonAllocator());
 if(deserializeJson(request,input,size))return -EINVAL;
 if(!request["offset"].is<uint64_t>()||!request["count"].is<uint32_t>()||!request["expected_size"].is<uint64_t>())return -EINVAL;
 uint64_t offset=request["offset"],expected=request["expected_size"];uint32_t count=request["count"];
 if(!count||count>67108864||offset>UINT64_MAX-count||!expected||offset+count>expected)return -EINVAL;
 const char* rawUrl=request["url"]|"";const char* rawPath=request["path"]|"";
 if(!Tab5HttpRange::safeHeader(rawUrl,1800))return -EINVAL;
 for(const char* p=rawUrl;*p;++p)if(*p==' ')return -EINVAL;
 String url(rawUrl);if(!url.startsWith("https://"))return -5005;
 char destination[264],temporary[272];
 if(!sdPath(rawPath,destination,sizeof(destination)))return -EINVAL;
 snprintf(temporary,sizeof(temporary),"%s.part",destination);
 String etag=request["etag"]|"",modified=request["last_modified"]|"";
 String digest=request["sha256"]|"";
 bool hasDigest=digest.length()==64;
 for(char ch:digest)if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')))hasDigest=false;
 if(!Tab5HttpRange::safeHeader(etag.c_str())||!Tab5HttpRange::safeHeader(modified.c_str()))return -EINVAL;
 if(etag.length()&&!Tab5HttpRange::strongEtag(etag.c_str()))etag="";
 if(offset&&!etag.length()&&!modified.length()&&!hasDigest)return -ESTALE;
 request.clear();
 Tab5StorageUse storage;if(!storage)return -ENODEV;
 struct stat previous;if(!stat(destination,&previous))return -EEXIST;if(errno!=ENOENT)return -errno;
 if(!tab5NetworkEnabled()||WiFi.status()!=WL_CONNECTED)return -5003;
 if(time(nullptr)<1700000000)return -5001;
 Transfer transfer;transfer.pause=pause;transfer.widget=widget;transfer.offset=offset;transfer.total=expected;
 Tab5NetworkLease lease("Wikipedia",6000,Transfer::cancel,&transfer);
 if(!lease)return transfer.cancelled()?transfer.error():-5002;
 for(unsigned redirect=0;redirect<6;++redirect) {
  if(transfer.cancelled())return transfer.error();
  auto admission=tab5ReserveMemory(Tab5Resources::Owner::Python,Tab5Resources::tlsBudget(),"TLS archive");
  if(!admission)return -ENOMEM;
  VerifiedRangeClient client(transfer);client.setHandshakeTimeout(8);client.setConnectionTimeout(8000);client.setTimeout(5000);
  HTTPClient http;http.setReuse(false);http.setConnectTimeout(8000);http.setTimeout(5000);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if(!http.begin(client,url))return -EINVAL;
  const char* headers[]={"Location","Content-Range","ETag","Last-Modified","Content-Encoding"};http.collectHeaders(headers,5);
  http.setUserAgent("Tab5OS/0.8.12 (M5Stack Tab5; Wikipedia archive)");http.addHeader("Accept-Encoding","identity");
  char range[64];snprintf(range,sizeof(range),"bytes=%llu-%llu",(unsigned long long)offset,(unsigned long long)(offset+count-1));http.addHeader("Range",range);
  if(etag.length())http.addHeader("If-Range",etag);else if(modified.length())http.addHeader("If-Range",modified);
  int status=http.GET();admission.release();
  if(transfer.cancelled()){http.end();client.stop();return transfer.error();}
  if(status>=300&&status<400){
   String next=http.header("Location");http.end();client.stop();
   if(next.startsWith("/")&&!next.startsWith("//")){int slash=url.indexOf('/',8);next=(slash<0?url:url.substring(0,slash))+next;}
   if(!next.startsWith("https://")||!Tab5HttpRange::safeHeader(next.c_str(),1800)||next.indexOf(' ')>=0)return -5005;
   url=next;continue;
  }
  if(status!=206){http.end();client.stop();return status==200?-5007:status>0?-(1000+status):-5004;}
  Tab5HttpRange::Range parsed;
  String encoding=http.header("Content-Encoding"),newEtag=http.header("ETag"),newModified=http.header("Last-Modified");
  if(!Tab5HttpRange::matches(http.header("Content-Range").c_str(),offset,count,0,parsed)||
     (http.getSize()>=0&&http.getSize()!=int(count))||
     (encoding.length()&&encoding!="identity")||
     !Tab5HttpRange::safeHeader(newEtag.c_str())||!Tab5HttpRange::safeHeader(newModified.c_str())) {
   http.end();client.stop();return -5010;
  }
  if(parsed.total!=expected){http.end();client.stop();return -5008;}
  if((etag.length()&&newEtag!=etag)||(!etag.length()&&modified.length()&&newModified!=modified)){http.end();client.stop();return -5009;}
  if(!Tab5HttpRange::strongEtag(newEtag.c_str()))newEtag="";
  if(!newEtag.length()&&!newModified.length()&&!hasDigest){http.end();client.stop();return -ESTALE;}
  if(unlink(temporary)&&errno!=ENOENT){int e=errno;http.end();client.stop();return -e;}
  int fd=open(temporary,O_WRONLY|O_CREAT|O_EXCL,0666);
  if(fd<0){int e=errno;http.end();client.stop();return -e;}
  PartSink sink(fd,count,transfer);int received=http.writeToStream(&sink);sink.flush();
  int error=sink.error?sink.error:received<0||sink.count!=count?-5004:0;
  if(close(fd)&&!error)error=-errno;
  http.end();client.stop();if(transfer.cancelled())error=transfer.error();
  if(error){unlink(temporary);if(error==-EIO||error==-ENODEV)tab5StorageFault("Archive",destination,-error);return error;}
  JsonDocument reply(tab5JsonAllocator());reply["size"]=parsed.total;reply["offset"]=offset;reply["count"]=count;reply["etag"]=newEtag;reply["last_modified"]=newModified;reply["mirror"]=url;
  int length=result(reply,output,capacity);
  if(length<0){unlink(temporary);return length;}
  if(rename(temporary,destination)){int e=errno;unlink(temporary);return -e;}
  return length;
 }
 return -ELOOP;
}
int verify(int widget,int pause,const void* input,size_t size,void* output,size_t capacity){
 if(!input||size>4096)return -EINVAL;
 JsonDocument request(tab5JsonAllocator());if(deserializeJson(request,input,size))return -EINVAL;
 if(!request["size"].is<uint64_t>())return -EINVAL;
 uint64_t total=request["size"];String digest=request["sha256"]|"";String title=request["label"]|"Verifying archive";char directory[264];
 if(!Tab5HttpRange::safeHeader(title.c_str(),160))return -EINVAL;
 if(!total||total>(1ULL<<50)||digest.length()!=64||!sdPath(request["path"]|"",directory,sizeof(directory)))return -EINVAL;
 for(char ch:digest)if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')))return -EINVAL;
 Tab5StorageUse storage;if(!storage)return -ENODEV;
 auto* buffer=static_cast<uint8_t*>(heap_caps_malloc(16384,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!buffer)return -ENOMEM;
 struct Free{uint8_t* p;~Free(){heap_caps_free(p);}}free{buffer};
 Transfer transfer;transfer.widget=widget;transfer.pause=pause;transfer.total=total;
 mbedtls_sha256_context hash;mbedtls_sha256_init(&hash);
 struct HashFree{mbedtls_sha256_context* p;~HashFree(){mbedtls_sha256_free(p);}}hashFree{&hash};
 if(mbedtls_sha256_starts(&hash,0))return -EIO;
 uint64_t done=0;
 for(uint32_t index=0;done<total;++index){
  if(transfer.cancelled())return transfer.error();
  char path[296];snprintf(path,sizeof(path),"%s/part-%06lu.bin",directory,(unsigned long)index);
  int fd=open(path,O_RDONLY);if(fd<0)return -errno;
  struct Close{int fd;~Close(){close(fd);}}closeFile{fd};
  struct stat info;uint32_t part=uint32_t(std::min<uint64_t>(67108864,total-done));
  if(fstat(fd,&info)||uint64_t(info.st_size)!=part)return -5012;
  for(uint32_t at=0;at<part;){
   if(transfer.cancelled())return transfer.error();
   int n=read(fd,buffer,std::min<uint32_t>(16384,part-at));if(n<=0){int error=errno;if(n<0&&error==EIO)tab5StorageFault("Archive",path,EIO);return n<0?-error:-5012;}
   if(mbedtls_sha256_update(&hash,buffer,n))return -EIO;at+=n;done+=n;
   if(widget>0&&millis()-transfer.last>200){char label[240];snprintf(label,sizeof(label),"%s · %llu / %llu MiB",title.c_str(),(unsigned long long)(done/1048576),(unsigned long long)(total/1048576));Tab5PythonUI::call(52,widget,0,0,label,strlen(label),nullptr,0);transfer.last=millis();}
   tab5_mp_yield();
  }
 }
 uint8_t raw[32];char hex[65];if(mbedtls_sha256_finish(&hash,raw))return -EIO;
 for(unsigned i=0;i<32;++i)snprintf(hex+i*2,3,"%02x",raw[i]);
 if(digest!=hex)return -5012;
 JsonDocument reply(tab5JsonAllocator());reply["verified"]=true;return result(reply,output,capacity);
}
}
int call(int op,int a,int b,int c,const void* input,size_t size,void* output,size_t capacity) {
 (void)a;if((size&&!input)||(capacity&&!output))return -EINVAL;
 if(op==82)return download(b,c,input,size,output,capacity);
 if(op==104)return verify(b,c,input,size,output,capacity);
 if(op==85){
  if(capacity<128)return -ENOSPC;Tab5StorageUse storage;if(!storage)return -ENODEV;
  if(tab5_mp_cancelled())return -ECANCELED;
  uint64_t total=0,free=0;
  if(esp_vfs_fat_info("/sd",&total,&free)!=ESP_OK||!total||free>total)return -EIO;
  tab5_mp_yield();if(tab5_mp_cancelled())return -ECANCELED;
  JsonDocument reply(tab5JsonAllocator());reply["total"]=total;reply["used"]=total-free;reply["free"]=free;
  return result(reply,output,capacity);
 }
 return -ENOSYS;
}
}
#endif
