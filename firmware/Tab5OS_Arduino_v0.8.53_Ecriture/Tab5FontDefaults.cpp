#include "Tab5FontDefaults.h"
#ifdef TAB5_FONT_DEFAULTS_HOST_TEST
#include "font_test_platform.h"
#else
#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5ResourceManager.h"
#include <esp_heap_caps.h>
#endif
#include <cstring>
#include <algorithm>
namespace Tab5FontDefaults {
Result load(const char* path,Blob& destination,size_t budget){
 if(destination.data||destination.size)return Result::InvalidFont;
 if(!validPath(path))return Result::InvalidPath;
 Tab5StorageUse lease;if(!lease)return Result::Unavailable;
 auto f=tab5FS.open(path,FILE_READ);if(!f||f.isDirectory())return Result::Unavailable;
 const size_t size=f.size();
 if(size>MaxFontBytes||size>budget)return Result::TooLarge;
 if(size<4096)return Result::InvalidFont;
 Tab5Resources::Budget memory;memory.psram=memory.psramBlock=size;
 auto admission=tab5ReserveMemory(Tab5Resources::Owner::System,memory,"Font");if(!admission)return Result::NoMemory;
 auto* data=static_cast<uint8_t*>(heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!data)return Result::NoMemory;
 admission.allocated(0,size);
 size_t done=0;while(done<size){const size_t count=std::min(size_t(16384),size-done);const size_t got=f.read(data+done,count);if(got!=count){heap_caps_free(data);return Result::ReadFailure;}done+=got;}
 f.close();
 if(!validate(data,size)){heap_caps_free(data);return Result::InvalidFont;}
 destination={data,size};return Result::Ok;
}
void release(Blob& blob){heap_caps_free(blob.data);blob={};}
Result inspect(const char* path){Blob blob;const auto result=load(path,blob);release(blob);return result;}
Result checkSelectionBudget(const char* candidate,const char* other){
 Tab5StorageUse lease;if(!lease)return Result::Unavailable;
 auto bytes=[](const char* path,size_t& result,bool required){
  result=0;if(!path||!*path||!strcmp(path,StandardPath))return Result::Ok;
  if(!validPath(path))return required?Result::InvalidPath:Result::Ok;
  auto file=tab5FS.open(path,FILE_READ);if(!file||file.isDirectory())return required?Result::Unavailable:Result::Ok;
  const size_t n=file.size();
  if(n<4096)return required?Result::InvalidFont:Result::Ok;
  if(n>MaxFontBytes)return required?Result::TooLarge:Result::Ok;
  result=n;return Result::Ok;
 };
 size_t candidateBytes=0,otherBytes=0;const auto result=bytes(candidate,candidateBytes,true);if(result!=Result::Ok)return result;
 if(candidate&&other&&!strcmp(candidate,other))return Result::Ok;
 (void)bytes(other,otherBytes,false);
 return candidateBytes>MaxCustomBytes-otherBytes?Result::CombinedTooLarge:Result::Ok;
}
const char* message(Result result){
 switch(result){case Result::Ok:return "Police vérifiée";case Result::InvalidPath:return "Choisir une police TrueType .ttf";case Result::Unavailable:return "Police inaccessible";case Result::TooLarge:return "Police trop volumineuse";case Result::InvalidFont:return "Police TrueType non prise en charge ou invalide";case Result::ReadFailure:return "Lecture de la police impossible";case Result::NoMemory:return "Mémoire insuffisante pour la police";case Result::CombinedTooLarge:return "Les deux polices dépassent le budget mémoire";}
 return "Police inaccessible";
}
Sources::~Sources(){release(blobs_[0]);release(blobs_[1]);}
void Sources::begin(const char* interfacePath,const char* documentPath){
 if(started_)return;started_=true;const char* paths[2]={interfacePath,documentPath};
 for(unsigned i=0;i<2;++i){const char* path=paths[i];if(!path||!*path||!strcmp(path,StandardPath))continue;
  if(i&&paths[0]&&!strcmp(path,paths[0])){slots_[1]=slots_[0];results_[1]=results_[0];continue;}
  results_[i]=load(path,blobs_[i],MaxCustomBytes-residentBytes());
  if(results_[i]==Result::Ok)slots_[i]=int8_t(i);
 }
}
const Blob* Sources::custom(Role role)const{const int slot=slots_[static_cast<unsigned>(role)];return slot>=0?&blobs_[slot]:nullptr;}
}
