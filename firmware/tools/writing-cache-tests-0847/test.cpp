#include "WriterDocument.h"
#include "WriterIndex.h"
#include "WriterSyntaxIndex.h"
#include "WriterRich.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
static unsigned checks;
static size_t liveBytes=0,allocationCalls=0;
static std::unordered_map<void*,size_t> allocations;
static bool failNext=false;
static void check(bool ok,const char* why){++checks;if(!ok){fprintf(stderr,"FAIL %u: %s\n",checks,why);abort();}}
void* heap_caps_malloc(size_t n,unsigned caps){check(caps==3,"writer allocations remain PSRAM-capable");++allocationCalls;if(failNext){failNext=false;return nullptr;}void* p=malloc(n);if(p){allocations[p]=n;liveBytes+=n;}return p;}
void heap_caps_free(void* p){if(!p)return;auto i=allocations.find(p);check(i!=allocations.end(),"allocation freed exactly once");liveBytes-=i->second;allocations.erase(i);free(p);}
struct MemoryTotals{size_t internal=0,psram=0;};
static MemoryTotals tab5MemoryTotals(){return {128*1024,32*1024*1024-liveBytes};}
struct Job{bool active=false;unsigned steps=0;bool busy()const{return active;}void step(){++steps;}};
struct Scanner{void service(uint32_t,bool){}bool busy()const{return false;}};
struct Workspace{unsigned count()const{return 0;}};
struct View{writing::RichText text;size_t anchor=0,caret=0;};
class Tab5WritingApp{
 public:
 struct Impl{
  enum{MenuDocuments=1};
  bool built=true,visible=false,split=false,headingsDirty=false,statsDirty=false,sessionDirty=false;
  unsigned resumePhase=0,openMenu=0,documentsMenuCount=0,outlineAfterIndex=0;
  uint32_t lastWorkspaceSave=0,lastEdit=0,lastAttempt=0,lastSession=0,sessionRetryDelay=5000,lastStatus=0;
  size_t cachedTitleCursor=0,refStart=0,start=0,end=0,cursor=0,lastWords=0,lastCharacters=0;
  int deferredEdge=0,referenceName=0;void* modal=nullptr;void* tableGrid=nullptr;
  unsigned saves=0,sessionSaves=0,positionCaptures=0,workspaceWrites=0;
  Job saveJob,sessionJob;writing::Document document,reference;
  Scanner workspaceScanner;Workspace workspace;writing::DocumentIndex mainIndex,referenceIndex;
  writing::SyntaxIndex syntaxIndex,referenceSyntaxIndex;
  std::vector<writing::Heading>& refHeadings=referenceIndex.titles;
  View view;std::string path="/Documents/notes.cpp",referencePath="/Documents/ref.py";
  void serviceBrowser(){}void serviceResume(){}void observeSave(){}void serviceSession(){if(sessionJob.busy())sessionJob.step();}
  void serialService(uint32_t){}void serviceConsole(uint32_t){}void closeMenu(bool){}void toggleMenu(unsigned){}
  void capturePosition(){++positionCaptures;}bool persistWorkspace(){++workspaceWrites;return true;}
  void save(){++saves;saveJob.active=true;}void navigate(bool,int,bool){}
  void indexHeadings(){cachedTitleCursor=SIZE_MAX;mainIndex.reset();mainIndex.step(document);headingsDirty=false;}
  void setLabel(int,const char*){}std::string breadcrumb(std::string,const std::vector<writing::Heading>&,size_t,unsigned){return "";}
  void outline(bool){}void render(){}void saveSession(){++sessionSaves;}void updateStatus(){}
  void serviceSyntax(){if(!visible)return;syntaxIndex.step(document,writing::SyntaxLanguage::Cpp);referenceSyntaxIndex.step(reference,writing::SyntaxLanguage::Python);}
 };
 Impl* impl_=nullptr;
 void service(uint32_t now,bool autosave,uint32_t delayMs);
 size_t reclaimHiddenIndexes();
};
/* production_service */
/* production_reclaim */
static std::string content(const writing::Document& doc){std::string s(doc.size(),' ');doc.copy(0,s.data(),s.size());return s;}
static void ready(writing::DocumentIndex& index,const writing::Document& doc){for(unsigned i=0;!index.ready();++i){check(i<1000,"structural index bounded completion");index.step(doc);}}
static void syntaxReady(writing::SyntaxIndex& index,const writing::Document& doc,writing::SyntaxLanguage language){for(unsigned i=0;index.scannedBytes()<doc.size();++i){check(i<1000,"lexical index bounded completion");index.step(doc,language);}}
static std::string fixture(unsigned rows){std::string text;for(unsigned i=0;i<rows;++i){text+="# Heading "+std::to_string(i)+"\n/* multiline\ncode α */\nint value = "+std::to_string(i)+";\n";}return text;}
static void hiddenAndAutosave(){Tab5WritingApp app;Tab5WritingApp::Impl s;app.impl_=&s;const auto input=fixture(1600);check(s.document.assign(input.data(),input.size()),"main document assigned");check(s.reference.assign(input.data(),input.size()),"reference assigned");s.document.markSaved(s.document.revision());check(s.document.replace(0,0,"// dirty\n",9,0,9,1000),"unsaved edit created");s.mainIndex.step(s.document);s.referenceIndex.step(s.reference);
 const size_t beforeMain=s.mainIndex.scannedBytes(),beforeRef=s.referenceIndex.scannedBytes(),beforeCalls=allocationCalls;
 for(unsigned i=0;i<1000;++i)app.service(1000+i,false,5000);
#if TAB5_TEST_FIXED
 check(s.mainIndex.scannedBytes()==beforeMain&&s.referenceIndex.scannedBytes()==beforeRef,"hidden structural indexing remains paused at exact partial position");check(allocationCalls==beforeCalls,"hidden idle service creates no writer index allocation");
#else
 check(s.mainIndex.scannedBytes()>beforeMain&&s.referenceIndex.scannedBytes()>beforeRef,"baseline continues scanning hidden documents");check(allocationCalls>beforeCalls,"baseline rebuilds structural caches while hidden");
#endif
 check(s.saves==0&&s.document.dirty(),"autosave-disabled hidden text stays unsaved");
 s.lastEdit=s.lastAttempt=10000;app.service(14999,true,5000);check(s.saves==0,"autosave respects user delay while hidden");app.service(15000,true,5000);check(s.saves==1&&s.saveJob.busy(),"enabled hidden autosave still starts");app.service(16000,false,5000);check(s.saveJob.steps==1,"already requested save continues after setting is disabled");s.sessionJob.active=true;app.service(16001,false,5000);check(s.sessionJob.steps==1,"pending session transaction continues while hidden");
 const size_t beforeVisible=s.mainIndex.scannedBytes();s.visible=true;app.service(16002,false,5000);
#if TAB5_TEST_FIXED
 check(s.mainIndex.scannedBytes()>beforeVisible&&s.mainIndex.scannedBytes()-beforeVisible<=4096,"visible service resumes index using normal chunk budget");
#else
 check(s.mainIndex.ready()&&s.mainIndex.scannedBytes()==beforeVisible,"baseline hidden indexing already completed");
#endif
 ready(s.mainIndex,s.document);ready(s.referenceIndex,s.reference);check(s.mainIndex.words()==s.document.words()&&s.mainIndex.characters()==s.document.characters(),"resumed index statistics match complete document");check(content(s.document)=="// dirty\n"+input,"service preserved all document bytes");}
