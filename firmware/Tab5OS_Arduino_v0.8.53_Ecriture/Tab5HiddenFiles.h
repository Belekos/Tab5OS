#pragma once
#include "Tab5FilePolicy.h"
#include "Tab5AtomicFile.h"
#include "Tab5Json.h"
#include "Tab5JsonFile.h"
#include "Tab5UsbStorage.h"
#include <ArduinoJson.h>

// Per-volume visibility preferences. Hiding never renames an application,
// document or directory, and therefore never breaks its stored paths.
namespace Tab5HiddenFiles {
struct Cache {
  JsonDocument document{tab5JsonAllocator()};
  uint32_t generation=0;
  bool loaded=false,valid=false;
};
inline Cache cards[2];
inline const char* registry(bool usb){return usb?"/USB/.tab5-hidden.json":"/.tab5-hidden.json";}
inline bool load(bool usb){
  auto& c=cards[usb?1:0];
  const uint32_t generation=usb?Tab5UsbStorage::revision():tab5SdGeneration();
  if(c.loaded&&c.generation==generation)return c.valid;
  c.document.clear();c.loaded=true;c.generation=generation;c.valid=false;
  if(usb?!Tab5UsbStorage::mounted():!tab5SdAvailable()){c.loaded=false;return false;}
  Tab5StorageUse lease(true);if(!lease){c.loaded=false;return false;}
  if(!tab5AtomicRecover(registry(usb)))return false;
  errno=0;File f=tab5FS.open(registry(usb),FILE_READ);
  if(!f){c.valid=errno==ENOENT;return c.valid;}
  if(f.size()>262144)return false;
  Tab5JsonFileReader reader(f);
  if(deserializeJson(c.document,reader)||!c.document["items"].is<JsonObject>())return false;
  c.valid=true;return true;
}
inline bool hidden(const char* path){
  if(!path||!*path)return false;
  const char* name=strrchr(path,'/');name=name?name+1:path;
  if(path[0]!='/')return Tab5Files::hidden(name);
  std::string key;if(!Tab5Files::canonical(path,key))return true;
  const bool usb=Tab5UsbStorage::path(path);
  if(load(usb)){
    JsonVariantConst value=cards[usb?1:0].document["items"][key.c_str()];
    if(value.is<bool>())return value.as<bool>();
  }
  return Tab5Files::hidden(name);
}
inline bool set(const char* path,bool value){
  if(!path||!Tab5Files::userWritable(path)||!strcmp(path,"/USB"))return false;
  const bool usb=Tab5UsbStorage::path(path);
  if(!strcmp(path,registry(usb))||!load(usb))return false;
  std::string key;if(!Tab5Files::canonical(path,key))return false;
  Tab5StorageUse lease(true);if(!lease)return false;
  JsonDocument next(tab5JsonAllocator());next.set(cards[usb?1:0].document);
  next["version"]=1;next["items"][key.c_str()]=value;
  if(next.overflowed())return false;
  String bytes;const size_t size=measureJson(next);
  if(size>262144||!bytes.reserve(size+1)||serializeJson(next,bytes)!=size)return false;
  if(!tab5AtomicWrite(registry(usb),bytes.c_str(),bytes.length()))return false;
  cards[usb?1:0].document=std::move(next);return true;
}
}
