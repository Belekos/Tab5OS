#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace Boundary {
inline const auto ui = std::this_thread::get_id();
inline std::thread worker;
inline std::mutex mutex;
inline std::condition_variable condition;
inline bool blocked=false,entered=false;
inline bool available=true,writable=true,failBrowser=false,failVolume=false;
inline bool failAllocation=false,failAdmission=false,failTask=false;
inline bool displayObserved=true;
inline std::atomic<uint32_t> underruns{0};
inline std::atomic<int64_t> clockUs{1000000};
inline std::map<std::string,std::vector<uint8_t>> values;
inline unsigned writes=0,attempts=0,uiWrites=0,launches=0,liveJobs=0,maxLiveJobs=0,reservations=0;
inline uint64_t bytesWritten=0;
inline uint32_t stackBytes=0;
inline void join(){if(worker.joinable())worker.join();}
inline void pause(){std::lock_guard<std::mutex> lock(mutex);blocked=true;entered=false;}
inline void waitEntered(){std::unique_lock<std::mutex> lock(mutex);if(!condition.wait_for(lock,std::chrono::seconds(3),[]{return entered;}))throw std::runtime_error("worker did not reach boundary");}
inline void release(){std::lock_guard<std::mutex> lock(mutex);blocked=false;condition.notify_all();}
inline void wait(){std::unique_lock<std::mutex> lock(mutex);entered=true;condition.notify_all();condition.wait(lock,[]{return !blocked;});}
}
