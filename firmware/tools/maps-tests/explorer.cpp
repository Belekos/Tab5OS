#include <lvgl.h>
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
struct String:std::string{using std::string::string;String(const std::string& v):std::string(v){}String()=default;};
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
static lv_obj_t *fileActionsMenuBtn,*fileUsbButton,*fileParentButton;
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
static const lv_font_t* uiFont(){return LV_FONT_DEFAULT;}
static void newDocEvent(lv_event_t*){}
static void newFolderEvent(lv_event_t*){}
static void fileSearchEvent(lv_event_t*){}
static void upFolderEvent(lv_event_t*){}
static void selectedOpenEvent(lv_event_t*);
static void selectedReadEvent(lv_event_t*){}
static void selectedExecuteEvent(lv_event_t*);
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
static void fileEntryActivateEvent(lv_event_t*);
static void clearFilePreview(){selectedPath="";selectedIsDir=false;fileSelection.clear();}
enum class Tab5Tool{Terminal,Print,Vault,Firmware};
static struct{lv_obj_t* scope(){return nullptr;}void open(Tab5Tool,const char* =nullptr){}void openWith(const char*){}void launchExecutable(const char*){}void openPythonApp(const char*){}}Tab5Tools;
namespace Tab5Terminal{static std::string submitted;static unsigned scripts=0;static bool occupied=false;bool busy(){return occupied;}bool submit(const char* text){submitted=text;return true;}bool runScript(const char* text){submitted=text;++scripts;return !occupied;}}
namespace Tab5GitView{static Start requested;static std::string directory;bool open(lv_obj_t*,const char* p,Hooks,Start s){requested=s;directory=p;return true;}lv_obj_t* scope(){return nullptr;}}
/* SELECTED_GIT */
/* CLOSE_DIALOG */
/* CLOSE_POPUP */
static void createFolder(){String value=lv_textarea_get_text(dialogInput);/* MKDIR */}
/* MENUS */
/* ROW_PRESS */
/* ROW_CLICK */
/* ROW_DOUBLE_CLICK */
/* OPEN_SHELL */
/* ROW_ACTIVATE */
/* SELECTED_OPEN */
/* SELECTED_EXECUTE */
static void imageSelection(){File f;String meta="Image 1000 octets";/* IMAGE_SELECTION */}
static lv_obj_t* makeButton(lv_obj_t* parent,const char* text,int w,int h){auto* b=lv_button_create(parent);lv_obj_set_size(b,w,h);lv_label_set_text(lv_label_create(b),text);return b;}
static lv_obj_t* makeIconButton(lv_obj_t* parent,const char* text,int w,int h){return makeButton(parent,text,w,h);}
/* PARENT_ICON */
/* TOOLBAR */
static void childDeleted(lv_event_t* e){
 if(dialogOverlay||dialogInput||dialogBox||keyboardTarget){++deletedWithLiveDialog;}
 // Reenter exactly the keyboard/layout boundary while LVGL is removing fields.
 if(keyboardTarget==lv_event_get_target_obj(e)){keyboardTarget=nullptr;setKeyboardVisible(false);}
}
static void showDialog(const char* name){
 dialogOverlay=lv_obj_create(root);lv_obj_set_size(dialogOverlay,720,500);
 dialogBox=lv_obj_create(dialogOverlay);dialogInput=lv_textarea_create(dialogBox);lv_textarea_set_text(dialogInput,name);
 lv_obj_add_event_cb(dialogInput,childDeleted,LV_EVENT_DELETE,nullptr);keyboardTarget=dialogInput;
 dialogConfirm=lv_button_create(dialogBox);lv_obj_add_event_cb(dialogConfirm,[](lv_event_t*){createFolder();},LV_EVENT_CLICKED,nullptr);
 Tab5Focus.focusInitial(dialogOverlay,dialogInput);dialogAction=TextDialogAction::NewFolder;dialogDirectory="/";
}
static bool containsText(lv_obj_t* o,const char* text){if(!o)return false;if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),text))return true;for(unsigned i=0;i<lv_obj_get_child_count(o);++i)if(containsText(lv_obj_get_child(o,i),text))return true;return false;}
static lv_obj_t* findText(lv_obj_t* o,const char* text){if(!o)return nullptr;if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),text))return lv_obj_get_parent(o);for(unsigned i=0;i<lv_obj_get_child_count(o);++i)if(auto* hit=findText(lv_obj_get_child(o,i),text))return hit;return nullptr;}
static void menuCheck(FileMenu which,const char* label,bool wanted,const char* why){showFileMenu(which,50,100);check(containsText(Tab5ContextMenu::scope(),label)==wanted,why);Tab5ContextMenu::close();Tab5ContextMenu::service();}
int main(int argc,char** argv){
 baseline=argc>1&&!strcmp(argv[1],"--baseline");const bool imageBaseline=argc>1&&!strcmp(argv[1],"--image-baseline");lv_init();lv_display_create(1280,720);root=lv_screen_active();filesPage=lv_obj_create(root);lv_obj_set_size(filesPage,1200,650);fileList=lv_list_create(filesPage);lv_obj_set_size(fileList,420,500);audioList=lv_obj_create(root);
 Tab5Focus.begin(root,LV_FONT_DEFAULT);Tab5ContextMenu::configure(uiFont,T);
 for(unsigned i=0;i<200;++i){char name[60];snprintf(name,sizeof(name),"folder-%u",i);showDialog(name);lv_obj_send_event(dialogConfirm,LV_EVENT_CLICKED,nullptr);check(!dialogOverlay&&!dialogInput&&!dialogBox,"dialog references cleared");check(directories.count(std::string("/")+name)==1,"folder created once");}
 check(mkdirCalls==200&&filesRefreshed==200,"one filesystem creation and Explorer refresh each time");
 check(audioRefreshed==0,"Explorer mkdir must not load or rebuild the inactive audio app");
 check(deletedWithLiveDialog==0,"DELETE keyboard callbacks cannot rediscover a retiring dialog");
 if(baseline){printf("BASELINE %u checks, %u expected failures\n",checks,failures);return failures==2?0:1;}
 filePreviewScroll=lv_obj_create(filesPage);lv_obj_set_size(filePreviewScroll,520,360);
 filePreviewMeta=lv_label_create(filesPage);filePreviewLabel=lv_label_create(filePreviewScroll);filePreviewImage=lv_image_create(filePreviewScroll);
 lv_obj_add_flag(filePreviewImage,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(filePreviewImage,LV_OBJ_FLAG_CLICKABLE);
 lv_obj_add_event_cb(filePreviewImage,selectedOpenEvent,LV_EVENT_CLICKED,nullptr);
 auto* imageRow=lv_button_create(filesPage);FileEntryCtx imageContext{};strcpy(imageContext.path,"/photo.jpg");
 lv_obj_add_event_cb(imageRow,fileEntryPressEvent,LV_EVENT_PRESSED,&imageContext);lv_obj_add_event_cb(imageRow,fileEntryClickEvent,LV_EVENT_CLICKED,&imageContext);lv_obj_add_event_cb(imageRow,fileEntryClickEvent,LV_EVENT_FOCUSED,&imageContext);
 lv_obj_add_event_cb(imageRow,fileEntryDoubleClickEvent,LV_EVENT_DOUBLE_CLICKED,&imageContext);
 for(unsigned i=0;i<20;++i){lv_obj_send_event(imageRow,LV_EVENT_PRESSED,nullptr);lv_obj_send_event(imageRow,LV_EVENT_FOCUSED,nullptr);lv_obj_send_event(imageRow,LV_EVENT_CLICKED,nullptr);}
 check(fileActivations==0&&selectedPath=="/photo.jpg","mouse single clicks and focus do not open the selected image");
 // The touchscreen follows select-then-open, independent of the desktop
 // double-click interval. Press snapshots selection before focus changes.
 lvTouch=lv_indev_create();lv_indev_set_type(lvTouch,LV_INDEV_TYPE_POINTER);
 clearFilePreview();lv_obj_send_event(imageRow,LV_EVENT_PRESSED,lvTouch);lv_obj_send_event(imageRow,LV_EVENT_FOCUSED,nullptr);lv_obj_send_event(imageRow,LV_EVENT_CLICKED,lvTouch);
 check(fileActivations==0,"first touch only selects even if focus precedes click");
 lv_tick_inc(5000);lv_obj_send_event(imageRow,LV_EVENT_PRESSED,lvTouch);lv_obj_send_event(imageRow,LV_EVENT_CLICKED,lvTouch);
 if(imageBaseline){check(fileActivations==1&&imageDecodes==0,".25 reproduces second-tap full viewer instead of right preview");printf("BASELINE .25 image activation: full viewer %u, previews %u\n",fileActivations,imageDecodes);return failures?1:0;}
 check(fileActivations==0&&imageDecodes==1,"second touch displays right preview after five seconds without launching viewer");
 check(imageWidth==lv_obj_get_content_width(filePreviewScroll)&&imageHeight==lv_obj_get_content_height(filePreviewScroll),"preview decoder is bounded to actual panel content area");
 check(!lv_obj_has_flag(filePreviewImage,LV_OBJ_FLAG_HIDDEN)&&lv_obj_has_flag(filePreviewLabel,LV_OBJ_FLAG_HIDDEN),"preview image replaces selection hint");
 lv_obj_send_event(imageRow,LV_EVENT_DOUBLE_CLICKED,lvTouch);check(fileActivations==0&&imageDecodes==1,"LVGL touch double-click cannot decode or launch twice");
 fileMultiSelect=true;lv_obj_send_event(imageRow,LV_EVENT_PRESSED,lvTouch);lv_obj_send_event(imageRow,LV_EVENT_CLICKED,lvTouch);
 check(fileActivations==0&&imageDecodes==1,"touch in multiselect does not decode or launch image");fileMultiSelect=false;
 fileSelectionLongPress=true;lv_obj_send_event(imageRow,LV_EVENT_CLICKED,lvTouch);check(fileActivations==0,"long press release does not launch image");
 freeFilePreviewImage();lv_obj_send_event(imageRow,LV_EVENT_DOUBLE_CLICKED,nullptr);check(fileActivations==0&&imageDecodes==2,"desktop mouse double-click displays right preview");
 lv_obj_send_event(filePreviewImage,LV_EVENT_CLICKED,nullptr);check(fileActivations==1,"clicking preview opens full image viewer");
 selectedOpenEvent(nullptr);check(fileActivations==2,"image action icon opens full image viewer");
 for(unsigned i=0;i<30;++i)showFilePreviewImage();check(imageDecodes==2,"repeated activation reuses thumbnail without SD decode");
 selectedPath="/different.jpg";showFilePreviewImage();check(imageDecodes==3&&filePreviewImagePath==selectedPath,"different path cannot reuse stale thumbnail");
 freeFilePreviewImage();imageDecodeFails=true;showFilePreviewImage();check(!filePreviewImageCanvas&&!strcmp(lv_label_get_text(filePreviewLabel),"Image inaccessible"),"decoder failure shows retryable error without stale canvas");imageDecodeFails=false;
 selectedPath="/photo.jpg";showFilePreviewImage();check(filePreviewImageCanvas&&imageDecodes==5,"preview can recover after decoder failure");
 fileSelection.clear();fileSelection.add("/photo.jpg");Tab5Focus.focusObject(imageRow);
 const unsigned previewBeforeMenus=previews;
 for(auto menu:{FileMenu::Create,FileMenu::Edit,FileMenu::Actions,FileMenu::View,FileMenu::Navigate,FileMenu::Sort}){
   for(unsigned i=0;i<20;++i){showFileMenu(menu,10,100);lv_obj_send_event(Tab5ContextMenu::scope(),LV_EVENT_CLICKED,nullptr);Tab5ContextMenu::service();check(Tab5Focus.focused()==imageRow,"dismissal restores selected row focus");check(previews==previewBeforeMenus,"menu dismissal preserves preview without filesystem reread");}
 }
 // Exercise actual LVGL pointer dispatch, including moving over labels,
 // rather than calling the menu's private hover callback.
 auto* pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_mode(pointer,LV_INDEV_MODE_EVENT);
 static lv_point_t mousePoint{1,1};lv_indev_set_read_cb(pointer,[](lv_indev_t*,lv_indev_data_t* d){d->point=mousePoint;d->state=LV_INDEV_STATE_RELEASED;});
 showFileMenu(FileMenu::Create,10,100);lv_obj_update_layout(root);
 auto* first=findText(Tab5ContextMenu::scope(),"Document");auto* second=findText(Tab5ContextMenu::scope(),"Nouveau dossier");
 for(unsigned i=0;i<40;++i){auto* target=i%2?first:second;lv_area_t bounds;lv_obj_get_coords(target,&bounds);mousePoint={bounds.x1+20,(bounds.y1+bounds.y2)/2};lv_indev_read(pointer);check(Tab5Focus.focused()==target,"context-menu highlight follows actual mouse pointer");check(!lv_obj_has_state(target==first?second:first,LV_STATE_FOCUS_KEY),"only hovered command keeps highlight");}
 mousePoint={1,1};lv_indev_read(pointer);check(Tab5Focus.focused()==nullptr,"leaving commands clears mouse highlight");Tab5ContextMenu::close();lv_indev_delete(pointer);
 english=true;showFileMenu(FileMenu::Actions,10,100);check(containsText(Tab5ContextMenu::scope(),"Delete"),"shortcut display passes through translator");Tab5ContextMenu::close();english=false;
 imageSelection();check(!strcmp(lv_label_get_text(filePreviewLabel),"Touchez à nouveau pour afficher l’aperçu."),"image selection shows metadata and preview instruction");
 strcpy(imageContext.path,"/USB/a'b $(danger); x/demo.sh");selectedPath=imageContext.path;selectedIsDir=false;selectedIsText=true;
 lv_obj_send_event(imageRow,LV_EVENT_READY,nullptr); // No activation binding in this fixture yet.
 lv_obj_add_event_cb(imageRow,fileEntryActivateEvent,LV_EVENT_READY,&imageContext);
 lv_obj_send_event(imageRow,LV_EVENT_READY,nullptr);check(Tab5Terminal::scripts==1&&Tab5Terminal::submitted==imageContext.path,"row activation sends shell path unchanged to quoting API");
 selectedOpenEvent(nullptr);check(Tab5Terminal::scripts==2,"Open command launches selected shell script");
 selectedExecuteEvent(nullptr);check(Tab5Terminal::scripts==3,"Execute command launches selected shell script");
 Tab5Terminal::occupied=true;selectedExecuteEvent(nullptr);check(Tab5Terminal::scripts==3,"busy terminal preserves running job");Tab5Terminal::occupied=false;
 clearFilePreview();lv_obj_delete(imageRow);
 for(int width:{720,1216}){
   auto* host=lv_obj_create(root);lv_obj_remove_style_all(host);lv_obj_set_size(host,width,60);createFileToolbar(host);lv_obj_update_layout(root);
   check(lv_obj_get_child_count(lv_obj_get_child(host,0))==7,"six explicit menus plus parent in toolbar");
   check(lv_obj_get_child(lv_obj_get_child(host,0),0)==fileParentButton,"folder parent is leftmost toolbar control");
   lv_area_t outer;lv_obj_get_coords(host,&outer);for(unsigned i=0;i<7;++i){lv_area_t bounds;lv_obj_get_coords(lv_obj_get_child(lv_obj_get_child(host,0),i),&bounds);check(bounds.x1>=outer.x1&&bounds.x2<=outer.x2,"toolbar adapts without floating or overflowing icons");}
   auto* sort=findText(host,"Trier par");lv_obj_send_event(sort,LV_EVENT_CLICKED,nullptr);check(Tab5ContextMenu::scope()!=nullptr,"sort toolbar opens real menu");
   for(unsigned order=0;order<6;++order){showFileMenu(FileMenu::Sort,10,40);auto* option=findText(Tab5ContextMenu::scope(),Tab5FileSort::caption(order));check(option!=nullptr,"all six sort choices visible");lv_obj_send_event(option,LV_EVENT_CLICKED,nullptr);Tab5ContextMenu::service();check(Tab5FileSort::selected==static_cast<Tab5FileSort::Order>(order),"sort selection runs deferred callback");}
   lv_obj_delete(host);
 }
 filesRefreshed=200;
 currentDir="/USB/a'b $(danger); x";selectedPath="/selected";selectedIsDir=true;
 menuCheck(FileMenu::Navigate,"Ouvrir le dossier courant dans le terminal",false,"Navigation does not duplicate terminal action");
 menuCheck(FileMenu::Create,"Ouvrir le dossier courant dans le terminal",false,"File does not duplicate terminal action");
 showFileMenu(FileMenu::Actions,10,40);auto* terminal=findText(Tab5ContextMenu::scope(),"Ouvrir le dossier courant dans le terminal");check(terminal!=nullptr,"current directory terminal action is explicit in Actions");lv_obj_send_event(terminal,LV_EVENT_CLICKED,nullptr);Tab5ContextMenu::service();
 check(Tab5Terminal::submitted=="cd '/USB/a'\\''b $(danger); x'","terminal uses current USB cwd and shell-quotes metacharacters");currentDir="/";clearFilePreview();
 activePage=Page::Audio;showDialog("album");createFolder();check(audioRefreshed==1&&filesRefreshed==200,"audio category refresh only its active page");activePage=Page::Files;
 showDialog("bad");createFolder();check(mkdirCalls==201,"invalid destination refused before mkdir");
 menuCheck(FileMenu::Context,"Projet Git du dossier",false,"empty selection has no Git command");
 menuCheck(FileMenu::Create,"Créer un dossier Git",true,"Git creation available in Create menu");
 menuCheck(FileMenu::Create,"Cloner un dépôt Git",true,"Git clone is explicit in Create menu");
 selectedPath="/plain";selectedIsDir=true;fileSelection.add("/plain");directories.insert("/plain");
 menuCheck(FileMenu::Actions,"Projet Git du dossier",false,"ordinary folder has no Git project command");
 menuCheck(FileMenu::Actions,"Initialiser ce dossier avec Git",true,"ordinary folder can be configured explicitly");
 directories.insert("/plain/.git");files.insert("/plain/.git/HEAD");files.insert("/plain/.git/config");++fsRevision;
 menuCheck(FileMenu::Actions,"Projet Git du dossier",true,"configured selected folder exposes Git");
 menuCheck(FileMenu::Actions,"Initialiser ce dossier avec Git",false,"configured folder hides meaningless init");
 fileSelection.add("/other");menuCheck(FileMenu::Actions,"Projet Git du dossier",false,"multi selection has no ambiguous Git target");
 fileSelection.clear();fileSelection.add("/plain");
 auto* a=lv_list_add_button(fileList,LV_SYMBOL_DIRECTORY,"plain");auto* b=lv_list_add_button(fileList,LV_SYMBOL_FILE,"other");fileSelectionRows={{"/plain",true,a},{"/other",false,b}};
 fileSelection.add("/other");fileMultiSelect=true;
 check(fileContextMenuAt(lv_obj_get_child(a,1),100,100),"right click resolves a row label to its directory");check(fileSelection.paths.size()==2,"right click preserves a multi selection member");Tab5ContextMenu::close();Tab5ContextMenu::service();
 fileSelection.clear();fileSelection.add("/plain");check(fileContextMenuAt(b,100,100),"right click selects a new file");check(selectedPath=="/other"&&fileSelection.paths.size()==1,"context targets actual clicked row");Tab5ContextMenu::close();Tab5ContextMenu::service();
 check(fileContextMenuAt(fileList,100,100),"background context menu");check(fileSelection.paths.empty()&&!selectedPath.length(),"background context clears old target");Tab5ContextMenu::close();Tab5ContextMenu::service();
 for(unsigned i=0;i<200;++i){showFileMenu(FileMenu::Create,1270,710);auto* scope=Tab5ContextMenu::scope();check(scope!=nullptr,"menu repeatedly allocated");Tab5ContextMenu::close();Tab5ContextMenu::service();check(!Tab5ContextMenu::scope(),"menu repeatedly retired");}
 lv_deinit();printf("%s %u Explorer production controllers/menu checks\n",failures?"FAIL":"PASS",checks);return failures?1:0;
}
