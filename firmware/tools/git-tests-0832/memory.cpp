#include "Tab5Git.h"
#include "Tab5GitMemory.h"
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <stdexcept>
#include <cstdint>
using namespace Tab5Resources;
static MemorySample memory;
static size_t initialFree=0, allocationCalls=0, admissionCalls=0, checks=0;
static bool cancelNow=false, failHeap=false;
static std::map<void*,size_t> blocks;
MemorySample sample(){return memory;}
MemoryTotals totals(){return {memory.internal,memory.psram};}
Tab5Resources::Manager Tab5MemoryManager(sample,totals);
Tab5Resources::Reservation tab5ReserveMemory(Owner owner,const Budget& budget,const char*,AdmissionReport* report){
 ++admissionCalls;return Tab5MemoryManager.reserveRecovering(owner,budget,true,report);
}
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
void* heap_caps_malloc(size_t bytes,int caps){
 if(caps!=(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))throw std::runtime_error("internal heap fallback");
 ++allocationCalls;if(failHeap||bytes>memory.psram||bytes>memory.psramBlock)return nullptr;
 void* p=std::malloc(bytes);if(p){blocks[p]=bytes;memory.psram-=bytes;}return p;
}
void heap_caps_free(void* p){auto it=blocks.find(p);if(it==blocks.end())throw std::runtime_error("unknown free");memory.psram+=it->second;blocks.erase(it);std::free(p);}
bool tab5_git_cancelled(){return cancelNow;}
namespace Tab5Git {
#include "allocator.inc"
}
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
void reset(size_t psram=16*MiB,size_t block=16*MiB){
 check(blocks.empty()&&Tab5Git::current==0,"all physical and tracked blocks released");
 memory={160*KiB,80*KiB,80*KiB,psram,block};initialFree=psram;cancelNow=false;failHeap=false;
 Tab5Git::peak=0;Tab5Git::allocationDenied=false;Tab5Git::allocationFailure={};
}
struct SessionBudget {
 Tab5Git::AllocationBudget budget;
 SessionBudget(){check(Tab5Git::allocationBudget==nullptr,"one live Git budget");Tab5Git::allocationBudget=&budget;}
 ~SessionBudget(){Tab5Git::allocationBudget=nullptr;}
};
// Execute the exact production UI worker against a deterministic backend.
// This checks orchestration and diagnostics, not a network clone.
static unsigned operationCalls=0,statusCalls=0;
static int operationResult=0,statusResult=0;
namespace Tab5Git {
int run(const std::string&,const std::string&,const std::vector<std::string>& args,
    Output,Cancel,Translate,Output diagnostics,Observer,Reservation* startupAdmission) {
#include "handoff.inc"
 if(args.size()>1&&args[1]=="status"){
  ++statusCalls;
  if(statusResult&&diagnostics){const char* m="Aucun dépôt Git compatible dans ce dossier.";diagnostics(m,strlen(m));}
  return statusResult;
 }
 ++operationCalls;return operationResult;
}
}
namespace UIHarness {
enum Action {Status,New,Clone,Stage,Commit,Diff,Log,Fetch,Pull,Push};
struct Model {Reservation startup;unsigned argCount=2;char args[8][128]{};char directory[128]{},nextDirectory[128]{};Action action=Clone;int result=0;} state;
Model* model=&state;
std::atomic<bool> cancelled{false},running{true},finished{false};
std::atomic<unsigned> revision{0};
std::string output;
template<size_t N>bool copy(char (&out)[N],const char* in){std::snprintf(out,N,"%s",in);return true;}
void append(const char* text,size_t n){output.append(text,n);}
std::string workerTr(const char* s){return s;}
void resetStatus(){}
Tab5Git::Observer observer(){return {};}
void tab5TaskCheckpoint(const char*){}
#include "worker.inc"
void test(Action action,int result,bool cancel){
 model->action=action;model->argCount=2;copy(model->args[0],"git");copy(model->args[1],action==Clone?"clone":action==New?"init":"fetch");
 copy(model->directory,"/Projects");copy(model->nextDirectory,"/Projects/new");
 cancelled=cancel;finished=false;operationCalls=statusCalls=0;output.clear();operationResult=result;statusResult=1;
 model->startup=Tab5Git::reserveStartup({"git","clone"},nullptr);check(bool(model->startup),"UI handoff has startup reservation");
 worker(nullptr);
 check(finished,"UI worker publishes completion");check(!model->startup,"UI startup ownership consumed");
 check(operationCalls==1,"one requested operation");
 const bool expectRefresh=!cancel&&(result==0||(action!=Clone&&action!=New));
 check(statusCalls==unsigned(expectRefresh),"status refresh only when useful");
 check(output.empty()==!expectRefresh,"failed create/clone preserves original diagnostic without repository noise");
 check(!strcmp(model->directory,result==0?"/Projects/new":"/Projects"),"directory changes only after successful operation");
}
}
static unsigned internalReclaims=0;
size_t reclaimInternal(void*){++internalReclaims;memory.internal+=12*KiB;return 12*KiB;}
void noReservations(){
 auto whole=Tab5MemoryManager.reserve(Owner::Video,{0,0,0,memory.psram-PsramSafety,0});
 check(bool(whole),"no stale Git batch pending after session");
}
int main(){try{
 reset(4*MiB,2*MiB);
 auto startup=Tab5Git::startupBudget(true);auto previous=startup;previous.psram+=Tab5Git::MemoryLimit;
 check(!Manager::fits(memory,previous),"regression: old all-8-MiB admission rejects 4-MiB free");
 check(Manager::fits(memory,startup),"new startup admits modest TLS workspace");
 check(startup.internal==TlsBudget.internal+8*KiB&&startup.psram==TlsBudget.psram+128*KiB,"TLS and adapter startup headroom unchanged");
 {
  auto tls=Tab5MemoryManager.reserve(Owner::Tools,startup);check(bool(tls),"startup reserve");
  SessionBudget arena;std::vector<void*> data;
  const auto before=admissionCalls;
  for(unsigned n=0;n<600;++n){auto* p=Tab5Git::allocate(1024,__FILE__,__LINE__);check(p!=nullptr,"small repository working set can grow below old 8-MiB threshold");std::memset(p,int(n%256),1024);data.push_back(p);}
  check(admissionCalls-before<=4,"admission batches avoid per-object heap scans");
  check(Tab5Git::current.load()<MiB,"small-repository allocation stays below 1 MiB");
  auto other=Tab5MemoryManager.reserve(Owner::Map,{0,0,0,MiB,128*KiB});check(bool(other),"another app can use real remaining PSRAM");other.release();
  for(unsigned n=0;n<data.size();++n){check(static_cast<unsigned char*>(data[n])[0]==n%256,"buffer data intact");Tab5Git::release(data[n]);}
  check(memory.psram==initialFree,"all physical bytes returned before session close");
 }
 noReservations();
 // Pending budgets of concurrent apps remain protected as Git grows.
 reset(3*MiB,3*MiB);
 {
  auto other=Tab5MemoryManager.reserve(Owner::Map,{0,0,0,2*MiB,0});check(bool(other),"reserve another app startup");
  SessionBudget arena;auto* p=Tab5Git::allocate(400*KiB,__FILE__,__LINE__);check(p,"first Git allocation fits with concurrent reserve");
  check(!Tab5Git::allocate(160*KiB,__FILE__,__LINE__),"Git cannot consume another app reserve or system margin");
  check(Tab5Git::allocationFailure.reason==Denial::Psram,"concurrent-budget refusal explains PSRAM");
  Tab5Git::release(p);
 }
 noReservations();
 // A small final allocation must not require a full 256 KiB batch.
 reset(PsramSafety+128,1024);
 {
  SessionBudget arena;auto* p=Tab5Git::allocate(128-sizeof(Tab5Git::Allocation),__FILE__,__LINE__);
  check(p!=nullptr,"exact allocation fallback after batch admission fails");check(memory.psram==PsramSafety,"system safety margin remains intact");
  check(!Tab5Git::allocate(1,__FILE__,__LINE__),"refuse crossing system safety");Tab5Git::release(p);
 }
 noReservations();
 // Distinguish total free bytes, block fragmentation and internal SRAM.
 reset(4*MiB,128);
 {SessionBudget arena;check(!Tab5Git::allocate(512,__FILE__,__LINE__),"fragmented PSRAM fails safely");check(Tab5Git::allocationFailure.reason==Denial::PsramBlock,"PSRAM block diagnostic");}
 reset();memory.internal=31*KiB;
 {SessionBudget arena;check(!Tab5Git::allocate(32,__FILE__,__LINE__),"PSRAM allocation respects internal safety");check(Tab5Git::allocationFailure.reason==Denial::Internal,"internal shortage diagnostic");}
 reset();memory.internal=75*KiB;
 check(Manager::rejection(memory,TaskBudget(Tab5Git::WorkerStackBytes))==Denial::Internal,"40-KiB stack still needs 76 KiB internal including guard");
 memory.internal=100*KiB;memory.internalBlock=39*KiB;
 check(Manager::rejection(memory,TaskBudget(Tab5Git::WorkerStackBytes))==Denial::InternalBlock,"40-KiB stack cannot use fragmented internal memory");
 memory.internalBlock=80*KiB;memory.internal=87*KiB;
 check(Manager::rejection(memory,Tab5Git::startupBudget(true))==Denial::Internal,"internal TLS fallback requires full 88-KiB startup margin");
 Budget externalTls{16*KiB,4*KiB,0,384*KiB,128*KiB};memory.internal=47*KiB;
 check(Manager::rejection(memory,externalTls)==Denial::Internal,"external TLS still preserves 48-KiB internal startup margin");
 // Reproduce the supplied .30 failure after stack allocation:46K free,
 //48K needed. .31 admits TLS/workspace before reserving the stack, so cache
 //collection sees both startup demands while reclaimers can still run.
 reset(27652*KiB,27135*KiB);memory.internal=86*KiB;memory.internalBlock=69*KiB;memory.dmaBlock=6*KiB;
 {
  auto oldStack=Tab5MemoryManager.reserve(Owner::Tools,TaskBudget(Tab5Git::WorkerStackBytes));check(bool(oldStack),"old stack-only admission accepts initial86K");
  memory.internal-=Tab5Git::WorkerStackBytes;memory.internalBlock-=Tab5Git::WorkerStackBytes;oldStack.release();
  AdmissionReport failure;auto denied=Tab5MemoryManager.reserve(Owner::Tools,externalTls,&failure);
  check(!denied&&failure.reason==Denial::Internal,"reproduced46K free vs48K required after old launch");
  check(failure.memory.internal==46*KiB&&failure.requested.internal==16*KiB&&failure.memory.internalBlock==29*KiB&&failure.memory.dmaBlock==6*KiB,"reported quantities reproduced exactly");
  memory.internal+=Tab5Git::WorkerStackBytes;memory.internalBlock+=Tab5Git::WorkerStackBytes;
 }
 check(Tab5MemoryManager.addCache(Owner::Audio,"idle-audio-test",reclaimInternal,nullptr,{MemoryDomain::Internal,0}),"register simulated reclaimable SRAM");
 {
  auto session=Tab5MemoryManager.reserve(Owner::Tools,externalTls);check(bool(session),"reserve full session before worker");
  AdmissionReport report;auto stack=Tab5MemoryManager.reserveRecovering(Owner::Tools,TaskBudget(Tab5Git::WorkerStackBytes),true,&report);
  check(bool(stack)&&internalReclaims==1,"combined stack+session demand invokes OS reclamation");
  check(report.pending.internal==16*KiB,"stack admission includes pending TLS/workspace");
  memory.internal-=Tab5Git::WorkerStackBytes;stack.release();
  const auto before=admissionCalls;auto worker=std::move(session);
  check(bool(worker)&&!session&&admissionCalls==before,"working-set handoff does not re-admit or double-charge");
  check(Manager::fits(memory,{},externalTls),"remaining SRAM covers session plus unchanged safety");
  auto competitor=Tab5MemoryManager.reserve(Owner::Video,{20*KiB,0,0,0,0});
  check(!competitor,"another app cannot steal retained Git startup headroom");
  worker.release();memory.internal+=Tab5Git::WorkerStackBytes;
 }
 noReservations();
 reset();
 for(const auto action:{UIHarness::New,UIHarness::Clone,UIHarness::Fetch})
  for(const int result:{0,1,130})for(const bool cancelled:{false,true})UIHarness::test(action,result,cancelled);
 noReservations();
 // Physical allocator failure, cancellation, overflow and exact 8-MiB quota.
 reset();
 {SessionBudget arena;failHeap=true;check(!Tab5Git::allocate(512,__FILE__,__LINE__),"physical OOM returns failure");check(Tab5Git::current==0,"failed allocation uncharged");failHeap=false;auto* p=Tab5Git::allocate(512,__FILE__,__LINE__);check(p,"retry after physical OOM");Tab5Git::release(p);}
 noReservations();reset();
 {SessionBudget arena;const auto before=admissionCalls;cancelNow=true;check(!Tab5Git::allocate(512,__FILE__,__LINE__),"cancellation allocates nothing");cancelNow=false;check(!Tab5Git::allocate(SIZE_MAX,__FILE__,__LINE__),"allocation size overflow rejected");check(admissionCalls==before,"cancel and overflow need no heap admission");}
 noReservations();reset();
 {SessionBudget arena;auto* p=Tab5Git::allocate(Tab5Git::MemoryLimit-sizeof(Tab5Git::Allocation),__FILE__,__LINE__);check(p,"exact total quota allocates");check(Tab5Git::current==Tab5Git::MemoryLimit,"headers included in quota");check(!Tab5Git::allocate(1,__FILE__,__LINE__),"quota cannot be exceeded");Tab5Git::release(p);}
 noReservations();reset();
 {SessionBudget arena;auto* p=Tab5Git::allocate(5*MiB,__FILE__,__LINE__);check(p,"resize original allocation");std::memset(p,0x5a,256);check(!Tab5Git::resize(p,4*MiB,__FILE__,__LINE__),"resize transient old+new respects quota");check(static_cast<unsigned char*>(p)[255]==0x5a,"failed resize preserves old data");auto* q=Tab5Git::resize(p,MiB,__FILE__,__LINE__);check(q&&static_cast<unsigned char*>(q)[255]==0x5a,"successful resize copies old prefix");check(Tab5Git::resize(q,0,__FILE__,__LINE__)==nullptr,"zero resize frees original");}
 noReservations();
 // Hundreds of successful and refused sessions must return all reservation slots.
 for(unsigned n=0;n<400;++n){reset();{SessionBudget arena;std::vector<void*> data;for(unsigned i=0;i<12;++i){auto* p=Tab5Git::allocate((i+1)*4096,__FILE__,__LINE__);check(p,"repeated allocation");data.push_back(p);}cancelNow=true;check(!Tab5Git::allocate(128,__FILE__,__LINE__),"repeat cancellation");cancelNow=false;for(auto* p:data)Tab5Git::release(p);}noReservations();}
 reset();printf("{\"passed\":true,\"checks\":%zu,\"repeatedSessions\":400,\"admissionCalls\":%zu,\"physicalAllocationCalls\":%zu}\n",checks,admissionCalls,allocationCalls);return 0;
 }catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
