#include "Tab5VolumeFS.h"
#include "Tab5SystemPath.h"
#include "Tab5UsbStorage.h"
#include "Tab5Storage.h"
#include <vfs_api.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include <new>
#include <atomic>
namespace {
std::atomic<unsigned> sdRefs{0};
std::atomic<bool> sdAvailable{true};
std::atomic<uint32_t> sdGeneration{1};
std::atomic<uint32_t> fsRevision{1};
bool retainSd(uint32_t& generation){if(!sdAvailable){errno=ENODEV;return false;}++sdRefs;if(!sdAvailable){--sdRefs;errno=ENODEV;return false;}generation=sdGeneration;return true;}
bool validSd(uint32_t generation){if(sdAvailable&&generation==sdGeneration)return true;errno=ENODEV;return false;}
void releaseVolume(bool usb){if(usb)Tab5UsbStorage::release();else --sdRefs;}
bool retainVolume(bool usb,uint32_t& generation){return usb?Tab5UsbStorage::retain(generation):retainSd(generation);}
struct RetainedVolume { bool usb,held=true; ~RetainedVolume(){if(held)releaseVolume(usb);} };
VFSImpl& backing(bool usb){static VFSImpl sd,external;static bool initialized=[](){sd.mountpoint("/sd");external.mountpoint("/sd/USB");return true;}();(void)initialized;return usb?external:sd;}
const char* local(const char* p){return p[4]?p+4:"/";}
fs::FileImplPtr openedFile(fs::FileImplPtr file){
 // Arduino VFS returns a non-null implementation even when fopen/opendir
 // failed. Only a live handle owns a volume reference: a false File kept by
 // an application must not permanently prevent recovery or card removal.
 if(file&&bool(*file))return file;
 const int error=errno;file.reset();errno=error;return {};
}
class VolumeFile:public fs::FileImpl {
 fs::FileImplPtr file;uint32_t generation;String full;bool usb;bool changed=false;
 bool ok()const{if(!file){errno=EBADF;return false;}return usb?Tab5UsbStorage::valid(generation):validSd(generation);}
 public:
 VolumeFile(fs::FileImplPtr f,uint32_t g,bool u,bool writable=false):file(f),generation(g),full(u?String("/USB")+(strcmp(f->path(),"/")?f->path():""):String(f->path())),usb(u),changed(writable){}
 ~VolumeFile(){close();}
 size_t write(const uint8_t* b,size_t n)override{size_t wrote=ok()?file->write(b,n):0;if(wrote)changed=true;return wrote;}
 size_t read(uint8_t* b,size_t n)override{return ok()?file->read(b,n):0;}
 void flush()override{if(ok())file->flush();}
 bool seek(uint32_t p,fs::SeekMode m)override{return ok()&&file->seek(p,m);}
 size_t position()const override{return ok()?file->position():0;}
 size_t size()const override{return ok()?file->size():0;}
 bool setBufferSize(size_t n)override{return ok()&&file->setBufferSize(n);}
 void close()override{if(file){file->close();file.reset();releaseVolume(usb);if(changed){++fsRevision;changed=false;}}}
 time_t getLastWrite()override{return ok()?file->getLastWrite():0;}
 const char* path()const override{return full.c_str();}
 const char* name()const override{return file?file->name():"";}
 bool isDirectory()override{return ok()&&file->isDirectory();}
 fs::FileImplPtr openNextFile(const char* mode)override{
  Tab5StorageUse use(true);uint32_t g;
  if(!use&&!tab5StorageExclusiveOwner()){errno=EBUSY;return {};}
  if(!ok()||!retainVolume(usb,g))return {};
  RetainedVolume retained{usb};
  auto next=openedFile(file->openNextFile(mode));if(!next)return {};
  auto wrapped=std::make_shared<VolumeFile>(next,g,usb,mode&&strpbrk(mode,"wa+"));retained.held=false;return wrapped;
 }
 bool seekDir(long p)override{return ok()&&file->seekDir(p);}
 String getNextFileName()override{return getNextFileName(nullptr);}
 String getNextFileName(bool* dir)override{if(!ok())return {};String n=file->getNextFileName(dir);return usb&&n.length()?String("/USB")+n:n;}
 void rewindDirectory()override{if(ok())file->rewindDirectory();}
 operator bool()override{return ok()&&bool(*file);}
};
struct Access {
 Tab5StorageUse use{true};bool usb=false,held=false;uint32_t generation=0;
 char shortPath[264]{};std::unique_ptr<char[]> longPath;char* resolved=shortPath;
 explicit Access(const char* p){
  if(!p){errno=EINVAL;return;}
  // Match the migration walker capacity. Ordinary UI paths allocate nothing;
  // deep Git/media paths use one temporary buffer, owned separately per argument.
  const size_t length=strnlen(p,1024);
  if(length>=1024){errno=ENAMETOOLONG;return;}
  size_t capacity=sizeof(shortPath);
  if(length>=capacity){longPath.reset(new(std::nothrow) char[length+1]);if(!longPath){errno=ENOMEM;return;}resolved=longPath.get();capacity=length+1;}
  if(!tab5CanonicalSystemPathBuffer(resolved,capacity,p,length,resolved,capacity)){errno=EINVAL;return;}
  usb=Tab5UsbStorage::path(resolved);
  if(!use&&(usb||!tab5StorageExclusiveOwner())){errno=EBUSY;return;}
  held=retainVolume(usb,generation);
 }
 ~Access(){if(held)releaseVolume(usb);}
 const char* name()const{return usb?local(resolved):resolved;}
};
class Routes:public fs::FSImpl {
 public:
 fs::FileImplPtr open(const char* p,const char* mode,bool create)override{
  Access a(p);if(!a.held)return {};auto f=openedFile(backing(a.usb).open(a.name(),mode,create));
  if(!f)return f;auto wrapped=std::make_shared<VolumeFile>(f,a.generation,a.usb,mode&&strpbrk(mode,"wa+"));a.held=false;return wrapped;
 }
 bool exists(const char* p)override{Access a(p);return a.held&&backing(a.usb).exists(a.name());}
 bool remove(const char* p)override{Access a(p);bool ok=a.held&&(!a.usb||strcmp(a.name(),"/"))&&backing(a.usb).remove(a.name());if(ok)++fsRevision;return ok;}
 bool mkdir(const char* p)override{Access a(p);bool ok=a.held&&backing(a.usb).mkdir(a.name());if(ok)++fsRevision;return ok;}
 bool rmdir(const char* p)override{Access a(p);bool ok=a.held&&(!a.usb||strcmp(a.name(),"/"))&&backing(a.usb).rmdir(a.name());if(ok)++fsRevision;return ok;}
 bool rename(const char* p,const char* q)override{Access a(p),b(q);if(a.usb!=b.usb){errno=EXDEV;return false;}if(a.usb&&(!strcmp(a.name(),"/")||!strcmp(b.name(),"/"))){errno=EBUSY;return false;}bool ok=a.held&&b.held&&backing(a.usb).rename(a.name(),b.name());if(ok)++fsRevision;return ok;}
};
}
fs::FS tab5FS(std::make_shared<Routes>());
unsigned tab5SdOpenFiles(){return sdRefs.load(std::memory_order_acquire);}
void tab5SdSetAvailable(bool available){sdAvailable.store(available,std::memory_order_release);if(!available)++sdGeneration;}
bool tab5SdAvailable(){return sdAvailable.load(std::memory_order_acquire);}
uint32_t tab5FsRevision(){return fsRevision.load(std::memory_order_acquire);}
uint32_t tab5SdGeneration(){return sdGeneration.load(std::memory_order_acquire);}
void tab5FsChanged(){++fsRevision;}
