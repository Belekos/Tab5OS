#include "Tab5SystemPath.h"
#include "Tab5FilePolicy.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <cerrno>
#include <sys/stat.h>
static std::string lastOpen,lastStat;
static int fake_open(const char* path,int,int){lastOpen=path;return 42;}
static int fake_stat(const char* path,struct stat* s){lastStat=path;s->st_mode=0x8000;return 0;}
using mp_import_stat_t=int;
enum {MP_IMPORT_STAT_NO_EXIST,MP_IMPORT_STAT_DIR,MP_IMPORT_STAT_FILE};
#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&0xf000)==0x4000)
#endif
#define O_RDONLY 0
#define open fake_open
#define stat(...) fake_stat(__VA_ARGS__)
#include "mp_import.inc"
#undef open
#undef stat
namespace service {
#include "service_path.inc"
}
namespace archives {
#include "archive_path.inc"
}
int main(){
 char out[320],other[320];
 const struct {const char* input;const char* expected;} cases[]={
  {"/M5Writer","/System"},{"/m5wRiTeR/Prefs.JSON","/System/Prefs.JSON"},
  {"/M5WriterX/a","/M5WriterX/a"},{"/M5Writer.bak/a","/M5Writer.bak/a"},
  {"/USB/M5Writer/a","/USB/M5Writer/a"},{"/Folder/M5Writer/a","/Folder/M5Writer/a"},
  {"/System/a","/System/a"},{"/","/"}
 };
 unsigned checks=0;
 for(const auto& c:cases){
  assert(tab5SystemPath(out,sizeof(out),c.input,strlen(c.input))&&!strcmp(out,c.expected));++checks;
  assert(tab5CanonicalSystemPath(out,sizeof(out),c.input,strlen(c.input))&&!strcmp(out,c.expected));++checks;
  assert(service::path(c.input,strlen(c.input),out)&&std::string(out)==std::string("/sd")+c.expected);++checks;
  assert(archives::sdPath(c.input,out,sizeof(out))&&std::string(out)==std::string("/sd")+c.expected);++checks;
  assert(tab5SystemPythonPath(out,sizeof(out),c.input)&&std::string(out)==std::string("/sd")+c.expected);++checks;
 }
 // Alias-only utility permits all overlap directions without corrupting suffix.
 for(int offset=-12;offset<=12;++offset){
  char storage[100];memset(storage,'!',sizeof(storage));char* src=storage+24;strcpy(src,"/M5Writer/sub/File.json");
  assert(tab5SystemPath(src+offset,sizeof(storage)-24-offset,src,strlen(src)));
  assert(!strcmp(src+offset,"/System/sub/File.json"));++checks;
 }
 for(const char* path:{"/./M5Writer/./Prefs.JSON","//M5Writer//Prefs.JSON","/tmp/../M5Writer/Prefs.JSON","/USB/../M5Writer/Prefs.JSON","\\M5Writer\\Prefs.JSON"}){
  assert(tab5CanonicalSystemPath(out,sizeof(out),path,strlen(path))&&!strcmp(out,"/System/Prefs.JSON"));++checks;
  std::string canonical;assert(Tab5Files::canonical(path,canonical)&&canonical=="/system/prefs.json");++checks;
 }
 assert(tab5CanonicalSystemPath(out,sizeof(out),"/uSb/M5Writer/a",15)&&!strcmp(out,"/USB/M5Writer/a"));++checks;
 for(const char* path:{"/../M5Writer/a","/M5Writer/../../a","relative"}){
  assert(!tab5CanonicalSystemPath(out,sizeof(out),path,strlen(path)));++checks;
 }
 // General VFS normalization defers name validation to FAT; command policy
 // remains stricter. In particular, valid tilde filenames must reach FAT.
 for(const char* path:{"/M5Writer /x","/M5WRIT~1/x","/M5Writer/x?","/M5Writer/x.","/M5Writer/x:"}){
  assert(tab5CanonicalSystemPath(out,sizeof(out),path,strlen(path)));
  assert(tab5SystemPath(other,sizeof(other),path,strlen(path))&&!strcmp(out,other));++checks;
  std::string canonical;assert(!Tab5Files::canonical(path,canonical));++checks;
 }
 assert(!tab5SystemPath(out,7,"/M5Writer",9));assert(tab5SystemPath(out,8,"/M5Writer",9));checks+=2;
 assert(!tab5CanonicalSystemPath(nullptr,sizeof(out),"/",1));assert(!tab5SystemPythonPath(nullptr,sizeof(out),"/M5Writer"));checks+=2;
 std::string large(261,'x');large[0]='/';assert(!tab5CanonicalSystemPath(out,sizeof(out),large.c_str(),large.size()));++checks;
 assert(service::path("/M5Writer/from",14,out));assert(service::path("/m5writer/to",12,other));
 assert(!strcmp(out,"/sd/System/from")&&!strcmp(other,"/sd/System/to"));++checks;
 assert(!service::path("/M5Writer/../x",14,out)); // Service API deliberately rejects traversal.
 assert(!archives::sdPath("/M5Writer/../x",out,sizeof(out)));checks+=2;
 assert(tab5SystemPythonPath(out,sizeof(out),"file.py")&&!strcmp(out,"/sd/MicroPython/file.py"));++checks;
 assert(tab5SystemPythonPath(out,sizeof(out),"../M5Writer/file.py")&&!strcmp(out,"/sd/System/file.py"));++checks;
 assert(tab5SystemPythonPath(out,sizeof(out),"/sd/./m5writer/file.py")&&!strcmp(out,"/sd/System/file.py"));++checks;
 assert(tab5SystemPythonPath(out,sizeof(out),"/USB/M5Writer/file.py")&&!strcmp(out,"/sd/USB/M5Writer/file.py"));++checks;
 assert(mp_import_stat("/sd/M5Writer/lib/a.py")==MP_IMPORT_STAT_FILE&&lastStat=="/sd/System/lib/a.py");++checks;
 assert(tab5_mp_open_import("/sd/m5writer/lib/a.py")==42&&lastOpen=="/sd/System/lib/a.py");++checks;
 assert(tab5_mp_open_import("/sd/USB/M5Writer/a.py")==42&&lastOpen=="/sd/USB/M5Writer/a.py");++checks;
 assert(tab5_mp_open_import("/sd/../../outside.py")==-1);++checks;
 assert(mp_import_stat("/other/a.py")==MP_IMPORT_STAT_NO_EXIST);++checks;
 std::printf("PASS: %u actual path-helper, service, archive, native import assertions; case, overlap, USB, boundaries, traversal, separate rename buffers.\n",checks);
}
