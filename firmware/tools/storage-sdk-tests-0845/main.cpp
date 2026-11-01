#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "vfs_api.h"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <iostream>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <string>
extern std::string testRoot,failOpen;
extern int failOpenError,failMalloc;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<" errno="<<errno<<" refs="<<tab5SdOpenFiles()<<"\n";std::abort();}}while(0)
void heap_caps_free(void* p){free(p);}
class Tab5MapEngine {public:File _offlineFile;void*_offlineDirectory=nullptr,*_offlineIndexBlock=nullptr;unsigned _offlineIndexCapacity=0;int _offlineActivePack=-1;uint32_t _offlineActiveBlock=UINT32_MAX;void closeOfflinePack();};
// MAP_CLOSE
int main(int argc,char** argv){
 CHECK(argc==2);testRoot=argv[1];std::filesystem::create_directories(testRoot+"/fixture");
 std::ofstream(testRoot+"/fixture/one.txt")<<"Tab5 data preserved";
 CHECK(tab5SdOpenFiles()==0);
 // Establish SDK semantics directly: pointer exists while implementation is false.
 VFSImpl sdk;sdk.mountpoint("/sd");errno=0;auto invalid=sdk.open("/does-not-exist","r",false);
 CHECK(invalid);CHECK(!bool(*invalid));CHECK(errno==ENOENT);invalid.reset();
 // A failed persistent application File must not hold a phantom SD reference.
 File persistent=tab5FS.open("/does-not-exist");CHECK(!persistent);
 unsigned missingRefs=tab5SdOpenFiles();bool missingBlocks=false;
 {Tab5StorageRecovery exclusive;CHECK(exclusive);missingBlocks=tab5SdOpenFiles()!=0;}
 #ifdef EXPECT_FIXED
 CHECK(missingRefs==0);CHECK(!missingBlocks);
 #else
 CHECK(missingRefs==1);CHECK(missingBlocks);
 #endif
 persistent.close();CHECK(tab5SdOpenFiles()==0);
 // An actual fopen EMFILE failure on an existing regular file. Stat succeeds;
 // the real VFSFileImpl stays false while retaining its allocated path only.
 failOpen="/sd/fixture/one.txt";failOpenError=EMFILE;errno=0;
 persistent=tab5FS.open("/fixture/one.txt");CHECK(!persistent);CHECK(errno==EMFILE);
 #ifdef EXPECT_FIXED
 CHECK(tab5SdOpenFiles()==0);
 #else
 CHECK(tab5SdOpenFiles()==1);
 #endif
 persistent.close();failOpen.clear();CHECK(tab5SdOpenFiles()==0);
 // Constructor allocation failure is another false/non-null SDK object, not a
 // live descriptor. Do not count it or fabricate a transport error here.
 failMalloc=1;errno=0;persistent=tab5FS.open("/fixture/one.txt");CHECK(!persistent);
 #ifdef EXPECT_FIXED
 CHECK(tab5SdOpenFiles()==0);
 #else
 CHECK(tab5SdOpenFiles()==1);
 #endif
 persistent.close();CHECK(tab5SdOpenFiles()==0);
 // Failed directory child open follows the same ownership rule. Keep the false
 // File alive, as application iterator state can, until after recovery gating.
 File directory=tab5FS.open("/fixture");CHECK(directory&&directory.isDirectory());
 failOpen="/sd/fixture/one.txt";File entry;
 for(unsigned i=0;i<20;++i){entry=directory.openNextFile();if(!entry)break;entry.close();}
 CHECK(!entry);directory.close();
 #ifdef EXPECT_FIXED
 CHECK(tab5SdOpenFiles()==0);
 #else
 CHECK(tab5SdOpenFiles()==1);
 #endif
 entry.close();failOpen.clear();CHECK(tab5SdOpenFiles()==0);
 // Unconditional owner close works even after the volume generation invalidates
 // a previously live File. Actual SDK File::close does not test operator bool.
 Tab5MapEngine map;map._offlineFile=tab5FS.open("/fixture/one.txt");CHECK(map._offlineFile);
 tab5SdSetAvailable(false);CHECK(!map._offlineFile);map.closeOfflinePack();
 unsigned invalidatedRefs=tab5SdOpenFiles();
 #ifdef EXPECT_FIXED
 CHECK(invalidatedRefs==0);
 #else
 CHECK(invalidatedRefs==1);
 #endif
 map._offlineFile.close();tab5SdSetAvailable(true);CHECK(tab5SdOpenFiles()==0);
 // Real FS/VFS round trips, shared ownership and mutation revision. File::close
 // intentionally closes aliases too; destruction alone retains shared aliases.
 for(unsigned n=0;n<1000;++n){
  File file=tab5FS.open("/fixture/one.txt");CHECK(file);File alias=file;
  file=File();CHECK(tab5SdOpenFiles()==1);uint8_t bytes[32]{};CHECK(alias.read(bytes,4)==4);CHECK(!memcmp(bytes,"Tab5",4));
  alias.close();CHECK(tab5SdOpenFiles()==0);
 }
 const auto rev=tab5FsRevision();File output=tab5FS.open("/fixture/new.txt","w");CHECK(output);
 CHECK(output.write(reinterpret_cast<const uint8_t*>("new"),3)==3);output.flush();output.close();CHECK(tab5FsRevision()==rev+1);
 File input=tab5FS.open("/fixture/new.txt");CHECK(input&&input.size()==3);input.close();
 CHECK(tab5FS.rename("/fixture/new.txt","/fixture/renamed.txt"));CHECK(tab5FS.remove("/fixture/renamed.txt"));
 CHECK(tab5SdOpenFiles()==0);
 // Concurrent filesystem metadata operations use actual SDK recursive mutexes.
 std::atomic<unsigned> cycles{0};std::thread first([&]{for(unsigned i=0;i<200;++i){auto f=tab5FS.open("/fixture/one.txt");if(f)++cycles;}});
 std::thread second([&]{for(unsigned i=0;i<200;++i){auto f=tab5FS.open("/fixture/one.txt");if(f)++cycles;}});first.join();second.join();
 CHECK(cycles==400);CHECK(tab5SdOpenFiles()==0);
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"missingPersistentRefs\":"<<missingRefs<<",\"missingBlocksRecovery\":"<<(missingBlocks?"true":"false")<<",\"invalidatedMapRefsAfterOwnerClose\":"<<invalidatedRefs<<",\"repeatedCycles\":1000,\"concurrentCycles\":400,\"remainingRefs\":"<<tab5SdOpenFiles()<<"}\n";
}
