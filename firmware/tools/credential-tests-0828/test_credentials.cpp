#include <lvgl.h>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <functional>
#include "WriterDocument.h"
namespace Allocator {
std::map<void*,size_t> live;std::set<void*> mustBeZero;unsigned checked=0,violations=0;
void expectZero(void* data){if(data)mustBeZero.insert(data);}
}
namespace writing {
void* allocate(size_t size){void* result=malloc(size?size:1);if(result)Allocator::live[result]=size;return result;}
void release(void* data){if(!data)return;auto found=Allocator::live.find(data);if(found==Allocator::live.end()){fprintf(stderr,"Unknown allocation\n");abort();}
 if(Allocator::mustBeZero.erase(data)){++Allocator::checked;auto* bytes=static_cast<unsigned char*>(data);for(size_t i=0;i<found->second;++i)if(bytes[i]){++Allocator::violations;break;}}
 Allocator::live.erase(found);free(data);
}
}
#include "utf8-production.inc"
#include "Tab5TextInput.cpp"
#include "Tab5UiStyle.cpp"
#include "credential-translations.inc"
class String {
 std::string value;
 public:
 String()=default;String(const char* s):value(s?s:""){}String(const std::string& s):value(s){}
 const char* c_str()const{return value.c_str();}size_t length()const{return value.size();}
 bool endsWith(const char* s)const{size_t n=strlen(s);return value.size()>=n&&value.compare(value.size()-n,n,s)==0;}
 String substring(size_t a,size_t b)const{return value.substr(a,b-a);}
 String operator+(const String& other)const{return value+other.value;}String& operator+=(const String& other){value+=other.value;return *this;}String& operator+=(char c){value+=c;return *this;}
 bool operator==(const String& s)const{return value==s.value;}bool operator!=(const String& s)const{return value!=s.value;}
};
namespace Fake {
bool unlocked=true,device=true,pin=true,legacy=false,keyBusy=false,vaultBusy=false,fail=false,envFull=false;
std::function<void()> task;bool operationOk=false,resetPending=false;
void complete(){if(task){auto action=task;task={};action();keyBusy=false;}}
std::string pinValue="1234";unsigned unlockCalls=0,legacyUnlockCalls=0,openCalls=0,configCalls=0,keyCreates=0,keyErases=0,backups=0,restores=0,cancels=0,imports=0,binds=0,listCalls=0,lastBackupSlot=0;
std::map<std::string,std::string> secrets;std::map<std::string,std::string> environment;
}
namespace Tab5KeyStore {
constexpr unsigned MaxKeys=16;
struct Info{String name,id;unsigned slot=0;};std::vector<Info> stored={{"Initial key","0123456789abcdef",0}};
bool pinConfigured(){return Fake::pin;}bool unlocked(){return Fake::unlocked;}bool usingDevice(){return Fake::device;}bool deviceConfigured(){return true;}
bool legacyAvailable(){return Fake::legacy;}bool busy(){return Fake::keyBusy;}const char* status(){return Fake::fail?"Operation failed":"Ready";}
void lock(){Fake::unlocked=false;}
bool unlockDevice(const char* pin,bool migrate=false){++Fake::unlockCalls;if(std::string(pin)!=Fake::pinValue)return false;Fake::unlocked=true;if(migrate)Fake::device=true;return true;}
bool openDevice(bool=false){++Fake::openCalls;Fake::device=true;Fake::unlocked=true;return true;}
bool configurePin(const char* pin){++Fake::configCalls;Fake::pinValue=pin;Fake::pin=*pin;return !Fake::fail;}
bool resetPending(){return Fake::resetPending;}bool operationSucceeded(){return Fake::operationOk;}
bool beginDeviceUnlock(const char* pin,bool initial,bool migrate,bool=false){std::string copy=pin?pin:"";Fake::keyBusy=true;Fake::task=[=](){Fake::operationOk=initial?openDevice(migrate)&&configurePin(copy.c_str()):(Fake::unlocked&&Fake::device)||unlockDevice(copy.c_str(),migrate);};return true;}
bool beginPinChange(const char* current,const char* next,bool=false){std::string a=current,b=next;Fake::keyBusy=true;Fake::task=[=](){Fake::operationOk=a==Fake::pinValue&&configurePin(b.c_str());};return true;}
bool beginReset(bool=false){Fake::keyBusy=true;Fake::task=[](){Fake::operationOk=true;Fake::unlocked=Fake::pin=false;Fake::secrets.clear();stored.clear();};return true;}
void finishUi(){}
bool unlock(const char*,bool){++Fake::legacyUnlockCalls;Fake::keyBusy=true;return true;}
int create(const char* name){++Fake::keyCreates;if(Fake::fail)return -1;unsigned slot=0;while(std::any_of(stored.begin(),stored.end(),[&](const Info& info){return info.slot==slot;}))++slot;stored.push_back({name,std::string("key-")+std::to_string(Fake::keyCreates),slot});return slot;}
bool erase(unsigned slot){if(Fake::fail)return false;for(auto i=stored.begin();i!=stored.end();++i)if(i->slot==slot){stored.erase(i);++Fake::keyErases;return true;}return false;}
std::vector<Info> list(){++Fake::listCalls;auto result=stored;std::sort(result.begin(),result.end(),[](const Info& a,const Info& b){return a.slot<b.slot;});return result;}
}
namespace Tab5Vault {
int keyForFile(const char*){return -2;}
bool busy(){return Fake::vaultBusy;}const char* status(){return Fake::fail?"Backup failed":"Backup complete";}
void cancel(){++Fake::cancels;Fake::vaultBusy=false;}
bool backupKey(unsigned key,const String&,const char*){++Fake::backups;Fake::lastBackupSlot=key;return Fake::vaultBusy=!Fake::fail&&key<Tab5KeyStore::MaxKeys;}
bool restoreKey(const String&,const char*){++Fake::restores;return Fake::vaultBusy=!Fake::fail;}
bool backupDevice(unsigned key,const String& file){return backupKey(key,file,"");}bool restoreDevice(const String& file){return restoreKey(file,"");}
}
namespace Tab5Secrets {
constexpr size_t MaxNameBytes=48,MaxValueBytes=512;
const char* status(){return Fake::fail?"Store failed":"Secret missing";}
bool ready(){return !Fake::fail;}
bool get(const char* name,char* out,size_t n){auto found=Fake::secrets.find(name);if(!Fake::unlocked||found==Fake::secrets.end()||found->second.size()>=n)return false;strcpy(out,found->second.c_str());return true;}
bool set(const char* name,const char* value){if(Fake::fail||!Fake::unlocked||!*name||(*name>='0'&&*name<='9'))return false;Fake::secrets[name]=value;return true;}
bool erase(const char* name){return !Fake::fail&&Fake::unlocked&&Fake::secrets.erase(name)>0;}
std::vector<String> names(){std::vector<String> result;for(const auto& item:Fake::secrets)result.emplace_back(item.first);return result;}
}
namespace tab5vault{void wipe(void* data,size_t n){volatile unsigned char* p=static_cast<unsigned char*>(data);while(n--)*p++=0;}}
static void tab5DiagnosticMark(unsigned,const char*,const char*,int){}
static void tab5_env_import_secrets(){++Fake::imports;}
static int tab5_env_bind(const char* name,const char* source){++Fake::binds;if(Fake::envFull)return -1;Fake::environment[name]=source;return 0;}
static unsigned utf8Characters(const char* text){return writing::charCount(text,strlen(text));}
enum class Tab5Tool { Calculator, Python, Print, Vault, Keys, Firmware, Executable, Theme, Terminal, Storage, Sdr, AppPanel, Secrets };
namespace Tab5SdrUi{void close(){}}namespace Tab5FilePicker{void close(){}}
namespace Tab5Terminal{void close(){}}namespace Tab5AppPanel{void close(){}}
namespace Tab5PythonReader{void suspend(){}}namespace Tab5Firmware{bool busy(){return false;}}
namespace Tab5PythonUI{void close(){}}namespace Tab5Python{void stop(){}}
namespace Tab5MapPrint{void cancel(){}}namespace Tab5Printers{struct Printer{};}
namespace Tab5CalculatorUi{void layout(int,int,int){}}
namespace writing{bool usbConnected(){return false;}}
static void freeScript(void* p){free(p);}
static void setKeyboardVisible(bool,lv_obj_t* = nullptr);
struct Impl {
 lv_obj_t *parent=nullptr,*root=nullptr,*status=nullptr,*keyList=nullptr,*keyName=nullptr,*path=nullptr,*password=nullptr,*confirmation=nullptr,*activeField=nullptr;
 bool vaultGate=false,vaultChanging=false,vaultUnlocking=false,keyJob=false,vaultUpdates=false;String vaultSelection;
 std::vector<Tab5KeyStore::Info> keys;Tab5Tool tool=Tab5Tool::Keys;
 struct{const lv_font_t* (*font)()=[](){return &lv_font_montserrat_24;};void(*keyboard)(bool,lv_obj_t*)=setKeyboardVisible;}hooks;
 struct Empty{bool result=false;void close(bool){}void clear(){}void refresh(){}}calculatorConversion,calculatorHistory,calculatorEntry,calculatorHistoryView;
 int calculatorHeight=0,calculator=0;unsigned mapFallbacks=0,consoleRevision=0;
 bool mapMode=false,scanning=false,estimateDirty=false,estimateWaiting=false,printUpdates=false,lockedFile=false,legacyFile=false,scriptLauncher=false,firmwareUpdates=false,sectionDirty=false,switchConfirmed=false,mapPreparing=false,editing=false;
 String pendingPythonPath,lastOutput;
 void* printSections=nullptr;
 lv_obj_t *printSection=nullptr,*printScopeHint=nullptr,*printPrevious=nullptr,*printNext=nullptr,*switchPrompt=nullptr,*pythonResume=nullptr;
 lv_obj_t *panelMode=nullptr,*panelValueLabel=nullptr,*panelSave=nullptr,*nativeRun=nullptr,*nativeStop=nullptr,*nativeConsole=nullptr,*nativeRecent=nullptr,*nativeMode=nullptr,*nativeFile=nullptr,*nativeFolder=nullptr;
 lv_obj_t *firmwareCancel=nullptr,*firmwareVerify=nullptr,*themePreview=nullptr,*themeRoles=nullptr,*themeHex=nullptr,*themeSwatch=nullptr,*themeLoad=nullptr;
 lv_obj_t *scale=nullptr,*pageLabel=nullptr,*printers=nullptr,*trustButton=nullptr,*trustHint=nullptr,*tlsButton=nullptr,*input=nullptr,*output=nullptr,*uri=nullptr,*paper=nullptr,*dpi=nullptr,*landscape=nullptr,*destination=nullptr,*fromUnit=nullptr,*toUnit=nullptr,*angle=nullptr;
 lv_obj_t *toolHeading=nullptr,*toolClose=nullptr,*toolKeyboard=nullptr,*storagePanel=nullptr,*scriptPanel=nullptr,*firmwareProgress=nullptr,*editButtons[8]{};
 struct LayoutAnchor{lv_obj_t* object;int16_t x,width;uint8_t mode;};LayoutAnchor layoutAnchors[128]{};unsigned layoutAnchorCount=0;int layoutWidth=0;
 std::vector<Tab5Printers::Printer> detected;
 void rememberPrinter(){}void stopOutline(){}void dismissScript(){}void layoutScript(){}
 const char* tr(const char* text){auto it=translations.find(text);return it==translations.end()?text:it->second.c_str();}uint32_t color(unsigned role){const uint32_t c[]={0x101010,0x202020,0xffffff,0xaaaaaa,0x4488aa};return c[role<5?role:0];}
 void pick(lv_obj_t*){}
 void act(int action){securityAction(action);}
 #include "credential-helpers.inc"
 #include "Tab5CredentialUi.inc"
};
namespace Tab5NumericKeyboard{static unsigned mode(lv_obj_t*){return 0;}}
namespace Tab5GitView{void resize(int){}}
enum class Page{Writing,Editor};static Page activePage=Page::Editor;
static lv_obj_t *keyboardPanel=nullptr,*keyboardTarget=nullptr,*editor=nullptr,*accentPopup=nullptr;
static bool keyboardVisible=false,keyboardUiNavigation=false,keyboardDockFocus=false,smoothFontReady=false;
static unsigned keyboardNumberMode=0;static void* keyTtfFont=nullptr;
static constexpr int PAGE_H=660,SCREEN_H=720;
static int keyboardHeight(){return 288;}
static void rebuildKeyboard(){}static bool ensureKeyFont(){return false;}
static Impl* activeApp=nullptr;
struct WritingBoundary{void resize(int){}}Tab5Writing;
struct ToolsBoundary{void resize(int height){if(activeApp)activeApp->resize(height);}}Tab5Tools;
#include "credential-os-keyboard.inc"
static unsigned checks=0;
static void check(bool value,const char* text){if(!value){fprintf(stderr,"FAIL: %s\n",text);exit(1);}++checks;}
static void ctrl(lv_obj_t* field,char c){writing::KeyEvent key;key.key=writing::Key::Text;key.ctrl=true;key.text[0]=c;Tab5TextInput::key(field,key);}
static void type(lv_obj_t* field,const char* text){Tab5TextInput::insert(field,text);}
static void click(Impl& app,int action){
 lv_obj_t* button=nullptr;
 for(unsigned i=0;app.root&&i<lv_obj_get_child_count(app.root);++i){auto* child=lv_obj_get_child(app.root,i);if(!lv_obj_check_type(child,&lv_button_class)||!lv_obj_get_event_count(child))continue;
  // The extracted production button helper owns this pair for CLICKED/DELETE.
  auto* pair=static_cast<std::pair<Impl*,int>*>(lv_event_dsc_get_user_data(lv_obj_get_event_dsc(child,0)));
  if(pair&&pair->first==&app&&pair->second==action){button=child;break;}
 }
 check(button!=nullptr,"real production button exists for action");lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);Fake::complete();app.securityService();app.securityService();
}
static void selectSecret(Impl& app,unsigned index){lv_dropdown_set_selected(app.secretList,index);lv_obj_send_event(app.secretList,LV_EVENT_VALUE_CHANGED,nullptr);}
static void privacyTests(lv_obj_t* screen){
 auto* field=lv_textarea_create(screen);lv_textarea_set_text(field,"");Tab5TextInput::track(field);type(field,"existing private value");
 auto* history=Tab5TextInput::history(field);check(history&&history->states.size()>1,"normal edit history initially exists");
 for(auto& snapshot:history->states)Allocator::expectZero(snapshot.text);
 Tab5TextInput::protect(field);check(history->states.empty()&&!Tab5TextInput::canUndo(field),"protect wipes previous history");
 Tab5TextInput::copy("ordinary",8);lv_textarea_set_password_mode(field,false);ctrl(field,'a');ctrl(field,'c');check(!strcmp(Tab5TextInput::clipboard(),"ordinary"),"revealed protected value cannot overwrite ordinary clipboard");
 ctrl(field,'x');check(!strcmp(lv_textarea_get_text(field),"existing private value"),"protected cut is blocked");
 check(!Tab5TextInput::copySelection(field),"selection menu copy blocked for revealed credentials");type(field,"replacement-secret");ctrl(field,'z');check(!strcmp(lv_textarea_get_text(field),"replacement-secret")&&history->states.empty(),"revealed protected edit has no undo snapshots");
 check(Tab5TextInput::copySecret("temporary-token",15),"explicit secret copy succeeds");Allocator::expectZero(Tab5TextInput::copied);
 lv_tick_inc(29999);check(!strcmp(Tab5TextInput::clipboard(),"temporary-token"),"secret available before30s");lv_tick_inc(1);Tab5TextInput::serviceSecrets();check(!*Tab5TextInput::clipboard(),"secret expires at30s");
 Tab5TextInput::copySecret("lock-token",10);Allocator::expectZero(Tab5TextInput::copied);Tab5TextInput::clearSecrets();check(!*Tab5TextInput::clipboard(),"lock wipes secret clipboard immediately");
 Tab5TextInput::copySecret("replace-token",13);Allocator::expectZero(Tab5TextInput::copied);Tab5TextInput::copy("ordinary survives lock",22);Tab5TextInput::clearSecrets();check(!strcmp(Tab5TextInput::clipboard(),"ordinary survives lock"),"ordinary copy replaces/wipes secret and survives vaultlock");
 lv_tick_inc((UINT32_MAX-15000u)-lv_tick_get());Tab5TextInput::copySecret("wrap",4);Allocator::expectZero(Tab5TextInput::copied);lv_tick_inc(29999);check(!strcmp(Tab5TextInput::clipboard(),"wrap"),"secret stays valid through tickcounter wrap before30s");lv_tick_inc(1);Tab5TextInput::serviceSecrets();check(!*Tab5TextInput::clipboard(),"clipboard expiry handles tickcounter wrap");
 lv_obj_delete(field);check(Tab5TextInput::historyBytes==0&&Allocator::violations==0,"protected history and clipboard wiped beforeallocator release");
}
static void geometry(Impl& app){
 lv_obj_update_layout(app.root);std::vector<lv_obj_t*> widgets;
 for(unsigned i=0;i<lv_obj_get_child_count(app.root);++i){auto* object=lv_obj_get_child(app.root,i);if(lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))continue;widgets.push_back(object);
  if(lv_obj_check_type(object,&lv_button_class)){
   auto* label=lv_obj_get_child(object,0);lv_area_t content,bounds;lv_obj_get_content_coords(object,&content);lv_obj_get_coords(label,&bounds);
   if(bounds.x1<content.x1||bounds.x2>content.x2||bounds.y1<content.y1||bounds.y2>content.y2)fprintf(stderr,"button label overflow: %s width%d label%d\n",lv_label_get_text(label),lv_obj_get_width(object),lv_obj_get_width(label));
   check(bounds.x1>=content.x1&&bounds.x2<=content.x2&&bounds.y1>=content.y1&&bounds.y2<=content.y2,"credential button label fits");
  }
 }
 for(size_t a=0;a<widgets.size();++a)for(size_t b=a+1;b<widgets.size();++b){lv_area_t x,y;lv_obj_get_coords(widgets[a],&x);lv_obj_get_coords(widgets[b],&y);
  const bool overlap=std::max(x.x1,y.x1)<=std::min(x.x2,y.x2)&&std::max(x.y1,y.y1)<=std::min(x.y2,y.y2);
  if(overlap){const char* first=lv_obj_check_type(widgets[a],&lv_label_class)?lv_label_get_text(widgets[a]):"<control>";const char* second=lv_obj_check_type(widgets[b],&lv_label_class)?lv_label_get_text(widgets[b]):"<control>";fprintf(stderr,"overlap: %s / %s (y%d-%d / %d-%d)\n",first,second,x.y1,x.y2,y.y1,y.y2);}
  check(!overlap,"credential labels/status do not overlap controls");
 }
}
int main(){
 lv_init();lv_display_create(1280,720);auto* screen=lv_screen_active();Tab5Focus.begin(screen,&lv_font_montserrat_24);
 uint32_t palette[]={0x101010,0x202020,0x303030,0x666666,0xffffff,0xaaaaaa,0x4488aa,0xffffff,0x556677};Tab5UiStyle::configure(palette,12);
 privacyTests(screen);Impl app;app.parent=screen;activeApp=&app;keyboardPanel=lv_obj_create(screen);lv_obj_add_flag(keyboardPanel,LV_OBJ_FLAG_HIDDEN);
 Fake::unlocked=false;Fake::legacy=true;app.open(Tab5Tool::Keys,nullptr);check(app.vaultGate&&Tab5TextInput::protectedField(app.password),"lockedPIN opens protected numericgate");
 std::string oversizedPassword;for(unsigned i=0;i<86;++i)oversizedPassword+="密";
 lv_textarea_set_text(app.legacyPassword,oversizedPassword.c_str());click(app,44);check(Fake::legacyUnlockCalls==0&&!Fake::keyBusy&&strlen(lv_textarea_get_text(app.legacyPassword))==258,"legacyCJKpasswordover256bytesrejectedbeforebackendwithouttruncation");
 lv_textarea_set_text(app.password,"12");click(app,47);check(Fake::unlockCalls==0&&app.vaultGate,"shortPIN rejected beforebackend");
 lv_textarea_set_text(app.password,"9999");click(app,47);check(Fake::unlockCalls==1&&!Fake::unlocked&&app.vaultGate,"incorrectPIN keepsgate");
 lv_textarea_set_text(app.password,"1234");click(app,47);check(Fake::unlocked&&!app.vaultGate&&app.keyList,"correctPIN entersKeys");
 unsigned created=Fake::keyCreates;click(app,410);check(Tab5KeyStore::stored.size()==1,"initialkey requiresconfirmation");click(app,410);check(Tab5KeyStore::stored.empty(),"last initialkey can be deleted");
 app.close();app.open(Tab5Tool::Keys,nullptr);check(Tab5KeyStore::stored.empty()&&Fake::keyCreates==created,"reopeningemptykeylist does not recreate initialkey");
 lv_textarea_set_text(app.keyName,"Replacement");click(app,40);check(Tab5KeyStore::stored.size()==1,"keygeneration still works");
 lv_textarea_set_text(app.keyName,"Second key");click(app,40);check(app.selectedKey()==1,"newslot1selected");
 lv_dropdown_set_selected(app.keyList,0);lv_obj_send_event(app.keyList,LV_EVENT_VALUE_CHANGED,nullptr);click(app,410);click(app,410);
 lv_textarea_set_text(app.keyName,"Reused slot0");click(app,40);check(app.selectedKey()==0,"reusedslot0selectedbyidentity ratherthanlastlistentry");
 click(app,41);check(Fake::lastBackupSlot==0,"backupusesnewkeyinreusedslot");Fake::vaultBusy=false;app.securityService();
 check(!app.keyJob&&!app.vaultUpdates,"completedbackupclearsjobandstatuspolling");
 click(app,410);std::string deletionNotice=lv_label_get_text(app.status);app.securityService();check(deletionNotice==lv_label_get_text(app.status),"completedbackupstatusdoesnotoverwritedeletionnotice");
 lv_obj_send_event(app.keyList,LV_EVENT_VALUE_CHANGED,nullptr);
 lv_dropdown_set_selected(app.keyList,1);click(app,41);check(Fake::lastBackupSlot==1,"backupusesexplicitlyselectedslot1");Fake::vaultBusy=false;app.securityService();check(app.selectedKey()==1,"keyrefreshafterbackupkeepsselectedidentity");
 for(unsigned cycle=0;cycle<200;++cycle){click(app,41);check(app.keyJob&&Fake::vaultBusy,"devicebackup startsasjob");app.close();check(!app.keyJob&&!app.securityBusy,"closing resetskeyjob");Fake::vaultBusy=false;app.open(Tab5Tool::Keys,nullptr);unsigned listed=Fake::listCalls;app.securityService();check(!app.keyJob&&Fake::listCalls==listed,"reopenafterbackup neverrefreshesstalejob");}
 lv_dropdown_set_selected(app.backupMode,1);lv_obj_send_event(app.backupMode,LV_EVENT_VALUE_CHANGED,nullptr);check(!lv_obj_has_flag(app.password,LV_OBJ_FLAG_HIDDEN),"portablebackup revealsprotectedpasswordfield");
 unsigned backups=Fake::backups;lv_textarea_set_text(app.password,"short");lv_textarea_set_text(app.confirmation,"short");click(app,41);check(Fake::backups==backups,"portablebackup rejects shortpassword");
 lv_textarea_set_text(app.password,oversizedPassword.c_str());lv_textarea_set_text(app.confirmation,oversizedPassword.c_str());click(app,41);check(Fake::backups==backups,"portableCJKpasswordover256bytesrejectedbeforebackend");
 lv_textarea_set_text(app.password,"long enough password");lv_textarea_set_text(app.confirmation,"different password");click(app,41);check(Fake::backups==backups,"portablebackup rejects mismatch");
 lv_dropdown_set_selected(app.backupMode,0);lv_obj_send_event(app.backupMode,LV_EVENT_VALUE_CHANGED,nullptr);check(!*lv_textarea_get_text(app.password)&&!*lv_textarea_get_text(app.confirmation),"leavingportablemode erasespasswordfields");
 app.open(Tab5Tool::Keys,nullptr,false,true);lv_textarea_set_text(app.password,"2468");lv_textarea_set_text(app.confirmation,"2469");unsigned configured=Fake::configCalls;click(app,47);check(Fake::configCalls==configured&&app.vaultGate,"mismatchedPINconfirmation does not changeaccess");
 lv_textarea_set_text(app.confirmation,"2468");lv_textarea_set_text(app.currentPinField,"0000");click(app,47);check(app.vaultGate&&Fake::pinValue=="1234","open session cannot change PIN without current PIN");lv_textarea_set_text(app.currentPinField,"1234");lv_textarea_set_text(app.password,"2468");lv_textarea_set_text(app.confirmation,"2468");click(app,47);check(Fake::pinValue=="2468"&&!app.vaultGate,"confirmedPINchange openskeypage");
 click(app,415);check(app.tool==Tab5Tool::Secrets&&app.secretValue&&Tab5TextInput::protectedField(app.secretValue),"passwordmanager opensprotectedvalue");
 click(app,420);lv_textarea_set_text(app.secretName,"GIT_TOKEN");type(app.secretValue,"token-01");click(app,421);check(Fake::secrets["GIT_TOKEN"]=="token-01"&&Fake::environment["GIT_TOKEN"]=="GIT_TOKEN","savesecret bindsnamewithoutvaluecopytoenv");
 click(app,424);check(!lv_textarea_get_password_mode(app.secretValue)&&Tab5TextInput::protectedField(app.secretValue),"revealdoesnotremoveprotection");ctrl(app.secretValue,'a');ctrl(app.secretValue,'c');check(!*Tab5TextInput::clipboard(),"ordinarycopycannotexportrevealedsecret");
 type(app.secretValue,"token-02");ctrl(app.secretValue,'z');check(!strcmp(lv_textarea_get_text(app.secretValue),"token-02")&&!Tab5TextInput::canUndo(app.secretValue),"revealededitcannotundotooldsecret");click(app,421);check(Fake::secrets["GIT_TOKEN"]=="token-02","updatesecret storescurrentvalue");
 click(app,420);lv_textarea_set_text(app.secretName,"ACCOUNT");type(app.secretValue,"user@example.test");click(app,421);check(Fake::secrets.size()==2,"secondsecretcreated");selectSecret(app,1);check(!strcmp(lv_textarea_get_text(app.secretName),"GIT_TOKEN")&&lv_textarea_get_password_mode(app.secretValue),"selectionrestoresrightsecret andhidesit");
 click(app,422);check(Fake::secrets.size()==2,"secretdeletionrequiresconfirmation");selectSecret(app,0);click(app,422);check(Fake::secrets.size()==2,"changingselectioninvalidatesdeleteconfirmation");click(app,422);check(Fake::secrets.size()==1&&!Fake::secrets.count("ACCOUNT"),"confirmeddeletiontargetsselectedsecret");
 click(app,423);check(!strcmp(Tab5TextInput::clipboard(),"token-02"),"explicitcopybuttonexportsfor30seconds");Allocator::expectZero(Tab5TextInput::copied);click(app,45);check(!Fake::unlocked&&!app.root&&!*Tab5TextInput::clipboard(),"lockclosesUIandwipesclipboard");
 app.open(Tab5Tool::Secrets,nullptr);lv_textarea_set_text(app.password,"2468");click(app,47);check(app.secretValue&&app.secretNames.size()==1,"unlockrestoressecretlist");
 click(app,422);click(app,422);check(Fake::secrets.empty()&&!app.secretNames.size()&&!*lv_textarea_get_text(app.secretValue),"deletinglastsecretleavesemptymanager");
 app.close();check(Tab5TextInput::historyBytes==0&&Allocator::violations==0&&Allocator::mustBeZero.empty(),"closingallUIreleaseshistoryandcheckedsecretbuffers");
 // Device PIN remains mandatory when bringing a legacy-unlocked key cache
 // into an already PIN-protected device vault.
 Fake::unlocked=true;Fake::device=false;Fake::pin=true;app.open(Tab5Tool::Keys,nullptr,false,true);
 check(app.vaultGate&&!app.confirmation,"migrationusesexistingdevicePINgate");lv_textarea_set_text(app.password,"0000");click(app,47);check(!Fake::device&&app.vaultGate,"wrongmigrationPINcannotimportlegacykeys");
 lv_textarea_set_text(app.password,"2468");click(app,47);check(Fake::device&&!app.vaultGate,"correctdevicePINallowsmigration");app.close();
 // An already-open .25 store without a PIN cannot expose cached credentials
 // before the user chooses its first PIN. Creating it preserves entries.
 Fake::unlocked=true;Fake::device=true;Fake::pin=false;Fake::secrets["RETAINED"]="upgrade-fixture";app.open(Tab5Tool::Secrets,nullptr);
 check(app.vaultGate&&!app.secretValue&&app.confirmation,"pinless upgrade must show gate even with old session open");
 lv_textarea_set_text(app.password,"2468");lv_textarea_set_text(app.confirmation,"2468");click(app,47);check(!app.vaultGate&&Fake::secrets["RETAINED"]=="upgrade-fixture","adding PIN preserves existing credentials");app.close();
 // Recovery requires a deliberate action and exact confirmation, and it
 // remains available from a locked or corrupt-PIN gate.
 Fake::unlocked=false;app.open(Tab5Tool::Secrets,nullptr);auto* previous=app.root;app.securityAction(430);check(app.root==previous&&!app.recoveryPage,"navigation does not delete live callback widgets");app.securityService();check(app.recoveryPage&&app.resetConfirmation,"locked user can reach recovery");
 click(app,431);check(Fake::pin&&!Fake::unlocked,"empty recovery confirmation does nothing");lv_textarea_set_text(app.resetConfirmation,"reset");click(app,431);check(Fake::pin,"recovery confirmation is explicit literal RESET");lv_textarea_set_text(app.resetConfirmation,"RESET");click(app,431);check(!Fake::pin&&app.vaultGate&&app.confirmation,"reset returns to mandatory PIN creation");
 lv_textarea_set_text(app.password,"123");lv_textarea_set_text(app.confirmation,"123");click(app,47);check(!Fake::pin,"first use cannot skip minimum PIN");lv_textarea_set_text(app.password,"9182");lv_textarea_set_text(app.confirmation,"9182");
 previous=app.root;app.securityAction(47);check(Fake::keyBusy&&!Fake::unlocked&&app.root==previous&&app.securityBusy,"PIN callback only queues worker, preserves root");Fake::complete();app.securityService();app.securityService();check(Fake::pin&&!app.vaultGate&&app.secretValue,"worker completion opens new vault");app.close();
 for(unsigned locale=0;locale<languageCount;++locale){language(locale);Fake::unlocked=true;Fake::pin=true;Fake::device=true;
  app.open(Tab5Tool::Keys,nullptr);geometry(app);lv_dropdown_set_selected(app.backupMode,1);lv_obj_send_event(app.backupMode,LV_EVENT_VALUE_CHANGED,nullptr);geometry(app);
  app.open(Tab5Tool::Secrets,nullptr);geometry(app);
  Fake::unlocked=false;Fake::legacy=true;app.open(Tab5Tool::Keys,nullptr);geometry(app);
  Fake::unlocked=true;app.open(Tab5Tool::Keys,nullptr,false,true);geometry(app);app.open(Tab5Tool::Keys,nullptr,false,false,true);geometry(app);app.close();
 }
 translations.clear();
 // Exact production close/open/resize and OS keyboard target DELETE callback.
 // Exercise success after keypad focus, with LVGL timers between every phase.
 Fake::legacy=false;Fake::pin=true;Fake::pinValue="9182";
 for(unsigned cycle=0;cycle<96;++cycle){
  Tab5Tool next=cycle%3==0?Tab5Tool::Keys:cycle%3==1?Tab5Tool::Secrets:Tab5Tool::Vault;
  Fake::unlocked=false;app.open(next,next==Tab5Tool::Vault?"/fixture.txt":nullptr);
  auto* pin=app.password;lv_obj_send_event(pin,LV_EVENT_CLICKED,nullptr);
  check(keyboardTarget==pin&&keyboardVisible,"real OS hook targets PIN and resizes Tools");
  lv_tick_inc(180);lv_timer_handler();lv_textarea_set_text(pin,"9182");
  click(app,47);lv_tick_inc(180);lv_timer_handler();
  check(!app.vaultGate&&app.root&&app.tool==next,"post-unlock production open reaches selected security page");
  check(!keyboardTarget&&!keyboardVisible,"deleted PIN target detaches keyboard through OS callback");
  app.close();lv_tick_inc(180);lv_timer_handler();check(!app.root&&!Tab5Focus.focused(),"production close releases final security page and focus");
 }
 Fake::unlocked=true;Fake::fail=true;Fake::secrets.clear();app.open(Tab5Tool::Secrets,nullptr);
 check(strstr(lv_label_get_text(app.status),"failed")!=nullptr,"failed preload error remains visible on secrets page");
 previous=app.root;app.securityAction(425);check(app.securityBusy&&Fake::keyBusy&&app.root==previous,"Retry queues worker without replacing the callback page");
 Fake::fail=false;Fake::complete();app.securityService();app.securityService();check(!app.securityBusy&&app.secretList&&!app.vaultGate,"Retry rebuilds page only after worker finishes");app.close();
 // Unavailable actions communicate their actual state, while backend checks
 // still reject synthetic/programmatic events as exercised above.
 auto actionEnabled=[&](int action){app.updateCredentialActions();for(const auto& item:app.credentialActions)if(item.action==action)return !lv_obj_has_state(item.object,LV_STATE_DISABLED);check(false,"requested credential action exists");return false;};
 Fake::unlocked=false;Fake::pin=true;Fake::pinValue="1357";app.open(Tab5Tool::Keys,nullptr);
 check(!actionEnabled(47)&&actionEnabled(430),"empty PIN disables Unlock while Recovery remains available");
 lv_textarea_set_text(app.password,"1357");check(actionEnabled(47),"valid PIN length enables Unlock");
 app.securityAction(47);check(!actionEnabled(47)&&!actionEnabled(430),"running authentication disables duplicate jobs and recovery");
 Fake::complete();app.securityService();app.securityService();
 Tab5KeyStore::stored.clear();app.refreshKeys();
 check(!actionEnabled(410)&&!actionEnabled(41)&&!actionEnabled(43),"empty key list disables Delete/Backup and idle Cancel");
 lv_textarea_set_text(app.keyName,"");check(!actionEnabled(40),"empty key name disables Generate");
 lv_textarea_set_text(app.keyName,"A key");check(actionEnabled(40),"nonempty key name enables Generate");
 Fake::vaultBusy=true;check(actionEnabled(43)&&!actionEnabled(40),"active backup enables Cancel and disables Generate");Fake::vaultBusy=false;
 Fake::secrets.clear();app.open(Tab5Tool::Secrets,nullptr);
 check(!actionEnabled(422)&&!actionEnabled(423)&&!actionEnabled(424)&&!actionEnabled(421),"empty manager disables Delete/Copy/Reveal/Save");
 lv_textarea_set_text(app.secretName,"NEW_TOKEN");lv_textarea_set_text(app.secretValue,"new value");
 check(actionEnabled(421)&&actionEnabled(423)&&!actionEnabled(422),"draft can Save/Copy but cannot Delete missing entry");
 for(const auto& item:app.credentialActions)if(item.action==423)check(!strcmp(lv_label_get_text(lv_obj_get_child(item.object,0)),"Copier"),"copy label is concise");
 Fake::fail=true;check(!actionEnabled(421)&&!actionEnabled(423),"failed store disables edits until explicit Retry");Fake::fail=false;
 app.open(Tab5Tool::Secrets,nullptr,false,false,true);
 check(!actionEnabled(431),"Reset disabled without explicit confirmation");lv_textarea_set_text(app.resetConfirmation,"RESET");check(actionEnabled(431),"exact RESET enables recovery");app.close();
 // The ordinary clipboard's empty string may retain its1-byte allocation.
 Tab5TextInput::copySecret("",0);Tab5TextInput::clearSecrets();check(Allocator::live.empty(),"textinputallocatorreturnsallbuffers");
 printf("PASS %u checks; real LVGLcredentialUI/TextInput;200backupclose-reopen cycles;96production unlock/open/close/resize cycles with actual OS keyboard DELETE callback;PIN/key/secret flows;mandatory PIN/current PIN reauthentication/deferred UI/recovery confirmation/asynchronous Retry;30s clipboard; %u wipe-before-free observations. Crypto/storage mocked; nohardwaretest.\n",checks,Allocator::checked);return 0;
}
