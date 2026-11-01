#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "esp_heap_caps.h"

static unsigned checks=0,groups=0;
static size_t allocations=0,liveBytes=0,peakBytes=0,failures=0;
static long failAfter=-1;
static std::map<void*,size_t> tracked;
static void check(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"FAIL %u %s\n",checks,why);std::exit(1);}}
static void group(const char* name){++groups;std::printf("CASE %s\n",name);}
static bool fail(){++allocations;if(failAfter<0)return false;if(failAfter--==0){++failures;return true;}return false;}
void* heap_caps_malloc(size_t n,unsigned caps){check(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),"JSON allocations use PSRAM-only flags");if(fail())return nullptr;void* p=std::malloc(n);if(p){tracked[p]=n;liveBytes+=n;peakBytes=std::max(peakBytes,liveBytes);}return p;}
void* heap_caps_realloc(void* p,size_t n,unsigned caps){check(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),"JSON reallocations use PSRAM-only flags");if(!p)return heap_caps_malloc(n,caps);if(fail())return nullptr;auto it=tracked.find(p);check(it!=tracked.end(),"realloc owns old pointer");size_t old=it->second;void* out=std::realloc(p,n);if(out){tracked.erase(it);tracked[out]=n;liveBytes=liveBytes-old+n;peakBytes=std::max(peakBytes,liveBytes);}return out;}
void heap_caps_free(void* p){if(!p)return;auto it=tracked.find(p);check(it!=tracked.end(),"free owns live pointer");liveBytes-=it->second;tracked.erase(it);std::free(p);}
#include "Tab5LaunchDefaults.h"

struct Entry{std::string text;bool directory=false;bool shortRead=false;};
static std::map<std::string,Entry> files;
static std::map<std::string,std::vector<unsigned char>> nvs;
static bool preferencesAvailable=true,storageAvailable=true,nvsShortRead=false,nvsShortWrite=false;
static unsigned opens=0,liveFiles=0,leases=0;
static std::string lastOpen;
class File{
 Entry* entry_=nullptr;
 public:
 explicit File(Entry* entry=nullptr):entry_(entry){if(entry_)++liveFiles;}
 File(const File& other):entry_(other.entry_){if(entry_)++liveFiles;}
 ~File(){if(entry_)--liveFiles;}
 explicit operator bool()const{return entry_!=nullptr;}
 bool isDirectory()const{return entry_&&entry_->directory;}
 size_t size()const{return entry_?entry_->text.size():0;}
 size_t read(uint8_t* data,size_t count){if(!entry_)return 0;size_t n=std::min(count,entry_->text.size());if(entry_->shortRead&&n)--n;std::memcpy(data,entry_->text.data(),n);return n;}
};
#define FILE_READ 0
struct FakeFS{File open(const char* path,int){check(leases>0,"sidecar reads hold storage lease");++opens;lastOpen=path;auto found=files.find(path);return File(found==files.end()?nullptr:&found->second);}}tab5FS;
struct Tab5StorageUse{bool held;Tab5StorageUse():held(storageAvailable){if(held)++leases;}~Tab5StorageUse(){if(held)--leases;}explicit operator bool()const{return held;}};
class Preferences{
 public:
 bool begin(const char* name,bool){check(!strcmp(name,"tab5launch"),"profile namespace");return preferencesAvailable;}
 void end(){}
 bool isKey(const char* key){return nvs.count(key)!=0;}
 size_t getBytesLength(const char* key){auto it=nvs.find(key);return it==nvs.end()?0:it->second.size();}
 size_t getBytes(const char* key,void* dst,size_t count){auto it=nvs.find(key);if(it==nvs.end())return 0;size_t n=std::min(count,it->second.size());if(nvsShortRead&&n)--n;memcpy(dst,it->second.data(),n);return n;}
 size_t putBytes(const char* key,const void* src,size_t count){if(nvsShortWrite)return 0;auto* p=static_cast<const unsigned char*>(src);nvs[key]=std::vector<unsigned char>(p,p+count);return count;}
 bool remove(const char* key){return nvs.erase(key)!=0;}
};
/* PROFILE_IMPLEMENTATION */

