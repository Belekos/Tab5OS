#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5Favorites.h"
#include "Tab5Json.h"
#include <SD.h>
#include <vector>
#include <algorithm>
namespace Tab5Favorites {namespace {
std::vector<String> entries[2];bool loaded=false;
constexpr const char* path="/System/audio_favorites.json";
bool load(){
 if(loaded)return true;File f=tab5FS.open(tab5FS.exists(path)?String(path):String(path)+".bak");if(!f){if(tab5FS.exists(path))return false;loaded=true;return true;}
 if(f.size()>262144)return false;JsonDocument doc(tab5JsonAllocator());if(deserializeJson(doc,f))return false;
 for(unsigned group=0;group<2;++group){auto array=doc[group?"radios":"tracks"].as<JsonArray>();if(array.size()>128)return false;for(JsonVariant v:array){const char* text=v|"";if(!*text||strlen(text)>1024)return false;}}
 for(unsigned group=0;group<2;++group)for(JsonVariant v:doc[group?"radios":"tracks"].as<JsonArray>())entries[group].emplace_back(v.as<const char*>());
 loaded=true;return true;
}
bool save(){
 JsonDocument doc(tab5JsonAllocator());doc["version"]=1;for(unsigned g=0;g<2;++g){auto array=doc[g?"radios":"tracks"].to<JsonArray>();for(auto& id:entries[g])array.add(id);}
 if(doc.overflowed())return false;String temp=String(path)+".tmp",backup=String(path)+".bak";if(tab5FS.exists(temp)&&!tab5FS.remove(temp)){tab5StorageFault("favoris temporaire",temp.c_str());return false;}
 errno=0;File f=tab5FS.open(temp,FILE_WRITE);if(!f){tab5StorageFault("favoris ouverture",temp.c_str());return false;}size_t expected=measureJson(doc),written=serializeJson(doc,f);f.flush();f.close();
 File check=tab5FS.open(temp);bool valid=check&&check.size()==expected;check.close();
 if(written!=expected||!valid){tab5StorageFault("favoris écriture",temp.c_str());tab5FS.remove(temp);return false;}
 bool existed=tab5FS.exists(path);if(existed){if(tab5FS.exists(backup)&&!tab5FS.remove(backup)){tab5FS.remove(temp);return false;}if(!tab5FS.rename(path,backup)){tab5StorageFault("favoris sauvegarde",path);tab5FS.remove(temp);return false;}}
 if(!tab5FS.rename(temp,path)){tab5StorageFault("favoris publication",path);if(existed)tab5FS.rename(backup,path);tab5FS.remove(temp);return false;}return true;
}
}
bool contains(bool radio,const String& id){if(!load())return false;auto& values=entries[radio];return std::find(values.begin(),values.end(),id)!=values.end();}
bool toggle(bool radio,const String& id){if(!id.length()||id.length()>1024||!load())return false;auto& values=entries[radio];auto it=std::find(values.begin(),values.end(),id);bool had=it!=values.end();size_t index=it-values.begin();if(!had&&values.size()>=128)return false;if(had)values.erase(it);else values.push_back(id);if(save())return true;if(had)values.insert(values.begin()+index,id);else values.pop_back();return false;}
}
