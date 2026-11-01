#include "Platform.h"
#include "Tab5KeyStore.h"
#include "Tab5Secrets.h"
#include "Tab5Vault.h"
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <thread>
#define REQUIRE(x) do{if(!(x)){std::cerr<<"fixture failure: "<<#x<<"\n";return 2;}}while(0)
using Snapshot=std::map<std::string,Node>;
Snapshot snapshot(){Snapshot s;for(auto& f:files)s[f.first]=*f.second;return s;}
bool same(const Snapshot& s){if(s.size()!=files.size())return false;for(auto& f:s){auto at=files.find(f.first);if(at==files.end()||at->second->directory!=f.second.directory||at->second->data!=f.second.data)return false;}return true;}
struct Gate {std::mutex lock;std::condition_variable cv;bool entered=false,released=false;
 void hold(){std::unique_lock<std::mutex> l(lock);entered=true;cv.notify_all();cv.wait(l,[&]{return released;});}
 void wait(){std::unique_lock<std::mutex> l(lock);cv.wait(l,[&]{return entered;});}
 void release(){std::lock_guard<std::mutex> l(lock);released=true;cv.notify_all();}};
int main(int argc,char**){
 bool baseline=argc>1;
 REQUIRE(Tab5KeyStore::initializeDevice("123456"));
 REQUIRE(Tab5Secrets::set("OLD","old-value"));REQUIRE(Tab5Secrets::set("NEW","latest-value"));
 auto stored=snapshot();auto keys=nvs;char value[513];
 closeError=true;bool closeCommitted=Tab5Secrets::set("NEW","unclosed-value");closeError=false;bool closePreserved=!closeCommitted&&same(stored);
 files.clear();for(auto& f:stored)files[f.first]=std::make_shared<Node>(f.second);Tab5Secrets::clearCache();
 flushError=true;bool flushCommitted=Tab5Secrets::set("NEW","unsynced-value");flushError=false;bool flushPreserved=!flushCommitted&&same(stored);
 files.clear();for(auto& f:stored)files[f.first]=std::make_shared<Node>(f.second);Tab5Secrets::clearCache();
 // Both banks exist, but stat reports I/O failure. This must not make a new
 // empty cache eligible for updates, including after the transient ends.
 Tab5Secrets::clearCache();existsError=[](const String& p){return p.find("credentials.")!=std::string::npos?EIO:0;};
 bool uncertainRead=Tab5Secrets::preload();existsError={};bool uncertainWrite=Tab5Secrets::set("REPLACE","replacement");
 bool bankErrorProtected=!uncertainRead&&!uncertainWrite&&same(stored);
 files.clear();for(auto& f:stored)files[f.first]=std::make_shared<Node>(f.second);Tab5Secrets::clearCache();
 // One uncertain bank must not silently downgrade to the older commit.
 existsError=[](const String& p){return p.find("credentials.b.")!=std::string::npos?EIO:0;};
 bool partialRead=Tab5Secrets::preload();existsError={};bool partialWrite=Tab5Secrets::set("REPLACE","replacement");
 bool partialErrorProtected=!partialRead&&!partialWrite&&same(stored);
 files.clear();for(auto& f:stored)files[f.first]=std::make_shared<Node>(f.second);Tab5Secrets::clearCache();
 REQUIRE(Tab5Secrets::preload());REQUIRE(Tab5Secrets::get("NEW",value,sizeof(value))&&!strcmp(value,"latest-value"));
 // A reset explicitly requested by a user still must not erase the master
 // when the archival source directory cannot be inspected.
 existsError=[](const String& p){return p=="/System/Secrets"?EIO:0;};REQUIRE(Tab5KeyStore::beginReset());pump();existsError={};
 bool archiveErrorProtected=!Tab5KeyStore::operationSucceeded()&&nvs.count("tab5keys/device3")&&nvs["tab5keys/device3"]==keys["tab5keys/device3"]&&same(stored);
 Tab5KeyStore::lock();nvs=keys;files.clear();for(auto& f:stored)files[f.first]=std::make_shared<Node>(f.second);
 // Hold the production key-store worker inside NVS. Polling state must
 // complete while the worker is still suspended, without authorizing access.
 Gate gate;REQUIRE(Tab5KeyStore::beginDeviceUnlock("123456"));nvsObserver=[&]{gate.hold();};
 std::thread worker([]{pump();});gate.wait();
 auto state=std::async(std::launch::async,[]{return !Tab5KeyStore::unlocked()&&!Tab5KeyStore::usingDevice()&&!Tab5KeyStore::credentialsAccessible();});
 bool statusNonblocking=state.wait_for(std::chrono::milliseconds(100))==std::future_status::ready;
 gate.release();worker.join();nvsObserver={};bool statusClosedDuringContention=state.get();
 REQUIRE(Tab5KeyStore::unlocked());REQUIRE(Tab5Secrets::ready());
 // Hold the credentials preload worker while it owns its cache mutex.
 Tab5Secrets::clearCache();Gate cacheGate;fsObserver=[&]{cacheGate.hold();};
 std::thread cacheWorker([]{Tab5Secrets::preload();});cacheGate.wait();
 auto ready=std::async(std::launch::async,[]{return !Tab5Secrets::ready();});
 bool readyNonblocking=ready.wait_for(std::chrono::milliseconds(100))==std::future_status::ready;
 cacheGate.release();cacheWorker.join();fsObserver={};bool readyClosedDuringContention=ready.get();
 REQUIRE(Tab5Secrets::get("NEW",value,sizeof(value))&&!strcmp(value,"latest-value"));
 bool all=closePreserved&&flushPreserved&&bankErrorProtected&&partialErrorProtected&&archiveErrorProtected&&statusNonblocking&&statusClosedDuringContention&&readyNonblocking&&readyClosedDuringContention;
 std::cout<<"{\"statErrorsPreserveBothBanks\":"<<bankErrorProtected<<",\"closeErrorPreservesCommittedBanks\":"<<closePreserved<<",\"flushErrorPreservesCommittedBanks\":"<<flushPreserved<<",\"singleBankStatErrorFailsClosed\":"<<partialErrorProtected<<",\"archiveStatErrorPreservesMaster\":"<<archiveErrorProtected<<",\"statusReturnsWhileWorkerHeld\":"<<statusNonblocking<<",\"statusClosedDuringContention\":"<<statusClosedDuringContention<<",\"cacheReadyReturnsWhileWorkerHeld\":"<<readyNonblocking<<",\"cacheReadyClosedDuringContention\":"<<readyClosedDuringContention<<",\"statusProbeDeadlineMs\":100,\"passed\":"<<all<<"}\n";
 return baseline||all?0:1;
}
