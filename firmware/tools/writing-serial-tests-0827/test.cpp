#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <lvgl.h>
#define private public
#include "Tab5Writing.h"
#undef private
#include "WriterDocument.h"
#include "WriterRich.h"
#include "WriterView.h"
#include "WriterSyntax.h"
#include "WriterIndex.h"
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5ContextMenu.h"
#include "Tab5UiStyle.h"
#include "Tab5Icons.h"
#include "Tab5Serial.h"
#if !BASELINE
#include "Tab5SerialText.h"
#endif

static unsigned checks=0;
static void check(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
using String=std::string;
static uint32_t clockMs=100;
static constexpr int FullHeight=672;
static uint32_t millis(){return clockMs;}
namespace writing { bool usbConnected(){return true;} }
namespace Tab5Serial {
static Config cfg;
static Stats state;
static std::string transmitted;
static std::deque<char> incoming;
static unsigned reads=0,claims=0,releases=0,cancels=0,requests=0;
static bool refused=false,configureRefused=false;
Config config(){return cfg;}
Stats stats(){auto out=state;out.config=cfg;out.available=incoming.size();return out;}
bool claimReceive(Client owner){++claims;if(!state.ready||cfg.mode!=Mode::Data||state.receiver!=Client::None&&state.receiver!=owner)return false;state.receiver=owner;return true;}
void releaseReceive(Client owner){++releases;if(state.receiver==owner)state.receiver=Client::None;}
size_t read(void* data,size_t n,Client owner){++reads;if(state.receiver!=owner)return 0;n=std::min(n,incoming.size());for(size_t i=0;i<n;++i){static_cast<char*>(data)[i]=incoming.front();incoming.pop_front();}state.received+=n;return n;}
bool sendSnapshot(size_t size,const std::function<void(char*,size_t)>& fill,Client owner){if(refused||!state.ready||cfg.mode!=Mode::Data||size>MaxSnapshot||state.pending)return false;transmitted.assign(size,'\0');fill(transmitted.data(),size);state.pending=size;state.sender=owner;return true;}
void cancelSend(Client owner){++cancels;if(state.sender==owner){state.pending=0;state.sender=Client::None;}}
bool requestConfigure(const Config& value,bool){++requests;if(configureRefused)return false;cfg=value;state.config=value;return true;}
static void reset(){cfg=Config{};cfg.mode=Mode::Data;state=Stats{};state.config=cfg;state.ready=state.connected=true;transmitted.clear();incoming.clear();reads=claims=releases=cancels=requests=0;refused=configureRefused=false;}
static void feed(const std::string& text){for(char c:text)incoming.push_back(c);}
}

// Exact active editor controllers are injected below. Storage/session autosave
// and unrelated editor actions are explicit no-op boundaries.
struct Tab5WritingApp::Impl {
 static Impl* instance;
 WritingHooks hooks;
 struct Workspace {void begin(){}void load(int){} } workspace;
 struct Scanner {void begin(Workspace&){}void service(uint32_t,bool){} } workspaceScanner;
 int workspaceFs=0;
 lv_style_t panelStyle{},buttonStyle{},selectedStyle{},referenceStyle{},mutedStyle{};
 struct Caption{lv_obj_t* object;std::string source;bool placeholder;};
 std::vector<Caption> captions;
 lv_obj_t *root=nullptr,*toolbar=nullptr,*body=nullptr,*mainPane=nullptr,*mainScroll=nullptr;
 lv_obj_t *mainPathButton=nullptr,*filename=nullptr,*mainPrev=nullptr,*mainNext=nullptr;
 lv_obj_t *refPane=nullptr,*refScroll=nullptr,*refPathButton=nullptr,*referenceName=nullptr;
 lv_obj_t *consolePane=nullptr,*consoleText=nullptr,*printButton=nullptr,*proxy=nullptr,*status=nullptr;
 lv_obj_t *target=nullptr,*menuShade=nullptr,*modalList=nullptr,*tableGrid=nullptr;
 lv_obj_t* menuButtons[8]{};
 void* browser=nullptr;
 char* consoleCache=nullptr;
 static constexpr int FullHeight=672;
 int Width=1216,height=FullHeight,lastLayoutHeight=-1,lastLayoutWidth=-1,deferredEdge=0;
 bool built=false,visible=true,keyboard=false,split=false,consoleVisible=false;
 bool lastLayoutSplit=false,lastLayoutConsole=false,layoutReclaimed=false,documentSelection=false;
 bool pageLiteral=false,statsDirty=false,sessionDirty=false,headingsDirty=false,codeDraft=false;
 int reclaimedMainScroll=0,reclaimedRefScroll=0;
 size_t cachedTitleCursor=SIZE_MAX,selectionAnchor=0,selectionCaret=0,start=0,end=0,cursor=0;
 uint32_t lastEdit=0;
 std::string pageSource,path="/Documents/test.txt",message;
 writing::Document document,reference;
 writing::DocumentIndex mainIndex;
 writing::RichView view,refView;
 unsigned renders=0,keyboardCalls=0;
 /* enum Action { */
 /* const char* tr( */
 /* uint32_t color( */
 /* writing::ViewTheme viewTheme( */
 /* static void setLabel( */
 /* void caption( */
 /* const lv_font_t* font( */
 /* void panel( */
 /* lv_obj_t* button( */
 /* lv_obj_t* label( */
 /* bool changedRich() */
 /* bool replaceSelection( */
 const char* actionName(int){return "action";}
 void note(const char* text){message=text?text:"";}
 bool isCode()const{return codeDraft;}
 void tableDialog(size_t){}
 void followLink(const char*){}
 void syncStyle(){}
 void updateStatus(){}
 void resume(){}
 void renderReference(){}
 void render(){++renders;pageSource.assign(document.size()-start,'\0');document.copy(start,pageSource.data(),pageSource.size());end=document.size();view.load(pageSource);}
 void releaseConsole(){consoleCache=nullptr;}
 void closeDialog(){browser=nullptr;}
 void closeMenu(bool){}
 void showKeyboard(bool on,lv_obj_t* object=nullptr){++keyboardCalls;keyboard=on;if(object)target=object;if(hooks.keyboard)hooks.keyboard(on,object);}
 writing::RichView& activeView(){return view;}
 /* serial */
 void act(int action){
#if !BASELINE
  switch(action){/* serialDispatch */default:break;}
#endif
 }
 /* void build() */
 /* void layout() */
 void init(lv_obj_t* parent){
  instance=this;root=lv_obj_create(parent);lv_obj_remove_style_all(root);
  for(auto* style:{&panelStyle,&buttonStyle,&selectedStyle,&referenceStyle,&mutedStyle})lv_style_init(style);
  hooks.font=[](uint16_t size,uint8_t){return size<=20?&lv_font_montserrat_20:&lv_font_montserrat_24;};
  build();
 }
};
Tab5WritingApp::Impl* Tab5WritingApp::Impl::instance=nullptr;
/* void Tab5WritingApp::show( */
/* void Tab5WritingApp::resize( */
size_t Tab5WritingApp::reclaimHiddenLayout(){if(impl_){impl_->view.discardLayout();impl_->refView.discardLayout();impl_->layoutReclaimed=true;}return 0;}

static bool hasLabel(lv_obj_t* root,const char* label){if(lv_obj_check_type(root,&lv_label_class)&&std::strcmp(lv_label_get_text(root),label)==0)return true;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(hasLabel(lv_obj_get_child(root,i),label))return true;return false;}
static void click(lv_obj_t* button){lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);lv_obj_update_layout(lv_screen_active());}
static std::string docText(const writing::Document& doc){std::string out(doc.size(),'\0');doc.copy(0,out.data(),out.size());return out;}
static void loadDoc(Tab5WritingApp::Impl& s,const std::string& text,size_t page=0,size_t length=4096){
 check(s.document.assign(text.data(),text.size()),"fixture document allocated");
 s.document.markSaved(s.document.revision());s.start=page;s.end=std::min(text.size(),page+length);s.cursor=page;
 s.pageSource=text.substr(page,s.end-page);s.view.load(s.pageSource);s.view.anchor=s.view.caret=0;s.documentSelection=false;
}
static void inside(lv_obj_t* outer,lv_obj_t* inner,const char* message){lv_area_t a{},b{};lv_obj_get_coords(outer,&a);lv_obj_get_coords(inner,&b);bool ok=b.x1>=a.x1&&b.x2<=a.x2&&b.y1>=a.y1&&b.y2<=a.y2;if(!ok)std::fprintf(stderr,"geometry outer=(%d,%d,%d,%d) inner=(%d,%d,%d,%d)\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);check(ok,message);}

int main(){
 std::setvbuf(stdout,nullptr,_IONBF,0);
 lv_init();lv_display_create(1280,720);
 Tab5Focus.begin(lv_screen_active(),&lv_font_montserrat_20);
 auto* parent=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(parent);lv_obj_set_size(parent,1216,672);
 // Private state access is enabled only in the generated harness declaration.
 Tab5WritingApp app;
 auto* impl=new Tab5WritingApp::Impl;app.impl_=impl;impl->init(parent);
 auto& s=*impl;
#if BASELINE
 check(!hasLabel(s.toolbar,"Série"),".26 active Tab5Writing toolbar has no discoverable serial command");
 check(lv_obj_get_child_count(s.toolbar)==10,".26 toolbar exposes eight menus, keyboard and print only");
 std::printf("BASELINE .26 active editor serial entry absent: %u checks, expected regression reproduced\n",checks);
#else
 check(s.serialButton!=nullptr,"active editor has dedicated serial toolbar button");
 check(hasLabel(s.toolbar,"Série"),"serial command is visible in real Writing toolbar");
 Tab5Serial::reset();
 click(s.serialButton);
 check(s.serialVisible,"toolbar button opens the serial pane");
 check(lv_obj_get_child(s.toolbar,1)==s.serialButton,"serial command is near the left edge, before long menu list");
 const auto keyboardBefore=s.keyboardCalls;s.target=s.proxy;s.view.editing=true;
 click(s.serialButton);click(s.serialButton);
 check(s.target==s.proxy&&s.view.editing&&s.keyboardCalls==keyboardBefore,"serial pane is nonmodal and preserves document input");
 for(int width:{1216,1280})for(int height:{672,420,360}){
  lv_obj_set_width(parent,width);app.resize(height);lv_obj_update_layout(parent);
  inside(s.body,s.mainPane,"document remains inside editor after serial split and resize");
  inside(s.body,s.serialPane,"serial pane remains inside editor at keyboard heights");
  inside(s.toolbar,s.serialButton,"serial entry remains immediately visible at both workspace widths");
  check(lv_obj_get_width(s.mainScroll)>=width-446,"serial does not reduce document below designed width");
  lv_obj_scroll_to_y(s.serialPane,LV_COORD_MAX,LV_ANIM_OFF);lv_obj_update_layout(parent);
  inside(s.serialPane,s.serialInsertButton,"serial insert remains reachable when keyboard reduces height");
 }
 app.resize(672);lv_obj_scroll_to_y(s.serialPane,0,LV_ANIM_OFF);
 std::puts("writing serial: layout passed");
 Tab5Serial::cfg.mode=Tab5Serial::Mode::Debug;s.serialService(clockMs,true);
 check(lv_obj_has_state(s.serialListenButton,LV_STATE_DISABLED)&&lv_obj_has_state(s.serialSendButton,LV_STATE_DISABLED),"debug mode disables raw data controls");
 check(!lv_obj_has_state(s.serialDataButton,LV_STATE_DISABLED),"data mode transition is available in serial pane");
 click(s.serialDataButton);
 check(Tab5Serial::requests==1&&Tab5Serial::cfg.mode==Tab5Serial::Mode::Data,"data mode requests async driver setup");
 std::string large;for(int i=0;i<1600;++i)large+="ligne française 0123456789\n";
 loadDoc(s,large,8192);
 click(s.serialSendButton);
 check(Tab5Serial::transmitted==large,"no selection sends complete multipage document, not rendered page");
 check(Tab5Serial::state.sender==Tab5Serial::Client::Editor,"editor owns its asynchronous transmission");
 check(lv_obj_has_state(s.serialSendButton,LV_STATE_DISABLED)&&!lv_obj_has_state(s.serialCancelButton,LV_STATE_DISABLED),"transmit state enables only valid actions");
 const auto snapshot=Tab5Serial::transmitted;
 check(s.document.replace(0,5,"MODIF",5,0,5,clockMs),"editing during send is allowed");
 check(Tab5Serial::transmitted==snapshot,"editing cannot mutate the asynchronous transmit snapshot");
 click(s.serialCancelButton);
 check(!Tab5Serial::state.pending,"cancel stops only this editor transmit");
 loadDoc(s,"préface\nalpha été fin\nsuite\n",9);
 s.view.anchor=6;s.view.caret=9;
 click(s.serialSendButton);
 check(Tab5Serial::transmitted=="été","visible UTF-8 selection maps glyph indices to exact file bytes");
 click(s.serialCancelButton);
 s.documentSelection=true;s.selectionAnchor=15;s.selectionCaret=9;
 click(s.serialSendButton);
 check(Tab5Serial::transmitted=="alpha ","reversed document selection sends exact selected bytes");
 click(s.serialCancelButton);
 loadDoc(s,"before\nafter\n");s.documentSelection=true;s.selectionAnchor=0;s.selectionCaret=s.document.size();
 click(s.serialSendButton);check(Tab5Serial::transmitted=="before\nafter\n","select-all sends whole document without extra terminator");click(s.serialCancelButton);
 loadDoc(s,"");Tab5Serial::transmitted.clear();s.serialSend();check(Tab5Serial::transmitted.empty()&&!Tab5Serial::state.pending,"empty document starts no transfer");
 loadDoc(s,std::string(writing::Document::Limit,'x'));s.serialSend();check(Tab5Serial::state.pending==writing::Document::Limit,"maximum-size editor document can be sent in full");click(s.serialCancelButton);
 s.documentSelection=true;s.selectionAnchor=writing::Document::Limit-3;s.selectionCaret=writing::Document::Limit;
 s.serialSend();check(Tab5Serial::transmitted=="xxx","small selection in maximum-size file remains transferable");click(s.serialCancelButton);
 loadDoc(s,"editable");
 Tab5Serial::refused=true;s.serialSend();check(!Tab5Serial::state.pending&&!s.serialWasSending,"allocation or driver refusal does not leave a phantom transfer");Tab5Serial::refused=false;
 std::puts("writing serial: transmission passed");
 click(s.serialListenButton);
 check(s.serialListening&&Tab5Serial::state.receiver==Tab5Serial::Client::Editor,"listen claims exclusive receive session");
 check(lv_obj_has_state(s.serialListenButton,LV_STATE_DISABLED)&&!lv_obj_has_state(s.serialStopButton,LV_STATE_DISABLED),"listening controls show current state");
 check(lv_obj_has_state(s.serialButton,LV_STATE_CHECKED),"toolbar indicates active reception");
 Tab5Serial::feed("reçu\r\n");s.serialService(clockMs+=250);
 check(std::string(s.serialCapture->data)=="reçu\n","receive handles UTF8 and CRLF");
 check(docText(s.document)=="editable"&&!s.document.dirty(),"received stream never silently modifies document");
 check(std::string(lv_label_get_text(s.serialText))=="reçu\n","received text reaches actual LVGL pane");
 click(s.serialButton);check(!s.serialVisible&&s.serialListening,"reducing pane keeps listening active");
 const auto revision=s.serialCapture->revision;
 Tab5Serial::feed(std::string(5000,'A'));s.serialService(clockMs+=10);
 check(Tab5Serial::incoming.size()==2952,"one UI service drains at most 2048 bytes");
 check(s.serialCapture->revision>revision,"reduced pane continues bounded stream capture");
 check(s.document.replace(0,8,"modified",8,0,8,clockMs),"document remains editable while reduced receiver is active");
 s.serialService(clockMs+=10);s.serialService(clockMs+=10);check(Tab5Serial::incoming.empty(),"continuous stream drains over later UI ticks");
 click(s.serialButton);
 check(s.serialVisible&&s.serialListening,"reopening pane preserves reception session");
 s.serialAction(s.SerialClear);check(s.serialCapture->size==0&&s.serialListening,"clear reception preserves active listening");
 Tab5Serial::feed("secret");s.serialService(clockMs+=250);click(s.serialSendButton);
 check(Tab5Serial::state.pending&&s.serialListening,"simultaneous send and listen is supported");
 click(s.serialCancelButton);check(s.serialListening&&std::string(s.serialCapture->data)=="secret","cancel transmit preserves received content and listening");
 s.serialAction(s.SerialClear);Tab5Serial::feed("RX");s.serialService(clockMs+=250);
 loadDoc(s,"a OLD z");s.documentSelection=true;s.selectionAnchor=2;s.selectionCaret=5;
 click(s.serialInsertButton);
 check(docText(s.document)=="a RX z"&&s.document.dirty(),"explicit insert replaces global document selection through actual controller");
 check(s.document.undo(s.cursor)&&docText(s.document)=="a OLD z","received insertion is undoable");
 loadDoc(s,"hello");s.view.caret=s.view.anchor=5;
 click(s.serialInsertButton);
 check(docText(s.document)=="helloRX","received insertion at caret runs real RichView change and document controller");
 std::puts("writing serial: receive and insert passed");
 lv_label_set_text_selection_start(s.serialText,0);lv_label_set_text_selection_end(s.serialText,1);
 Tab5Serial::feed("later");s.serialService(clockMs+=250);
 check(std::string(lv_label_get_text(s.serialText))=="RX","selected receive text remains stable while new stream bytes are captured");
 s.serialAction(s.SerialClear);
 check(std::strlen(lv_label_get_text(s.serialText))==0&&!Tab5TextInput::hasSelection(s.serialText),"clear removes visible selected receive text immediately");
 std::string utf8;for(unsigned i=0;i<2000;++i)utf8+="€";
 Tab5Serial::feed(utf8);for(unsigned i=0;i<3;++i)s.serialService(clockMs+=250);
 check(std::strlen(lv_label_get_text(s.serialText))<=4096&&writing::validUtf8(lv_label_get_text(s.serialText),std::strlen(lv_label_get_text(s.serialText))),"display window starts on complete UTF8 character");
 check(s.serialCapture->size==6000&&std::strstr(lv_label_get_text(s.serialWarning),"Affichage partiel"),"partial display is disclosed without dropping retained receive bytes");
 s.serialAction(s.SerialClear);
 auto longLineBegin=std::chrono::steady_clock::now();
 for(unsigned i=0;i<20;++i){Tab5Serial::feed(std::string(2048,char('B'+i)));s.serialService(clockMs+=250);check(s.serialCapture->size<=Tab5Serial::TextCapture::Capacity,"long streams stay bounded");}
 std::printf("long-line receive: 20 x2048bytes, %lld ms\n",(long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-longLineBegin).count());
 check(std::strlen(lv_label_get_text(s.serialText))<=4096,"rendered receive window stays bounded independently from capture");
 check(s.serialCapture->size>4096,"render window does not truncate retained insertion capture");
 std::puts("writing serial: large receive passed");
 check(s.serialCapture->trimmed>0&&std::strstr(lv_label_get_text(s.serialWarning),"Réception tronquée"),"bounded truncation is visible to user");
 Tab5Serial::incoming.clear();Tab5Serial::feed("\xe2\x82");std::puts("writing serial: partial feed queued");s.serialService(clockMs+=250);std::puts("writing serial: partial feed service done");click(s.serialStopButton);std::puts("writing serial: partial stop clicked");
 check(!s.serialListening&&Tab5Serial::state.receiver==Tab5Serial::Client::None,"stop releases receive ownership");
 check(s.serialCapture->invalid>0,"stop terminates partial UTF8 safely");
 std::puts("writing serial: bounds and stop passed");
 Tab5Serial::state.receiver=Tab5Serial::Client::Terminal;s.serialAction(s.SerialListen);
 check(!s.serialListening&&Tab5Serial::state.receiver==Tab5Serial::Client::Terminal,"terminal receiver is not stolen by editor");
 Tab5Serial::state.receiver=Tab5Serial::Client::None;click(s.serialListenButton);
 loadDoc(s,"pending transfer");click(s.serialSendButton);const auto capture=s.serialCapture;
 app.show(false);
 check(!s.visible&&!s.serialListening&&Tab5Serial::state.receiver==Tab5Serial::Client::None,"leaving editor stops and releases receive");
 check(!Tab5Serial::state.pending,"leaving editor cancels its pending transfer");
 check(s.serialCapture==capture&&capture->size>0,"leaving editor retains bounded receive text for review");
 const auto reads=Tab5Serial::reads;Tab5Serial::feed("ignored while away");s.serialService(clockMs+=250);check(Tab5Serial::reads==reads,"hidden editor never drains driver data");
 app.show(true);check(!s.serialListening&&s.serialCapture==capture,"returning does not silently resume listening or lose captured text");
 std::puts("writing serial: first return passed");
 s.serialAction(s.SerialClear);
 for(unsigned loop=0;loop<250;++loop){
  Tab5Serial::incoming.clear();click(s.serialListenButton);check(s.serialListening,"repeat listen works after return");
  Tab5Serial::feed("cycle\n");s.serialService(clockMs+=250);app.show(false);check(!s.serialListening,"repeat close releases reader");app.show(true);s.serialService(clockMs,true);
  if(loop%50==0)std::printf("writing serial: repeat %u passed\n",loop);
 }
 s.serialStopSession();if(s.serialCapture){s.serialCapture->~TextCapture();writing::release(s.serialCapture);s.serialCapture=nullptr;}
 std::printf("CURRENT writing serial: %u checks\n",checks);
#endif
 app.show(false);
 lv_deinit();delete impl;
 return 0;
}
