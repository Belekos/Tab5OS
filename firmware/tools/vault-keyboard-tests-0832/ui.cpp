#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <lvgl.h>
#include "Tab5VaultPrompt.h"
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5KeyStore.h"
#include "Tab5Vault.h"
#include "Tab5UiStyle.h"
static unsigned checks=0,cycles=0;
static void check(bool ok,const char* message){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
namespace backend {
bool working=false,accessible=false,configured=true,device=true,success=false,failStart=false,fileBusy=false,usb=true;
unsigned retries=0,generation=0,startedGeneration=0,starts=0,locks=0,finished=0,cancels=0,wiped=0;
bool initialize=false,migrate=false,continuation=false;
std::string submitted;
void reset(){working=false;accessible=false;configured=true;device=true;success=false;failStart=false;fileBusy=false;retries=0;submitted.clear();}
void complete(bool ok){working=false;success=ok&&generation==startedGeneration;accessible=success;if(success)configured=true;}
}
namespace Tab5KeyStore {
bool busy(){return backend::working;}
bool credentialsAccessible(){return backend::accessible&&!backend::working;}
bool unlocked(){return credentialsAccessible();}
bool pinConfigured(){return backend::configured;}
bool usingDevice(){return backend::device;}
unsigned pinRetrySeconds(){return backend::retries;}
bool operationSucceeded(){return backend::success;}
const char* status(){return backend::working?"Ouverture du coffre.":backend::success?"Coffre déverrouillé":"PIN incorrect";}
void lock(){++backend::locks;++backend::generation;backend::accessible=false;}
void finishUi(){++backend::finished;}
bool beginDeviceUnlock(const char* pin,bool initialize,bool migrate,bool continuation){
 ++backend::starts;if(backend::failStart)return false;
 backend::working=true;backend::startedGeneration=backend::generation;backend::submitted=pin;
 backend::initialize=initialize;backend::migrate=migrate;backend::continuation=continuation;return true;
}
}
namespace Tab5Vault {bool busy(){return backend::fileBusy;}void cancel(){++backend::cancels;backend::fileBusy=false;}}
namespace Tab5Secrets {bool ready(){return true;}}
namespace tab5vault {void wipe(void* data,size_t bytes){backend::wiped+=bytes;volatile unsigned char* p=static_cast<volatile unsigned char*>(data);while(bytes--)*p++=0;}}
namespace writing {bool usbConnected(){return backend::usb;}}
static lv_obj_t* root=nullptr;
static lv_obj_t* touchKeyboard=nullptr;
static writing::KeyEvent key(writing::Key k){writing::KeyEvent e;e.key=k;return e;}
static std::vector<lv_obj_t*> fields(lv_obj_t* object){std::vector<lv_obj_t*> out;if(lv_obj_check_type(object,&lv_textarea_class))out.push_back(object);for(unsigned i=0;i<lv_obj_get_child_count(object);++i){auto next=fields(lv_obj_get_child(object,i));out.insert(out.end(),next.begin(),next.end());}return out;}
static lv_obj_t* button(lv_obj_t* object,const char* caption){if(lv_obj_has_class(object,&lv_button_class))for(unsigned i=0;i<lv_obj_get_child_count(object);++i){auto* child=lv_obj_get_child(object,i);if(lv_obj_check_type(child,&lv_label_class)&&!strcmp(lv_label_get_text(child),caption))return object;}for(unsigned i=0;i<lv_obj_get_child_count(object);++i)if(auto* found=button(lv_obj_get_child(object,i),caption))return found;return nullptr;}
static unsigned callbacks=0,accepted=0,cancelled=0;
static bool callbackReopens=false;
static void completed(bool ok,void* context){check(!Tab5VaultPrompt::scope(),"continuation runs after prompt destroyed");check(context==&callbacks,"continuation identity preserved");++callbacks;if(ok)++accepted;else ++cancelled;if(callbackReopens){backend::accessible=false;callbackReopens=false;check(Tab5VaultPrompt::open(completed,&callbacks),"continuation may open a replacement prompt");}}
static void type(lv_obj_t* object,const char* text){Tab5Focus.editObject(object);for(const char* p=text;*p;++p){auto e=key(writing::Key::Text);e.text[0]=*p;check(Tab5VaultPrompt::key(e),"PIN physical text handled");}}

// The dispatcher below is extracted byte-for-byte from the production sketch.
// Every named close hook is represented by a scoped teardown, never by an
// implementation that swallows or remaps the keyboard events under test.
static lv_obj_t *sidebarSettingsOverlay=nullptr,*fileDeleteOverlay=nullptr,*fileOperationOverlay=nullptr,*dialogOverlay=nullptr,*filePopupOverlay=nullptr,*wifiOverlay=nullptr,*mapExportProgress=nullptr,*mapPlacesDeleteOverlay=nullptr,*mapPlacesOverlay=nullptr,*mapStylesOverlay=nullptr,*mapSearchOverlay=nullptr,*mapDownloadOverlay=nullptr;
static lv_obj_t* keyboardTarget=nullptr;static lv_obj_t* testScope=nullptr;
static bool keyboardUiNavigation=true,keyboardDockFocus=false,mapJpegPreparing=false,mapJpegEncoding=false;
static unsigned closedDialogs=0,submittedDialogs=0;
static lv_obj_t* keyboardScope(){return testScope?testScope:root;}
static void closeScope(lv_obj_t*& pointer){auto* old=pointer;pointer=nullptr;testScope=nullptr;keyboardTarget=nullptr;if(old){++closedDialogs;lv_obj_delete(old);}}
static void closeSidebarSettings(){closeScope(sidebarSettingsOverlay);}static void closeFileDeleteDialog(){closeScope(fileDeleteOverlay);}static void closeFileOperationDialog(){closeScope(fileOperationOverlay);}static void closeTextDialog(){closeScope(dialogOverlay);}static void closeFilePopup(){closeScope(filePopupOverlay);}static void closeWifiPopup(){closeScope(wifiOverlay);}static void closeMapPlaceDeleteOverlay(){closeScope(mapPlacesDeleteOverlay);}
static void closeMapPlacesEvent(lv_event_t*){closeScope(mapPlacesOverlay);}static void closeMapStylesEvent(lv_event_t*){closeScope(mapStylesOverlay);}static void closeMapSearchEvent(lv_event_t*){closeScope(mapSearchOverlay);}
namespace Tab5MapPrint {void cancel(){}}namespace Tab5Jpeg {void cancel(){}}
/* DIALOGS */

enum class Tab5Tool {Vault,Keys,Secrets};
struct CredentialForm {
 Tab5Tool tool=Tab5Tool::Secrets;bool securityBusy=false,vaultGate=false,vaultUnlocking=false,legacyFile=false;
 lv_obj_t *root=nullptr,*password=nullptr,*confirmation=nullptr,*currentPinField=nullptr,*legacyPassword=nullptr,*resetConfirmation=nullptr,*secretName=nullptr,*secretValue=nullptr,*secretList=nullptr,*keyName=nullptr,*keyList=nullptr,*path=nullptr,*backupMode=nullptr;
 struct CredentialAction{lv_obj_t* object;int action;};std::vector<CredentialAction> credentialActions;
 std::vector<int> keys;std::vector<std::string> secretNames;
 unsigned lastAction=0,actions=0,closes=0;std::string message;
 void note(const char* text){message=text;}
 void inputKeyboard(lv_obj_t* target){Tab5Focus.editObject(target);}
 void act(int action){lastAction=action;++actions;}
 void hideKeyboard(){Tab5Focus.finishEditing();}
 void close(){++closes;auto* old=root;root=nullptr;Tab5Focus.clear();lv_obj_delete(old);}
 void enabled(lv_obj_t* o,bool yes){if(!o)return;if(yes)lv_obj_remove_state(o,LV_STATE_DISABLED);else lv_obj_add_state(o,LV_STATE_DISABLED);}
 static unsigned utf8Characters(const char* text){unsigned n=0;for(auto* p=reinterpret_cast<const unsigned char*>(text);*p;++p)if((*p&0xc0)!=0x80)++n;return n;}
 /* CREDENTIAL_METHODS */
 lv_obj_t* field(){auto* f=lv_textarea_create(root);lv_textarea_set_one_line(f,true);return f;}
 lv_obj_t* action(unsigned id){auto* b=lv_button_create(root);credentialActions.push_back({b,int(id)});return b;}
};

int main(){
 lv_init();auto* display=lv_display_create(1280,720);static uint8_t pixels[1280*720*2];
 lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,pixels,nullptr,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);
 lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});
 root=lv_screen_active();lv_display_set_theme(display,lv_theme_mono_init(display,true,LV_FONT_DEFAULT));Tab5Focus.begin(root,LV_FONT_DEFAULT);
 uint32_t palette[9]={0x101215,0x202428,0x30343a,0x505860,0xeeeeee,0x999999,0x666666,0x444444,0x336699};Tab5UiStyle::configure(palette,8);
 Tab5VaultPrompt::Hooks hooks;hooks.translate=[](const char* text){return text;};hooks.font=[](){return LV_FONT_DEFAULT;};
 hooks.keyboard=[](bool visible,lv_obj_t* target){if(target)check(Tab5TextInput::protectedField(target),"PIN stays protected through keyboard hook");if(visible&&touchKeyboard)lv_obj_move_foreground(touchKeyboard);};Tab5VaultPrompt::begin(hooks);
 backend::usb=false;touchKeyboard=lv_obj_create(root);auto* callerButton=lv_button_create(root);Tab5Focus.focusObject(callerButton);check(Tab5VaultPrompt::open(completed,&callbacks),"PIN touch keyboard prompt");check(lv_obj_get_parent(Tab5VaultPrompt::scope())==root,"PIN shares active screen with OS touch keyboard");check(lv_obj_get_index(touchKeyboard)>lv_obj_get_index(Tab5VaultPrompt::scope()),"numeric keyboard remains above PIN backdrop");Tab5VaultPrompt::close();check(Tab5Focus.focused()==callerButton,"PIN cancellation restores caller even without an OS service tick");lv_obj_delete(callerButton);lv_obj_delete(touchKeyboard);touchKeyboard=nullptr;backend::usb=true;
 for(unsigned cycle=0;cycle<200;++cycle){++cycles;backend::reset();const auto before=callbacks;
  check(Tab5VaultPrompt::open(completed,&callbacks),"open existing PIN prompt");auto f=fields(Tab5VaultPrompt::scope());check(f.size()==1,"one existing PIN field");
  check(lv_textarea_get_password_mode(f[0])&&Tab5TextInput::protectedField(f[0]),"PIN masked and clipboard protected");
  auto* ok=button(Tab5VaultPrompt::scope(),"Déverrouiller");check(ok&&lv_obj_has_state(ok,LV_STATE_DISABLED),"empty PIN cannot submit");
  type(f[0],"1234");check(!lv_obj_has_state(ok,LV_STATE_DISABLED),"valid PIN enables submit");
  if(cycle%4==0){check(Tab5VaultPrompt::key(key(writing::Key::Escape)),"Escape cancels while editing");check(callbacks==before+1&&!Tab5VaultPrompt::scope(),"cancel returns once");continue;}
  Tab5Focus.finishEditing();check(Tab5VaultPrompt::key(key(writing::Key::Enter)),"Enter submits even after edit mode ends");
  check(backend::working&&backend::submitted=="1234"&&!backend::initialize&&backend::continuation,"asynchronous unlock uses copied PIN");
  check(!*lv_textarea_get_text(f[0]),"PIN erased immediately after worker accepts");const auto starts=backend::starts;
  Tab5VaultPrompt::key(key(writing::Key::Enter));check(starts==backend::starts,"pending Enter cannot submit twice");
  if(cycle%4==1){Tab5VaultPrompt::key(key(writing::Key::Escape));backend::complete(true);check(!backend::accessible,"cancelled worker cannot unlock later");check(callbacks==before+1,"cancel continuation once");}
  else if(cycle%4==2){backend::complete(false);Tab5VaultPrompt::service();check(Tab5VaultPrompt::scope()&&Tab5Focus.focused()==f[0],"failed PIN retains retry field");backend::retries=2;type(f[0],"5678");check(lv_obj_has_state(ok,LV_STATE_DISABLED),"PIN retry interval enforced");Tab5VaultPrompt::key(key(writing::Key::Enter));check(starts==backend::starts,"retry interval cannot be bypassed by Enter");backend::retries=0;Tab5VaultPrompt::service();Tab5VaultPrompt::key(key(writing::Key::Enter));backend::complete(true);Tab5VaultPrompt::service();check(callbacks==before+1&&!Tab5VaultPrompt::scope(),"successful retry returns once");}
  else{backend::complete(true);Tab5VaultPrompt::service();check(callbacks==before+1&&!Tab5VaultPrompt::scope(),"successful unlock returns once");}
 }
 for(unsigned cycle=0;cycle<100;++cycle){++cycles;backend::reset();backend::configured=false;check(Tab5VaultPrompt::open(completed,&callbacks),"first-use PIN prompt");auto f=fields(Tab5VaultPrompt::scope());check(f.size()==2,"first use includes confirmation");type(f[0],"2468");Tab5VaultPrompt::key(key(writing::Key::Enter));check(Tab5Focus.focused()==f[1]&&!backend::working,"first Enter advances to confirmation");type(f[1],"1111");Tab5VaultPrompt::key(key(writing::Key::Enter));check(!backend::working,"mismatched confirmation refuses creation");lv_textarea_set_text(f[1],"2468");Tab5VaultPrompt::key(key(writing::Key::Enter));check(backend::working&&backend::initialize,"matching confirmation starts initialization");backend::complete(true);Tab5VaultPrompt::service();check(!Tab5VaultPrompt::scope(),"creation returns to caller");}
 backend::reset();backend::accessible=true;auto before=callbacks;check(Tab5VaultPrompt::open(completed,&callbacks)&&callbacks==before+1&&!Tab5VaultPrompt::scope(),"unlocked vault continues without settings or prompt");
 backend::reset();backend::working=true;check(!Tab5VaultPrompt::open(completed,&callbacks),"busy vault ownership preserved");backend::reset();
 check(Tab5VaultPrompt::open(completed,&callbacks),"prompt before memory failure");auto f=fields(Tab5VaultPrompt::scope());type(f[0],"1234");backend::failStart=true;Tab5VaultPrompt::key(key(writing::Key::Enter));check(Tab5VaultPrompt::scope()&&!backend::working&&!strcmp(lv_textarea_get_text(f[0]),"1234"),"failed admission leaves editable PIN for retry");Tab5VaultPrompt::close();
 backend::reset();check(Tab5VaultPrompt::open(completed,&callbacks),"external-teardown prompt");f=fields(Tab5VaultPrompt::scope());type(f[0],"1234");Tab5VaultPrompt::key(key(writing::Key::Enter));before=callbacks;lv_obj_delete(Tab5VaultPrompt::scope());check(!Tab5VaultPrompt::scope()&&callbacks==before,"external teardown detaches callbacks");backend::complete(true);check(!backend::accessible,"external teardown invalidates worker");Tab5VaultPrompt::service();
 backend::reset();check(Tab5VaultPrompt::open(completed,&callbacks),"replacement continuation prompt");callbackReopens=true;Tab5VaultPrompt::close();check(Tab5VaultPrompt::scope(),"replacement survives old prompt close");Tab5VaultPrompt::close();

 for(unsigned cycle=0;cycle<200;++cycle){++cycles;
  dialogOverlay=testScope=lv_obj_create(root);auto* input=lv_textarea_create(dialogOverlay);lv_textarea_set_one_line(input,true);lv_textarea_set_text(input,"Dossier avec espaces");keyboardTarget=input;
  lv_obj_add_event_cb(input,[](lv_event_t* e){check(!strcmp(lv_textarea_get_text(lv_event_get_target_obj(e)),"Dossier avec espaces"),"Explorer READY preserves input");++submittedDialogs;closeTextDialog();auto* replacement=lv_textarea_create(root);Tab5Focus.editObject(replacement);},LV_EVENT_READY,nullptr);
  Tab5Focus.editObject(input);keyboardUiNavigation=cycle%2;if(cycle%3==0)Tab5Focus.finishEditing();
  auto submitted=submittedDialogs;check(dispatchDialogKeyboard(key(writing::Key::Enter)),"Explorer dialog Enter handled");check(!dialogOverlay&&submittedDialogs==submitted+1,"Explorer Enter commits once");auto* replacement=Tab5Focus.focused();check(replacement&&Tab5Focus.controlling(),"replacement input keeps focus after submission");Tab5Focus.clear();lv_obj_delete(replacement);
  dialogOverlay=testScope=lv_obj_create(root);input=lv_textarea_create(dialogOverlay);lv_textarea_set_one_line(input,true);Tab5Focus.editObject(input);keyboardTarget=input;keyboardUiNavigation=false;
  auto count=closedDialogs;check(dispatchDialogKeyboard(key(writing::Key::Escape)),"Explorer Escape handled");check(!dialogOverlay&&closedDialogs==count+1,"one Escape cancels active input");
 }
 dialogOverlay=testScope=lv_obj_create(root);auto* choice=lv_dropdown_create(dialogOverlay);lv_dropdown_set_options(choice,"one\ntwo");Tab5Focus.focusObject(choice);lv_dropdown_open(choice);check(dispatchDialogKeyboard(key(writing::Key::Escape))&&dialogOverlay&&!lv_dropdown_is_open(choice),"Escape first closes expanded dropdown");check(dispatchDialogKeyboard(key(writing::Key::Escape))&&!dialogOverlay,"second Escape closes dialog");
 fileDeleteOverlay=testScope=lv_obj_create(root);auto* cancel=lv_button_create(fileDeleteOverlay);auto* remove=lv_button_create(fileDeleteOverlay);unsigned deleted=0;
 lv_obj_add_event_cb(cancel,[](lv_event_t*){closeFileDeleteDialog();},LV_EVENT_CLICKED,nullptr);lv_obj_add_event_cb(remove,[](lv_event_t* e){++*static_cast<unsigned*>(lv_event_get_user_data(e));closeFileDeleteDialog();},LV_EVENT_CLICKED,&deleted);
 Tab5Focus.focusInitial(fileDeleteOverlay,cancel);check(dispatchDialogKeyboard(key(writing::Key::Enter))&&!fileDeleteOverlay&&!deleted,"delete confirmation Enter honors default Cancel");

 for(unsigned cycle=0;cycle<100;++cycle){++cycles;backend::reset();CredentialForm form;form.root=lv_obj_create(root);form.vaultGate=true;form.tool=Tab5Tool::Keys;form.currentPinField=form.field();form.password=form.field();form.confirmation=form.field();form.action(47);
  lv_textarea_set_text(form.currentPinField,"1234");lv_textarea_set_text(form.password,"5678");Tab5Focus.editObject(form.currentPinField);
  check(form.securityKeyboard(key(writing::Key::Enter))&&Tab5Focus.focused()==form.password,"settings Enter current PIN moves to new PIN");
  check(form.securityKeyboard(key(writing::Key::Enter))&&Tab5Focus.focused()==form.confirmation&&!form.actions,"settings Enter new PIN moves to confirmation");
  lv_textarea_set_text(form.confirmation,"1111");form.securityKeyboard(key(writing::Key::Enter));check(!form.actions&&form.message=="Les PIN diffèrent","settings confirmation mismatch blocks action");
  lv_textarea_set_text(form.confirmation,"5678");form.securityKeyboard(key(writing::Key::Enter));check(form.actions==1&&form.lastAction==47,"settings valid confirmation invokes PIN action once");
  form.securityBusy=true;backend::working=true;const auto locks=backend::locks;check(form.securityKeyboard(key(writing::Key::Escape))&&!form.root&&backend::locks==locks+1,"settings Escape cancels pending PIN worker and closes");
 }
 backend::reset();CredentialForm form;form.root=lv_obj_create(root);form.secretName=form.field();form.secretValue=form.field();form.action(421);lv_textarea_set_text(form.secretName,"GIT_TOKEN");Tab5Focus.editObject(form.secretName);
 check(form.securityKeyboard(key(writing::Key::Enter))&&Tab5Focus.focused()==form.secretValue,"secrets Enter name moves to protected value");lv_textarea_set_text(form.secretValue,"test-token");check(form.securityKeyboard(key(writing::Key::Enter))&&form.actions==1&&form.lastAction==421,"secrets Enter value saves visible primary action");
 check(form.securityKeyboard(key(writing::Key::Escape))&&!form.root,"secrets Escape closes even while editing");
 check(backend::wiped>0,"PIN buffers actually wiped");Tab5Focus.clear();lv_obj_clean(root);lv_deinit();
 printf("{\"passed\":true,\"checks\":%u,\"cycles\":%u}\n",checks,cycles);
}
