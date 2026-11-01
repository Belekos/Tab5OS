// Runs the complete production service/sleep methods. UI, storage transactions
// and indexing are boundary doubles; no target-board or LVGL claim is made.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Document {bool modified=true;bool dirty()const{return modified;}size_t lineStart(size_t p){return p;}bool boundary(size_t){return true;}};
struct Job {bool active=false;unsigned steps=0;bool busy()const{return active;}void step(){++steps;}};
struct Fs {bool active=false;bool opened()const{return active;}};
struct Browser {bool scanning(){return false;}};
struct Scanner {void service(uint32_t,bool){}bool busy()const{return false;}bool storageIdle()const{return true;}};
struct Workspace {unsigned count()const{return 0;}};
struct Index {bool ready()const{return true;}void step(Document&){}unsigned words(){return 0;}unsigned characters(){return 0;}};
struct Text {size_t sourceAt(size_t p){return p;}};
struct View {size_t anchor=0,caret=0;Text text;};
class Tab5WritingApp {
public:
 struct Impl {
  enum{MenuDocuments=1};
  bool built=true,visible=false,split=false,headingsDirty=false,statsDirty=false,sessionDirty=false;
  unsigned resumePhase=0,openMenu=0,documentsMenuCount=0,outlineAfterIndex=0;
  uint32_t lastWorkspaceSave=0,lastEdit=0,lastAttempt=0,lastSession=0,sessionRetryDelay=5000,lastStatus=0;
  size_t cachedTitleCursor=0,refStart=0,start=0,end=0,cursor=0,lastWords=0,lastCharacters=0;
  int deferredEdge=0,referenceName=0,refHeadings=0;
  unsigned saves=0,sessionSaves=0,positionCaptures=0,workspaceWrites=0;
  Job saveJob,sessionJob;Fs fs,resumeFs,sessionFs,workspaceFs;Browser* browser=nullptr;
  Document document,reference;Scanner workspaceScanner;Workspace workspace;Index mainIndex,referenceIndex;View view;
  std::string path="/Documents/roman.md",referencePath;
  void serviceBrowser(){}void serviceResume(){}void observeSave(){}void serviceSession(){}
  void serialService(uint32_t){}void serviceConsole(uint32_t){}void closeMenu(bool){}void toggleMenu(unsigned){}
  void capturePosition(){++positionCaptures;}bool persistWorkspace(){++workspaceWrites;return true;}
  void save(){++saves;saveJob.active=true;}void navigate(bool,int,bool){}void indexHeadings(){}
  void setLabel(int,const char*){}std::string breadcrumb(std::string,int,size_t,unsigned){return "";}
  void outline(bool){}void render(){}void saveSession(){++sessionSaves;}void updateStatus(){}
 };
 Impl* impl_=nullptr;
 void service(uint32_t now,bool autosave,uint32_t delayMs);
 bool storageIdle()const;
 bool prepareSleep();
};
/* production_service */
/* production_storageIdle */
/* production_prepareSleep */
int main(){
 Tab5WritingApp app;app.service(100000,true,5000);CHECK(app.prepareSleep());
 Tab5WritingApp::Impl s;app.impl_=&s;
 // With autosave disabled, elapsed time and dirty content must never request a save.
 for(uint32_t t=0;t<=120000;t+=1000)app.service(t,false,5000);
 CHECK(s.saves==0);CHECK(s.document.dirty());CHECK(s.path=="/Documents/roman.md");
 // Preparing sleep must preserve unsaved edits and cannot silently save them.
 CHECK(!app.prepareSleep());CHECK(s.saves==0);CHECK(s.document.dirty());
 // Existing autosave still waits for the configured delay then starts one save.
 s.lastEdit=120000;s.lastAttempt=120000;app.service(124999,true,5000);CHECK(s.saves==0);
 app.service(125000,true,5000);CHECK(s.saves==1);CHECK(s.saveJob.busy());
 app.service(126000,true,5000);CHECK(s.saves==1);CHECK(s.saveJob.steps==1);
 // Disabling the setting does not corrupt an already requested transaction.
 app.service(127000,false,5000);CHECK(s.saves==1);CHECK(s.saveJob.steps==2);
 s.saveJob.active=false;s.document.modified=false;app.service(140000,true,5000);CHECK(s.saves==1);
 CHECK(app.prepareSleep());CHECK(s.saves==1);
 // No autosave before initial resume completes or before app initialization.
 s.document.modified=true;s.resumePhase=1;app.service(180000,true,5000);CHECK(s.saves==1);
 s.resumePhase=0;s.built=false;app.service(190000,true,5000);CHECK(s.saves==1);
 // Pending explicit storage work still blocks sleep.
 s.built=true;s.document.modified=false;s.fs.active=true;CHECK(!app.storageIdle());CHECK(!app.prepareSleep());
 s.fs.active=false;s.saveJob.active=true;CHECK(!app.prepareSleep());
 std::printf("PASS %u checks\n",checks);
}
