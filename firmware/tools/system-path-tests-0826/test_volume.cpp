#include "Tab5VolumeFS.h"
#include "Tab5UsbStorage.h"
#include "vfs_api.h"
#include <cassert>
#include <cstdio>
int main(){
 const uint8_t data[]={1,2,3,4,5};uint8_t read[5]{};
 {auto f=tab5FS.open("/M5Writer/File.bin","w");assert(f);assert(FakeVolume::lastPath=="/sd/System/File.bin");assert(!strcmp(f->path(),"/System/File.bin"));assert(f->write(data,5)==5);}
 assert(tab5SdOpenFiles()==0);
 {auto f=tab5FS.open("/./m5writer/File.bin");assert(f);assert(f->read(read,5)==5&&!memcmp(data,read,5));}
 assert(tab5FS.exists("/tmp/../M5Writer/File.bin"));
 assert(tab5FS.rename("/M5Writer/File.bin","/m5writer/Other.bin"));
 assert(FakeVolume::renameFrom=="/sd/System/File.bin"&&FakeVolume::renameTo=="/sd/System/Other.bin");
 {auto f=tab5FS.open("/System/Other.bin");assert(f&&f->read(read,5)==5&&!memcmp(data,read,5));}
 assert(tab5FS.remove("/M5Writer/Other.bin"));assert(!tab5FS.exists("/System/Other.bin"));
 assert(tab5FS.mkdir("/M5Writer/folder"));assert(FakeVolume::lastPath=="/sd/System/folder");
 assert(tab5FS.rmdir("/m5writer/folder"));
 {auto f=tab5FS.open("/USB/M5Writer/U.bin","w");assert(f&&f->write(data,5)==5);assert(FakeVolume::lastPath=="/sd/USB/M5Writer/U.bin");assert(Tab5UsbStorage::references==1);assert(tab5SdOpenFiles()==0);}
 {auto f=tab5FS.open("/usb/M5Writer/U.bin");assert(f&&f->read(read,5)==5&&!memcmp(read,data,5));assert(Tab5UsbStorage::references==1);}
 assert(!tab5FS.rename("/USB/M5Writer/U.bin","/M5Writer/U.bin"));
 {auto f=tab5FS.open("/M5WriterX/File","w");assert(f);assert(FakeVolume::lastPath=="/sd/M5WriterX/File");}
 {auto f=tab5FS.open("/USB/../M5Writer/SD.bin","w");assert(f);assert(FakeVolume::lastPath=="/sd/System/SD.bin");assert(tab5SdOpenFiles()==1);assert(Tab5UsbStorage::references==0);}
 assert(!tab5FS.open("/../../outside","w"));assert(!tab5FS.open(nullptr,"w"));
 assert(!tab5FS.rename("/USB/../M5Writer/SD.bin","/USB"));
 const std::string deep=std::string(190,'a')+"/"+std::string(190,'b')+"/"+std::string(190,'c')+"/file.bin";
 const std::string legacy="/M5Writer/"+deep,modern="/System/"+deep;
 {auto f=tab5FS.open(legacy.c_str(),"w");assert(f&&f->write(data,5)==5);assert(FakeVolume::lastPath=="/sd"+modern);}
 {auto f=tab5FS.open(modern.c_str());assert(f&&f->read(read,5)==5&&!memcmp(data,read,5));}
 assert(tab5FS.rename(legacy.c_str(),(modern+".renamed").c_str()));
 assert(FakeVolume::renameFrom=="/sd"+modern&&FakeVolume::renameTo=="/sd"+modern+".renamed");
 assert(tab5FS.remove(("/tmp/../M5Writer/"+deep+".renamed").c_str()));
 {auto f=tab5FS.open(("/USB/M5Writer/"+deep).c_str(),"w");assert(f);assert(FakeVolume::lastPath=="/sd/USB/M5Writer/"+deep);}
 {auto f=tab5FS.open("/M5Writer/~backup.dat","w");assert(f);assert(FakeVolume::lastPath=="/sd/System/~backup.dat");}
 std::string boundary="/System";while(boundary.size()+201<1023)boundary+="/"+std::string(200,'x');boundary+="/"+std::string(1022-boundary.size(),'y');
 assert(boundary.size()==1023);{auto f=tab5FS.open(boundary.c_str(),"w");assert(f);}
 assert(!tab5FS.open((boundary+"z").c_str(),"w"));assert(errno==ENAMETOOLONG);
 assert(tab5SdOpenFiles()==0&&Tab5UsbStorage::references==0);
 std::puts("PASS: production VolumeFS open/read/write/exists/rename/remove/mkdir/rmdir; SD alias, USB retained, case, dot segments, independent rename buffers, deep paths through 1023 bytes and zero references.");
}
