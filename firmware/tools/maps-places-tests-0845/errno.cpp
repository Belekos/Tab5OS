#include "vfs_api.h"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <iostream>
#include <cstdlib>
#include <string>
extern std::string testRoot,failOpen,failStat,failDir;
extern int failOpenError,failStatError,failDirError,failMalloc;
extern bool clearStatErrno,failStrdup,failFlush,failSync,clearSyncErrno;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAILED "<<__LINE__<<": "<<#x<<" errno="<<errno<<"\n";std::abort();}}while(0)
static void errorIs(int expected){
 #ifdef EXPECT_FIXED
 CHECK(errno==expected);
 #else
 CHECK(errno!=expected);
 #endif
}
int main(int argc,char** argv){
 CHECK(argc==2);testRoot=argv[1];std::filesystem::create_directories(testRoot+"/fixture");std::ofstream(testRoot+"/fixture/one.txt")<<"keep";
 VFSImpl sdk;sdk.mountpoint("/sd");VFSImpl unavailable;
 errno=0;CHECK(!unavailable.open("/one","r",false));errorIs(ENODEV);
 errno=0;CHECK(!sdk.open("relative","r",false));errorIs(EINVAL);
 errno=0;CHECK(!sdk.rename("relative","/other"));errorIs(EINVAL);
 errno=0;CHECK(!sdk.remove("relative"));errorIs(EINVAL);
 // All explicit allocation branches must define ENOMEM regardless of a heap
 // implementation's errno behavior. The injected allocator leaves errno zero.
 errno=0;failMalloc=1;auto invalid=sdk.open("/fixture/one.txt","r",false);CHECK(invalid&&!bool(*invalid));errorIs(ENOMEM);invalid.reset();
 errno=0;failStrdup=true;invalid=sdk.open("/fixture/one.txt","r",false);CHECK(invalid&&!bool(*invalid));errorIs(ENOMEM);invalid.reset();failStrdup=false;
 errno=0;failMalloc=1;CHECK(!sdk.open("/fixture/one.txt","w",true));errorIs(ENOMEM);
 errno=0;failMalloc=1;CHECK(!sdk.mkdir("/fixture/another"));errorIs(ENOMEM);
 errno=0;failMalloc=1;CHECK(!sdk.remove("/fixture/one.txt"));errorIs(ENOMEM);
 errno=0;failMalloc=1;CHECK(!sdk.rename("/fixture/one.txt","/fixture/two.txt"));errorIs(ENOMEM);
 errno=0;failMalloc=2;CHECK(!sdk.rename("/fixture/one.txt","/fixture/two.txt"));errorIs(ENOMEM);
 errno=0;failMalloc=1;CHECK(!sdk.rmdir("/fixture"));errorIs(ENOMEM);
 auto directory=sdk.open("/fixture","r",false);CHECK(directory&&bool(*directory));errno=0;failMalloc=1;CHECK(!directory->openNextFile("r"));errorIs(ENOMEM);directory->close();
 auto file=sdk.open("/fixture/out.txt","w",false);CHECK(file&&bool(*file));CHECK(file->write(reinterpret_cast<const uint8_t*>("data"),4)==4);errno=0;failMalloc=1;file->size();errorIs(ENOMEM);file->close();
 errno=0;CHECK(!sdk.mkdir("/fixture/one.txt"));errorIs(EEXIST);
 // Successful stat is permitted to change errno. Keep the actual fopen failure.
 failOpen="/sd/fixture/one.txt";failOpenError=EMFILE;clearStatErrno=true;errno=0;
 invalid=sdk.open("/fixture/one.txt","r",false);CHECK(invalid&&!bool(*invalid));errorIs(EMFILE);invalid.reset();
 failOpenError=ENOSPC;errno=0;invalid=sdk.open("/fixture/one.txt","w",false);CHECK(invalid&&!bool(*invalid));errorIs(ENOSPC);invalid.reset();clearStatErrno=false;
 failOpenError=EIO;failStat=failOpen;failDir=failOpen;errno=0;invalid=sdk.open("/fixture/one.txt","r",false);CHECK(invalid&&!bool(*invalid));errorIs(EIO);invalid.reset();failStat.clear();failDir.clear();
 // A real directory's failed opendir remains its own cause, not fopen EISDIR.
 failOpen="/sd/fixture";failOpenError=EISDIR;failDir=failOpen;failDirError=EMFILE;errno=0;
 invalid=sdk.open("/fixture","r",false);CHECK(invalid&&!bool(*invalid));CHECK(errno==EMFILE);invalid.reset();failOpen.clear();failDir.clear();
 // flush remains two calls, but a later successful sync cannot hide ENOSPC.
 file=sdk.open("/fixture/out.txt","w",false);CHECK(file&&bool(*file));CHECK(file->write(reinterpret_cast<const uint8_t*>("data"),4)==4);
 failFlush=true;clearSyncErrno=true;errno=0;file->flush();errorIs(ENOSPC);failFlush=false;clearSyncErrno=false;
 failSync=true;errno=0;file->flush();CHECK(errno==EIO);failSync=false;file->close();
 // Normal mkdir-existingdir and normal data remain compatible.
 CHECK(sdk.mkdir("/fixture"));file=sdk.open("/fixture/one.txt","r",false);CHECK(file&&bool(*file));uint8_t bytes[4]{};CHECK(file->read(bytes,4)==4);CHECK(!memcmp(bytes,"keep",4));file->close();
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"physicalDeviceTested\":false}"<<std::endl;
}
