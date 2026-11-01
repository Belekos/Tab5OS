// Compile the complete production services under its supported host-test mode.
#include "Tab5PythonServices.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <sys/stat.h>
#include <dirent.h>

static std::string disk;
static unsigned directoryOpens=0, stats=0;
static std::string mapped(const char* path){
 return !strncmp(path,"/sd",3)&&(path[3]=='/'||!path[3])?disk+std::string(path+3):path;
}
extern "C" DIR* __real_opendir(const char*);
extern "C" int __real_stat(const char*,struct stat*);
extern "C" DIR* __wrap_opendir(const char* path){++directoryOpens;return __real_opendir(mapped(path).c_str());}
extern "C" int __wrap_stat(const char* path,struct stat* info){++stats;return __real_stat(mapped(path).c_str(),info);}
extern "C" int tab5_mp_cancelled(){return 0;}
extern "C" void tab5_mp_yield(){}
static void file(const char* leaf){std::ofstream(disk+"/MicroPython/"+leaf)<<"# user payload retained\n";}
static std::set<std::string> scan(){
 using namespace Tab5PythonServices;
 std::set<std::string> result;
 directoryOpens=stats=0;
 assert(beginAppScan());
 for(unsigned i=0;i<100;++i){AppInfo info;int n=nextApp(info);assert(n>=0&&n!=3);
  if(n==1)assert(result.insert(info.path).second);
  if(n==2){assert(directoryOpens==1);return result;}
 }
 assert(false);return {};
}
int main(int argc,char** argv){
 assert(argc==2);disk=argv[1];std::filesystem::create_directories(disk+"/MicroPython");
 const char* old[]={"bibliotheques.py","exemple.py","importer_cartes.py","importer_radios.py","mettre_a_jour_sd.py"};
 const char* current[]={"libraries.py","example.py","import_maps.py","import_radios.py","update_sd.py"};
 for(const char* leaf:old)file(leaf);
 file("creer_theme.py");file("create_theme.py");file("custom.py");
 auto legacy=scan();assert(legacy.size()==6);
 for(const char* leaf:old)assert(legacy.count(std::string("/MicroPython/")+leaf));
 for(const char* leaf:current)file(leaf);
 auto both=scan();assert(both.size()==6);
 for(unsigned i=0;i<5;++i){assert(!both.count(std::string("/MicroPython/")+old[i]));assert(both.count(std::string("/MicroPython/")+current[i]));assert(std::filesystem::is_regular_file(disk+"/MicroPython/"+old[i]));}
 assert(stats<=11); // five counterpart probes plus six actual returned files
 assert(both.count("/MicroPython/custom.py"));
 // A directory with the replacement name must not hide a valid legacy script.
 std::filesystem::remove(disk+"/MicroPython/libraries.py");
 std::filesystem::create_directory(disk+"/MicroPython/libraries.py");
 auto invalid=scan();assert(invalid.size()==6);
 assert(invalid.count("/MicroPython/bibliotheques.py")&&!invalid.count("/MicroPython/libraries.py"));
 Tab5PythonServices::endAppScan();
 puts("PASS: production Python scan prefers English scripts, preserves legacy files/fallback and user scripts; one directory scan, <=5 counterpart probes");
}
