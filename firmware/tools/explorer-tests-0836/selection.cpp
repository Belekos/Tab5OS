#include <lvgl.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "Tab5DirectoryModel.h"
#include "Tab5FileSelection.h"
#include "Tab5ImageFormats.h"
#include "Tab5Focus.h"
#include "Tab5UiStyle.h"
struct String:std::string{
 using std::string::string;using std::string::operator=;
 String(const std::string& s):std::string(s){};String()=default;
 String(unsigned long n):std::string(std::to_string(n)){}
 String(unsigned n):std::string(std::to_string(n)){}
 String(int n):std::string(std::to_string(n)){}
 void toCharArray(char* p,size_t n)const{snprintf(p,n,"%s",c_str());}
 bool startsWith(const String& s)const{return compare(0,s.size(),s)==0;}
 int lastIndexOf(char c)const{auto p=rfind(c);return p==npos?-1:int(p);}
 int indexOf(char c,size_t start=0)const{auto p=find(c,start);return p==npos?-1:int(p);}
 String substring(size_t start)const{return substr(start);}
 String substring(size_t start,size_t end)const{return substr(start,end-start);}
};
static unsigned checks=0;static void check(bool value,const char*why){++checks;if(!value){fprintf(stderr,"FAIL %u: %s\n",checks,why);std::abort();}}
#define CHECK(x) check((x),#x)
static uint32_t now=1000;uint32_t millis(){return now;}
namespace writing{static uint8_t modifiers=0;uint8_t usbModifiers(){return modifiers;}}
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
static std::set<void*> contexts;static void*heap_caps_malloc(size_t n,int){void*p=std::malloc(n);if(p)contexts.insert(p);return p;}static void heap_caps_free(void*p){if(p){CHECK(contexts.erase(p)==1);std::free(p);}}
static const char*T(const char*s){return s;}
static constexpr int FILE_LEFT_W=380;static constexpr size_t FILE_PATH_CAPACITY=260,FILE_LIST_PAGE=128;
struct FileEntryCtx{char path[FILE_PATH_CAPACITY];bool isDir=false,right=false;uint16_t depth=0;};
struct FileSelectionRow{std::string path;bool directory;lv_obj_t*object;bool right;uint16_t depth;};
static std::vector<FileSelectionRow>fileSelectionRows;static Tab5FileSelection fileSelection;static Tab5DirectoryModel fileDirectoryModel;
static lv_obj_t*filesPage=nullptr,*fileList=nullptr,*folderPreviewHost=nullptr,*folderPreviewRows=nullptr,*selectedFileBtn=nullptr,*filePreviewMeta=nullptr,*filePathLabel=nullptr,*fileOpenWithBtn=nullptr,*fileMultiSelectBtn=nullptr,*fileSearchClear=nullptr;
static lv_obj_t*dialogOverlay=nullptr,*filePopupOverlay=nullptr,*fileDeleteOverlay=nullptr,*fileOperationOverlay=nullptr,*scope=nullptr;
static bool selectedIsDir=false,selectedIsText=false,selectedIsImage=false,selectedIsAudio=false,selectedIsVideo=false;
static String currentDir="/",fileContentDir="/",fileTreeRoot="/",selectedPath,fileSearchQuery,fileSearchRoot,fileListRevealPath,fileSelectionScope,fileSelectAllRoot;
static String filePendingOpenPath,filePendingOpenParent,fileTreeRevealPath;static uint32_t filePendingOpenAt=0;
static bool fileActiveRight=false,fileMultiSelect=false,fileSelectionLongPress=false,fileDirectoryFailed=false,fileSelectAllSearch=false,sdReady=true;
static size_t fileListOffset=0,fileListShown=0;static uint32_t fileKnownRevision=0;
static std::vector<std::string>fileClipboardPaths;static String clipboardPath;
static struct {bool active=false;}filePasteBatch;
enum class Page{Files,Other};static Page activePage=Page::Files;
static lv_obj_t*keyboardScope(){return scope?scope:filesPage;}
static lv_color_t C_SURFACE(){return lv_color_hex(0xffffff);}static lv_color_t C_SELECTED(){return lv_color_hex(0xd9eadc);}static lv_color_t C_TEXT(){return lv_color_hex(0x222824);}static lv_color_t C_MUTED(){return lv_color_hex(0x69716a);}static lv_color_t C_ACTIVE(){return lv_color_hex(0x427b57);}static const lv_font_t*uiFont(){return &lv_font_montserrat_22;}
static lv_color_t fileRowColor(const String&){return C_TEXT();}
static void setLabelIfChanged(lv_obj_t*p,const char*s){if(p)lv_label_set_text(p,s);}
static String tailPath(const char*s,const lv_font_t*,int){return s;}
static String parentDirOf(const String&p){auto n=p.rfind('/');return n==String::npos||!n?String("/"):String(p.substr(0,n));}
static String baseNameOf(const String&p){return p.substr(p.rfind('/')+1);}
static String joinPath(const String&d,const String&n){return d=="/"?String("/"+n):String(d+"/"+n);}
struct File{bool opened=false,directory=false;explicit operator bool()const{return opened;}bool isDirectory()const{return directory;}void close(){opened=false;}}fileListDir,fileSelectAllDirectory;
static std::map<std::string,bool>filesystem;
struct FS{bool exists(const char*p){return filesystem.count(p);}File open(const char*p,int=0){auto i=filesystem.find(p);return i==filesystem.end()?File{}:File{true,i->second};}}tab5FS;
static bool isExistingDirectory(const String&p){return filesystem.count(p)&&filesystem.at(p);}static bool pathStorageReady(const String&){return true;}static unsigned tab5FsRevision(){return 42;}
namespace Tab5UsbStorage{static bool connected=true;bool mounted(){return connected;}}
namespace Tab5GitView{bool busy(){return false;}lv_obj_t*modalScope(){return nullptr;}lv_obj_t*scope(){return nullptr;}}
namespace Tab5Git{bool busy(){return false;}}
namespace Tab5FileOperation{bool busy(){return false;}}
namespace Tab5Files{bool showHidden=false;}
struct Tools{lv_obj_t*scope(){return nullptr;}void openWith(const char*){}}Tab5Tools;
static bool selectedFolderIsGit(){return false;}static bool isEpubExtension(const String&){return false;}static bool isPythonExtension(const String&p){return Tab5ImageFormats::extension(p.c_str(),".py");}static bool isShellExtension(const String&){return false;}static bool isExecutableExtension(const String&){return false;}static bool isFirmwareExtension(const String&){return false;}
struct Tree{
 struct Row{std::string path,name;bool directory=false;uint16_t depth=0;};
 std::vector<Row>rows;std::set<std::string>expandedPaths;String root="/";bool pending=false,changed=false;unsigned begins=0;
 size_t rowCount()const{return rows.size();}Row row(size_t i)const{return rows.at(i);}bool busy()const{return pending;}bool dirty()const{return changed;}void clearDirty(){changed=false;}bool failed()const{return false;}
 bool expanded(const char*p)const{return expandedPaths.count(p);}bool expanded(const String&p)const{return expanded(p.c_str());}
 bool expand(const char*p){expandedPaths.insert(p);changed=true;return true;}bool expand(const String&p){return expand(p.c_str());}
 bool collapse(const char*p){expandedPaths.erase(p);changed=true;return true;}bool collapse(const String&p){return collapse(p.c_str());}bool toggle(const String&p){return expanded(p)?collapse(p):expand(p);}
 void clear(){rows.clear();expandedPaths.clear();root="";}void cancel(){pending=false;}void refresh(){changed=true;}bool begin(const String&p){++begins;root=p;changed=true;return true;}const char*rootPath()const{return root.c_str();}void service(){pending=false;}
}fileTree;
struct Searcher{bool active=false;bool busy(){return active;}void cancel(){active=false;}void begin(const String&,const String&){active=true;}}fileSearcher;
static void updateFileSearchCaption(){}static void setTopActionVisible(lv_obj_t*,bool){}
static unsigned previews=0,actions=0,navigations=0,fileOpens=0;static String openedFile;
static void updateFileActionState(){++actions;}
static void previewSelectedFile(){++previews;selectedIsDir=filesystem.count(selectedPath)&&filesystem.at(selectedPath);selectedIsImage=Tab5ImageFormats::supported(selectedPath.c_str());selectedIsText=Tab5ImageFormats::extension(selectedPath.c_str(),".txt")||Tab5ImageFormats::extension(selectedPath.c_str(),".py");}
static void freeFilePreviewImage(){}
static void clearFilePreview();static void updateFileSelectionUi();static void startFileListPage(size_t);static void focusRevealedFile();static void openFileContentDirectory(const String&);static void refreshFileList();
static void newFolderEvent(lv_event_t*){}static void renameSelectedEvent(lv_event_t*){}static void toggleSelectedHidden(lv_event_t*){}static void selectAllFiles(){}
static void fileEntryActivateEvent(lv_event_t*);static void fileRequestTreeReveal(const String&);static void serviceFileTreeReveal();
static void selectedOpenEvent(lv_event_t*){if(selectedIsDir)openFileContentDirectory(selectedPath);else{++fileOpens;openedFile=selectedPath;}}
static void setKeyboardVisible(bool){}static void toast(const char*,unsigned=0){}
static void serviceFolderPreview(){}static void cancelFolderPreview(){lv_obj_clean(folderPreviewRows);}
#include "Tab5ExplorerRowStyle.inc"
#include "selection-extracted.inc"
namespace Tab5ContextMenu{constexpr size_t Capacity=32;struct Item{const char*title;const char*shortcut;void(*command)(void*);void*data;bool enabled;};std::vector<Item>last;bool show(lv_obj_t*,int,int,Item*items,size_t count){last.assign(items,items+count);return count>0&&count<=Capacity;}}
static void fileMenuCommand(void*){}
#include "menus-extracted.inc"
static bool menuHas(FileCommand c){return std::any_of(Tab5ContextMenu::last.begin(),Tab5ContextMenu::last.end(),[&](const Tab5ContextMenu::Item&i){return uintptr_t(i.data)==uintptr_t(c)&&i.enabled;});}
static void openFileContentDirectory(const String&directory){const String path=directory;filePendingOpenPath="";if(!isExistingDirectory(path))return;++navigations;fileContentDir=path;lv_obj_clean(folderPreviewRows);for(const auto&[name,dir]:filesystem)if(parentDirOf(name)==path&&name!=path){auto*r=fileCreateContentRow(folderPreviewRows,baseNameOf(name).c_str(),dir,C_TEXT());CHECK(fileBindRow(r,name,dir,true,0));}}
static void clearFilePreview(){fileListRevealPath="";fileSelection.clear();selectedPath="";selectedFileBtn=nullptr;updateFileSelectionUi();}
static lv_display_t*display;static lv_indev_t*pointer;static lv_indev_data_t pointerState={};static std::vector<uint8_t>pixels(1280*720*3);
static void pump(unsigned milliseconds=20){now+=milliseconds;lv_tick_inc(milliseconds);lv_timer_handler();lv_obj_update_layout(filesPage);}
static void readPointer(lv_indev_t*,lv_indev_data_t*data){*data=pointerState;}
static lv_obj_t*row(const char*path,bool right){for(const auto&r:fileSelectionRows)if(r.path==path&&r.right==right)return r.object;return nullptr;}
static void pointerAt(lv_obj_t*object){lv_area_t a;lv_obj_get_coords(object,&a);pointerState.point={a.x1+(a.x2-a.x1)/2,a.y1+(a.y2-a.y1)/2};}
static void click(lv_obj_t*object){CHECK(object);lv_obj_scroll_to_view(object,LV_ANIM_OFF);pump();pointerAt(object);pointerState.state=LV_INDEV_STATE_PRESSED;lv_indev_read(pointer);pump(30);pointerState.state=LV_INDEV_STATE_RELEASED;lv_indev_read(pointer);pump(20);}
static void setup(){lv_init();display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size(),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});filesPage=lv_screen_active();lv_obj_set_style_pad_all(filesPage,0,0);lv_obj_remove_flag(filesPage,LV_OBJ_FLAG_SCROLLABLE);Tab5Focus.begin(filesPage,uiFont());Tab5Focus.theme(C_SELECTED(),C_TEXT(),C_SURFACE());fileList=lv_list_create(filesPage);lv_obj_set_pos(fileList,0,40);lv_obj_set_size(fileList,FILE_LEFT_W,650);folderPreviewHost=lv_obj_create(filesPage);lv_obj_set_pos(folderPreviewHost,400,40);lv_obj_set_size(folderPreviewHost,850,650);lv_obj_set_style_pad_all(folderPreviewHost,0,0);folderPreviewRows=lv_obj_create(folderPreviewHost);lv_obj_set_size(folderPreviewRows,lv_pct(100),lv_pct(100));lv_obj_set_flex_flow(folderPreviewRows,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(folderPreviewRows,0,0);filePreviewMeta=lv_label_create(filesPage);filePathLabel=lv_label_create(filesPage);fileOpenWithBtn=lv_button_create(filesPage);lv_obj_set_pos(fileOpenWithBtn,1100,0);pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,readPointer);lv_indev_set_double_click_time(pointer,LV_INDEV_DEF_DOUBLE_CLICK_TIME);pointerState.state=LV_INDEV_STATE_RELEASED;pump();}
static void reset(){Tab5Focus.clear();lv_obj_clean(fileList);lv_obj_clean(folderPreviewRows);fileSelection.clear();fileSelectionRows.clear();fileTree.clear();fileListRevealPath="";fileTreeRevealPath="";filePendingOpenPath="";selectedPath="";selectedFileBtn=nullptr;fileActiveRight=false;fileMultiSelect=false;fileSelectionLongPress=false;fileContentDir="/";currentDir="/";fileTreeRoot="/";fileSearchQuery="";fileSearchRoot="";fileListOffset=0;writing::modifiers=0;navigations=fileOpens=0;filesystem.clear();filesystem["/"]=true;fileTree.root="/";scope=nullptr;pump(500);}
static void treeEntry(const char*path,bool dir,uint16_t depth=0){filesystem[path]=dir;fileTree.rows.push_back({path,baseNameOf(path),dir,depth});}
int main(){setup();reset();treeEntry("/Images",true);treeEntry("/Documents",true);filesystem["/Images/photo.png"]=false;filesystem["/Images/a.txt"]=false;filesystem["/Images/b.txt"]=false;startFileListPage(0);openFileContentDirectory("/Images");navigations=0;
 // Selecting a file in the independent right pane changes command target only.
 selectFolderPreviewEntry("/Images/photo.png",false);CHECK(currentDir=="/");CHECK(fileContentDir=="/Images");CHECK(fileActiveRight);CHECK(selectedPath=="/Images/photo.png");CHECK(fileSelection.paths==std::vector<std::string>{"/Images/photo.png"});CHECK(selectedIsImage);CHECK(fileGitDirectory()=="/Images");CHECK(navigations==0);
 CHECK(showFileMenu(FileMenu::Context,0,0));for(auto c:{FileCommand::Copy,FileCommand::Cut,FileCommand::Rename,FileCommand::Delete,FileCommand::Info,FileCommand::ViewImage,FileCommand::EditImage,FileCommand::OpenWith})CHECK(menuHas(c));
 CHECK(showFileMenu(FileMenu::ImageOpenWith,0,0));CHECK(Tab5ContextMenu::last.size()==3);CHECK(menuHas(FileCommand::ViewImage));CHECK(menuHas(FileCommand::EditImage));CHECK(menuHas(FileCommand::OpenWithOthers));
 // The last selection owns the context, regardless of pane or old focus.
 fileSelectEntry("/Documents",true,row("/Documents",false),false);Tab5Focus.focusObject(row("/Documents",false));CHECK(!fileActiveRight);CHECK(fileContentDir=="/Images");CHECK(fileGitDirectory()=="/Documents");selectFolderPreviewEntry("/Images/photo.png",false);CHECK(!lv_obj_has_state(row("/Documents",false),LV_STATE_CHECKED));CHECK(lv_color_to_u32(lv_obj_get_style_bg_color(row("/Documents",false),LV_PART_MAIN))!=lv_color_to_u32(C_SELECTED()));
 // Ctrl/Shift remain selection gestures, never open files or folders.
 fileSelectEntry("/Images/a.txt",false,row("/Images/a.txt",true),true);fileSelectEntry("/Images/b.txt",false,row("/Images/b.txt",true),true,true);CHECK(fileSelection.paths.size()==2);fileSelectEntry("/Images/photo.png",false,row("/Images/photo.png",true),true,false,true);CHECK(fileSelection.paths.size()==2);CHECK(fileOpens==0&&navigations==0);
 // Right-click child resolves row identity and the exact contextual path.
 CHECK(fileContextMenuAt(lv_obj_get_child(row("/Images/photo.png",true),1),600,100));CHECK(selectedPath=="/Images/photo.png");CHECK(fileContentDir=="/Images"&&currentDir=="/");CHECK(menuHas(FileCommand::Delete));
 // Repeated independent selections do not retain old contexts or UI fills.
 for(unsigned i=0;i<200;++i){fileSelectEntry("/Documents",true,row("/Documents",false),false);selectFolderPreviewEntry("/Images/photo.png",false);CHECK(fileSelection.paths.size()==1);CHECK(fileContentDir=="/Images"&&currentDir=="/");CHECK(!lv_obj_has_state(row("/Documents",false),LV_STATE_CHECKED));}
 
 // Native pointer dispatch uses the actual LVGL DOUBLE_CLICKED-before-CLICKED
 // sequence; navigation deletes the live event target and its userdata.
 reset();treeEntry("/Folder",true);treeEntry("/Other",true);filesystem["/Folder/child.txt"]=false;
 startFileListPage(0);openFileContentDirectory("/");navigations=0;pump();
 auto*leftFolder=row("/Folder",false);auto*rightFolder=row("/Folder",true);
 click(rightFolder);CHECK(filePendingOpenPath=="/Folder");CHECK(fileContentDir=="/");CHECK(fileActiveRight);CHECK(Tab5Focus.focused()==rightFolder);
 pump(LV_INDEV_DEF_DOUBLE_CLICK_TIME+1);serviceFilePendingOpen();CHECK(fileContentDir=="/Folder");CHECK(navigations==1);CHECK(currentDir=="/");CHECK(!lv_obj_has_state(leftFolder,LV_STATE_CHECKED));CHECK(filePendingOpenPath.empty());
 // Plain left selection does not navigate; chevron only changes expansion.
 click(leftFolder);CHECK(fileContentDir=="/Folder");CHECK(!fileActiveRight);CHECK(navigations==1);
 auto*chevron=lv_obj_get_child(leftFolder,2);click(chevron);CHECK(fileTree.expanded("/Folder"));CHECK(selectedPath=="/Folder");CHECK(!fileActiveRight);CHECK(navigations==1);
 // Right double click opens once then selects/focuses corresponding tree row.
 openFileContentDirectory("/");navigations=0;pump(600);rightFolder=row("/Folder",true);click(rightFolder);click(rightFolder);CHECK(fileContentDir=="/Folder");CHECK(navigations==1);CHECK(selectedPath=="/Folder");CHECK(!fileActiveRight);CHECK(Tab5Focus.focused()==row("/Folder",false));CHECK(filePendingOpenPath.empty());CHECK(fileTreeRevealPath.empty());
 pump(600);serviceFilePendingOpen();CHECK(navigations==1);
 // Ctrl, Shift, and touch multi-selection suppress opening.
 openFileContentDirectory("/");navigations=0;pump(600);writing::modifiers=0x01;click(row("/Folder",true));pump(600);serviceFilePendingOpen();CHECK(navigations==0);CHECK(fileContentDir=="/");writing::modifiers=0x02;click(row("/Other",true));pump(600);serviceFilePendingOpen();CHECK(navigations==0);CHECK(fileSelection.paths.size()==2);writing::modifiers=0;
 fileMultiSelect=true;click(row("/Folder",true));pump(600);serviceFilePendingOpen();CHECK(navigations==0);fileMultiSelect=false;
 // Selecting elsewhere or opening a menu cancels an already queued click.
 click(row("/Folder",true));CHECK(filePendingOpenPath=="/Folder");fileSelectEntry("/Other",true,row("/Other",false),false);pump(600);serviceFilePendingOpen();CHECK(navigations==0);CHECK(selectedPath=="/Other");
 click(row("/Folder",true));CHECK(filePendingOpenPath=="/Folder");showFileMenu(FileMenu::Context,500,100);pump(600);serviceFilePendingOpen();CHECK(navigations==0);CHECK(filePendingOpenPath.empty());
 click(row("/Folder",true));scope=folderPreviewHost;pump(600);serviceFilePendingOpen();CHECK(navigations==0);CHECK(filePendingOpenPath.empty());scope=nullptr;
 
 // A held second press cannot be deleted while the OS is servicing navigation.
 pump(600);rightFolder=row("/Folder",true);click(rightFolder);pump(LV_INDEV_DEF_DOUBLE_CLICK_TIME-80);pointerAt(rightFolder);pointerState.state=LV_INDEV_STATE_PRESSED;lv_indev_read(pointer);pump(100);serviceFilePendingOpen();CHECK(navigations==0);CHECK(lv_obj_is_valid(rightFolder));pointerState.state=LV_INDEV_STATE_RELEASED;lv_indev_read(pointer);pump();pump(600);serviceFilePendingOpen();CHECK(navigations==1);CHECK(fileContentDir=="/Folder");
 
 // File single-click remains a selection; double-click opens the application.
 pump(600);auto*child=row("/Folder/child.txt",true);click(child);CHECK(fileOpens==0);click(child);CHECK(fileOpens==1);CHECK(openedFile=="/Folder/child.txt");
 // Explicit reveal walks ancestors as their incremental scans become ready.
 reset();treeEntry("/A",true);filesystem["/A/B"]=true;filesystem["/A/B/C"]=true;fileContentDir="/A/B/C";startFileListPage(0);fileSelectEntry("/A/B/C",true,nullptr,true);fileRequestTreeReveal("/A/B/C");CHECK(fileTree.expanded("/A"));CHECK(fileTreeRevealPath=="/A/B/C");CHECK(fileContentDir=="/A/B/C");
 fileTree.pending=true;serviceFileTreeReveal();CHECK(fileTreeRevealPath=="/A/B/C");CHECK(!fileTree.expanded("/A/B"));fileTree.pending=false;treeEntry("/A/B",true,1);serviceFileTreeReveal();CHECK(fileTree.expanded("/A/B"));treeEntry("/A/B/C",true,2);startFileListPage(0);serviceFileTreeReveal();CHECK(fileTreeRevealPath.empty());CHECK(selectedPath=="/A/B/C");CHECK(!fileActiveRight);CHECK(Tab5Focus.focused()==row("/A/B/C",false));CHECK(fileContentDir=="/A/B/C");
 // Escape cancels a pending reveal/open without altering right content.
 fileSelectEntry("/A",true,row("/A",false),false);filePendingOpenPath="/A/B";fileTreeRevealPath="/A/B";writing::KeyEvent escape{};escape.key=writing::Key::Escape;CHECK(fileSelectionKey(escape));CHECK(filePendingOpenPath.empty()&&fileTreeRevealPath.empty());CHECK(fileContentDir=="/A/B/C");
 // Left/right arrow only expands/collapses the tree; Enter activation opens.
 Tab5Focus.focusObject(row("/A/B",false));writing::KeyEvent arrow{};arrow.key=writing::Key::Left;CHECK(fileSelectionKey(arrow));CHECK(!fileTree.expanded("/A/B"));CHECK(fileContentDir=="/A/B/C");arrow.key=writing::Key::Right;CHECK(fileSelectionKey(arrow));CHECK(fileTree.expanded("/A/B"));
 // Distant rows switch the left page only, and preserve the right list/context.
 reset();for(unsigned i=0;i<400;++i){char path[32];snprintf(path,sizeof(path),"/D%03u",i);treeEntry(path,true);}fileContentDir="/D260";startFileListPage(0);fileSelectEntry("/D260",true,nullptr,true);fileRequestTreeReveal("/D260");CHECK(fileListOffset==256);CHECK(Tab5Focus.focused()==row("/D260",false));CHECK(selectedPath=="/D260");CHECK(fileContentDir=="/D260");CHECK(fileTreeRevealPath.empty());
 // Reveals defer behind modal scopes and cannot override a later selection.
 fileSelectEntry("/D001",true,nullptr,true);scope=folderPreviewHost;fileRequestTreeReveal("/D001");CHECK(fileTreeRevealPath=="/D001");CHECK(fileListOffset==256);fileSelectEntry("/D261",true,row("/D261",false),false);scope=nullptr;serviceFileTreeReveal();CHECK(fileTreeRevealPath.empty());CHECK(selectedPath=="/D261");CHECK(fileListOffset==256);
 // A removed volume empties the tree identity, and the same root is rebuilt
 // on remount even if its path string did not otherwise change.
 reset();treeEntry("/Images",true);startFileListPage(0);openFileContentDirectory("/");fileSelectEntry("/Images",true,row("/Images",false),false);sdReady=false;Tab5UsbStorage::connected=false;refreshFileList();CHECK(fileTreeRoot.empty());CHECK(!fileTree.rootPath()[0]);CHECK(selectedPath.empty());CHECK(fileSelectionRows.empty());CHECK(contexts.empty());unsigned beforeBegins=fileTree.begins;sdReady=true;refreshFileList();CHECK(fileTree.begins==beforeBegins+1);CHECK(fileTreeRoot=="/");CHECK(std::string(fileTree.rootPath())=="/");CHECK(currentDir=="/");CHECK(fileContentDir=="/");
 // Explicit background reclamation can clear only the model; rootPath remains
 // authoritative so the next refresh still starts a new scan.
 fileTree.clear();fileTreeRoot="/";beforeBegins=fileTree.begins;refreshFileList();CHECK(fileTree.begins==beforeBegins+1);CHECK(std::string(fileTree.rootPath())=="/");Tab5UsbStorage::connected=true;
 Tab5Focus.clear();lv_obj_clean(filesPage);CHECK(contexts.empty());lv_indev_delete(pointer);lv_deinit();printf("{\"checks\":%u,\"repeatedSelections\":200,\"passed\":true,\"lvgl\":\"9.6.0\"}\n",checks);return 0;
}
