#include "Arduino.h"
#include "FS.h"
#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5Serial.h"
#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
using namespace fs;
String operator+(const std::string& left,unsigned value){return String(left+std::to_string(value));}
void heap_caps_free(void* p){free(p);}
extern std::string testRoot,failOpen;
extern int failOpenError,injectedRemoveError;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";std::abort();}}while(0)
static unsigned sdEnds=0,sdBegins=0,spiEnds=0,uiClosures=0,toastCalls=0;
static bool sdReady=true,systemMigrationFailed=false,mountSucceeds=true,migrationSucceeds=true,settingsReloadSucceeds=true;
static unsigned diagnosticBegin=0,diagnosticEnd=0;
static bool homeAppsScanning=false,homeAppsDirty=false,fileSessionRestorePending=false,fileSelectAllSearch=false,documentDirty=false,settingsDirty=false,readerProgressDirty=false;
enum class Page { Home,Files,Map,Settings };static Page activePage=Page::Settings;
constexpr unsigned SD_SPI_CS_PIN=42,SD_SPI_SCK_PIN=43,SD_SPI_MISO_PIN=39,SD_SPI_MOSI_PIN=44,OUTPUT=1,HIGH=1;
void pinMode(unsigned,unsigned){}void digitalWrite(unsigned,unsigned){}
struct Bus {void end(){CHECK(tab5StorageExclusiveOwner());++spiEnds;}void begin(unsigned,unsigned,unsigned,unsigned){CHECK(tab5StorageExclusiveOwner());}}SPI;
struct Card {void end(){CHECK(tab5StorageExclusiveOwner());CHECK(tab5SdOpenFiles()==0);++sdEnds;}bool begin(unsigned,Bus&,unsigned,const char*,unsigned,bool){CHECK(tab5StorageExclusiveOwner());++sdBegins;return mountSucceeds;}}SD;
struct CancelObject {void cancel(){++uiClosures;}}fileTree,fileSearcher;
File fileListDir,fileSelectAllDirectory;
void rememberFileSession(){}void cancelFolderPreview(){++uiClosures;}
void tab5DiagnosticMark(unsigned,const char*,const char*,unsigned active){if(active)++diagnosticBegin;else ++diagnosticEnd;}
void ensureDirectories(){}void refreshFileList(){}void clearFilePreview(){}void toast(const char*,unsigned){++toastCalls;}
bool migrateSystemDirectory(){return migrationSucceeds;}bool reloadSettingsAfterMigration(){return settingsReloadSucceeds;}
bool saveSettings(){return true;}void saveReaderProgressNow(){readerProgressDirty=false;}
namespace Tab5SystemResources {void cancel(){}void start(bool){} }
namespace Tab5AppCatalogue {void endScan(){} }
namespace Tab5ImageEditor {bool dirtyState=false,busyState=false;bool dirty(){return dirtyState;}bool busy(){return busyState;} }
namespace Tab5MapPrint {bool state=false;bool busy(){return state;} }
namespace Tab5FilePicker {std::unique_ptr<Tab5StorageUse> held;void close(){held.reset();++uiClosures;} }
struct Audio {bool idle=true;unsigned setters=0;bool available=true;bool storageIdle(){return idle;}void setStorageAvailable(bool v){CHECK(tab5StorageExclusiveOwner());CHECK(idle);available=v;++setters;}}Tab5Audio;
struct Writing {bool idle=true;bool storageIdle(){return idle;}bool prepareSleep(){return idle;}}Tab5Writing;
struct Video {unsigned setters=0;bool available=true;void setStorageAvailable(bool v){CHECK(tab5StorageExclusiveOwner());available=v;++setters;}}Tab5Video;
class Tab5MapEngine {public:File _offlineFile;void*_offlineDirectory=nullptr,*_offlineIndexBlock=nullptr;unsigned _offlineIndexCapacity=0;int _offlineActivePack=-1;uint32_t _offlineActiveBlock=UINT32_MAX;unsigned released=0,setters=0;bool available=true;void closeOfflinePack();void releaseStorageHandles(){++released;closeOfflinePack();}void setStorageAvailable(bool v){CHECK(tab5StorageExclusiveOwner());available=v;++setters;}}Tab5Map;
// MAP_CLOSE
bool tab5AtomicProbe(const char* path,bool& present){errno=0;present=tab5FS.exists(path);return present||errno==ENOENT;}
// PRODUCTION_MAIN
static bool runProbe(){
#ifdef OWNED_PROBE_RESULT
 int failure=0;return probeSdWrite(failure);
#else
 return probeSdWrite();
#endif
}
static void reset(){
 Tab5FilePicker::close();fileListDir.close();fileSelectAllDirectory.close();Tab5Map._offlineFile.close();
 CHECK(tab5SdOpenFiles()==0);tab5SdSetAvailable(true);tab5StorageBootResult(true,0);
 sdReady=true;systemMigrationFailed=false;mountSucceeds=migrationSucceeds=settingsReloadSucceeds=true;
 Tab5Audio.idle=Tab5Writing.idle=true;Tab5ImageEditor::busyState=Tab5ImageEditor::dirtyState=Tab5MapPrint::state=false;
 documentDirty=settingsDirty=readerProgressDirty=false;sdEnds=sdBegins=spiEnds=0;toastCalls=0;diagnosticBegin=diagnosticEnd=0;
}
static bool concurrentDiagnostic=false;
static unsigned diagnosticInjections=0;
extern "C" void __real__Z16tab5StorageFaultPKcS0_i(const char*,const char*,int);
extern "C" void __wrap__Z16tab5StorageFaultPKcS0_i(const char* operation,const char* path,int error){
 __real__Z16tab5StorageFaultPKcS0_i(operation,path,error);
 if(concurrentDiagnostic&&tab5StorageExclusiveOwner()){
  concurrentDiagnostic=false;
  std::thread denied([]{
   errno=0;File file=tab5FS.open("/worker-access-during-recovery");
   CHECK(!file&&errno==EBUSY);
   tab5StorageFault("worker busy","/worker-access-during-recovery",errno);
  });denied.join();++diagnosticInjections;
 }
}
extern "C" int __real_fflush(FILE*);
extern "C" int __real_fsync(int);
extern "C" int __real_fclose(FILE*);
static const char* stage="";static int injectedError=EIO;static bool preserveErrnoAcrossSuccessfulSync=true;
// VFS flush()/close() are the actual SDK functions. Only their POSIX results
// are injected; the real file is still closed so every test releases resources.
extern "C" int __wrap_fflush(FILE* f){int r=__real_fflush(f);if(!strcmp(stage,"flush")){errno=injectedError;return EOF;}return r;}
extern "C" int __wrap_fsync(int fd){int old=errno;int r=__real_fsync(fd);if(!strcmp(stage,"sync")){errno=injectedError;return -1;}if(!preserveErrnoAcrossSuccessfulSync)errno=0;else errno=old;return r;}
extern "C" int __wrap_fclose(FILE* f){int r=__real_fclose(f);if(!strcmp(stage,"close")){errno=injectedError;return EOF;}return r;}
int main(int argc,char**argv){CHECK(argc==2);testRoot=argv[1];std::filesystem::create_directories(testRoot);
 // Missing-path File is a nonnull SDK FileImpl with a false File bool.
 // Full main recovery demonstrates why a retained invalid member blocked remount.
 reset();Tab5Map._offlineFile=tab5FS.open("/missing-map-pack");CHECK(!Tab5Map._offlineFile);
 unsigned failedRefs=tab5SdOpenFiles();tab5StorageRequestRecovery();serviceSdRecovery(3001);
 bool invalidMemberBlocked=sdEnds==0;unsigned refsAfterAttempt=tab5SdOpenFiles();
#ifdef EXPECT_FIXED
 CHECK(!invalidMemberBlocked&&failedRefs==0&&refsAfterAttempt==0&&sdBegins==1);
#else
 CHECK(invalidMemberBlocked&&failedRefs==1&&refsAfterAttempt==1);
#endif
 Tab5Map._offlineFile.close();reset();
 // Auto recovery must close UI-only modal leases. Never bypass a real worker.
 Tab5FilePicker::held.reset(new Tab5StorageUse(true));CHECK(*Tab5FilePicker::held);
 tab5StorageRequestRecovery();serviceSdRecovery(3001);bool pickerBlocked=sdEnds==0;
#ifdef EXPECT_FIXED
 CHECK(!pickerBlocked&&sdBegins==1);
#else
 CHECK(pickerBlocked&&sdBegins==0);
#endif
 reset();
 // Active worker lease always prevents unmount and preserves retry budget.
 {Tab5StorageUse worker(true);CHECK(worker);tab5StorageRequestRecovery();serviceSdRecovery(3001);CHECK(sdEnds==0);CHECK(tab5StorageRecoveryAttempts()==0);}
 serviceSdRecovery(3002);CHECK(sdEnds==1&&sdBegins==1);CHECK(tab5StorageLastError()==0);
 reset();
 // Live local playback, unsaved recording/writing and print gates keep ownership.
 for(unsigned kind=0;kind<4;++kind){
  reset();if(kind==0)Tab5Audio.idle=false;if(kind==1)Tab5Writing.idle=false;if(kind==2)Tab5ImageEditor::busyState=true;if(kind==3)Tab5MapPrint::state=true;
  tab5StorageRequestRecovery();serviceSdRecovery(3001);CHECK(sdEnds==0&&tab5StorageRecoveryAttempts()==0);
 }
 reset();
 unsigned falseProbeSuccesses=0;
 for(const char* failure:{"flush","sync","close"})for(int error:{EIO,ENOSPC,ENOMEM,EROFS}){
  reset();stage=failure;injectedError=error;bool verified=runProbe();stage="";
  if(verified)++falseProbeSuccesses;
#ifdef EXPECT_FIXED
  CHECK(!verified&&tab5StorageLastError()==error);
#endif
  CHECK(tab5SdOpenFiles()==0);
 }
#ifdef EXPECT_FIXED
 CHECK(falseProbeSuccesses==0);
#else
 CHECK(falseProbeSuccesses>0);
#endif
 // A real SDK flush error followed by errno-clearing fsync must survive too.
 reset();stage="flush";injectedError=ENOSPC;preserveErrnoAcrossSuccessfulSync=false;
 bool hiddenFlushSuccess=runProbe();stage="";preserveErrnoAcrossSuccessfulSync=true;
#ifdef EXPECT_FIXED
 CHECK(!hiddenFlushSuccess&&tab5StorageLastError()==ENOSPC);
#endif
 reset();CHECK(runProbe());CHECK(tab5SdOpenFiles()==0);
 // A failed probe must retry only transport failures, matching the central
 // classifier. Cleanup failures force both controllers down the failure path.
 unsigned nonTransportReattempts=0;
 for(int error:{EEXIST,EINVAL}){
  reset();injectedRemoveError=error;tab5StorageRequestRecovery();serviceSdRecovery(3001);injectedRemoveError=0;
  CHECK(sdEnds==1&&sdBegins==1&&tab5StorageRecoveryAttempts()==1);
  CHECK(tab5StorageLastError()==error);
  const bool due=tab5StorageRecoveryDue(100000);if(due)++nonTransportReattempts;
  serviceSdRecovery(100000);
#ifdef EXPECT_FIXED
  CHECK(!due&&sdEnds==1&&sdBegins==1);
#else
  CHECK(due&&sdEnds==2&&sdBegins==2);
#endif
 }
#ifdef EXPECT_FIXED
 CHECK(nonTransportReattempts==0);
#else
 CHECK(nonTransportReattempts==2);
#endif
 unsigned transportReattempts=0;
 for(int error:{EIO,ENODEV,ENXIO,ETIMEDOUT}){
  reset();injectedRemoveError=error;tab5StorageRequestRecovery();serviceSdRecovery(3001);injectedRemoveError=0;
  CHECK(sdEnds==1&&sdBegins==1&&tab5StorageRecoveryAttempts()==1);
  CHECK(tab5StorageLastError()==error);CHECK(tab5StorageRecoveryDue(100000));
  serviceSdRecovery(100000);CHECK(sdEnds==2&&sdBegins==2);++transportReattempts;
 }
 CHECK(transportReattempts==4);
 // A real rejected worker may publish EBUSY after the probe reports EIO.
 // A global last-event diagnostic must not become this operation's result.
 reset();injectedRemoveError=EIO;concurrentDiagnostic=true;
 unsigned beforeInjections=diagnosticInjections;
 tab5StorageRequestRecovery();serviceSdRecovery(3001);injectedRemoveError=0;
 CHECK(diagnosticInjections==beforeInjections+1);CHECK(tab5StorageLastError()==EBUSY);
 const bool diagnosticRaceSuppressedTransportRetry=!tab5StorageRecoveryDue(100000);
#ifdef EXPECT_RACE_BUG
 CHECK(diagnosticRaceSuppressedTransportRetry);
#else
 CHECK(!diagnosticRaceSuppressedTransportRetry);
#endif
 // The command itself is asynchronous and non-destructive until service owns exclusivity.
 reset();CHECK(std::string(storageSdAction(false))=="Reconnexion SD demandée");CHECK(sdEnds==0);serviceSdRecovery(3001);CHECK(sdEnds==1);
 reset();documentDirty=true;CHECK(std::string(storageSdAction(true))=="Enregistrez les documents et attendez la fin des écritures");CHECK(sdEnds==0);
 reset();CHECK(std::string(storageSdAction(true))=="Carte SD éjectée : vous pouvez la retirer");CHECK(sdEnds==1&&!sdReady);CHECK(!tab5StorageRecoveryDue(999999));
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"failedOpenRefs\":"<<failedRefs<<",\"invalidMemberBlockedRecovery\":"<<(invalidMemberBlocked?"true":"false")<<",\"pickerBlockedRecovery\":"<<(pickerBlocked?"true":"false")<<",\"falseProbeSuccesses\":"<<falseProbeSuccesses<<",\"hiddenFlushSuccess\":"<<(hiddenFlushSuccess?"true":"false")<<",\"nonTransportReattempts\":"<<nonTransportReattempts<<",\"transportReattempts\":"<<transportReattempts<<",\"diagnosticRaceSuppressedTransportRetry\":"<<(diagnosticRaceSuppressedTransportRetry?"true":"false")<<",\"refsAfter\":"<<tab5SdOpenFiles()<<",\"physicalDeviceTested\":false}"<<std::endl;
}
