#define ARDUINOJSON_ENABLE_ARDUINO_STRING 0
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_STD_STRING 0
#define ARDUINOJSON_ENABLE_STD_STREAM 0
#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5Json.h"
#include "vfs_api.h"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unistd.h>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAILED "<<__LINE__<<" "<<#x<<" errno="<<errno<<" refs="<<tab5SdOpenFiles()<<"\n";std::abort();}}while(0)
extern std::string testRoot,failOpen;extern int failRead,failStat,failClose,failPublish;extern unsigned writeOpens,renames,removes;
static bool failJson=false;
static std::unordered_map<void*,size_t> jsonAllocations;
void* heap_caps_malloc(size_t n,unsigned){if(failJson)return nullptr;void*p=malloc(n);if(p)jsonAllocations[p]=n;return p;}
void* heap_caps_realloc(void*p,size_t n,unsigned){if(failJson)return nullptr;void*q=realloc(p,n);if(q){jsonAllocations.erase(p);jsonAllocations[q]=n;}return q;}
void heap_caps_free(void*p){jsonAllocations.erase(p);free(p);}
static constexpr size_t MAP_SD_IO_CHUNK=32768,MAX_PLACES_BYTES=65536,TAB5_MAP_MAX_PLACES=64;
static constexpr char MAP_PLACES_FILE[]="/System/map_places.json";
void delay(unsigned){}template<class T>T constrain(T a,T b,T c){return std::clamp(a,b,c);}
namespace Tab5GeoIndex{bool validCoordinate(double a,double b){return std::isfinite(a)&&std::isfinite(b)&&a>=-85.051129&&a<=85.051129&&b>=-180&&b<=180;}}
// HELPERS
struct Tab5MapPlace{char name[64]{};double latitude=0,longitude=0;uint8_t zoom=0;};
class Tab5MapEngine{public:bool _ready=true,_sdAvailable=true,_placesLoaded=false;size_t _placeCount=0;uint32_t _localMarkerRevision=1;Tab5MapPlace memory[TAB5_MAP_MAX_PLACES]{};Tab5MapPlace*_places=memory;
 bool loadPlaces();bool savePlaces();bool addPlace(const String&,double,double,uint8_t);bool renamePlace(size_t,const String&);bool removePlace(size_t);
};
// METHODS
static const char initial[]="{\"places\":[{\"name\":\"Alice\",\"lat\":47.7,\"lon\":-3.3,\"zoom\":12},{\"name\":\"Bob\",\"lat\":48.1,\"lon\":-3.8,\"zoom\":9}]}";
void put(const char*text,const char*name=MAP_PLACES_FILE){std::ofstream f(testRoot+name);f<<text;CHECK(bool(f));}
std::string get(const char*name=MAP_PLACES_FILE){std::ifstream f(testRoot+name);return std::string(std::istreambuf_iterator<char>(f),{});}
void reset(){CHECK(tab5SdOpenFiles()==0);CHECK(jsonAllocations.empty());std::filesystem::remove_all(testRoot);std::filesystem::create_directories(testRoot+"/System");failOpen.clear();failRead=failStat=failClose=failPublish=0;failJson=false;writeOpens=renames=removes=0;tab5SdSetAvailable(true);}
int main(int argc,char**argv){CHECK(argc==2);testRoot=argv[1];reset();
 // SCENARIOS
 CHECK(!tab5SdOpenFiles());CHECK(jsonAllocations.empty());std::filesystem::remove_all(testRoot);
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"physicalDeviceTested\":false,\"handlesAfter\":"<<tab5SdOpenFiles()<<",\"jsonAllocationsAfter\":"<<jsonAllocations.size()<<"}\n";
}
