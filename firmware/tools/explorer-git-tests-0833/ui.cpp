#include <lvgl.h>
#include <Arduino.h>
#include "Tab5VaultPrompt.h"
#include "Tab5KeyStore.h"
#include "Tab5Secrets.h"
#include "Tab5Localization.h"
#include <misc/cache/instance/lv_image_cache.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5UiStyle.h"
#include "Tab5ContextMenu.h"
#include "Tab5GitView.h"
#include "Tab5FileSelection.h"
#include "Tab5FileSort.h"

static unsigned checks=0,failures=0;
static void check(bool ok,const char* why){++checks;if(!ok){++failures;fprintf(stderr,"FAIL %s\n",why);}}
static bool english=false;
const char* T(const char* p){if(english&&!strcmp(p,"Suppr"))return "Delete";return p;}
static bool baseline=false;
enum class Page{Home,Files,Audio};static Page activePage=Page::Files;
enum class TextDialogAction{None,NewFolder};static TextDialogAction dialogAction=TextDialogAction::None;
static lv_obj_t *root,*filesPage,*fileList,*dialogOverlay,*dialogBox,*dialogInput,*keyboardTarget,*filePopupOverlay,*fileDeleteOverlay,*fileOperationOverlay,*selectedFileBtn,*dialogConfirm;
static lv_obj_t *filePreviewTitle,*filePreviewMeta,*filePreviewLabel;
static lv_obj_t *filePreviewScroll,*filePreviewImage;
static lv_image_dsc_t filePreviewImageDsc{};
static unsigned imageDecodes=0,imageFrees=0;static bool imageDecodeFails=false;
static int imageWidth=0,imageHeight=0;
struct M5Canvas{void deleteSprite(){++imageFrees;}};
static M5Canvas* filePreviewImageCanvas=nullptr;
static String filePreviewImagePath;
static lv_obj_t *fileActionsMenuBtn,*fileUsbButton,*fileParentButton,*fileGitHost;
static bool sdReady=true;namespace Tab5UsbStorage{bool mounted(){return true;}}
static String fileSearchQuery;
static bool fileSelectionLongPress=false;static unsigned fileActivations=0,previews=0;
static lv_indev_t* lvTouch=nullptr;
struct FileEntryCtx{char path[512];bool isDir=false,selectedOnPress=false;};
static String selectedPath,currentDir="/",pendingRenamePath,dialogDirectory,clipboardPath;
static size_t pendingMapPlaceIndex=0;static bool selectedIsDir=false,selectedIsText=false,selectedIsImage=false,selectedIsAudio=false,selectedIsVideo=false,fileMultiSelect=false,fileSelectionFocusOnly=false,fileSelectAllSearch=false,fileSelectAllDirectory=false;
static std::vector<std::string> fileClipboardPaths;
struct FileSelectionRow{std::string path;bool directory;lv_obj_t* object;};static std::vector<FileSelectionRow> fileSelectionRows;
static Tab5FileSelection fileSelection;
static struct{bool active=false;}filePasteBatch;
namespace Tab5FileOperation{bool busy(){return false;}}
namespace Tab5Files{bool showHidden=false;bool userWritable(const char* p){return std::string(p).find("bad")==std::string::npos;}}
static unsigned fsRevision=1,filesRefreshed=0,audioRefreshed=0,mkdirCalls=0,deletedWithLiveDialog=0;
static std::set<std::string> directories,files;
struct File{bool valid=false,dir=false;explicit operator bool()const{return valid;}bool isDirectory()const{return dir;}size_t size()const{return valid?10:0;}void close(){}};
static struct Fs{File open(const char* p,int=0){return {directories.count(p)||files.count(p),directories.count(p)!=0};}bool exists(const char* p){return directories.count(p)||files.count(p);}bool mkdir(const char* p){++mkdirCalls;if(exists(p))return false;directories.insert(p);++fsRevision;return true;}}tab5FS;
constexpr int FILE_READ=0;
static uint32_t tab5FsRevision(){return fsRevision;}
static String joinPath(const String& a,const String& b){return a+(a=="/"?"":"/")+b;}
static String sanitizeFilename(const String& a,bool){return a;}
static bool pathStorageReady(const String&){return true;}
static bool isEpubExtension(const String& p){return p.find(".epub")!=std::string::npos;}
static bool isPythonExtension(const String& p){return p.find(".py")!=std::string::npos;}
static bool isShellExtension(const String& p){return p.size()>=3&&p.substr(p.size()-3)==".sh";}
static bool isExecutableExtension(const String& p){return p.find(".elf")!=std::string::npos;}
static bool isFirmwareExtension(const String& p){return p.find(".bin")!=std::string::npos;}
static bool isAudioExtension(const String&){return false;}
static bool isVideoExtension(const String&){return false;}
static bool isReadableTextFile(const String& p){return isShellExtension(p);}
static bool isReadableImageFile(const String& p){return p.find(".jpg")!=std::string::npos;}
static String baseNameOf(const String& p){return p.substr(p.find_last_of('/')+1);}
static void openAudioPath(const String&){}
static void startVideoViewerSource(const String&,const String&){}
static void loadDocumentFrom(const String&){}
static void startReader(const String&){}
static void startImageViewer(const String&){++fileActivations;}
static bool loadSdImageCanvas(const String&,int width,int height,M5Canvas*& canvas,lv_image_dsc_t& dsc,int& nw,int& nh,String& error){
 ++imageDecodes;imageWidth=width;imageHeight=height;
 if(imageDecodeFails){error="Image inaccessible";return false;}
 canvas=new M5Canvas;static uint16_t pixels[4]{};dsc={};dsc.header.magic=LV_IMAGE_HEADER_MAGIC;dsc.header.cf=LV_COLOR_FORMAT_RGB565;dsc.header.w=nw=2;dsc.header.h=nh=2;dsc.header.stride=4;dsc.data_size=8;dsc.data=reinterpret_cast<const uint8_t*>(pixels);return true;
}
static void cancelFolderPreview(){}
/* FREE_IMAGE */
/* SHOW_IMAGE */
static lv_color_t C_TEXT(){return lv_color_hex(0xffffff);}
static lv_color_t C_SURFACE(){return lv_color_hex(0x202020);}
static lv_color_t C_SELECTED(){return lv_color_hex(0x505050);}
static void toast(const char*,unsigned=0){}
static void markSettingsDirty(){}
static void refreshFileList(){++filesRefreshed;}
static lv_obj_t* audioList=nullptr;
static void refreshAudioList(){++audioRefreshed;}
static lv_obj_t* keyboardScope(){return dialogOverlay?dialogOverlay:filesPage;}
static void setKeyboardVisible(bool,lv_obj_t* input=nullptr){if(input)keyboardTarget=input;lv_obj_update_layout(root);}
static bool largeUiFont=false;
static const lv_font_t* uiFont(){return largeUiFont?&lv_font_montserrat_24:LV_FONT_DEFAULT;}
static void newDocEvent(lv_event_t*){}
static void newFolderEvent(lv_event_t*){}
static void fileSearchEvent(lv_event_t*){}
static void upFolderEvent(lv_event_t*){}
static void selectedOpenEvent(lv_event_t*){}
static void selectedReadEvent(lv_event_t*){}
static void selectedExecuteEvent(lv_event_t*){}
static void copySelectedEvent(lv_event_t*){}
static void cutSelectedEvent(lv_event_t*){}
static void pasteClipboardEvent(lv_event_t*){}
static void renameSelectedEvent(lv_event_t*){}
static void deleteDocEvent(lv_event_t*){}
static void showFileInfoEvent(lv_event_t*){}
static void toggleSelectedHidden(lv_event_t*){}
static void selectAllFiles(){}
static void previewSelectedFile(){++previews;}
static void updateFileSelectionUi(){}
static void updateFileActionState(){}
static std::vector<std::string> fileVisiblePaths(){return {"/photo.jpg"};}
static void fileEntryActivateEvent(lv_event_t*){}
static void clearFilePreview(){selectedPath="";selectedIsDir=false;fileSelection.clear();}
enum class Tab5Tool{Terminal,Print,Vault,Firmware};
static struct{lv_obj_t* scope(){return nullptr;}void open(Tab5Tool,const char* =nullptr){}void openWith(const char*){}void launchExecutable(const char*){}void openPythonApp(const char*){}}Tab5Tools;
namespace Tab5Terminal{static std::string submitted;static unsigned scripts=0;static bool occupied=false;bool busy(){return occupied;}bool submit(const char* text){submitted=text;return true;}bool runScript(const char* text){submitted=text;++scripts;return !occupied;}}

