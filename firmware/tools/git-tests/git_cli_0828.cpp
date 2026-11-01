#define TAB5_GIT_HOST 1
#include "ShellPlatform.h"
#include <cstdio>
#include <string>
#define TAB5_GIT_TEST_PHASE 1
static std::string lastGitPhase;
static void tab5_git_test_phase(const char* token,bool active){
 if(lastGitPhase==token)return;
 lastGitPhase=token;std::fprintf(stderr,"TAB5_GIT_PHASE %s %d\n",token,int(active));
}
#include "Tab5Git.cpp"
#include "Tab5GitPort.cpp"
#include <iostream>
#include <chrono>
#include <windows.h>
#include <dbghelp.h>
LONG WINAPI crashTrace(EXCEPTION_POINTERS*){void* frames[48];USHORT count=CaptureStackBackTrace(0,48,frames,nullptr);SymInitialize(GetCurrentProcess(),nullptr,TRUE);for(USHORT i=0;i<count;++i){char raw[sizeof(SYMBOL_INFO)+512]{};auto* symbol=reinterpret_cast<SYMBOL_INFO*>(raw);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=511;DWORD64 delta=0;if(SymFromAddr(GetCurrentProcess(),DWORD64(frames[i]),&delta,symbol))std::fprintf(stderr,"CRASH %s+%llu\n",symbol->Name,delta);else std::fprintf(stderr,"CRASH %p\n",frames[i]);}return EXCEPTION_CONTINUE_SEARCH;}
int main(int argc,char** argv){
 if(std::getenv("TAB5_GIT_DEBUG_CRASH"))AddVectoredExceptionHandler(1,crashTrace);
 if(argc<4)return 2;std::vector<std::string> a;for(int n=3;n<argc;++n)a.emplace_back(argv[n]);
 int ticks=0;const char* stop=std::getenv("TAB5_GIT_CANCEL_AFTER");int stopAfter=stop?atoi(stop):0;
 const char* delay=std::getenv("TAB5_GIT_CANCEL_MS");int delayMs=delay?atoi(delay):0;const auto started=std::chrono::steady_clock::now();
 auto out=[](const char* p,size_t n){std::cout.write(p,n);};auto err=[](const char* p,size_t n){std::cerr.write(p,n);};
 const char* failing=std::getenv("TAB5_GIT_FAIL_ALLOC");const std::string savedFailure=failing?failing:"";DWORD handlesBefore=0;
 const bool recover=std::getenv("TAB5_GIT_RECOVER_AFTER_FAILURE")!=nullptr;
 if(recover){std::cout<<"TAB5_GIT_WARMUP\n"<<std::flush;std::cerr<<"TAB5_GIT_WARMUP\n"<<std::flush;_putenv_s("TAB5_GIT_FAIL_ALLOC","");for(int n=0;n<4;++n){int warm=Tab5Git::run(argv[1],argv[2],{"git","status"},[](const char*,size_t){},{},{},err);if(warm)return 96;GetProcessHandleCount(GetCurrentProcess(),&handlesBefore);std::cerr<<"TAB5_GIT_WARM_HANDLES "<<n<<" "<<handlesBefore<<"\n";}_putenv_s("TAB5_GIT_FAIL_ALLOC",savedFailure.c_str());}
 const char* repeated=std::getenv("TAB5_GIT_REPEAT");int repeat=repeated?atoi(repeated):1,result=0;
 for(int i=0;i<repeat;++i){result=Tab5Git::run(argv[1],argv[2],a,out,[&]{return (stopAfter&&++ticks>=stopAfter)||(delayMs&&std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count()>=delayMs);},{},err);
  auto stats=Tab5Git::memoryStats();std::cerr<<"\nTAB5_GIT_MEMORY "<<stats.current<<" "<<stats.peak<<"\n";
  std::cerr<<"TAB5_GIT_RECLAIMED "<<stats.reclaimedBlocks<<" "<<stats.reclaimedBytes<<"\n";
  if(stats.current)return 99;if(result)break;}
 if(recover){_putenv_s("TAB5_GIT_FAIL_ALLOC","");for(int i=0;i<3;++i){int healthy=Tab5Git::run(argv[1],argv[2],{"git","status"},[](const char*,size_t){},{},{},err);auto stats=Tab5Git::memoryStats();if(healthy||stats.current||stats.reclaimedBlocks)return 97;}
  DWORD after=0;GetProcessHandleCount(GetCurrentProcess(),&after);std::cerr<<"TAB5_GIT_HANDLES "<<handlesBefore<<" "<<after<<"\n";if(after>handlesBefore)return 98;}
 return result;
}