static bool parse(const std::string& json,Tab5LaunchProfile& out){bool ok=tab5ParseLaunchDefaults(json.data(),json.size(),out);check(liveBytes==0&&tracked.empty(),"JSON document returns every tracked allocation");return ok;}
static void rejected(const std::string& json){Tab5LaunchProfile out;out.mode=Tab5LaunchMode::File;strcpy(out.argument,"/unchanged");check(!parse(json,out),"malformed or unsupported metadata rejected");check(out.mode==Tab5LaunchMode::File&&!strcmp(out.argument,"/unchanged"),"failed parse preserves prior output");}
static void accepted(const std::string& json,Tab5LaunchMode mode,const char* argument=""){Tab5LaunchProfile out;out.mode=Tab5LaunchMode::File;strcpy(out.argument,"/old");check(parse(json,out),"valid portable defaults accepted");check(out.mode==mode&&!strcmp(out.argument,argument),"parsed mode and canonical argument");}
static void clean(){check(liveFiles==0&&leases==0&&liveBytes==0,"sidecar closes file and releases storage/JSON");}
int main(){
 group("modes-and-portable-paths");
 accepted("{\"mode\":\"none\"}",Tab5LaunchMode::None);
 accepted("{\"mode\":\"ask\"}",Tab5LaunchMode::Ask);
 accepted("{\"mode\":\"ask\",\"argument\":null}",Tab5LaunchMode::Ask);
 accepted("{\"mode\":\"none\",\"argument\":\"\",\"name\":\"Pentest\"}",Tab5LaunchMode::None);
 accepted("{\"mode\":\"file\",\"argument\":\"sd:/Documents/book.epub\"}",Tab5LaunchMode::File,"/Documents/book.epub");
 accepted("{\"mode\":\"file\",\"argument\":\"USB:/Dossier/jeu.gb\"}",Tab5LaunchMode::File,"/USB/Dossier/jeu.gb");
 accepted("{\"mode\":\"file\",\"argument\":\"/usb/Dossier/jeu.gb\"}",Tab5LaunchMode::File,"/USB/Dossier/jeu.gb");
 accepted("{\"mode\":\"file\",\"argument\":\"sd:/Documents/日本語 é.txt\"}",Tab5LaunchMode::File,"/Documents/日本語 é.txt");
 accepted("{\"mode\":\"file\",\"argument\":\"usb:/\"}",Tab5LaunchMode::File,"/USB");
 group("invalid-json-and-value-types");
 for(const char* text:{"","{","[]","null","true","123","\"none\"","{}","{\"mode\":1}","{\"mode\":null}","{\"mode\":\"NONE\"}","{\"mode\":\"file\"}","{\"mode\":\"file\",\"argument\":1}","{\"mode\":\"none\",\"argument\":1}","{\"mode\":\"ask\",\"argument\":\"/hidden\"}","{\"mode\":\"none\"}garbage","{\"mode\":\"none\"}{\"mode\":\"file\"}"})rejected(text);
 group("bounds-nuls-and-traversal");
 for(const char* path:{"","relative.txt","sd:relative.txt","sd:/USB/file","usb:/../file","sd:/a//b","sd:/a/./b","sd:/a/../b","sd:/a/","sd:/a. ","sd:/bad*file","sd:/bad~file"})rejected(std::string("{\"mode\":\"file\",\"argument\":\"")+path+"\"}");
 rejected("{\"mode\":\"none\\u0000file\"}");rejected("{\"mode\":\"file\",\"argument\":\"sd:/safe\\u0000../bad\"}");
 std::string rawNul="{\"mode\":\"none\"}";rawNul.push_back(0);rawNul+="{}";rejected(rawNul);
 rejected(std::string(1025,' '));rejected("{\"mode\":\"none\",\"extra\":[[[[[[0]]]]]]}");
 std::string edge="{\"mode\":\"none\"}";edge.append(1024-edge.size(),' ');accepted(edge,Tab5LaunchMode::None);edge+=' ';rejected(edge);
 std::string sdPath="sd:/"+std::string(259,'a');accepted("{\"mode\":\"file\",\"argument\":\""+sdPath+"\"}",Tab5LaunchMode::File,sdPath.substr(3).c_str());
 rejected("{\"mode\":\"file\",\"argument\":\""+sdPath+"a\"}");
 group("allocation-failures-and-recovery");
 const std::string complex="{\"mode\":\"file\",\"argument\":\"usb:/Documents/"+std::string(120,'b')+".bin\",\"name\":\"Portable application\"}";
 const auto before=allocations;Tab5LaunchProfile sample;check(parse(complex,sample),"measure real JSON allocations");const size_t count=allocations-before;
 for(size_t point=0;point<count;++point){failAfter=long(point);Tab5LaunchProfile out;out.mode=Tab5LaunchMode::None;bool ok=tab5ParseLaunchDefaults(complex.data(),complex.size(),out);failAfter=-1;check(!ok&&out.mode==Tab5LaunchMode::None&&!out.argument[0],"injected allocation failure refuses defaults without partial profile");check(liveBytes==0&&tracked.empty(),"OOM releases complete partial JSON graph");accepted("{\"mode\":\"none\"}",Tab5LaunchMode::None);}
 group("sidecar-storage-and-usb");
 files["/Applications/Test.elf.launch.json"]={"{\"mode\":\"none\"}"};Tab5LaunchProfile out;
 check(tab5LaunchProfileLoad("sd:/Applications/Test.elf",out)&&out.mode==Tab5LaunchMode::None,"sidecar used without saved preference");check(lastOpen=="/Applications/Test.elf.launch.json","SD filename uses exact executable case");clean();
 files["/USB/Apps/Test.elf.launch.json"]={"{\"mode\":\"file\",\"argument\":\"usb:/roms/test.gb\"}"};
 check(tab5LaunchProfileLoad("usb:/Apps/Test.elf",out)&&!strcmp(out.argument,"/USB/roms/test.gb"),"USB executable and argument use volume notation");clean();
 for(int kind=0;kind<4;++kind){auto& f=files["/Applications/Test.elf.launch.json"];f={"{\"mode\":\"none\"}"};if(kind==0)f.text="";if(kind==1)f.text=std::string(1025,' ');if(kind==2)f.directory=true;if(kind==3)f.shortRead=true;check(!tab5LaunchProfileLoad("sd:/Applications/Test.elf",out)&&out.mode==Tab5LaunchMode::Ask,"invalid or short sidecar leaves ordinary dialog");clean();}
 files["/Applications/Test.elf.launch.json"]={"{\"mode\":\"none\"}"};storageAvailable=false;check(!tab5LaunchProfileLoad("sd:/Applications/Test.elf",out),"unavailable storage does not open sidecar");storageAvailable=true;clean();
 group("user-preferences-precede-sidecar");
 Tab5LaunchProfile saved;saved.mode=Tab5LaunchMode::Ask;check(tab5LaunchProfileSave("SD:/Applications/Test.elf",saved),"save explicit ask");const auto beforeOpen=opens;
 check(tab5LaunchProfileLoad("sd:/Applications/Test.elf",out)&&out.mode==Tab5LaunchMode::Ask&&opens==beforeOpen,"saved ask overrides packaged no-file and avoids storage");
 saved.mode=Tab5LaunchMode::File;strcpy(saved.argument,"usb:/user/selected.bin");check(tab5LaunchProfileSave("sd:/Applications/Test.elf",saved),"save user argument");
 check(tab5LaunchProfileLoad("sd:/applications/test.ELF",out)&&out.mode==Tab5LaunchMode::File&&!strcmp(out.argument,"/USB/user/selected.bin"),"case-folded executable identity keeps user file override");
 auto snapshot=nvs;auto& bytes=nvs.begin()->second;bytes[0]^=1;
 check(!tab5LaunchProfileLoad("sd:/Applications/Test.elf",out)&&out.mode==Tab5LaunchMode::Ask&&opens==beforeOpen,"corrupt stored preference does not silently launch packaged defaults");nvs=snapshot;
 nvsShortRead=true;check(!tab5LaunchProfileLoad("sd:/Applications/Test.elf",out)&&out.mode==Tab5LaunchMode::Ask,"short stored record leaves dialog");nvsShortRead=false;
 check(tab5LaunchProfileForget("sd:/Applications/Test.elf"),"forget saved user preference");check(tab5LaunchProfileLoad("sd:/Applications/Test.elf",out)&&out.mode==Tab5LaunchMode::None,"forget restores package defaults");clean();
 group("repeat-parse-and-resource-release");
 for(unsigned i=0;i<2000;++i){accepted(i%2?"{\"mode\":\"ask\"}":"{\"mode\":\"none\"}",i%2?Tab5LaunchMode::Ask:Tab5LaunchMode::None);}
 clean();std::printf("PASS %u launch-defaults checks; groups=%u; cycles=2000; allocation_failures=%zu; retained_bytes=%zu; peak_bytes=%zu; ArduinoJson=%s\n",checks,groups,failures,liveBytes,peakBytes,ARDUINOJSON_VERSION);
}
