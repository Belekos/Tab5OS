#include "Tab5GitView.h"
#include "Tab5Git.h"
#include "Tab5GitMemory.h"
#include "Tab5Secrets.h"
#include "Tab5VaultPrompt.h"
#include "Tab5ExplorerGitContext.h"
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5UiStyle.h"
#include "Tab5Task.h"
#include "Tab5Localization.h"
#include "Tab5TerminalBuffer.h"
#include "Tab5ContextMenu.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <new>

namespace Tab5GitView {
namespace {
constexpr unsigned PageRows=128,VisibleRows=16,RowHeight=38,PathBytes=2049,OutputBytes=32768;
// libgit2 status bits: new, modified, deleted, renamed and type change.
constexpr uint32_t WorkChanges=0x0f80u,IndexChanges=0x001fu;
enum Action {Status,New,Clone,Stage,Commit,Diff,Log,Fetch,Pull,Push,Terminal,Settings,Stop,Previous,Next,Clear,StageAll,Unlock,ActionCount};
struct Entry {char path[PathBytes]{};uint32_t flags=0;bool usable=false;};
struct Model {
 Tab5Resources::Reservation startup;
 Entry entries[PageRows];
 char args[8][PathBytes]{},directory[PathBytes]{},nextDirectory[PathBytes]{},project[PathBytes]{};
 char remoteName[256]{"origin"},upstreamBranch[256]{};size_t ahead=0,behind=0;bool trackingKnown=false;
 char branch[256]{},remote[PathBytes]{},name[256]{},email[256]{},credential[PathBytes]{},usernameSecret[49]{},tokenSecret[49]{};
 char selectedPath[PathBytes]{};
 // Only the UI writes these; the worker owns directory/status while running.
 char requestedDirectory[PathBytes]{},requestedSelection[PathBytes]{},contextSelection[PathBytes]{};
 bool pendingContext=false,treeChanged=false;
 Tab5Terminal::OutputRing<OutputBytes> output;
 char snapshot[OutputBytes]{};
 size_t total=0;unsigned count=0,offset=0,argCount=0;
 bool workChanges=false,indexChanges=false,conflicts=false;
 Action action=Status;bool repository=false,notRepository=false,unborn=true,detached=false,more=false,truncated=false;int result=0;
};
Model* model=nullptr;
std::atomic<bool> running{false},finished{true},cancelled{false};
std::atomic<uint32_t> revision{0};
StaticSemaphore_t mutexStorage;
SemaphoreHandle_t mutex(){static auto value=xSemaphoreCreateMutexStatic(&mutexStorage);return value;}
struct Lock{Lock(){xSemaphoreTake(mutex(),portMAX_DELAY);}~Lock(){xSemaphoreGive(mutex());}};
Hooks hooks;
lv_obj_t *window=nullptr,*box=nullptr,*heading=nullptr,*state=nullptr,*list=nullptr,*spacer=nullptr,*report=nullptr,*prompt=nullptr;
lv_obj_t *fields[2]{},*choice=nullptr,*promptError=nullptr,*confirm=nullptr,*row[VisibleRows]{},*buttons[ActionCount]{};
lv_obj_t* editingTarget=nullptr;lv_obj_t* promptEdits=nullptr;lv_obj_t* secretChoices[2]{};
lv_obj_t *promptBox=nullptr,*promptBody=nullptr;
lv_obj_t *retiringWindow=nullptr,*retiringPrompt=nullptr;
Start requestedStart=Start::Browse;
lv_obj_t *toolbar=nullptr,*contextLabel=nullptr;
int visibleHeight=720;
uint32_t viewGeneration=0,promptGeneration=0,pendingVaultGeneration=0;
unsigned rowIndex[VisibleRows]{},page=0;int selected=-1;Action promptAction=Status;
uint32_t painted=0,lastPaint=0;bool handled=true,closing=false,repainting=false;
const char* tr(const char* text){return hooks.translate?hooks.translate(text):text;}
const lv_font_t* font(){return hooks.font?hooks.font():LV_FONT_DEFAULT;}
lv_color_t color(unsigned role){return lv_color_hex(Tab5UiStyle::color(role));}
template<size_t N>bool copy(char (&out)[N],const char* value){if(!value)value="";const size_t n=strlen(value);if(n>=N){out[0]=0;return false;}memcpy(out,value,n+1);return true;}
std::string workerTr(const char* source){size_t n=Tab5Locale.copyText(source,nullptr,0);if(n>8192)return source;std::string value(n+1,'\0');size_t actual=Tab5Locale.copyText(source,&value[0],value.size());if(actual>n)return source;value.resize(actual);return value;}
void append(const char* text,size_t n){Lock lock;if(model){if(model->output.size()+n>=OutputBytes)model->truncated=true;model->output.append(text,n);++revision;}}
void message(const char* text){if(state)lv_label_set_text(state,tr(text));}
void resetStatus(){model->count=0;model->total=0;model->workChanges=model->indexChanges=model->conflicts=false;model->repository=false;model->notRepository=false;model->unborn=true;model->detached=false;model->project[0]=0;model->branch[0]=0;model->remote[0]=0;copy(model->remoteName,"origin");model->upstreamBranch[0]=0;model->ahead=model->behind=0;model->trackingKnown=false;model->name[0]=0;model->email[0]=0;model->credential[0]=0;model->usernameSecret[0]=model->tokenSecret[0]=0;}
Tab5Git::Observer observer(){
 Tab5Git::Observer value;
 value.notRepository=[](){model->notRepository=true;};
 value.worktreeChanged=[](){model->treeChanged=true;};
 value.project=[](const char* p){copy(model->project,p);model->repository=true;};
 value.repository=[](const char* branch,bool unborn,bool detached){copy(model->branch,branch);model->unborn=unborn;model->detached=detached;};
 value.upstream=[](const char* name,const char* branch,size_t ahead,size_t behind,bool known){copy(model->remoteName,name);copy(model->upstreamBranch,branch);model->ahead=ahead;model->behind=behind;model->trackingKnown=known;};
 value.remote=[](const char* name,const char* url){if(!strcmp(name,model->remoteName))copy(model->remote,url);};
 value.config=[](const char* name,const char* text){if(!strcmp(name,"user.name"))copy(model->name,text);else if(!strcmp(name,"user.email"))copy(model->email,text);else if(!strcmp(name,"tab5.credentialFile"))copy(model->credential,text);else if(!strcmp(name,"tab5.usernameSecret"))copy(model->usernameSecret,text);else if(!strcmp(name,"tab5.tokenSecret"))copy(model->tokenSecret,text);};
 value.status=[](const char* path,uint32_t flags){
  // Include entries outside the visible page: a commit concerns the entire
  // index, and a conflict on another page still prevents that commit.
  model->workChanges|=(flags&WorkChanges)!=0;
  model->indexChanges|=(flags&IndexChanges)!=0;
  model->conflicts|=(flags&Tab5Git::Conflicted)!=0;
  const size_t index=model->total++;
  if(index<model->offset||model->count>=PageRows)return;
  auto& entry=model->entries[model->count++];entry.flags=flags;entry.usable=copy(entry.path,path);
  if(!entry.usable)copy(entry.path,"…");
 };
 return value;
}
void worker(void*){
 {
  std::vector<std::string> args;args.reserve(model->argCount);
  for(unsigned i=0;i<model->argCount;++i)args.emplace_back(model->args[i]);
  const auto stop=[](){return cancelled.load();};
  resetStatus();
  int code=Tab5Git::run("/sd",model->directory,args,append,stop,workerTr,append,observer(),&model->startup);
  if(code==0&&model->nextDirectory[0])copy(model->directory,model->nextDirectory);
  // A failed creation has no repository to inspect. Preserve its original
  // diagnostic instead of appending an unrelated "not a repository" error.
  if(model->action!=Status&&!cancelled.load()&&
      (code==0||(model->action!=New&&model->action!=Clone))){
   resetStatus();
   const int status=Tab5Git::run("/sd",model->directory,{"git","status"},[](const char*,size_t){},stop,workerTr,append,observer());
   if(code==0&&status!=0)code=status;
  }
  model->result=code;
 }
 tab5TaskCheckpoint("git-ui");running=false;++revision;
 // All C++ scopes have unwound. Publish the final access to the ordinary
 // model, not RTOS task destruction: the task owns no static stack or TCB.
 // The trampoline only returns to vTaskDelete after this function returns.
 finished.store(true,std::memory_order_release);
}
void panel(lv_obj_t* o){lv_obj_set_style_bg_color(o,color(1),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_text_color(o,color(4),0);lv_obj_set_style_text_font(o,font(),0);lv_obj_set_style_border_color(o,color(3),0);lv_obj_set_style_radius(o,Tab5UiStyle::radius(),0);}
void flagVisible(lv_obj_t* o,bool visible){if(!o)return;if(visible)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
void enabled(lv_obj_t* o,bool yes){if(!o)return;if(yes)lv_obj_remove_state(o,LV_STATE_DISABLED);else lv_obj_add_state(o,LV_STATE_DISABLED);}
void paintRows();void controls();void action(Action);void dismissPrompt();void showPrompt(Action);void layout();
lv_obj_t* button(lv_obj_t* parent,const char* icon,const char* tip,lv_event_cb_t event,void* data=nullptr){
 auto* b=lv_button_create(parent);lv_obj_set_size(b,48,44);panel(b);lv_obj_set_style_bg_color(b,color(8),LV_STATE_CHECKED);Tab5UiStyle::button(b);
 auto* text=lv_label_create(b);lv_label_set_text(text,icon);lv_obj_set_style_text_font(text,&lv_font_montserrat_24,0);lv_obj_center(text);
 Tab5Focus.describe(b,tr(tip));lv_obj_add_event_cb(b,event,LV_EVENT_CLICKED,data);return b;
}
bool launch(Action kind,const std::vector<std::string>& args,const char* next=nullptr){
 if(!model||busy()||Tab5Git::busy()||args.size()>8){message("Une opération Git est déjà en cours.");return false;}
 size_t bytes=0;for(const auto& arg:args)bytes+=arg.size();
 if(bytes>4096){message("Paramètres Git trop longs.");return false;}
 for(unsigned i=0;i<args.size();++i)if(!copy(model->args[i],args[i].c_str())){message("Paramètres Git trop longs.");return false;}
 if(selected>=0&&unsigned(selected)<model->count)copy(model->selectedPath,model->entries[selected].path);
 else model->selectedPath[0]=0;
 model->argCount=args.size();model->action=kind;model->treeChanged=false;copy(model->nextDirectory,next?next:"");
 if(kind==New||kind==Clone)page=0;
 model->offset=page*PageRows;model->output.clear();model->truncated=false;model->result=0;cancelled=false;finished=false;running=true;handled=false;++revision;selected=-1;
 Tab5Resources::AdmissionReport admission;
 model->startup=Tab5Git::reserveStartup(args,&admission);
 if(!model->startup||tab5CreateTask(worker,"tab5_git_ui",Tab5Git::WorkerStackBytes,nullptr,1,nullptr,0,&admission)!=pdPASS){model->startup.release();running=false;finished=true;handled=true;if(admission.reason!=Tab5Resources::Denial::None){char detail[512];Tab5Resources::describeAdmission(admission,detail,sizeof(detail));append(detail,strlen(detail));append("\n",1);}message(admission.reason==Tab5Resources::Denial::None?"Impossible de créer la tâche Git.":"Mémoire insuffisante pour démarrer Git.");controls();return false;}
 message("Opération Git en cours…");controls();return true;
}
void controls(){
 const bool working=busy();
 for(unsigned i=0;i<ActionCount;++i)enabled(buttons[i],!working);
 {Lock lock;enabled(buttons[Clear],model&&model->output.size()!=0);}
 flagVisible(buttons[Stop],working);enabled(buttons[Stop],true);
 enabled(list,!working);
 if(working)return; // Worker owns status fields until its final-access publication.
 const bool repo=model&&model->repository;
 for(Action a:{Stage,StageAll,Commit,Diff,Settings})flagVisible(buttons[a],repo);
 flagVisible(buttons[New],!repo);flagVisible(buttons[Clone],!repo);
 const bool selectedWork=repo&&selected>=0&&unsigned(selected)<model->count?
     model->entries[selected].usable&&(model->entries[selected].flags&(WorkChanges|Tab5Git::Conflicted))!=0:repo&&(model->workChanges||model->conflicts);
 enabled(buttons[Stage],selectedWork);
 enabled(buttons[StageAll],repo&&(model->workChanges||model->conflicts));
 // Let the user reach identity setup instead of silently disabling Commit.
 enabled(buttons[Commit],repo&&model->indexChanges&&!model->conflicts);
 if(buttons[Stage])lv_label_set_text(lv_obj_get_child(buttons[Stage],0),tr(selected>=0||model->contextSelection[0]?"Préparer le fichier":"Préparer ce dossier"));
 if(buttons[Commit])Tab5Focus.describe(buttons[Commit],tr(!model->name[0]||!model->email[0]?"Configurer le nom et l’adresse e-mail avant le commit":"Enregistrer un commit (tout l’index du dépôt)"));
 enabled(buttons[Diff],repo&&(model->workChanges||model->indexChanges||model->conflicts));
 enabled(buttons[Terminal],bool(hooks.terminal));
 flagVisible(buttons[Log],repo&&!model->unborn);
 flagVisible(buttons[Fetch],repo&&model->remote[0]);
 for(Action a:{Pull,Push})flagVisible(buttons[a],repo&&model->remote[0]&&!model->unborn&&!model->detached);
 enabled(buttons[Pull],repo&&!model->workChanges&&!model->indexChanges&&!model->conflicts);
 flagVisible(buttons[Previous],repo&&page>0);
 flagVisible(buttons[Next],repo&&model->total>model->offset+model->count);
 enabled(list,!working);
 layout();
}
void paintRows(){
 if(!list||!model||busy()||repainting)return;repainting=true;
 unsigned first=unsigned(std::max<int32_t>(0,lv_obj_get_scroll_y(list))/int(RowHeight));
 if(first>=model->count)first=model->count?model->count-1:0;
 for(unsigned i=0;i<VisibleRows;++i){
  const unsigned index=first+i;rowIndex[i]=index;flagVisible(row[i],index<model->count);
  if(index>=model->count)continue;
  const auto& entry=model->entries[index];char prefix[16];
  const char indexCode=(entry.flags&(Tab5Git::IndexNew|Tab5Git::IndexModified|Tab5Git::IndexDeleted|Tab5Git::IndexRenamed))?'+':' ';
  const char workCode=(entry.flags&Tab5Git::Conflicted)?'!':(entry.flags&Tab5Git::WorkNew)?'?':(entry.flags&Tab5Git::WorkDeleted)?'D':(entry.flags&(Tab5Git::WorkModified|Tab5Git::WorkRenamed))?'M':' ';
  snprintf(prefix,sizeof(prefix),"%c%c  ",indexCode,workCode);
  std::string label=prefix;label+=entry.path;lv_label_set_text(lv_obj_get_child(row[i],0),label.c_str());
  lv_obj_set_pos(row[i],0,index*RowHeight);if(int(index)==selected)lv_obj_add_state(row[i],LV_STATE_CHECKED);else lv_obj_remove_state(row[i],LV_STATE_CHECKED);
  enabled(row[i],entry.usable);
 }
 repainting=false;
}
void focusEntry(unsigned index){
 if(!list||!model||index>=model->count)return;
 const int top=lv_obj_get_scroll_y(list),height=lv_obj_get_content_height(list);
 const int y=int(index*RowHeight);
 if(y<top)lv_obj_scroll_to_y(list,y,LV_ANIM_OFF);
 else if(y+int(RowHeight)>top+height)lv_obj_scroll_to_y(list,y+RowHeight-height,LV_ANIM_OFF);
 paintRows();for(unsigned i=0;i<VisibleRows;++i)if(rowIndex[i]==index){Tab5Focus.focusObject(row[i]);break;}
}
void dismissPrompt(){
 ++promptGeneration;
 auto* old=prompt;
 if(old&&Tab5Focus.contains(old,Tab5Focus.focused()))Tab5Focus.clear();
 prompt=nullptr;promptEdits=nullptr;secretChoices[0]=secretChoices[1]=nullptr;fields[0]=fields[1]=choice=promptError=confirm=editingTarget=promptBox=promptBody=nullptr;
 if(hooks.keyboard)hooks.keyboard(false,nullptr);
 if(old){lv_obj_add_flag(old,LV_OBJ_FLAG_HIDDEN);retiringPrompt=old;}
 if(window&&!closing)Tab5Focus.focusInitial(window,list);
}
const char* settingValue(unsigned index){if(!model)return "";switch(index){case 0:return model->name;case 1:return model->email;case 2:return model->remote;case 3:return model->credential;case 4:return model->usernameSecret;default:return model->tokenSecret;}}
std::string selectedSecret(unsigned index){if(!secretChoices[index]||!lv_dropdown_get_selected(secretChoices[index]))return "";char value[49]{};lv_dropdown_get_selected_str(secretChoices[index],value,sizeof(value));return value;}
lv_obj_t* secretSelector(lv_obj_t* form,int y,const char* label,const char* selected=""){
 if(label){auto* title=lv_label_create(form);lv_label_set_text(title,tr(label));lv_obj_set_pos(title,0,y);lv_obj_set_width(title,350);}
 auto* select=lv_dropdown_create(form);lv_obj_set_pos(select,label?360:0,y-8);lv_obj_set_size(select,label?410:770,46);Tab5UiStyle::control(select);
 std::string options=tr("Aucun");unsigned chosen=0,index=0;auto names=Tab5Secrets::names();for(const auto& name:names){++index;options+='\n';options+=name.c_str();if(name==selected)chosen=index;}
 if(selected&&*selected&&!chosen){options+='\n';options+=selected;chosen=++index;}lv_dropdown_set_options(select,options.c_str());lv_dropdown_set_selected(select,chosen);return select;
}
void submitPrompt(){
 if(!prompt||!model)return;
 const std::string a=fields[0]?lv_textarea_get_text(fields[0]):"",b=fields[1]?lv_textarea_get_text(fields[1]):"";
 std::vector<std::string> args{"git"};std::string next;
 if(promptAction==Push){args.push_back("push");}
 else if(promptAction==Diff){args.push_back("diff");if(lv_dropdown_get_selected(choice))args.push_back("--cached");
  args.push_back("--");args.push_back(selected>=0&&unsigned(selected)<model->count?Tab5ExplorerGit::repositoryPath(model->project,model->entries[selected].path):(model->contextSelection[0]?model->contextSelection:model->directory));}
 else if(a.empty()&&!(promptAction==Settings&&lv_dropdown_get_selected(choice)>=4)){lv_label_set_text(promptError,tr("Compléter les champs nécessaires."));return;}
 else if(promptAction==New||promptAction==Clone){
  const auto& name=promptAction==New?a:b;
  if(name.empty()||name=="."||name==".."||name.find_first_of("/\\\r\n")!=std::string::npos||name[0]=='-'){
   lv_label_set_text(promptError,tr("Choisir un nom de dossier simple."));return;
  }
  next=model->directory;if(next!="/")next+='/';next+=name;
  if(next.size()>=PathBytes){lv_label_set_text(promptError,tr("Paramètres Git trop longs."));return;}
  if(promptAction==New)args.insert(args.end(),{"init",name});else {args.push_back("clone");auto user=selectedSecret(0),token=selectedSecret(1);if(user.empty()!=token.empty()){lv_label_set_text(promptError,tr("Choisir les deux secrets Git, ou aucun."));return;}if(!user.empty())args.insert(args.end(),{"--username-secret",user,"--token-secret",token});args.insert(args.end(),{a,name});}
 }else if(promptAction==Commit)args.insert(args.end(),{"commit","-m",a});
 else if(promptAction==Settings){
  const unsigned index=lv_dropdown_get_selected(choice);
  if(index==2)args.insert(args.end(),{"remote","set-url",model->remoteName,a});
  else if(index>=4){const char* key=index==4?"tab5.usernameSecret":"tab5.tokenSecret";auto value=selectedSecret(0);if(value.empty())args.insert(args.end(),{"config","--unset",key});else args.insert(args.end(),{"config",key,value});}
  else args.insert(args.end(),{"config",index==0?"user.name":index==1?"user.email":"tab5.credentialFile",a});
 }
 if((promptAction==Clone&&secretChoices[0]&&!selectedSecret(0).empty()||promptAction==Push&&(model->usernameSecret[0]||model->tokenSecret[0]||model->credential[0]))&&!Tab5Secrets::available()){
  if(!Tab5VaultPrompt::open([](bool unlocked,void* generation){if(prompt&&promptGeneration==uintptr_t(generation)){Tab5Focus.focusInitial(prompt,editingTarget?editingTarget:fields[0]?fields[0]:confirm);if(unlocked)submitPrompt();}},reinterpret_cast<void*>(uintptr_t(promptGeneration))))lv_label_set_text(promptError,tr("Opération en cours : patienter ou annuler"));return;
 }
 const auto kind=promptAction;dismissPrompt();launch(kind,args,next.c_str());
}
void showPrompt(Action kind){
 if(busy()||!window||retiringPrompt)return;dismissPrompt();promptAction=kind;
 prompt=lv_obj_create(hooks.dialogParent?hooks.dialogParent:window);lv_obj_set_size(prompt,lv_pct(100),lv_pct(100));panel(prompt);lv_obj_set_style_bg_opa(prompt,LV_OPA_90,0);lv_obj_remove_flag(prompt,LV_OBJ_FLAG_SCROLLABLE);
 auto* p=promptBox=lv_obj_create(prompt);lv_obj_set_size(p,840,430);lv_obj_center(p);panel(p);lv_obj_set_style_pad_all(p,18,0);lv_obj_remove_flag(p,LV_OBJ_FLAG_SCROLLABLE);
 auto* title=lv_label_create(p);lv_obj_set_width(title,790);lv_label_set_text(title,tr(kind==New?"Créer un dossier Git":kind==Clone?"Cloner un dépôt Git":kind==Commit?"Enregistrer un commit":kind==Push?"Envoyer les commits vers le dépôt distant ?":kind==Diff?"Voir les différences":"Identité et accès Git"));
 auto* form=promptBody=lv_obj_create(p);lv_obj_remove_style_all(form);lv_obj_set_pos(form,0,48);lv_obj_set_size(form,790,286);lv_obj_set_scroll_dir(form,LV_DIR_VER);lv_obj_set_scrollbar_mode(form,LV_SCROLLBAR_MODE_AUTO);
 if(kind==Push){auto* destination=lv_label_create(form);std::string text=model->directory;text+="\n";text+=model->branch;text+=" → ";text+=model->remoteName;text+="/";text+=model->upstreamBranch[0]?model->upstreamBranch:model->branch;lv_label_set_text(destination,text.c_str());lv_obj_set_width(destination,770);}
 if(kind==Diff){choice=lv_dropdown_create(form);lv_obj_set_size(choice,770,46);std::string options=tr("Modifications locales");options+='\n';options+=tr("Modifications préparées");lv_dropdown_set_options(choice,options.c_str());Tab5UiStyle::control(choice);}
 if(kind==Settings){choice=lv_dropdown_create(form);lv_obj_set_size(choice,770,46);lv_obj_set_pos(choice,0,0);std::string options=tr("Nom de l'auteur");options+='\n';options+=tr("Adresse e-mail de l'auteur");options+='\n';options+=tr("Adresse du dépôt distant");options+='\n';options+=tr("Fichier d’identifiants (avancé)");options+='\n';options+=tr("Identifiant du coffre");options+='\n';options+=tr("Jeton du coffre");lv_dropdown_set_options(choice,options.c_str());Tab5UiStyle::control(choice);lv_obj_add_event_cb(choice,[](lv_event_t*){unsigned index=lv_dropdown_get_selected(choice);lv_textarea_set_text(fields[0],settingValue(index));if(index>=4){lv_obj_add_flag(fields[0],LV_OBJ_FLAG_HIDDEN);if(promptEdits)lv_obj_add_flag(promptEdits,LV_OBJ_FLAG_HIDDEN);if(hooks.keyboard)hooks.keyboard(false,nullptr);if(secretChoices[0])lv_obj_delete(secretChoices[0]);secretChoices[0]=secretSelector(promptBody,80,nullptr,settingValue(index));}else{lv_obj_remove_flag(fields[0],LV_OBJ_FLAG_HIDDEN);if(promptEdits)lv_obj_remove_flag(promptEdits,LV_OBJ_FLAG_HIDDEN);if(secretChoices[0]){lv_obj_delete(secretChoices[0]);secretChoices[0]=nullptr;}}},LV_EVENT_VALUE_CHANGED,nullptr);}
 if(kind!=Push&&kind!=Diff){
  const unsigned count=kind==Clone?2:1;
  for(unsigned i=0;i<count;++i){fields[i]=lv_textarea_create(form);lv_textarea_set_one_line(fields[i],true);lv_obj_set_size(fields[i],770,48);lv_obj_set_pos(fields[i],0,kind==Settings?64:i*70);lv_textarea_set_max_length(fields[i],kind==Clone&&i==0?2048:kind==Commit?1024:kind==Settings?2048:255);Tab5UiStyle::control(fields[i]);
   lv_textarea_set_placeholder_text(fields[i],tr(kind==Clone?(i==0?"Adresse du dépôt distant":"Nom du nouveau dossier"):kind==New?"Nom du nouveau dossier":kind==Commit?"Message du commit":"Valeur"));
   lv_obj_add_event_cb(fields[i],[](lv_event_t* e){editingTarget=lv_event_get_target_obj(e);if(hooks.keyboard)hooks.keyboard(true,editingTarget);},LV_EVENT_CLICKED,nullptr);
   lv_obj_add_event_cb(fields[i],[](lv_event_t* e){editingTarget=lv_event_get_target_obj(e);},LV_EVENT_FOCUSED,nullptr);
  }
  if(kind==Settings)lv_textarea_set_text(fields[0],settingValue(0));
  if(kind==Clone){secretChoices[0]=secretSelector(form,148,"Identifiant du coffre");secretChoices[1]=secretSelector(form,204,"Jeton du coffre");}
  auto* edits=promptEdits=lv_obj_create(form);lv_obj_remove_style_all(edits);lv_obj_set_size(edits,300,44);lv_obj_set_pos(edits,0,kind==Clone?260:156);lv_obj_set_flex_flow(edits,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(edits,8,0);
  const char* icons[]={LV_SYMBOL_CUT,LV_SYMBOL_COPY,LV_SYMBOL_PASTE,LV_SYMBOL_LEFT,LV_SYMBOL_RIGHT};const char* tips[]={"Couper","Copier","Coller","Annuler","Rétablir"};
  for(unsigned i=0;i<5;++i)button(edits,icons[i],tips[i],[](lv_event_t* e){auto* f=editingTarget?editingTarget:fields[0];writing::KeyEvent k{};k.ctrl=true;k.key=writing::Key::Text;k.text[0]="xcvzy"[uintptr_t(lv_event_get_user_data(e))];Tab5TextInput::key(f,k);},reinterpret_cast<void*>(uintptr_t(i)));
 }
 promptError=lv_label_create(form);lv_obj_set_pos(promptError,0,kind==Clone?316:220);lv_obj_set_size(promptError,770,48);lv_label_set_text(promptError,"");
 confirm=button(p,LV_SYMBOL_OK,"Valider",[](lv_event_t*){submitPrompt();});lv_obj_align(confirm,LV_ALIGN_BOTTOM_RIGHT,0,0);
 if(kind==Clone||kind==Settings){auto* unlock=button(p,LV_SYMBOL_SETTINGS,"Déverrouiller le coffre",[](lv_event_t*){action(Unlock);});lv_obj_align(unlock,LV_ALIGN_BOTTOM_MID,0,0);}
 auto* cancel=button(p,LV_SYMBOL_CLOSE,"Annuler",[](lv_event_t*){dismissPrompt();});lv_obj_align(cancel,LV_ALIGN_BOTTOM_LEFT,0,0);
 resize(visibleHeight);
 Tab5Focus.focusInitial(prompt,fields[0]?fields[0]:choice?choice:cancel);
}
void unlockVault(){
 if(hooks.keyboard)hooks.keyboard(false,nullptr);
 if(!Tab5VaultPrompt::open([](bool unlocked,void* generation){
  if(!window||viewGeneration!=uintptr_t(generation))return;
  if(prompt)Tab5Focus.focusInitial(prompt,editingTarget?editingTarget:fields[0]);else Tab5Focus.focusInitial(window,buttons[Unlock]);
  if(!unlocked)return;
  // Update names in place: keep the URL, folder name, and configured choices.
  if(prompt&&(promptAction==Clone||promptAction==Settings)){
   const unsigned count=promptAction==Clone?2:1;
   for(unsigned i=0;i<count;++i)if(secretChoices[i]){
    const auto selected=selectedSecret(i);std::string options=tr("Aucun");unsigned chosen=0,index=0;
    for(const auto& name:Tab5Secrets::names()){++index;options+='\n';options+=name.c_str();if(name==selected.c_str())chosen=index;}
    if(!selected.empty()&&!chosen){options+='\n';options+=selected;chosen=++index;}
    lv_dropdown_set_options(secretChoices[i],options.c_str());lv_dropdown_set_selected(secretChoices[i],chosen);
   }
  }
  message("Coffre déverrouillé");
 },reinterpret_cast<void*>(uintptr_t(viewGeneration))))message("Opération en cours : patienter ou annuler");
}
void action(Action a){
 if(!model)return;if(a==Unlock){unlockVault();return;}if(a==Clear){clearOutput();return;}if(a==Stop){if(busy()){cancelled=true;message("Arrêt en cours.");}return;}if(busy())return;
 controls();if(buttons[a]&&(lv_obj_has_state(buttons[a],LV_STATE_DISABLED)||lv_obj_has_flag(buttons[a],LV_OBJ_FLAG_HIDDEN)))return;
 if(a==Commit&&(!model->name[0]||!model->email[0])){showPrompt(Settings);lv_label_set_text(promptError,tr("Configurer le nom et l’adresse e-mail avant le commit"));return;}
 if(a==New||a==Clone||a==Commit||a==Settings||a==Push||a==Diff){showPrompt(a);return;}
 if((a==Fetch||a==Pull)&&(model->usernameSecret[0]||model->tokenSecret[0]||model->credential[0])&&!Tab5Secrets::available()){
  if(Tab5VaultPrompt::scope()){message("Opération en cours : patienter ou annuler");return;}
  pendingVaultGeneration=viewGeneration;
  if(!Tab5VaultPrompt::open([](bool unlocked,void* value){if(window&&pendingVaultGeneration==viewGeneration){if(unlocked)action(Action(uintptr_t(value)));else Tab5Focus.focusInitial(window,buttons[Action(uintptr_t(value))]);}},reinterpret_cast<void*>(uintptr_t(a))))message("Opération en cours : patienter ou annuler");return;
 }
 if(a==Terminal){if(hooks.terminal&&hooks.terminal(model->directory))close();else message("Le terminal est occupé.");return;}
 if(a==Previous&&page)--page;else if(a==Next)++page;
 switch(a){
 case Status:case Previous:case Next:launch(Status,{"git","status"});break;
 case Stage:if(selected>=0&&unsigned(selected)<model->count&&model->entries[selected].usable)launch(Stage,{"git","add","--",Tab5ExplorerGit::repositoryPath(model->project,model->entries[selected].path)});else launch(Stage,{"git","add","--",model->contextSelection[0]?model->contextSelection:model->directory});break;
 case StageAll:launch(StageAll,{"git","add","-A"});break;
 case Log:launch(Log,{"git","log","-n","20"});break;
 case Fetch:launch(Fetch,{"git","fetch"});break;
 case Pull:launch(Pull,{"git","pull","--ff-only"});break;
 default:break;
 }
}
}
bool busy(){return running.load()||!finished.load(std::memory_order_acquire);}
void cancel(){if(busy())cancelled.store(true,std::memory_order_release);}
void clearOutput(){
 {Lock lock;if(model){model->output.clear();model->snapshot[0]=0;model->truncated=false;++revision;}}
 if(report){lv_textarea_clear_selection(report);lv_textarea_set_text(report,"");lv_obj_scroll_to_y(report,0,LV_ANIM_OFF);}
 enabled(buttons[Clear],false);
}
lv_obj_t* scope(){return prompt?prompt:window;}
lv_obj_t* modalScope(){return prompt;}
namespace {
void layout(){
 if(!window)return;lv_obj_update_layout(window);
 const int width=lv_obj_get_content_width(window),height=lv_obj_get_content_height(window);
 // Labels stay visible on the Explorer pane; the tools wrap and scroll.
 const int barHeight=std::min(144,std::max(88,height/4));
 if(toolbar){lv_obj_set_pos(toolbar,0,72);lv_obj_set_size(toolbar,width,barHeight);}
 if(state){lv_obj_set_pos(state,0,80+barHeight);lv_obj_set_size(state,width,48);}
 const int top=132+barHeight,remaining=std::max(80,height-top);
 if(list){lv_obj_set_pos(list,0,top);lv_obj_set_size(list,width,std::max(36,remaining*48/100));}
 if(report){const int split=remaining*48/100;lv_obj_set_pos(report,0,top+split+6);lv_obj_set_size(report,width,std::max(36,remaining-split-6));}
}
bool selectContext(){
 if(!model||busy()||!handled||strcmp(model->directory,model->requestedDirectory))return false;
 // A file highlight changes only the target. Reuse this directory's status
 // snapshot instead of allocating a 40-KiB worker for every arrow key.
 copy(model->contextSelection,model->requestedSelection);model->pendingContext=false;
 const auto relative=Tab5ExplorerGit::relativePath(model->project,model->contextSelection);
 copy(model->selectedPath,relative.c_str());selected=-1;
 for(unsigned i=0;i<model->count;++i)if(!strcmp(model->selectedPath,model->entries[i].path)){selected=i;break;}
 paintRows();controls();return true;
}
void startContext(){
 if(!model||busy()||!window)return;
 copy(model->directory,model->requestedDirectory);copy(model->contextSelection,model->requestedSelection);model->pendingContext=false;
 copy(model->selectedPath,"");page=0;selected=-1;
 if(requestedStart==Start::Initialize){requestedStart=Start::Browse;launch(New,{"git","init","."});}
 else launch(Status,{"git","status"});
}
}
void context(const char* directory,const char* selection){
 if(!window||!model||!directory||strlen(directory)>=PathBytes)return;
 if(!selection)selection="";if(strlen(selection)>=PathBytes)return;
 if(!strcmp(directory,model->requestedDirectory)&&!strcmp(selection,model->requestedSelection))return;
 copy(model->requestedDirectory,directory);copy(model->requestedSelection,selection);model->pendingContext=true;
 // A visible form retains its captured context until it is accepted/cancelled.
 // Navigation is applied after its command, never midway through a worker.
 if(!busy()&&!prompt&&!retiringPrompt&&!Tab5VaultPrompt::scope()&&!selectContext())startContext();
}
void resize(int availableHeight){
 visibleHeight=std::max(280,availableHeight);
 if(!window)return;
 // The pane belongs to Explorer. Only its full-screen input dialog follows IME.
 if(prompt){lv_obj_set_height(prompt,visibleHeight);}
 if(promptBox){const int height=std::min(430,visibleHeight-36);lv_obj_set_height(promptBox,height);lv_obj_set_height(promptBody,std::max(100,height-144));lv_obj_center(promptBox);}
 layout();
}
bool open(lv_obj_t* parent,const char* directory,Hooks value,Start start,const char* selection){
 if(!directory||strlen(directory)>=PathBytes)return false;
 if(window){if(prompt||busy()){message("Une opération Git est déjà en cours.");return false;}requestedStart=start;copy(model->requestedDirectory,directory);copy(model->requestedSelection,selection?selection:"");startContext();Tab5Focus.focusInitial(window,buttons[Status]);return true;}
 if(busy()||retiringWindow||retiringPrompt)return false;
 ++viewGeneration;hooks=value;if(!model){auto admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,{0,0,0,sizeof(Model),sizeof(Model)},"git-ui-model");if(!admission)return false;auto* data=heap_caps_malloc(sizeof(Model),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!data)return false;model=new(data)Model;admission.allocated(0,sizeof(Model));}
 copy(model->directory,directory);copy(model->requestedDirectory,directory);copy(model->requestedSelection,selection?selection:"");page=0;closing=false;requestedStart=start;
 window=lv_obj_create(parent);lv_obj_set_size(window,lv_pct(100),lv_pct(100));lv_obj_set_pos(window,0,0);lv_obj_set_style_pad_all(window,8,0);panel(window);lv_obj_remove_flag(window,LV_OBJ_FLAG_SCROLLABLE);lv_obj_move_foreground(window);
 box=window;heading=lv_label_create(window);lv_obj_set_pos(heading,0,0);lv_obj_set_size(heading,lv_pct(75),32);lv_label_set_long_mode(heading,LV_LABEL_LONG_DOT);lv_label_set_text(heading,directory);
 auto* exit=button(window,LV_SYMBOL_CLOSE,"Revenir à l’aperçu · Échap",[](lv_event_t*){close();});lv_obj_align(exit,LV_ALIGN_TOP_RIGHT,0,0);
 contextLabel=lv_label_create(window);lv_obj_set_pos(contextLabel,0,38);lv_obj_set_size(contextLabel,lv_pct(100),32);lv_label_set_long_mode(contextLabel,LV_LABEL_LONG_DOT);lv_label_set_text(contextLabel,"");
 auto* bar=toolbar=lv_obj_create(window);lv_obj_remove_style_all(bar);lv_obj_set_flex_flow(bar,LV_FLEX_FLOW_ROW_WRAP);lv_obj_set_style_pad_column(bar,6,0);lv_obj_set_style_pad_row(bar,6,0);lv_obj_set_scroll_dir(bar,LV_DIR_VER);lv_obj_set_scrollbar_mode(bar,LV_SCROLLBAR_MODE_AUTO);
 const Action actions[]={Status,Stage,Commit,Diff,Log,Fetch,Pull,Push,Settings,StageAll,Unlock,Terminal,New,Clone,Stop,Previous,Next,Clear};
 const char* captions[]={"Actualiser","Préparer ce dossier","Commit","Différences","Historique","Récupérer","Mettre à jour","Envoyer","Configurer","Tout préparer","Déverrouiller le coffre","Terminal","Nouveau","Cloner","Arrêter","Précédent","Suivant","Effacer la sortie"};
 const char* tips[]={"Actualiser · F5","Préparer le fichier sélectionné, ou le dossier courant","Enregistrer un commit (tout l’index du dépôt)","Différences du fichier sélectionné ou du dossier courant","Historique Git","Récupérer les commits distants","Mettre à jour sans fusion","Envoyer les commits vers la branche amont","Identité et accès Git","Préparer toutes les modifications du dépôt","Déverrouiller le coffre","Ouvrir le terminal ici","Créer un dossier Git","Cloner un dépôt Git","Arrêter · Ctrl+.","Page précédente","Page suivante","Effacer la sortie · Ctrl+L"};
 for(unsigned i=0;i<ActionCount;++i){auto* b=buttons[actions[i]]=button(bar,"",tips[i],[](lv_event_t* e){action(Action(uintptr_t(lv_event_get_user_data(e))));},reinterpret_cast<void*>(uintptr_t(actions[i])));lv_obj_set_width(b,lv_pct(31));auto* text=lv_obj_get_child(b,0);lv_label_set_text(text,tr(captions[i]));lv_obj_set_style_text_font(text,font(),0);lv_obj_set_width(text,lv_pct(100));lv_label_set_long_mode(text,LV_LABEL_LONG_DOT);lv_obj_center(text);}
 state=lv_label_create(window);lv_label_set_long_mode(state,LV_LABEL_LONG_WRAP);
 list=lv_obj_create(window);panel(list);lv_obj_set_style_pad_all(list,0,0);lv_obj_set_scroll_dir(list,LV_DIR_VER);lv_obj_set_scrollbar_mode(list,LV_SCROLLBAR_MODE_AUTO);
 spacer=lv_obj_create(list);lv_obj_remove_style_all(spacer);lv_obj_set_size(spacer,1,1);lv_obj_remove_flag(spacer,LV_OBJ_FLAG_CLICKABLE);
 for(unsigned i=0;i<VisibleRows;++i){row[i]=button(list,"","Sélectionner le fichier",[](lv_event_t* e){if(busy())return;unsigned slot=uintptr_t(lv_event_get_user_data(e));const int value=int(rowIndex[slot]);selected=selected==value?-1:value;paintRows();controls();},reinterpret_cast<void*>(uintptr_t(i)));lv_obj_set_size(row[i],lv_pct(96),RowHeight-2);lv_obj_set_style_pad_ver(row[i],2,0);lv_obj_set_style_pad_hor(row[i],8,0);auto* text=lv_obj_get_child(row[i],0);lv_obj_set_style_text_font(text,font(),0);lv_obj_set_width(text,lv_pct(100));lv_label_set_long_mode(text,LV_LABEL_LONG_DOT);}
 lv_obj_add_event_cb(list,[](lv_event_t*){paintRows();},LV_EVENT_SCROLL,nullptr);
 report=lv_textarea_create(window);Tab5TextInput::setReadOnly(report);Tab5UiStyle::control(report);lv_textarea_set_text(report,"");
 Tab5ContextMenu::bindOutput(report,[](void*){clearOutput();});
 lv_obj_add_event_cb(window,[](lv_event_t*){layout();},LV_EVENT_SIZE_CHANGED,nullptr);
 layout();Tab5Focus.focusInitial(window,buttons[Status]);startContext();return true;
}
void close(){
 if(!window&&!model)return;
 ++viewGeneration;closing=true;cancelled=true;requestedStart=Start::Browse;
 dismissPrompt();
 auto* old=window;
 if(old&&Tab5Focus.contains(old,Tab5Focus.focused()))Tab5Focus.clear();
 // Remove all public/UI references before keyboard/layout hooks can reenter.
 // The LVGL tree is retired by service(), after the current input event unwinds.
 window=box=heading=state=list=spacer=report=toolbar=contextLabel=nullptr;
 prompt=nullptr;promptEdits=nullptr;secretChoices[0]=secretChoices[1]=nullptr;fields[0]=fields[1]=choice=promptError=confirm=editingTarget=promptBox=promptBody=nullptr;
 std::fill(std::begin(buttons),std::end(buttons),nullptr);std::fill(std::begin(row),std::end(row),nullptr);
 if(old){lv_obj_add_flag(old,LV_OBJ_FLAG_HIDDEN);retiringWindow=old;}
 if(hooks.keyboard)hooks.keyboard(false,nullptr);
 if(old&&hooks.closed)hooks.closed();
}
void service(){
 // Never called from close(): deleting the current event's ancestors also
 // invokes textarea/keyboard deletion hooks and can recurse into layout.
 if(retiringPrompt){auto* old=retiringPrompt;retiringPrompt=nullptr;lv_obj_delete(old);}
 if(retiringWindow){auto* old=retiringWindow;retiringWindow=nullptr;lv_obj_delete(old);}
 if(!model)return;
 if(closing&&!busy()){Lock lock;model->~Model();heap_caps_free(model);model=nullptr;return;}
 if(!window)return;
 if(!busy()&&!handled){
  handled=true;
  if(model->result==0&&page&&model->offset>=model->total){page=model->total?unsigned((model->total-1)/PageRows):0;launch(Status,{"git","status"});return;}
  // Keep the Explorer working directory; project is only the repository root.
  if(model->nextDirectory[0]&&model->result==0&&!model->pendingContext){copy(model->requestedDirectory,model->directory);model->requestedSelection[0]=model->contextSelection[0]=0;if(hooks.directoryChanged)hooks.directoryChanged(model->directory);}
  if(model->contextSelection[0]&&!model->selectedPath[0]){const auto relative=Tab5ExplorerGit::relativePath(model->project,model->contextSelection);copy(model->selectedPath,relative.c_str());}
  for(unsigned i=0;i<model->count;++i)if(!strcmp(model->selectedPath,model->entries[i].path)){selected=i;break;}
  std::string title="Git · ";title+=model->directory;if(model->branch[0]){title+=" · ";title+=model->branch;}lv_label_set_text(heading,title.c_str());
  std::string rootLabel=tr("Dépôt");rootLabel+=" : ";rootLabel+=model->project[0]?model->project:model->directory;
  if(model->upstreamBranch[0]){rootLabel+=" · ";rootLabel+=model->remoteName;rootLabel+="/";rootLabel+=model->upstreamBranch;}
  lv_label_set_text(contextLabel,rootLabel.c_str());
  char counts[100];snprintf(counts,sizeof(counts),"%u–%u / %u",model->count?model->offset+1:0,model->offset+model->count,unsigned(model->total));
  const bool plainFolder=model->action==Status&&model->notRepository&&model->result==1;
  std::string status=tr(plainFolder?"Dossier sans dépôt Git":model->result==130?"Opération interrompue":model->result?"Opération Git en échec":model->repository?(model->total?"Fichiers modifiés":"Aucune modification locale"):"Dossier sans dépôt Git");
  if(model->result==0&&model->action!=Status){status=tr("Commande terminée");status+=" · ";status+=model->args[1];status+=" · ";status+=model->directory;}
  if(model->result==0&&model->action==Diff){Lock lock;if(!model->output.size()){const char* empty=tr("Aucune différence dans cette sélection.");model->output.append(empty,strlen(empty));model->output.append("\n",1);++revision;}}
  if(plainFolder){Lock lock;model->output.clear();++revision;}
  if(model->repository&&model->total){status+=" · ";status+=counts;}
  if(model->repository&&model->trackingKnown){status+=" · ";status+=std::to_string(model->ahead);status+=" ";status+=tr("à envoyer");status+=" · ";status+=std::to_string(model->behind);status+=" ";status+=tr("à recevoir (dernier relevé)");}
  if(model->truncated){status+=" · ";status+=tr("Dernières lignes du rapport");}lv_label_set_text(state,status.c_str());
  lv_obj_set_height(spacer,std::max(1u,model->count*RowHeight));lv_obj_scroll_to_y(list,0,LV_ANIM_OFF);paintRows();controls();
  if(Tab5Focus.contains(window,Tab5Focus.focused())){if(selected>=0)focusEntry(selected);else Tab5Focus.focusInitial(window,model->repository?(model->count?row[0]:buttons[Status]):buttons[New]);}
  // Only an operation that can change Explorer's files invalidates its listing.
  // Clone/New already notify directoryChanged when they enter their new folder.
  const bool treeChanged=model->treeChanged&&!(model->nextDirectory[0]&&model->result==0&&!model->pendingContext);
  if(hooks.changed&&treeChanged){const bool restore=Tab5Focus.contains(window,Tab5Focus.focused());if(restore)Tab5Focus.remember(window);hooks.changed();if(restore&&window)Tab5Focus.restore(window);}
  if(requestedStart!=Start::Browse){auto start=requestedStart;requestedStart=Start::Browse;showPrompt(start==Start::Create?New:Clone);}
 }
 if(model->pendingContext&&!busy()&&!prompt&&!retiringPrompt&&!Tab5VaultPrompt::scope()){if(!selectContext())startContext();return;}
 const uint32_t rev=revision.load();if(rev!=painted&&(!busy()||millis()-lastPaint>=500)&&!Tab5TextInput::hasSelection(report)){
  {Lock lock;model->output.copy(model->snapshot,sizeof(model->snapshot));painted=revision.load();enabled(buttons[Clear],model->output.size()!=0);}
  const bool bottom=lv_obj_get_scroll_bottom(report)<8;const int scroll=lv_obj_get_scroll_y(report);
  lv_textarea_set_text(report,model->snapshot);lv_obj_scroll_to_y(report,bottom?LV_COORD_MAX:scroll,LV_ANIM_OFF);lastPaint=millis();
 }
}
bool key(const writing::KeyEvent& e){
 if(!window||lv_obj_get_screen(window)!=lv_screen_active())return false;
 for(auto* ancestor=window;ancestor;ancestor=lv_obj_get_parent(ancestor))if(lv_obj_has_flag(ancestor,LV_OBJ_FLAG_HIDDEN))return false;
 using K=writing::Key;auto* focused=Tab5Focus.focused();
 if(!prompt&&!Tab5Focus.contains(window,focused))return false;
 // Explorer owns Tab traversal across its file list and embedded Git pane.
 if(!prompt&&e.key==K::Tab)return false;
 if(e.key==K::Escape){if(prompt){if(focused&&lv_obj_check_type(focused,&lv_dropdown_class)&&lv_dropdown_is_open(focused)){lv_dropdown_close(focused);return true;}dismissPrompt();}else close();return true;}
 if((e.ctrl||e.meta)&&e.key==K::Text&&e.text[0]=='.'){action(Stop);return true;}
 if(!prompt&&(e.ctrl||e.meta)&&e.key==K::Text&&(e.text[0]=='l'||e.text[0]=='L')){clearOutput();return true;}
 if(prompt&&e.key==K::Enter&&(e.ctrl||e.meta||focused==fields[0]||focused==fields[1])){
  if(promptAction==Clone&&focused==fields[0]&&!e.ctrl&&!e.meta){Tab5Focus.editObject(fields[1]);return true;}submitPrompt();return true;
 }
 if(focused&&lv_obj_check_type(focused,&lv_textarea_class)&&Tab5TextInput::key(focused,e))return true;
 if(e.key==K::F5&&!prompt){action(Status);return true;}
 if(!prompt&&!busy()){
  if(e.alt&&(e.key==K::Left||e.key==K::Right)){if(e.key==K::Left&&page)action(Previous);else if(e.key==K::Right&&model->total>model->offset+model->count)action(Next);return true;}
  for(unsigned i=0;i<VisibleRows;++i)if(focused==row[i]){
   const int index=int(rowIndex[i]);
   const bool previous=e.key==K::Up||(e.key==K::Tab&&e.shift);
   const bool next=e.key==K::Down||(e.key==K::Tab&&!e.shift);
   if(previous&&index>0){focusEntry(index-1);return true;}
   if(next&&index+1<int(model->count)){focusEntry(index+1);return true;}
  }
 }
 if(!prompt&&(e.key==K::PageUp||e.key==K::PageDown)){if(!busy())lv_obj_scroll_by(list,0,e.key==K::PageUp?320:-320,LV_ANIM_OFF);return true;}
 Tab5Focus.key(e,scope());return true;
}
}
