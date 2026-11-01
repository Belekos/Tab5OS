#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <lvgl.h>
#define private public
#include "Tab5Writing.h"
#undef private
#include "WriterDocument.h"
#include "WriterStorage.h"
static unsigned checks=0;
static void check(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
struct MemoryFS:writing::FileSystem{
 std::map<std::string,std::string> files;
 std::string active;
 size_t cursor=0,maxWrite=0,maxRead=0;
 unsigned writes=0,reads=0,renames=0,operations=0,failRenameAt=0;
 bool writeError=false,readError=false,verifyError=false,openError=false,renameError=false;
 bool exists(const char* p)override{++operations;return files.count(p)!=0;}
 bool remove(const char* p)override{++operations;return files.erase(p)!=0;}
 bool rename(const char* a,const char* b)override{++operations;++renames;if(renameError||renames==failRenameAt||!files.count(a))return false;files[b]=files[a];files.erase(a);return true;}
 bool openWrite(const char* p)override{++operations;if(openError)return false;active=p;files[active].clear();cursor=0;return true;}
 bool openRead(const char* p)override{++operations;if(openError||!files.count(p))return false;active=p;cursor=0;return true;}
 size_t length()override{return files[active].size();}
 size_t write(const char* p,size_t n)override{++operations;++writes;maxWrite=std::max(maxWrite,n);if(writeError)return 0;files[active].append(p,n);return n;}
 size_t read(char* p,size_t n)override{++operations;++reads;maxRead=std::max(maxRead,n);if(readError)return 0;n=std::min(n,files[active].size()-cursor);std::memcpy(p,files[active].data()+cursor,n);cursor+=n;if(n&&verifyError)p[0]^=1;return n;}
 void close()override{active.clear();}
};
struct Directories{
 bool failed=false;unsigned operations=0;
 bool exists(const char*){++operations;return !failed;}
 bool mkdir(const char*){++operations;return !failed;}
}tab5FS;
struct Tab5WritingApp::Impl{
 bool built=true,code=false;unsigned resumePhase=0;
 MemoryFS fs,sessionFs,recoveryFs;
 writing::Document document;
 writing::SaveJob saveJob{fs},sessionJob{sessionFs},recoveryJob{recoveryFs};
 RecoverySaveState recoveryState=RecoverySaveState::Idle;
 bool recoveryStarted=false;
 const char* recoveryPath="";const char* recoveryError="";
 std::string path="/Documents/original.txt";
 bool isCode(){return code;}
 /* void serviceRecoverySave() */
};
/* void Tab5WritingApp::requestRecoverySave() */
/* Tab5WritingApp::RecoverySaveState Tab5WritingApp::recoverySaveState() */
/* const char* Tab5WritingApp::recoverySavePath() */
/* const char* Tab5WritingApp::recoverySaveError() */
/* void Tab5WritingApp::cancelRecoverySave() */
static void dirty(Tab5WritingApp::Impl& s,const std::string& text){check(s.document.assign(text.data(),text.size()),"document allocated");s.document.markSaved(s.document.revision());check(s.document.replace(text.size(),text.size(),"!",1,0,1,1),"document changed");}
static void drain(Tab5WritingApp& app,Tab5WritingApp::Impl& s){for(unsigned i=0;i<2000&&app.recoverySaveState()==Tab5WritingApp::RecoverySaveState::Pending;++i){unsigned writes=s.recoveryFs.writes,reads=s.recoveryFs.reads;s.serviceRecoverySave();check((s.recoveryFs.writes-writes)+(s.recoveryFs.reads-reads)<=1,"recovery performs at most one bounded data block per service");}}
int main(){
 using State=Tab5WritingApp::RecoverySaveState;
 Tab5WritingApp uninitialized;uninitialized.requestRecoverySave();check(uninitialized.recoverySaveState()==State::Saved,"uninitialized editor requires no save");
 for(unsigned scenario=0;scenario<8;++scenario){
  tab5FS=Directories{};Tab5WritingApp app;Tab5WritingApp::Impl s;app.impl_=&s;
  std::string original(12289,'A');dirty(s,original);const auto revision=s.document.revision();const auto undo=s.document.canUndo();
  s.recoveryFs.files["/System/writing/recovery.md"]="previous recovery";
  if(scenario==1)tab5FS.failed=true;
  if(scenario==2)s.recoveryFs.openError=true;
  if(scenario==3)s.recoveryFs.writeError=true;
  if(scenario==4)s.recoveryFs.readError=true;
  if(scenario==5)s.recoveryFs.verifyError=true;
  if(scenario==6)s.recoveryFs.renameError=true;
  if(scenario==7)s.code=true;
  app.requestRecoverySave();check(app.recoverySaveState()==State::Pending,"request queues asynchronous copy");
  check(s.recoveryFs.operations==0&&tab5FS.operations==0,"request does not touch SD");
  app.requestRecoverySave();drain(app,s);
  check(s.document.dirty()&&s.document.revision()==revision&&s.document.canUndo()==undo&&s.path=="/Documents/original.txt","recovery leaves original path revision dirty and undo unchanged");
  check(s.recoveryFs.maxRead<=4096&&s.recoveryFs.maxWrite<=4096,"data blocks limited to 4096 bytes");
  if(scenario==0||scenario==7){
   check(app.recoverySaveState()==State::Saved,"copy succeeds");
   check(s.recoveryFs.files[app.recoverySavePath()]==original+"!","copy contains full document");
   check(!*app.recoverySaveError(),"successful copy has no error");
   if(!scenario)check(s.recoveryFs.files["/.tab5-backups/System/writing/recovery.md.writer.bak"]=="previous recovery","previous recovery retained as verified-save backup");
  }else{
   check(app.recoverySaveState()==State::Failed&&*app.recoverySaveError(),"SD failure is explicit");
   check(s.recoveryFs.files["/System/writing/recovery.md"]=="previous recovery","failed transaction preserves previous recovery");
  }
  const auto operations=s.recoveryFs.operations+tab5FS.operations;
  for(unsigned i=0;i<100;++i)s.serviceRecoverySave();
  check(operations==s.recoveryFs.operations+tab5FS.operations,"no SD retries or periodic recovery writes");
 }
 {
  tab5FS=Directories{};Tab5WritingApp app;Tab5WritingApp::Impl s;app.impl_=&s;dirty(s,"busy");
  check(s.saveJob.begin(s.document,"/Documents/original.txt"),"normal save started");
  app.requestRecoverySave();for(unsigned i=0;i<20;++i)s.serviceRecoverySave();
  check(s.recoveryFs.operations==0&&app.recoverySaveState()==State::Pending,"recovery waits for existing save");
  while(s.saveJob.busy())s.saveJob.step();s.serviceRecoverySave();
  check(app.recoverySaveState()==State::Saved&&!*app.recoverySavePath(),"already-saved document needs no redundant copy");
 }
 for(unsigned stage:{0,1,3,7}){
  tab5FS=Directories{};Tab5WritingApp app;Tab5WritingApp::Impl s;app.impl_=&s;dirty(s,std::string(24000,'B'));
  s.recoveryFs.files["/System/writing/recovery.md"]="safe original";app.requestRecoverySave();
  for(unsigned i=0;i<stage;++i)s.serviceRecoverySave();
  app.cancelRecoverySave();check(app.recoverySaveState()==State::Failed&&!s.recoveryJob.busy()&&s.recoveryFs.active.empty(),"deadline cancel closes temporary copy");
  check(s.recoveryFs.files["/System/writing/recovery.md"]=="safe original"&&s.document.dirty(),"cancel never promotes partial recovery or marks document saved");
  app.requestRecoverySave();drain(app,s);check(app.recoverySaveState()==State::Saved,"new battery countdown may explicitly request another attempt");
 }
 {
  tab5FS=Directories{};Tab5WritingApp app;Tab5WritingApp::Impl s;app.impl_=&s;dirty(s,"editing old recovery");s.path="/System/writing/recovery.md";s.recoveryFs.files[s.path]="untouched original";app.requestRecoverySave();drain(app,s);
  check(app.recoverySaveState()==State::Saved&&s.path!=app.recoverySavePath()&&s.recoveryFs.files[s.path]=="untouched original","editing a previous recovery still preserves its source file");
 }
 {
  tab5FS=Directories{};Tab5WritingApp app;Tab5WritingApp::Impl s;app.impl_=&s;dirty(s,std::string(writing::Document::Limit-1,'L'));
  app.requestRecoverySave();drain(app,s);check(app.recoverySaveState()==State::Saved&&s.recoveryFs.files[app.recoverySavePath()].size()==writing::Document::Limit,"maximum 2MiB document recovered completely");
 }
 {
  tab5FS=Directories{};Tab5WritingApp app;Tab5WritingApp::Impl s;app.impl_=&s;dirty(s,"rollback");s.recoveryFs.files["/System/writing/recovery.md"]="previous";s.recoveryFs.failRenameAt=2;
  app.requestRecoverySave();drain(app,s);check(app.recoverySaveState()==State::Failed&&s.recoveryFs.files["/System/writing/recovery.md"]=="previous","failed final promotion restores previous recovery");
 }
 {
  tab5FS=Directories{};Tab5WritingApp app;Tab5WritingApp::Impl s;app.impl_=&s;dirty(s,"waiting");s.resumePhase=1;app.requestRecoverySave();s.serviceRecoverySave();check(s.recoveryFs.operations==0,"resume finishes before recovery starts");s.resumePhase=0;
  writing::Document session;session.assign("session",7);check(s.sessionJob.begin(session,"/System/writing/session.txt"),"session transaction starts");s.serviceRecoverySave();check(s.recoveryFs.operations==0,"recovery waits for session transaction");while(s.sessionJob.busy())s.sessionJob.step();drain(app,s);check(app.recoverySaveState()==State::Saved,"recovery starts once existing session completes");
 }
 {
  tab5FS=Directories{};Tab5WritingApp app;Tab5WritingApp::Impl s;app.impl_=&s;dirty(s,"snapshot");app.requestRecoverySave();s.serviceRecoverySave();
  check(s.document.replace(0,8,"modified",8,0,8,2),"editing while copy runs");drain(app,s);
  check(s.recoveryFs.files[app.recoverySavePath()]=="snapshot!"&&s.document.dirty(),"recovery captures request snapshot without overwriting later changes");
 }
 std::printf("Writing recovery: %u checks\n",checks);
}
