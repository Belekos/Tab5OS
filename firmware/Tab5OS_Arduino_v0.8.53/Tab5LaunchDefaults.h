#pragma once
#include "Tab5LaunchProfile.h"
#include "Tab5Json.h"

// Optional portable defaults beside an ELF. User-saved preferences take
// precedence. Malformed metadata always leaves the ordinary launch dialog.
inline bool tab5ParseLaunchDefaults(const char* data,size_t length,Tab5LaunchProfile& result){
 if(!data||!length||length>1024||memchr(data,0,length))return false;
 JsonDocument doc(tab5JsonAllocator());
 if(deserializeJson(doc,data,length,DeserializationOption::NestingLimit(4))||!doc.is<JsonObject>())return false;
 JsonString mode=doc["mode"].as<JsonString>();
 if(mode.isNull()||strlen(mode.c_str())!=mode.size())return false;
 Tab5LaunchProfile next;
 if(!strcmp(mode.c_str(),"none"))next.mode=Tab5LaunchMode::None;
 else if(!strcmp(mode.c_str(),"ask"))next.mode=Tab5LaunchMode::Ask;
 else if(!strcmp(mode.c_str(),"file"))next.mode=Tab5LaunchMode::File;
 else return false;
 if(next.mode==Tab5LaunchMode::File){
  JsonString argument=doc["argument"].as<JsonString>();
  if(argument.isNull()||argument.size()>265||strlen(argument.c_str())!=argument.size()||!tab5LaunchPath(argument.c_str(),next.argument,sizeof(next.argument)))return false;
 }else if(!doc["argument"].isNull()){
  JsonString argument=doc["argument"].as<JsonString>();if(argument.isNull()||argument.size())return false;
 }
 result=next;return true;
}
