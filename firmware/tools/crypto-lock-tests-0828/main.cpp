#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <condition_variable>
#include <chrono>
#include <sys/lock.h>
static bool requiresMpi=true;
bool ecdsa_ll_is_mpi_required(){return requiresMpi;}
#include "LockSource.inc"
static unsigned checks=0;
static void check(bool ok,const char* label){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
thread_local std::vector<std::string> trace;
thread_local bool monitor=false;
static std::mutex signalGuard;
static std::condition_variable signal;
static bool started=false,blocked=false,entered=false;
static std::atomic<bool> observe{false};
static const char* name(_lock_t* p){
 if(p==&s_crypto_ecdsa_lock)return "ecdsa";
 if(p==&s_crypto_ecc_lock)return "ecc";
 if(p==&s_crypto_sha_aes_lock)return "sha-aes";
 if(p==&s_crypto_mpi_lock)return "mpi";
 if(p==&s_crypto_ds_lock)return "ds";
 if(p==&s_crypto_hmac_lock)return "hmac";
 if(p==&s_crypto_key_manager_lock)return "key-manager";
 return "unknown";
}
void _lock_acquire(_lock_t* p){
 if(monitor)trace.emplace_back(name(p));
 if(observe.load()&&p==&s_crypto_sha_aes_lock){std::lock_guard<std::mutex> lock(signalGuard);blocked=true;signal.notify_all();}
 p->mutex.lock();
}
void _lock_release(_lock_t* p){p->mutex.unlock();}
static void exclusion(){
 started=blocked=entered=false;
 esp_crypto_sha_aes_lock_acquire();observe=true;
 std::thread worker([]{
  {std::lock_guard<std::mutex> lock(signalGuard);started=true;signal.notify_all();}
  esp_crypto_ecdsa_lock_acquire();
  {std::lock_guard<std::mutex> lock(signalGuard);entered=true;signal.notify_all();}
  esp_crypto_ecdsa_lock_release();
 });
 {
  std::unique_lock<std::mutex> lock(signalGuard);
  check(signal.wait_for(lock,std::chrono::seconds(3),[]{return started&&(blocked||entered);}),"worker reached ECDSA lock boundary");
#if EXPECT_PATCHED
  check(blocked&&!entered,"ECDSA cannot reset SHA while its transfer is active");
#else
  check(entered&&!blocked,"SDK counterproof: ECDSA entered during active SHA transfer");
#endif
 }
 observe=false;esp_crypto_sha_aes_lock_release();worker.join();check(entered,"ECDSA proceeds after SHA release");
}
int main(){
 monitor=true;esp_crypto_ecdsa_lock_acquire();esp_crypto_ecdsa_lock_release();monitor=false;
#if EXPECT_PATCHED
 check(trace==std::vector<std::string>({"ecdsa","ecc","sha-aes","mpi"}),"upstream acquisition order SHA/AES before MPI");
#else
 check(trace==std::vector<std::string>({"ecdsa","ecc","mpi"}),"SDK baseline omits SHA/AES lock");
#endif
 for(unsigned n=0;n<32;++n)exclusion();
 requiresMpi=false;exclusion();
 monitor=true;trace.clear();esp_crypto_ecdsa_lock_acquire();esp_crypto_ecdsa_lock_release();monitor=false;
#if EXPECT_PATCHED
 check(trace==std::vector<std::string>({"ecdsa","ecc","sha-aes","mpi"}),"MPI reset coupling protected on every chip revision");
#else
 check(trace==std::vector<std::string>({"ecdsa","ecc"}),"SDK baseline varies MPI protection by revision");
#endif
 requiresMpi=true;
 std::atomic<unsigned> completed{0};std::atomic<unsigned> shaActive{0},overlap{0};
 auto sha=[&]{for(unsigned n=0;n<2000;++n){esp_crypto_sha_aes_lock_acquire();++shaActive;std::this_thread::yield();--shaActive;esp_crypto_sha_aes_lock_release();++completed;}};
 auto ecdsa=[&]{for(unsigned n=0;n<2000;++n){esp_crypto_ecdsa_lock_acquire();if(shaActive.load())++overlap;std::this_thread::yield();esp_crypto_ecdsa_lock_release();++completed;}};
 auto ds=[&]{for(unsigned n=0;n<2000;++n){esp_crypto_ds_lock_acquire();std::this_thread::yield();esp_crypto_ds_lock_release();++completed;}};
 auto ecc=[&]{for(unsigned n=0;n<2000;++n){esp_crypto_ecc_lock_acquire();std::this_thread::yield();esp_crypto_ecc_lock_release();++completed;}};
 std::thread a(sha),b(ecdsa),c(ds),d(ecc);a.join();b.join();c.join();d.join();
 check(completed==8000,"8000 mixed peripheral acquisitions complete without deadlock");
#if EXPECT_PATCHED
 check(overlap==0,"no SHA/ECDSA overlap in concurrent stress");
#endif
 std::printf("PASS %u crypto lock checks; patched=%u; controlled_rounds=33; concurrent_operations=%u; overlap=%u; hardware_tested=false\n",checks,unsigned(EXPECT_PATCHED),completed.load(),overlap.load());
 return 0;
}
