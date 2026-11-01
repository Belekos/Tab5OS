#include "Tab5FileOperation.h"
#include "io_shim.h"
#undef open
#undef read
#undef write
#undef fsync
#undef mkdir
#include <algorithm>
#include <cstdarg>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
int testStorageUsers = 0;
Fault testFault = None;
unsigned testReads = 0, testWrites = 0, testSyncs = 0;
std::string mountPath, fixtureSource;
int checks = 0;
void require(bool condition,const char* name) { ++checks; if (!condition) { std::cerr << "FAIL: " << name << "\n"; std::exit(1); } }
int testOpen(const char* path,int flags,...) {
  require(testStorageUsers > 0,"storage lease held for I/O");
  unsigned mode = 0;
  if (flags & O_CREAT) { va_list a; va_start(a,flags); mode=va_arg(a,int); va_end(a); }
#ifdef _WIN32
  flags |= O_BINARY;
#endif
  if (testFault == LateCollision && (flags & O_EXCL)) {
    int collision = ::open(path,flags,mode); require(collision >= 0,"late collision fixture");
    ::write(collision,"KEEP",4); ::close(collision);
  }
  return ::open(path,flags,mode);
}
ssize_t testRead(int fd,void* data,size_t size) {
  ++testReads;
  if (testFault == EarlyRead && testReads == 2) return 0;
  return ::read(fd,data,size);
}
ssize_t testWrite(int fd,const void* data,size_t size) {
  ++testWrites;
  if (testFault == WriteError && testWrites == 2) { errno=ENOSPC;return -1; }
  if (testFault == ShortWrite && size > 1) size = std::max(size_t(1),size / 3);
  if (testFault == CorruptWrite && testWrites == 1) {
    std::vector<unsigned char> corrupted(static_cast<const unsigned char*>(data),static_cast<const unsigned char*>(data)+size);
    corrupted[0] ^= 0x55;
    return ::write(fd,corrupted.data(),size);
  }
  return ::write(fd,data,size);
}
int testSync(int fd) {
  ++testSyncs;
  if (testFault == SyncError) { errno=EIO;return -1; }
#ifdef _WIN32
  return _commit(fd);
#else
  return ::fsync(fd);
#endif
}
int testMkdir(const char* path,unsigned mode) {
#ifdef _WIN32
  (void)mode; return _mkdir(path);
#else
  return ::mkdir(path,mode);
#endif
}
void writeFile(const std::string& path,const std::string& data) { std::ofstream f(path,std::ios::binary);f.write(data.data(),data.size());require(bool(f),"fixture write"); }
std::string readFile(const std::string& path) { std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}}; }
void testTick() {
  if (testFault == NewChild && testWrites == 1) {
    writeFile(fixtureSource+"/new", "NEW");testFault = None;
  }
  if (testFault == Cancelled && testWrites) Tab5FileOperation::cancel();
  if (testFault == ChangeSource && testWrites == 1) {
    writeFile(fixtureSource,std::string(33001,'X'));
    testFault = None;
  }
}
Tab5FileOperation::Completion run(const std::string& from,const std::string& to,bool move,Fault fault) {
  testFault=fault;testReads=testWrites=testSyncs=0;fixtureSource=mountPath+from;
  require(Tab5FileOperation::startCopy(from.c_str(),to.c_str(),move),"job accepted");
  require(!Tab5FileOperation::busy(),"job completes");
  Tab5FileOperation::Completion result;
  require(Tab5FileOperation::takeCompletion(result),"completion delivered");
  require(!Tab5FileOperation::takeCompletion(result),"completion delivered once");
  require(testStorageUsers == 0,"lease released on every exit");
  return result;
}
int main(int argc,char** argv) {
  require(argc==2,"mount argument");mountPath=argv[1];
  std::string contents;for(unsigned i=0;i<33000;++i)contents += char(i%251);
  const Fault faults[]={None,EarlyRead,WriteError,ShortWrite,SyncError,CorruptWrite,Cancelled,ChangeSource};
  for (unsigned i=0;i<sizeof(faults)/sizeof(faults[0]);++i) {
    std::string src="/source"+std::to_string(i),dst="/target"+std::to_string(i);
    writeFile(mountPath+src,contents);
    auto result=run(src,dst,true,faults[i]);
    require(result.success == (faults[i]==None || faults[i]==ShortWrite),"only complete verified copies succeed");
    require(!result.moved && result.sourceRetained,"fallback move never deletes source");
    require(result.destinationPresent,"partial destination explicitly reported");
    require(result.cancelled == (faults[i]==Cancelled),"cancellation explicit");
    if(faults[i]!=ChangeSource)require(readFile(mountPath+src)==contents,"original intact after I/O fault");
    if(result.success)require(readFile(mountPath+dst)==contents,"verified destination exact");
  }
  writeFile(mountPath+"/collision-source",contents);writeFile(mountPath+"/collision-destination","KEEP");
  auto collision=run("/collision-source","/collision-destination",false,None);
  require(!collision.success && collision.error==EEXIST,"existing destination rejected");
  require(readFile(mountPath+"/collision-destination")=="KEEP","collision never overwritten");
  require(!collision.destinationPresent,"existing destination not marked job-owned");
  auto late=run("/collision-source","/late-collision",false,LateCollision);
  require(!late.success && late.error==EEXIST,"concurrent destination rejected by O_EXCL");
  require(readFile(mountPath+"/late-collision")=="KEEP","concurrent destination intact");
  require(!late.destinationPresent,"concurrent destination not job-owned");
  testMkdir((mountPath+"/changing-tree").c_str(),0777);
  writeFile(mountPath+"/changing-tree/a",contents);
  auto changed=run("/changing-tree","/changed-tree-copy",true,NewChild);
  require(!changed.success && changed.error==ESTALE,"new directory entry detected");
  require(readFile(mountPath+"/changing-tree/new")=="NEW","new original entry preserved");
  testMkdir((mountPath+"/tree").c_str(),0777);testMkdir((mountPath+"/tree/sub").c_str(),0777);
  writeFile(mountPath+"/tree/a",contents);writeFile(mountPath+"/tree/sub/empty","");
  auto tree=run("/tree","/tree-copy",true,None);
  require(tree.success && tree.sourceIsDirectory,"nested directory with empty file");
  require(readFile(mountPath+"/tree-copy/a")==contents,"nested bytes preserved");
  struct stat info{};require(!stat((mountPath+"/tree/sub/empty").c_str(),&info),"nested original retained");
  require(!Tab5FileOperation::startCopy("/tree","/tree/child",false),"descendant destination rejected");
  require(!Tab5FileOperation::startCopy("/USB","/copy",true),"volume root cannot be moved");
  std::cout << "PASS: " << checks << " checks; read/write/short-write/sync/corruption/cancel/change/collision/concurrent-creation/directory-change/tree\n";
}