static String parentDirOf(const String& p){auto i=p.find_last_of('/');return i==std::string::npos||i==0?"/":p.substr(0,i);}
static void freeFilePreviewImage(){}
static std::string openedImage;static bool openImageEditor(const char* path=nullptr){openedImage=path?path:"";return true;}
static bool tab5ExecutableRun(const char*,const char*,const char*){return true;}
static const char* tab5ExecutableStatus(){return "ok";}
#include "explorer_context.inc"
#include "Tab5GitView.cpp"
// Production PIN modal, with only the asynchronous keystore worker boundary
// scripted. Cancelling increments its generation just as the device store does.
namespace vaultBackend {static bool working=false,success=false,failStart=false;static unsigned generation=0,token=0,starts=0;static std::string submitted;}
namespace Tab5KeyStore {
bool busy(){return vaultBackend::working;}bool credentialsAccessible(){return !busy()&&Tab5Secrets::unlocked;}bool unlocked(){return credentialsAccessible();}bool usingDevice(){return true;}bool pinConfigured(){return true;}unsigned pinRetrySeconds(){return 0;}bool operationSucceeded(){return vaultBackend::success;}
const char* status(){return busy()?"Ouverture du coffre.":vaultBackend::success?"Coffre déverrouillé":"PIN incorrect";}
void lock(){++vaultBackend::generation;Tab5Secrets::unlocked=false;}void finishUi(){}
bool beginDeviceUnlock(const char* pin,bool,bool,bool){++vaultBackend::starts;if(vaultBackend::failStart)return false;vaultBackend::submitted=pin;vaultBackend::token=vaultBackend::generation;vaultBackend::working=true;vaultBackend::success=false;return true;}
}
namespace Tab5Vault {bool busy(){return false;}void cancel(){}}
namespace tab5vault {void wipe(void* value,size_t n){volatile unsigned char* p=static_cast<volatile unsigned char*>(value);while(n--)*p++=0;}}
namespace writing {bool usbConnected(){return true;}}
static lv_obj_t* pinField(lv_obj_t* object){if(!object)return nullptr;if(lv_obj_check_type(object,&lv_textarea_class))return object;for(unsigned i=0;i<lv_obj_get_child_count(object);++i)if(auto* field=pinField(lv_obj_get_child(object,i)))return field;return nullptr;}
static void completePin(bool success){check(vaultBackend::working,"PIN worker pending");vaultBackend::working=false;vaultBackend::success=success&&vaultBackend::token==vaultBackend::generation;Tab5Secrets::unlocked=vaultBackend::success;Tab5VaultPrompt::service();}
static void submitPin(){auto* field=pinField(Tab5VaultPrompt::scope());check(field,"real PIN input present");if(!field)return;lv_textarea_set_text(field,"1234");Tab5Focus.editObject(field);writing::KeyEvent enter{};enter.key=writing::Key::Enter;check(Tab5VaultPrompt::key(enter),"physical Enter submits real PIN");}
static void acceptPin(){submitPin();completePin(true);}
static std::vector<std::vector<std::string>> commands;static std::vector<std::string> commandDirs;
static bool indexed=false,identity=false,gitFailure=false;static unsigned fakeEntries=2;
namespace Tab5Git{
bool busy(){return false;}
int run(const std::string&,const std::string& cwd,const std::vector<std::string>& args,Output out,Cancel cancel,Translate,Output diag,Observer o,Tab5Resources::Reservation* reservation){
 if(reservation)reservation->release();commands.push_back(args);commandDirs.push_back(cwd);if(cancel&&cancel())return 130;
 if(gitFailure){const char* s="diagnostic réel de frontière simulée";diag(s,strlen(s));return 1;}
 if(o.worktreeChanged&&(args[1]=="pull"||args[1]=="init"||args[1]=="clone"))o.worktreeChanged();
 if(args[1]=="add")indexed=true;if(args[1]=="commit")indexed=false;if(args[1]=="config")identity=true;
 if(out&&args[1]!="status"){std::string s="executed "+args[1]+"\n";out(s.data(),s.size());}
 if(cwd=="/plain"&&args[1]=="status"){if(o.notRepository)o.notRepository();return 1;}
 const char* project=cwd.rfind("/other",0)==0?"/other":"/repo";
 if(o.project)o.project(project);if(o.repository)o.repository("main",false,false);if(o.remote)o.remote("origin","https://example.org/r.git");
 if(o.config){if(identity){o.config("user.name","A");o.config("user.email","a@example.org");}o.config("tab5.usernameSecret","GIT_USER");o.config("tab5.tokenSecret","GIT_TOKEN");}
 if(o.status)for(unsigned i=0;i<fakeEntries;++i){std::string p=i==0?"sub/a[1].txt":i==1?"root.txt":"f"+std::to_string(i);o.status(p.c_str(),indexed?IndexModified:WorkModified);}
 return 0;
}
}
#include "Tab5ExplorerMenus.inc"
static void flushWorker(){check(pendingTask!=nullptr,"worker scheduled");if(!pendingTask)return;auto fn=pendingTask;pendingTask=nullptr;fn(pendingTaskArgument);Tab5GitView::service();lv_obj_update_layout(root);}
static void checkPaneBounds(){
 using namespace Tab5GitView;lv_obj_update_layout(root);const int height=lv_obj_get_content_height(window),width=lv_obj_get_content_width(window);
 for(auto* object:{toolbar,list,report}){check(object,"pane section exists");if(!object)continue;if(lv_obj_get_style_y(object,LV_PART_MAIN)+lv_obj_get_height(object)>height)fprintf(stderr,"bounds %s y=%d h=%d content=%d\n",object==toolbar?"toolbar":object==list?"list":"report",int(lv_obj_get_style_y(object,LV_PART_MAIN)),int(lv_obj_get_height(object)),height);check(lv_obj_get_style_y(object,LV_PART_MAIN)>=0&&lv_obj_get_style_y(object,LV_PART_MAIN)+lv_obj_get_height(object)<=height,"pane section remains inside vertical content bounds");check(lv_obj_get_style_x(object,LV_PART_MAIN)>=0&&lv_obj_get_style_x(object,LV_PART_MAIN)+lv_obj_get_width(object)<=width,"pane section remains inside horizontal content bounds");}
}
static writing::KeyEvent key(writing::Key k){writing::KeyEvent e{};e.key=k;return e;}
static void setup(){
 lv_init();auto* display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);root=lv_obj_create(nullptr);lv_screen_load(root);Tab5Focus.begin(root,LV_FONT_DEFAULT);
 filesPage=lv_obj_create(root);lv_obj_set_size(filesPage,1180,650);lv_obj_remove_flag(filesPage,LV_OBJ_FLAG_SCROLLABLE);
 fileList=lv_list_create(filesPage);lv_obj_set_size(fileList,400,570);lv_obj_set_pos(fileList,0,55);
 fileGitHost=lv_obj_create(filesPage);lv_obj_remove_style_all(fileGitHost);lv_obj_set_size(fileGitHost,700,570);lv_obj_set_pos(fileGitHost,420,55);
 directories={"/repo","/repo/sub","/repo/.git","/other","/other/.git"};files={"/repo/.git/HEAD","/repo/.git/config","/other/.git/HEAD","/other/.git/config"};
}
int main(){
 setup();using namespace Tab5GitView;
 currentDir="/repo/sub";selectedPath="";selectedIsDir=false;fileSelection.clear();check(selectedFolderIsGit(),"nested current folder discovered without left selection");
 currentDir="/USB/plain";check(!selectedFolderIsGit(),"repository discovery does not leave USB");
 currentDir="/repo/sub";openFileGit();check(scope()!=nullptr,"open current folder without selection");check(lv_obj_get_parent(window)==fileGitHost,"Git embedded in right Explorer pane");check(modalScope()==nullptr,"pane is not modal");flushWorker();checkPaneBounds();check(commandDirs.back()=="/repo/sub"&&std::string(model->directory)=="/repo/sub","status does not replace cwd with repository root");
 const unsigned beforeIndex=filesRefreshed;
 action(Stage);flushWorker();check(commands[commands.size()-2]==std::vector<std::string>({"git","add","--","/repo/sub"}),"folder stage literal subtree only");check(std::string(lv_label_get_text(state)).find("Commande terminée")!=std::string::npos,"successful command feedback shown");
 check(filesRefreshed==beforeIndex,"staging index does not rebuild Explorer listing");
 indexed=false;action(Status);flushWorker();selectedPath="/repo/sub/a[1].txt";selectedIsDir=false;fileSelection.add(selectedPath.c_str());context(fileGitDirectory().c_str(),selectedPath.c_str());check(!pendingTask,"same-directory file selection reuses status without worker");check(selected==0,"Explorer file preselects repository status row");
 action(Stage);flushWorker();check(commands[commands.size()-2].back()=="/repo/sub/a[1].txt","selected status row uses absolute literal project path");
 indexed=false;action(Status);flushWorker();selected=-1;model->contextSelection[0]=0;action(StageAll);flushWorker();check(commands[commands.size()-2]==std::vector<std::string>({"git","add","-A"}),"explicit whole repository stage");
 action(Commit);check(prompt&&promptAction==Settings,"missing identity opens setup rather than inert commit");
 Tab5GitView::key(::key(writing::Key::Escape));service();check(!modalScope(),"Escape closes setup");identity=true;action(Status);flushWorker();
 action(Diff);check(prompt&&promptAction==Diff,"diff choices modal");lv_dropdown_set_selected(choice,1);submitPrompt();service();flushWorker();check(commands[commands.size()-2]==std::vector<std::string>({"git","diff","--cached","--","/repo/sub"}),"diff scopes current folder");
 const unsigned beforeRead=filesRefreshed;
 action(Log);flushWorker();Tab5Secrets::unlocked=true;action(Fetch);flushWorker();
 check(filesRefreshed==beforeRead,"history and fetch do not rebuild Explorer listing");
 model->workChanges=false;model->indexChanges=false;controls();action(Pull);flushWorker();
 check(filesRefreshed==beforeRead+1,"pull invalidates Explorer files once");
 // A different upstream remote must be visible and used by configuration.
 copy(model->remoteName,"upstream");copy(model->upstreamBranch,"release");model->ahead=2;model->behind=3;model->trackingKnown=true;
 showPrompt(Push);check(std::string(lv_label_get_text(lv_obj_get_child(promptBody,0))).find("upstream/release")!=std::string::npos,"push confirmation names actual remote branch");dismissPrompt();service();
 showPrompt(Settings);lv_dropdown_set_selected(choice,2);lv_obj_send_event(choice,LV_EVENT_VALUE_CHANGED,nullptr);lv_textarea_set_text(fields[0],"https://example.org/new.git");submitPrompt();service();flushWorker();check(commands[commands.size()-2]==std::vector<std::string>({"git","remote","set-url","upstream","https://example.org/new.git"}),"remote configuration updates contextual remote rather than hardcoded origin");
 // A long status page still uses the selected Explorer path, never a whole folder.
 indexed=false;fakeEntries=200;context("/repo","/repo/f199");flushWorker();check(selected==-1,"off-page status file not spuriously selected");action(Stage);flushWorker();check(commands[commands.size()-2].back()=="/repo/f199","off-page file stage remains exact");fakeEntries=2;
 context("/repo/sub",nullptr);flushWorker();Tab5Secrets::unlocked=true;action(Fetch);check(busy(),"fetch queued");context("/other",nullptr);check(std::string(model->directory)=="/repo/sub","navigation does not mutate active worker cwd");flushWorker();check(pendingTask!=nullptr,"new context starts after worker ends");flushWorker();check(std::string(model->directory)=="/other","last navigation becomes active context");
 auto* left=lv_button_create(fileList);Tab5Focus.focusObject(left);action(Status);flushWorker();check(Tab5Focus.focused()==left,"background completion preserves Explorer focus");check(!Tab5GitView::key(::key(writing::Key::F5)),"pane ignores keys focused in left list");Tab5Focus.focusObject(buttons[Status]);check(!Tab5GitView::key(::key(writing::Key::Tab)),"Tab traversal delegated to full Explorer");
 lv_obj_add_flag(filesPage,LV_OBJ_FLAG_HIDDEN);check(!Tab5GitView::key(::key(writing::Key::Escape)),"hidden Explorer cannot steal keys");lv_obj_remove_flag(filesPage,LV_OBJ_FLAG_HIDDEN);auto* native=lv_obj_create(nullptr);lv_screen_load(native);check(!Tab5GitView::key(::key(writing::Key::Escape)),"native screen cannot lose keys to background Git");lv_screen_load(root);lv_obj_delete(native);
 // Full-screen dialogs preserve the pane and the Explorer context.
 showPrompt(Clone);check(lv_obj_get_parent(prompt)==root,"input dialog has full OS width");lv_textarea_set_text(fields[0],"https://example.org/r.git");Tab5Focus.focusObject(fields[0]);Tab5GitView::key(::key(writing::Key::Enter));check(Tab5Focus.focused()==fields[1],"Enter advances clone URL to name");lv_textarea_set_text(fields[1],"new clone");
 Tab5Secrets::unlocked=false;unlockVault();check(Tab5VaultPrompt::scope()!=nullptr,"inline PIN requested");acceptPin();check(std::string(lv_textarea_get_text(fields[0]))=="https://example.org/r.git"&&std::string(lv_textarea_get_text(fields[1]))=="new clone","PIN preserves clone form");lv_dropdown_set_selected(secretChoices[0],1);lv_dropdown_set_selected(secretChoices[1],2);Tab5Focus.focusObject(fields[1]);Tab5GitView::key(::key(writing::Key::Enter));service();flushWorker();check(currentDir=="/other/new clone","successful clone navigates current Explorer");check(commands[commands.size()-2][1]=="clone","clone command executed");
 // Cancellation keeps the Git form editable; no command is silently launched.
 showPrompt(Clone);lv_textarea_set_text(fields[0],"https://example.org/second.git");lv_textarea_set_text(fields[1],"second clone");Tab5Secrets::unlocked=false;
 const auto commandCount=commands.size();unlockVault();check(Tab5VaultPrompt::scope(),"second inline PIN opens");Tab5VaultPrompt::key(::key(writing::Key::Escape));
 check(prompt&&std::string(lv_textarea_get_text(fields[0]))=="https://example.org/second.git"&&std::string(lv_textarea_get_text(fields[1]))=="second clone","cancel PIN preserves both clone inputs");check(commands.size()==commandCount&&!pendingTask,"PIN cancel does not execute Git");
 unlockVault();vaultBackend::failStart=true;submitPin();check(Tab5VaultPrompt::scope()&&!vaultBackend::working&&!pendingTask,"PIN memory-admission failure preserves Git form");vaultBackend::failStart=false;acceptPin();check(prompt&&std::string(lv_textarea_get_text(fields[1]))=="second clone","PIN retry preserves form values");
 const auto childCount=lv_obj_get_child_count(promptBody);unlockVault();check(lv_obj_get_child_count(promptBody)==childCount,"repeat unlocked refresh does not duplicate secret labels");dismissPrompt();service();
 // A requested folder change waits for the credential continuation and worker.
 context("/repo/sub",nullptr);flushWorker();Tab5Secrets::unlocked=false;action(Fetch);check(Tab5VaultPrompt::scope()&&!pendingTask,"protected Fetch waits for PIN");context("/other",nullptr);service();check(std::string(model->directory)=="/repo/sub","PIN preserves captured command directory");acceptPin();check(pendingTask,"unlock resumes Fetch");flushWorker();check(commandDirs[commandDirs.size()-2]=="/repo/sub","resumed Fetch uses original directory");check(pendingTask,"deferred navigation resumes after command");flushWorker();check(std::string(model->directory)=="/other","deferred folder eventually displayed");
 // A cancelled automatic continuation cannot start a network command.
 Tab5Secrets::unlocked=false;const auto beforeCancel=commands.size();action(Fetch);submitPin();Tab5VaultPrompt::key(::key(writing::Key::Escape));completePin(true);check(!Tab5Secrets::unlocked&&!pendingTask&&commands.size()==beforeCancel,"cancelled PIN worker cannot resume Fetch later");
 // Closing/reopening Git invalidates old PIN continuations, including an
 // attempted Fetch while the older modal still owns the unlock operation.
 Tab5Secrets::unlocked=false;action(Fetch);check(Tab5VaultPrompt::scope(),"old view PIN prompt exists");const auto oldGeneration=pendingVaultGeneration;close();service();currentDir="/repo";selectedPath="";fileSelection.clear();openFileGit();flushWorker();action(Fetch);check(pendingVaultGeneration==oldGeneration,"failed second open does not rewrite old continuation generation");const auto beforeReopen=commands.size();acceptPin();check(!pendingTask&&commands.size()==beforeReopen,"old view unlock cannot execute in replacement Git view");
 // Replacing a Git input while its PIN is pending must not submit the new form.
 showPrompt(Push);Tab5Secrets::unlocked=false;submitPrompt();check(Tab5VaultPrompt::scope(),"push confirmation opens PIN");dismissPrompt();service();showPrompt(New);lv_textarea_set_text(fields[0],"keep-editing");const auto beforeReplacement=commands.size();acceptPin();check(prompt&&promptAction==New&&!pendingTask&&commands.size()==beforeReplacement,"old push PIN cannot submit replacement New form");dismissPrompt();service();
 // Every close path defers widget destruction and frees the model after worker.
 for(unsigned i=0;i<100;++i){close();service();check(!model&&!scope(),"close releases Git model and view");largeUiFont=i==0;openFileGit();flushWorker();if(i==0){checkPaneBounds();check(lv_obj_get_style_text_font(lv_obj_get_child(buttons[Status],0),LV_PART_MAIN)==&lv_font_montserrat_24,"pane controls follow 24px OS font");}showPrompt(New);lv_textarea_set_text(fields[0],"bad/name");Tab5Focus.focusObject(fields[0]);Tab5GitView::key(::key(writing::Key::Enter));check(prompt!=nullptr&&!pendingTask,"invalid folder stays in input");Tab5GitView::key(::key(writing::Key::Escape));service();check(!modalScope(),"Escape retires prompt safely");}
 action(Status);close();service();check(model!=nullptr,"live worker retains model during close");flushWorker();check(model==nullptr,"model freed after final worker access");
 printf("{\"checks\":%u,\"failures\":%u,\"dialogCycles\":100}\n",checks,failures);return failures?1:0;
}
