// SD and verified HTTPS services for the MicroPython worker. No LVGL calls.
#include "Tab5PythonServices.h"
#include "Tab5SystemPath.h"
#ifndef TAB5_PYTHON_HOST_TEST
#include "Tab5ResourceManager.h"
#endif
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#ifdef TAB5_PYTHON_HOST_TEST
#include <openssl/sha.h>
#include <zlib.h>
#else
#include <Arduino.h>
#include <HTTPClient.h>
#include "Tab5SecureClient.h"
#include <WiFi.h>
#include <mbedtls/sha256.h>
#include <esp_crc.h>
#include <time.h>
#include <esp_heap_caps.h>
#include "Tab5NetworkGate.h"
#include "Tab5PythonUI.h"
#include "Tab5Storage.h"
#include "Tab5CooperativeClient.h"
#include "Tab5PythonArchives.h"
#include "Tab5PythonZimBridge.h"
#include "Tab5PythonIrc.h"
#endif
extern "C" int tab5_mp_cancelled();
extern "C" void tab5_mp_yield();
namespace Tab5PythonServices {
namespace {
DIR* directories[8]{};
DIR* appDirectory=nullptr;
unsigned appEntries=0,appCount=0;
#ifndef TAB5_PYTHON_HOST_TEST
Tab5StorageUse* appStorage=nullptr;
#endif
int storageError(const char* operation,const char* name,int error=errno){
 if(!error)error=EIO;
#ifndef TAB5_PYTHON_HOST_TEST
 // Missing optional files and ordinary user errors do not mean the SD failed.
 if(error==EIO||error==ENODEV||error==EBADF||error==ETIMEDOUT)
  tab5StorageFault(operation,name,error);
#else
 (void)operation;(void)name;
#endif
 return -error;
}
struct WorkBuffer {
 uint8_t* data=nullptr;
 explicit WorkBuffer(size_t bytes){
#ifdef TAB5_PYTHON_HOST_TEST
  data=static_cast<uint8_t*>(std::malloc(bytes));
#else
  data=static_cast<uint8_t*>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#endif
 }
 ~WorkBuffer(){
#ifdef TAB5_PYTHON_HOST_TEST
  std::free(data);
#else
  heap_caps_free(data);
#endif
 }
 WorkBuffer(const WorkBuffer&)=delete;
 WorkBuffer& operator=(const WorkBuffer&)=delete;
};
bool path(const void* data,size_t n,char out[320]) {
 if(!data||!n||n>260)return false;
 const char* p=(const char*)data;
 if(p[0]!='/'||memchr(p,0,n))return false;
 size_t start=1;
 for(size_t i=0;i<=n;++i){
  if(i<n&&(uint8_t(p[i])<32||p[i]=='\\'||p[i]==':'))return false;
  if(i==n||p[i]=='/'){
   size_t count=i>=start?i-start:0;
   if((count==1&&p[start]=='.')||(count==2&&p[start]=='.'&&p[start+1]=='.'))return false;
   start=i+1;
  }
 }
 memcpy(out,"/sd",3);return tab5SystemPath(out+3,317,p,n);
}
int hashFile(const char* name,void* out,size_t cap){
 if(cap<64)return -ENOSPC;
 // This runs below the MicroPython C stack: avoid consuming another 4 KiB
 // while the interpreter is already nested in an installer/import operation.
 WorkBuffer buffer(4096);if(!buffer.data)return -ENOMEM;
 int fd=open(name,O_RDONLY);if(fd<0)return storageError("Python SHA ouvrir",name);
 uint8_t digest[32];int err=0;
#ifdef TAB5_PYTHON_HOST_TEST
 SHA256_CTX hash;SHA256_Init(&hash);
#else
 mbedtls_sha256_context hash;mbedtls_sha256_init(&hash);mbedtls_sha256_starts(&hash,0);
#endif
 while(true){
  if(tab5_mp_cancelled()){err=-ECANCELED;break;}
  int n=read(fd,buffer.data,4096);if(n<0){err=storageError("Python SHA lire",name);break;}if(!n)break;
#ifdef TAB5_PYTHON_HOST_TEST
  SHA256_Update(&hash,buffer.data,n);
#else
  if(mbedtls_sha256_update(&hash,buffer.data,n)){err=-EIO;break;}
#endif
  tab5_mp_yield();
 }
 close(fd);
#ifdef TAB5_PYTHON_HOST_TEST
 SHA256_Final(digest,&hash);
#else
 if(!err&&mbedtls_sha256_finish(&hash,digest))err=-EIO;
 mbedtls_sha256_free(&hash);
#endif
 if(err)return err;
 static const char hex[]="0123456789abcdef";
 for(unsigned i=0;i<32;++i){((char*)out)[i*2]=hex[digest[i]>>4];((char*)out)[i*2+1]=hex[digest[i]&15];}
 return 64;
}
#ifndef TAB5_PYTHON_HOST_TEST
bool cancelled(void*){return tab5_mp_cancelled();}
class VerifiedClient:public Tab5CooperativeClient<Tab5SecureClient> {
 public:VerifiedClient():Tab5CooperativeClient(cancelled){}
};
// HTTPClient decodes chunked responses. The sink enforces the limit even without Content-Length.
class DownloadFile:public Stream {
 public:int fd=-1,error=0;size_t count=0,limit;int widget;uint32_t started=millis(),last=0;
 DownloadFile(int f,size_t l,int id):fd(f),limit(l),widget(id){}
 int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}
 void flush()override{if(fsync(fd))error=errno;}
 size_t write(uint8_t b)override{return write(&b,1);}
 size_t write(const uint8_t* data,size_t n)override{
  if(error)return 0;
  if(tab5_mp_cancelled()){error=ECANCELED;return 0;}
  if(millis()-started>1800000){error=ETIMEDOUT;return 0;}
  if(n>limit-count){error=EFBIG;return 0;}
  int got=::write(fd,data,n);if(got<0){error=errno;return 0;}
  if(size_t(got)!=n){error=ENOSPC;return got;}
  count+=n;
  if(widget>0&&millis()-last>180){char text[64];snprintf(text,sizeof(text),"%u KiB",unsigned(count/1024));Tab5PythonUI::call(52,widget,0,0,text,strlen(text),nullptr,0);last=millis();}
  tab5_mp_yield();return n;
 }
};
int download(int limit,int widget,const void* data,size_t n){
 if(limit<1||limit>512*1024*1024||n>1300)return -EINVAL;
 const char* p=(const char*)data;const char* sep=(const char*)memchr(p,0,n);
 if(!sep||sep==p||sep-p>1024)return -EINVAL;
 char destination[320];if(!path(sep+1,n-(sep-p)-1,destination))return -EINVAL;
 String url;url.concat(p,sep-p);
 for(unsigned i=0;i<url.length();++i)if(uint8_t(url[i])<=32)return -EINVAL;
 if(!url.startsWith("https://"))return -5005;
 if(!tab5NetworkEnabled()||WiFi.status()!=WL_CONNECTED)return -5003;
 if(time(nullptr)<1700000000)return -5001; // Certificate validity requires a set clock.
 Tab5NetworkLease lease("MicroPython",6000,cancelled);
 if(!lease)return tab5_mp_cancelled()?-5006:-5002;
 for(unsigned redirect=0;redirect<4;++redirect){
  if(tab5_mp_cancelled())return -ECANCELED;
  auto admission=tab5ReserveMemory(Tab5Resources::Owner::Python,Tab5Resources::tlsBudget(),"TLS Python");
  if(!admission)return -ENOMEM;
  VerifiedClient client;client.setHandshakeTimeout(8);client.setConnectionTimeout(8000);client.setTimeout(5000);
  HTTPClient http;http.setReuse(false);http.setConnectTimeout(8000);http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if(!http.begin(client,url))return -EINVAL;
  const char* headers[]={"Location"};http.collectHeaders(headers,1);
  http.setUserAgent("Tab5OS/0.8.10 (M5Stack Tab5; MicroPython)");http.addHeader("Accept-Encoding","identity");
  int status=http.GET();
  admission.release(); // Connection allocations now belong to the live heap.
  if(status>=300&&status<=399){String next=http.header("Location");http.end();client.stop();
   if(next.startsWith("/")&&!next.startsWith("//")){int slash=url.indexOf('/',8);next=(slash<0?url:url.substring(0,slash))+next;}
   if(!next.startsWith("https://"))return -5005;
   url=next;continue;
  }
  if(status!=200){http.end();client.stop();return status>0?-(1000+status):-5004;}
  int length=http.getSize();if(length>limit){http.end();client.stop();return -EFBIG;}
  int fd=open(destination,O_WRONLY|O_CREAT|O_EXCL,0666);
  if(fd<0){int e=errno;http.end();client.stop();return -e;}
  DownloadFile sink(fd,limit,widget);int got=http.writeToStream(&sink);sink.flush();
  int error=sink.error? -sink.error : got<0?-5004:0;
  if(!error&&length>=0&&sink.count!=size_t(length))error=-5004;
  if(tab5_mp_cancelled())error=-ECANCELED;
  close(fd);http.end();client.stop();
  if(error){unlink(destination);if(sink.error)storageError("Téléchargement SD",destination,sink.error);return error;}
  return sink.count;
 }
 return -ELOOP;
}
#endif
}
bool storageIdle(){for(auto* d:directories)if(d)return false;return true;}
void reset(){for(auto& d:directories){if(d)closedir(d);d=nullptr;}
#ifndef TAB5_PYTHON_HOST_TEST
 Tab5PythonIrc::reset();
#endif
}
void endAppScan(){
 if(appDirectory)closedir(appDirectory);appDirectory=nullptr;
#ifndef TAB5_PYTHON_HOST_TEST
 delete appStorage;appStorage=nullptr;
#endif
}
bool beginAppScan(){
 endAppScan();appEntries=appCount=0;
#ifndef TAB5_PYTHON_HOST_TEST
 appStorage=new(std::nothrow) Tab5StorageUse(true);
 if(!appStorage||!bool(*appStorage)){endAppScan();return false;}
#endif
 appDirectory=opendir("/sd/MicroPython");
 if(!appDirectory){storageError("Liste applications Python","/MicroPython");endAppScan();return false;}
 return true;
}
int nextApp(AppInfo& app){
 if(!appDirectory)return 2;
 if(appEntries>=256||appCount>=MaxApps){endAppScan();return 3;}
 for(unsigned step=0;step<8&&appEntries<256;++step){
  errno=0;auto* entry=readdir(appDirectory);
  if(!entry){int error=errno;endAppScan();return error?storageError("Liste applications Python","/MicroPython",error):2;}
  ++appEntries;const char* leaf=entry->d_name;size_t n=strlen(leaf);
  if(n<4||leaf[0]=='.'||leaf[0]=='_'||!strcasecmp(leaf,"atelier.py")||(!strcasecmp(leaf,"create_theme.py")||!strcasecmp(leaf,"creer_theme.py"))||strcmp(leaf+n-3,".py")||n+13>=sizeof(app.path))continue;
  // Existing SDs may retain former bundled names. Prefer the English script
  // without deleting a legacy file or enumerating any additional directory.
  struct Alias {const char* legacy;const char* current;};
  static const Alias aliases[]={
   {"bibliotheques.py","libraries.py"},{"exemple.py","example.py"},
   {"importer_cartes.py","import_maps.py"},{"importer_radios.py","import_radios.py"},
   {"mettre_a_jour_sd.py","update_sd.py"},
  };
  bool superseded=false;
  for(const auto& alias:aliases)if(!strcmp(leaf,alias.legacy)){
   char replacement[64];snprintf(replacement,sizeof(replacement),"/sd/MicroPython/%s",alias.current);
   struct stat replacementInfo;
   superseded=stat(replacement,&replacementInfo)==0&&S_ISREG(replacementInfo.st_mode);
   break;
  }
  if(superseded)continue;
  app=AppInfo{};snprintf(app.path,sizeof(app.path),"/MicroPython/%s",leaf);
  char full[sizeof(app.path)+4];snprintf(full,sizeof(full),"/sd%s",app.path);
  struct stat s;if(stat(full,&s)){int error=errno;if(error==ENOENT)continue;endAppScan();return storageError("Application Python",app.path,error);}
  if(!S_ISREG(s.st_mode))continue;
  struct Known {const char* file;const char* title;AppIcon icon;};
  static const Known known[]={
   {"libraries.py","Bibliothèques Python",AppIcon::Libraries},
   {"bibliotheques.py","Bibliothèques Python",AppIcon::Libraries},
   {"wikipedia.py","Wikipédia",AppIcon::Libraries},
   {"irc.py","Discussion IRC",AppIcon::Python},
   {"import_maps.py","Cartes hors ligne",AppIcon::Map},
   {"importer_cartes.py","Cartes hors ligne",AppIcon::Map},
   {"import_radios.py","Importer des radios",AppIcon::Audio},
   {"importer_radios.py","Importer des radios",AppIcon::Audio},
   {"create_theme.py","Créer un thème",AppIcon::Theme},
   {"creer_theme.py","Créer un thème",AppIcon::Theme},
   {"update_sd.py","Vérifier la carte SD",AppIcon::Storage},
   {"mettre_a_jour_sd.py","Vérifier la carte SD",AppIcon::Storage},
   {"example.py","Exemple Python",AppIcon::Demo},
   {"exemple.py","Exemple Python",AppIcon::Demo},
   {"interface.py","Interface Python",AppIcon::Demo},
  };
  for(const auto& item:known)if(!strcmp(item.file,leaf)){
   snprintf(app.title,sizeof(app.title),"%s",item.title);app.icon=item.icon;break;
  }
  if(!app.title[0]){
   size_t length=n-3;if(length>=sizeof(app.title))length=sizeof(app.title)-1;
   // Truncate only on UTF-8 character boundaries.
   while(length&&(uint8_t(leaf[length])&0xc0)==0x80)--length;
   memcpy(app.title,leaf,length);app.title[length]=0;
   for(size_t i=0;i<length;++i)if(app.title[i]=='_')app.title[i]=' ';
  }
  ++appCount;return 1;
 }
 return 0;
}
int call(int op,int a,int b,int c,const void* input,size_t n,void* output,size_t capacity){
 (void)c;char name[320];if((n&&!input)||(capacity&&!output))return -EINVAL;
 if(op==68){if(n<4||capacity<4)return -EINVAL;auto* p=(const uint8_t*)input;
  uint32_t crc=uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
#ifdef TAB5_PYTHON_HOST_TEST
  crc=crc32(crc,p+4,n-4);
#else
  crc=esp_crc32_le(crc,p+4,n-4);
#endif
  for(unsigned i=0;i<4;++i)((uint8_t*)output)[i]=crc>>(8*i);return 4;
 }

#ifndef TAB5_PYTHON_HOST_TEST
 if(op==80)return download(a,b,input,n);
 if(op==82||op==85||op==104)return Tab5PythonArchives::call(op,a,b,c,input,n,output,capacity);
 if(op==83||op==84)return Tab5PythonZimBridge::call(op,input,n,output,capacity);
 if(op>=86&&op<=89)return Tab5PythonIrc::call(op,a,b,input,n,output,capacity);
#endif
 if(op==65){
  if(a<0||a>=8||!directories[a]||capacity<262)return -EINVAL;
  errno=0;dirent* e;
  while((e=readdir(directories[a]))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
   size_t len=strlen(e->d_name);if(len>255)return -ENAMETOOLONG;
   memcpy(output,e->d_name,len);return len;}
  return errno?storageError("Python lire dossier",nullptr):0;
 }
 if(op==66){if(a<0||a>=8||!directories[a])return -EINVAL;closedir(directories[a]);directories[a]=nullptr;return 0;}
 if(op==63){const char* sep=(const char*)memchr(input,0,n);if(!sep)return -EINVAL;
  char target[320];size_t first=sep-(const char*)input;
  if(!path(input,first,name)||!path(sep+1,n-first-1,target))return -EINVAL;
  if(rename(name,target))return storageError("Python renommer",name);return 0;}
 if(!path(input,n,name))return -EINVAL;
 if(op==60){struct stat s;if(stat(name,&s))return storageError("Python informations",name);if(capacity<12)return -ENOSPC;
  uint32_t mode=s.st_mode;uint64_t size=s.st_size;auto* p=(uint8_t*)output;
  for(unsigned i=0;i<4;++i)p[i]=mode>>(8*i);for(unsigned i=0;i<8;++i)p[4+i]=size>>(8*i);return 12;}
 if(op==61)return mkdir(name,0777)?storageError("Python creer dossier",name):0;
 if(op==62)return (a?rmdir(name):unlink(name))?storageError("Python supprimer",name):0;
 if(op==64){unsigned slot=0;while(slot<8&&directories[slot])++slot;if(slot==8)return -EMFILE;
  directories[slot]=opendir(name);if(!directories[slot])return storageError("Python ouvrir dossier",name);return slot;}
 if(op==69)return hashFile(name,output,capacity);
 return -ENOSYS;
}
}
