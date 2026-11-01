#include "Tab5WikiConvert.h"
#include "Tab5WikiMarkup.h"
#include "Tab5FilePolicy.h"
#include "Tab5Storage.h"
#include "Tab5Json.h"
#include "Tab5ResourceManager.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <esp_heap_caps.h>
#include <new>
extern "C" bool tab5_mp_cancelled();
extern "C" void tab5_mp_yield();
namespace {
struct Output {
 int fd=-1;char data[2048];size_t used=0;
 bool flush(){if(!used)return true;ssize_t n=write(fd,data,used);if(n!=ssize_t(used))return false;used=0;return true;}
 bool add(const char* text,size_t size){while(size){size_t n=std::min(sizeof(data)-used,size);memcpy(data+used,text,n);used+=n;text+=n;size-=n;if(used==sizeof(data)&&!flush())return false;}return true;}
 ~Output(){if(fd>=0)close(fd);}
};
struct Work {Output text,links;char input[4096];};
}
int tab5WikiConvert(const void* data,size_t length){
 if(!data||length>1100)return -EINVAL;
 const char* p=static_cast<const char*>(data);size_t at=0;char names[4][266];char language[21];
 for(unsigned i=0;i<5;++i){size_t begin=at;while(at<length&&p[at])++at;size_t n=at-begin;if(!n)return -EINVAL;
  if(i==4){if(n>20)return -EINVAL;for(size_t j=0;j<n;++j)if(!isalnum((unsigned char)p[begin+j])&&p[begin+j]!='-')return -EINVAL;memcpy(language,p+begin,n);language[n]=0;}
  else {if(n>260)return -EINVAL;std::string canonical,source(p+begin,n);if(!Tab5Files::canonical(source,canonical))return -EINVAL;snprintf(names[i],sizeof(names[i]),"/sd%s",source.c_str());}
  if(i<4&&at==length)return -EINVAL;++at;
 }
 for(unsigned i=0;i<4;++i)for(unsigned j=0;j<i;++j)if(!strcasecmp(names[i],names[j]))return -EINVAL;
 Tab5StorageUse lease;if(!lease)return -EBUSY;
 Tab5Resources::Budget budget{};budget.psram=96*1024;budget.psramBlock=16*1024;auto admission=tab5ReserveMemory(Tab5Resources::Owner::Python,budget,"Wikipédia HTML");if(!admission)return -ENOMEM;
 auto* memory=heap_caps_malloc(sizeof(Work),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!memory)return -ENOMEM;auto* work=new(memory)Work;
 struct Release {Work* w;~Release(){w->~Work();heap_caps_free(w);}} release{work};
 int source=open(names[0],O_RDONLY);if(source<0)return -errno;
 struct Fd {int n;~Fd(){if(n>=0)close(n);}} sourceGuard{source},media{-1};
 struct stat st;if(fstat(source,&st)||st.st_size>2097152||!S_ISREG(st.st_mode))return -EINVAL;
 work->text.fd=open(names[1],O_CREAT|O_EXCL|O_WRONLY,0666);if(work->text.fd<0)return -errno;
 work->links.fd=open(names[2],O_CREAT|O_EXCL|O_WRONLY,0666);if(work->links.fd<0)return -errno;
 media.n=open(names[3],O_CREAT|O_EXCL|O_WRONLY,0666);if(media.n<0)return -errno;
 Tab5WikiMarkup::Parser parser([](void* w,const char* p,size_t n){return static_cast<Work*>(w)->text.add(p,n);},[](void* w,const char* p,size_t n){return static_cast<Work*>(w)->links.add(p,n);},work,language);
 for(;;){if(tab5_mp_cancelled())return -ECANCELED;ssize_t n=read(source,work->input,sizeof(work->input));if(n<0){tab5StorageFault("Wikipédia",names[0],errno);return -errno;}if(!n)break;if(!parser.feed(work->input,n)){if(parser.error==EIO)tab5StorageFault("Wikipédia",names[1],EIO);return -parser.error;}tab5_mp_yield();}
 if(!parser.feed("",0,true)){if(parser.error==EIO)tab5StorageFault("Wikipédia",names[1],EIO);return -parser.error;}
 JsonDocument doc(tab5JsonAllocator());auto rows=doc.to<JsonArray>();for(auto& image:parser.images){auto row=rows.add<JsonObject>();row["offset"]=image.offset;row["url"]=image.url.c_str();row["description"]=image.description.c_str();row["alt"]=image.alt.c_str();}
 if(doc.overflowed())return -ENOMEM;
 size_t n=measureJson(doc);char* buffer=static_cast<char*>(heap_caps_malloc(n+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!buffer)return -ENOMEM;
 serializeJson(doc,buffer,n+1);bool ok=write(media.n,buffer,n)==ssize_t(n);heap_caps_free(buffer);
 ok=ok&&work->text.flush()&&work->links.flush();ok=ok&&fsync(work->text.fd)==0&&fsync(work->links.fd)==0&&fsync(media.n)==0;
 if(!ok){tab5StorageFault("Wikipédia",names[1],EIO);return -EIO;}return int(parser.offset);
}
