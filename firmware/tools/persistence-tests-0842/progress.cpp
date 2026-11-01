#include "Arduino.h"
#include "FS.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
static fs::FS tab5FS;
static bool sdReady=true,readerActive=true,readerIsEpub=false,readerPageDisplayed=true,readerProgressDirty=true;
static String readerPath="/Books/book.txt";
static const char* PROGRESS_DIR="/System/progress";
static const void* readerContent=reinterpret_cast<void*>(1);static const char* readerText="text";
static unsigned readerPageIndex=0,readerRichPageStartCount=1,readerPageStartCount=1,readerChapter=4;
static struct{unsigned blockCount=0;}readerRichDoc;
static struct{uint16_t block;uint32_t offset;}readerRichPageStarts[]={{2,700}};
static uint32_t readerPageStarts[]={600};
static int32_t lv_obj_get_scroll_y(const void*){return -900;}
static void tab5StorageFault(const char*,const char*,int=0){}
static void delay(int){}static void toast(const char*){}
namespace Tab5Files {inline bool userWritable(const char*){return true;}}
#include "Tab5AtomicFile.h"
/* CONTROLLERS */
void reset(){tab5FS=fs::FS{};tab5FS.seedDir(PROGRESS_DIR);readerProgressDirty=true;}
bool preserved(const std::string& bytes){for(auto& f:tab5FS.tree)if(!f.second.directory&&f.second.contents==bytes)return true;return false;}
int main(int argc,char**){const bool baseline=argc>1;const String book="/Books/book.txt";const String path=readerProgressPath(book);const std::string original="path=/Books/book.txt\nchapter=4\nblock=2\noffset=700\nscroll=-900\n";
 bool readSafe=true;unsigned readFaults=0,stalledReads=0;
 for(size_t byte=0;byte<original.size();++byte){reset();tab5FS.seedFile(path.c_str(),original);tab5FS.failReadAfter=byte;uint16_t c=99,b=99;uint32_t o=99;int32_t y=99;bool rich=false;bool ok=false;try{ok=loadReaderProgress(book,c,b,o,y,rich);}catch(const fs::ReadStall&){++stalledReads;readSafe=false;}readSafe&=!ok&&c==99&&b==99&&o==99&&y==99&&!rich;++readFaults;}
 reset();tab5FS.seedFile((path+".bak").c_str(),original);uint16_t c=0,b=0;uint32_t o=0;int32_t y=0;bool rich=false;bool backupLoaded=loadReaderProgress(book,c,b,o,y,rich)&&c==4&&b==2&&o==700&&y==-900&&rich;
 bool saveSafe=true;for(unsigned kind=0;kind<3;++kind){reset();tab5FS.seedFile(path.c_str(),original);if(kind==2)tab5FS.closeError=true;else if(kind)tab5FS.flushError=true;else tab5FS.failWriteAfter=5;saveReaderProgressNow();saveSafe&=tab5FS.tree[path.c_str()].contents==original&&readerProgressDirty;}
 bool atomicTextSafe=true;for(unsigned kind=0;kind<3;++kind){reset();tab5FS.seedFile("/document.txt","original text");if(kind==2)tab5FS.closeError=true;else if(kind)tab5FS.flushError=true;else tab5FS.failWriteAfter=2;bool ok=atomicWriteText("/document.txt","replacement",11);atomicTextSafe&=!ok&&tab5FS.tree["/document.txt"].contents=="original text";}
 const String moved="/Books/moved.txt",dest=readerProgressPath(moved);bool migrationSafe=true;
 for(unsigned fault=1;fault<=2;++fault){reset();tab5FS.seedFile(path.c_str(),original);tab5FS.seedFile(dest.c_str(),"previous destination");tab5FS.failRenameAt=fault;migrateReaderProgress(book,moved);migrationSafe&=tab5FS.tree[path.c_str()].contents==original&&preserved("previous destination");}
 bool commitSafe=true;for(unsigned point=1;point<=30;++point){reset();tab5FS.seedFile(path.c_str(),original);tab5FS.failAt=point;saveReaderProgressNow();commitSafe&=preserved(original);}
 bool all=readSafe&&backupLoaded&&saveSafe&&atomicTextSafe&&migrationSafe&&commitSafe;
 std::cout<<"{\"partialReadsNeverPublishPosition\":"<<readSafe<<",\"partialReadFaults\":"<<readFaults<<",\"stalledReadLoops\":"<<stalledReads<<",\"backupPositionRecoverable\":"<<backupLoaded<<",\"shortWriteAndFlushDoNotCommit\":"<<saveSafe<<",\"documentFlushFailurePreserved\":"<<atomicTextSafe<<",\"migrationRenameFailurePreservesBothPositions\":"<<migrationSafe<<",\"commitFaultsPreservePreviousPosition\":"<<commitSafe<<",\"commitFaultInjectionPoints\":30,\"passed\":"<<all<<"}\n";return baseline||all?0:1;
}