#if TAB5_TEST_FIXED
static size_t reclaimCycle(unsigned iteration){Tab5WritingApp app;Tab5WritingApp::Impl s;app.impl_=&s;const auto input=fixture(180);check(s.document.assign(input.data(),input.size()),"main assigned");check(s.reference.assign(input.data(),input.size()),"reference assigned");s.document.markSaved(s.document.revision());check(s.document.replace(0,0,"// edit\n",8,0,8,1000),"undo entry created");s.view.text.parse("selected text");s.view.anchor=2;s.view.caret=8;
 s.mainIndex.step(s.document);s.referenceIndex.step(s.reference);syntaxReady(s.syntaxIndex,s.document,writing::SyntaxLanguage::Cpp);syntaxReady(s.referenceSyntaxIndex,s.reference,writing::SyntaxLanguage::Python);
 writing::SyntaxState expected;check(s.syntaxIndex.stateAt(s.document,writing::SyntaxLanguage::Cpp,s.document.size()-1,expected),"full lexical context available");
 const size_t bytes=s.syntaxIndex.bytes()+s.referenceSyntaxIndex.bytes();const auto revision=s.document.revision();const auto history=s.document.historyBytes();const auto mainScanned=s.mainIndex.scannedBytes(),refScanned=s.referenceIndex.scannedBytes();const auto documentBytes=content(s.document),referenceBytes=content(s.reference),richBytes=s.view.text.markdown();
 s.visible=true;check(app.reclaimHiddenIndexes()==0&&s.syntaxIndex.bytes()+s.referenceSyntaxIndex.bytes()==bytes,"visible cache never reclaimed");s.visible=false;s.modal=&app;check(!app.reclaimHiddenIndexes()&&s.syntaxIndex.bytes()>0,"modal retains cache");s.modal=nullptr;s.tableGrid=&app;check(!app.reclaimHiddenIndexes()&&s.syntaxIndex.bytes()>0,"table editor retains cache");s.tableGrid=nullptr;s.resumePhase=1;check(!app.reclaimHiddenIndexes()&&s.syntaxIndex.bytes()>0,"resuming document retains cache");s.resumePhase=0;
 check(app.reclaimHiddenIndexes()==bytes,"pressure trim returns actual lexical PSRAM gains");check(!s.syntaxIndex.bytes()&&!s.referenceSyntaxIndex.bytes(),"both lexical arrays freed");check(s.mainIndex.scannedBytes()==mainScanned&&s.referenceIndex.scannedBytes()==refScanned,"structural partial state deliberately retained");check(!app.reclaimHiddenIndexes(),"repeated trim is idempotent");
 for(unsigned i=0;i<20;++i)app.service(100+i,false,5000);check(!s.syntaxIndex.bytes()&&!s.referenceSyntaxIndex.bytes(),"hidden service does not repopulate reclaimed checkpoints");check(content(s.document)==documentBytes&&content(s.reference)==referenceBytes&&s.document.revision()==revision&&s.document.historyBytes()==history&&s.document.dirty(),"trim preserves documents revision dirty bit and undo");check(s.view.text.markdown()==richBytes&&s.view.anchor==2&&s.view.caret==8,"trim preserves view text and selection");
 if(iteration%10==0){failNext=true;writing::SyntaxState state;check(!s.syntaxIndex.stateAt(s.document,writing::SyntaxLanguage::Cpp,9000,state)&&s.syntaxIndex.failed(),"failed optional checkpoint allocation is recoverable");check(content(s.document)==documentBytes&&s.document.canUndo(),"OOM cannot discard edits or undo");s.syntaxIndex.discard();}
 s.visible=true;syntaxReady(s.syntaxIndex,s.document,writing::SyntaxLanguage::Cpp);writing::SyntaxState rebuilt;check(s.syntaxIndex.stateAt(s.document,writing::SyntaxLanguage::Cpp,s.document.size()-1,rebuilt)&&rebuilt==expected,"rebuilt multiline context equals original");size_t caret=0;check(s.document.undo(caret)&&content(s.document)==input,"undo survives cache trim");check(s.document.redo(caret)&&content(s.document)==documentBytes,"redo survives cache trim");return bytes;}
#endif
int main(){hiddenAndAutosave();check(allocations.empty()&&liveBytes==0,"hidden service scenario releases all tracked model memory");size_t reclaimed=0;
#if TAB5_TEST_FIXED
 for(unsigned i=0;i<100;++i){reclaimed=reclaimCycle(i);check(allocations.empty()&&liveBytes==0,"cycle returns all tracked writer allocations");}
#endif
 printf("{\"passed\":true,\"checks\":%u,\"cycles\":%u,\"lexicalCacheBytesPerCycle\":%zu,\"trackedBytesAtEnd\":%zu}\n",checks,TAB5_TEST_FIXED?100:0,reclaimed,liveBytes);}
