#include "Tab5GitPort.h"
#include "Tab5GitJournal.h"
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifndef TAB5_GIT_HOST
#include "Tab5NetworkGate.h"
#include "Tab5Diagnostics.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
namespace {
std::string workingRoot;std::function<bool()> cancelled;bool networkSession=false;
Tab5GitJournal::Ops filesystem(){return {
 [](const std::string& p){struct stat st{};if(!stat(p.c_str(),&st))return 1;return errno==ENOENT?0:-1;},
 [](const std::string& a,const std::string& b){return rename(a.c_str(),b.c_str())==0;},
 [](const std::string& p){return remove(p.c_str())==0;},
 [](const std::string& p,const void* data,size_t size){int fd=open(p.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return false;bool ok=write(fd,data,size)==ssize_t(size);if(ok)ok=fsync(fd)==0;if(close(fd))ok=false;return ok;},
 [](const std::string& p,void* data,size_t size){FILE* f=fopen(p.c_str(),"rb");if(!f)return false;bool ok=fread(data,1,size,f)==size&&fgetc(f)==EOF&&!ferror(f);if(fclose(f))ok=false;return ok;}
 };}
}
namespace Tab5GitPort {
bool project(const std::string& root){workingRoot=root;return Tab5GitJournal::recover(root,filesystem());}
void clear(){std::string().swap(workingRoot);cancelled={};networkSession=false;}
void cancellation(std::function<bool()> callback){cancelled=std::move(callback);}
void networking(bool required){networkSession=required;}
void phase(const char* token){
#ifndef TAB5_GIT_HOST
 tab5DiagnosticMark(7,token,"",1);
#elif defined(TAB5_GIT_TEST_PHASE)
 tab5_git_test_phase(token,true);
#else
 (void)token;
#endif
}
void idle(){
#ifndef TAB5_GIT_HOST
 tab5DiagnosticMark(7,"idle","",0);
#elif defined(TAB5_GIT_TEST_PHASE)
 tab5_git_test_phase("idle",false);
#endif
}
}
extern "C" int tab5_git_needs_tls(void){return networkSession?1:0;}
extern "C" int tab5_git_rename(const char* from,const char* to){
 if(!from||!to||workingRoot.empty()){errno=EINVAL;return -1;}
 if(!Tab5GitJournal::replace(workingRoot,from,to,filesystem())){if(!errno)errno=EIO;return -1;}return 0;
}
extern "C" int tab5_git_cancelled(void){
#ifndef TAB5_GIT_HOST
 static TickType_t lastYield=0;TickType_t now=xTaskGetTickCount();if(now-lastYield>=pdMS_TO_TICKS(8)){lastYield=now;vTaskDelay(1);}
#endif
 return cancelled&&cancelled();
}
