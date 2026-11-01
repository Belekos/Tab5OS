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
 bool reserve(size_t n){std::string::reserve(n);return true;}
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
#include "config-extracted.inc"
static std::vector<FileSelectionRow>fileSelectionRows;static Tab5FileSelection fileSelection;static Tab5DirectoryModel fileDirectoryModel;
static lv_obj_t*filesPage=nullptr,*fileList=nullptr,*folderPreviewHost=nullptr,*folderPreviewRows=nullptr,*selectedFileBtn=nullptr,*filePreviewMeta=nullptr,*filePathLabel=nullptr,*fileOpenWithBtn=nullptr,*fileMultiSelectBtn=nullptr,*fileSearchClear=nullptr;
static lv_obj_t*dialogOverlay=nullptr,*filePopupOverlay=nullptr,*fileDeleteOverlay=nullptr,*fileOperationOverlay=nullptr,*scope=nullptr;
static bool selectedIsDir=false,selectedIsText=false,selectedIsImage=false,selectedIsAudio=false,selectedIsVideo=false;
static String currentDir="/",fileContentDir="/",fileTreeRoot="/",selectedPath,fileSearchQuery,fileSearchRoot,fileListRevealPath,fileSelectionScope,fileSelectAllRoot;
static String fileTreeRevealPath;
static FileContentMode fileContentMode=FileContentMode::Empty;
static String fileDisplayedPath,fileMetadataPath,filePreviewImagePath;
static uint32_t fileDisplayedRevision=0,fileMetadataRevision=0;
static size_t fileSelectedBytes=0;static bool fileTreeRequested=false;
static FileClickGesture fileClickGesture;
static void showFileSelectionContent(bool force=false);static void restoreFileContent();static void clearFileContent();
static lv_obj_t*filePreviewTitle,*filePreviewLabel,*filePreviewScroll,*filePreviewImage,*folderPreviewPageLabel,*folderPreviewPrevious,*folderPreviewNext;
static unsigned imagePreviews=0;
static void showFilePreviewImage(){++imagePreviews;filePreviewImagePath=selectedPath;lv_obj_add_flag(filePreviewLabel,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(filePreviewImage,LV_OBJ_FLAG_HIDDEN);}
static bool fileActiveRight=false,fileMultiSelect=false,fileSelectionLongPress=false,fileDirectoryFailed=false,fileSelectAllSearch=false,sdReady=true;
static size_t fileListOffset=0,fileListShown=0;static uint32_t fileKnownRevision=0;
static std::vector<std::string>fileClipboardPaths;static String clipboardPath;
static struct {bool active=false;}filePasteBatch;
enum class Page{Files,Editor,Other};static Page activePage=Page::Files;
static lv_obj_t*keyboardScope(){return fileDeleteOverlay?fileDeleteOverlay:scope?scope:filesPage;}
static lv_color_t C_SURFACE(){return lv_color_hex(0xffffff);}static lv_color_t C_SELECTED(){return lv_color_hex(0xd9eadc);}static lv_color_t C_TEXT(){return lv_color_hex(0x222824);}static lv_color_t C_MUTED(){return lv_color_hex(0x69716a);}static lv_color_t C_ACTIVE(){return lv_color_hex(0x427b57);}static const lv_font_t*uiFont(){return &lv_font_montserrat_22;}
static lv_color_t fileRowColor(const String&){return C_TEXT();}
static void setLabelIfChanged(lv_obj_t*p,const char*s){if(p&&strcmp(lv_label_get_text(p),s)!=0)lv_label_set_text(p,s);}
static String tailPath(const char*s,const lv_font_t*,int){return s;}
static String parentDirOf(const String&p){auto n=p.rfind('/');return n==String::npos||!n?String("/"):String(p.substr(0,n));}
static String baseNameOf(const String&p){return p.substr(p.rfind('/')+1);}
static String joinPath(const String&d,const String&n){return d=="/"?String("/"+n):String(d+"/"+n);}
static std::map<std::string,bool>filesystem;
static std::map<std::string,std::string>fileBytes;
static unsigned fsOpens=0,bytesRead=0;
constexpr int FILE_READ=0;
struct File{
 bool opened=false,directory=false;String path;size_t position=0;
 explicit operator bool()const{return opened;}bool isDirectory()const{return directory;}
 void close(){opened=false;}size_t size()const{return fileBytes[path].size();}
 time_t getLastWrite()const{return 1700000000;}
 int available(){return opened&&position<size();}
 size_t read(uint8_t*out,size_t n){n=std::min(n,size()-position);memcpy(out,fileBytes[path].data()+position,n);position+=n;bytesRead+=n;return n;}
 int read(){if(!available())return -1;++bytesRead;return uint8_t(fileBytes[path][position++]);}
 bool seek(size_t n){position=std::min(n,size());return true;}
 String name()const{return path;}
}fileListDir,fileSelectAllDirectory;
struct FS{bool exists(const char*p){return filesystem.count(p);}File open(const char*p,int=0){++fsOpens;auto i=filesystem.find(p);return i==filesystem.end()?File{}:File{true,i->second,p,0};}}tab5FS;
static bool isExistingDirectory(const String&p){return filesystem.count(p)&&filesystem.at(p);}static bool pathStorageReady(const String&){return true;}static unsigned tab5FsRevision(){return 42;}
namespace Tab5UsbStorage{static bool connected=true;bool mounted(){return connected;}}
namespace Tab5GitView{bool busy(){return false;}lv_obj_t*modalScope(){return nullptr;}lv_obj_t*scope(){return nullptr;}}
namespace Tab5Git{bool busy(){return false;}}
namespace Tab5FileOperation{bool busy(){return false;}}
namespace Tab5Files{bool showHidden=false;bool userWritable(const char*p){return p&&*p&&std::string(p)!="/"&&std::string(p)!="/USB";}}
struct Tools{lv_obj_t*scope(){return nullptr;}void openWith(const char*){}}Tab5Tools;
static bool selectedFolderIsGit(){return false;}static bool isEpubExtension(const String&){return false;}static bool isPythonExtension(const String&p){return Tab5ImageFormats::extension(p.c_str(),".py");}static bool isShellExtension(const String&){return false;}static bool isExecutableExtension(const String&){return false;}static bool isFirmwareExtension(const String&){return false;}
struct Tree{
 struct Row{std::string path,name;bool directory=false;uint16_t depth=0;};
 std::vector<Row>rows;std::set<std::string>expandedPaths;String root="/";bool pending=false,changed=false,asyncRefresh=false;unsigned begins=0;
 bool rowIndex(const String&p,size_t&i)const{for(i=0;i<rows.size();++i)if(rows[i].path==p)return true;return false;}
 size_t rowCount()const{return rows.size();}Row row(size_t i)const{return rows.at(i);}bool busy()const{return pending;}bool dirty()const{return changed;}void clearDirty(){changed=false;}bool failed()const{return false;}
 bool expanded(const char*p)const{return expandedPaths.count(p);}bool expanded(const String&p)const{return expanded(p.c_str());}
 bool expand(const char*p){expandedPaths.insert(p);changed=true;return true;}bool expand(const String&p){return expand(p.c_str());}
 bool collapse(const char*p){expandedPaths.erase(p);changed=true;return true;}bool collapse(const String&p){return collapse(p.c_str());}bool toggle(const String&p){return expanded(p)?collapse(p):expand(p);}
 void clear(){rows.clear();expandedPaths.clear();root="";}void cancel(){pending=false;}void refresh(){changed=true;pending=asyncRefresh;}bool begin(const String&p){++begins;root=p;changed=true;return true;}const char*rootPath()const{return root.c_str();}void service(){pending=false;}
}fileTree;
struct Searcher{bool active=false;bool busy(){return active;}void cancel(){active=false;}void begin(const String&,const String&){active=true;}}fileSearcher;
static void updateFileSearchCaption(){}static void setTopActionVisible(lv_obj_t*,bool){}
static unsigned actions=0,fileOpens=0;static String openedFile;
static void updateFileActionState(){++actions;}
static void previewSelectedFile();
static String formatFileTime(time_t){return "2023-11-14";}
static bool isVideoExtension(const String&p){return Tab5ImageFormats::extension(p.c_str(),".mp4");}
static bool isAudioExtension(const String&p){return Tab5ImageFormats::extension(p.c_str(),".mp3");}
static bool isReadableImageFile(const String&p){return Tab5ImageFormats::supported(p.c_str());}
static bool isImageProject(const String&p){return Tab5ImageFormats::extension(p.c_str(),".t5i");}
static bool isEditableTextExtension(const String&p){return Tab5ImageFormats::extension(p.c_str(),".txt")||Tab5ImageFormats::extension(p.c_str(),".py");}
static bool looksLikeTextFile(File&f){return fileBytes[f.path].find(char(0))==std::string::npos;}
static String fileInfoText;
static void freeFilePreviewImage(){filePreviewImagePath="";if(filePreviewImage)lv_obj_add_flag(filePreviewImage,LV_OBJ_FLAG_HIDDEN);}
static void clearFilePreview();static void updateFileSelectionUi();static void startFileListPage(size_t);static void focusRevealedFile();static void openFileContentDirectory(const String&);static void refreshFileList();
static void newFolderEvent(lv_event_t*){}static void renameSelectedEvent(lv_event_t*){}static void toggleSelectedHidden(lv_event_t*){}static void selectAllFiles(){}
static void fileEntryActivateEvent(lv_event_t*);static void fileRequestTreeReveal(const String&);static void serviceFileTreeReveal();
static void selectedOpenEvent(lv_event_t*){if(selectedIsDir)openFileContentDirectory(selectedPath);else{++fileOpens;openedFile=selectedPath;}}
static void setKeyboardVisible(bool){}static void toast(const char*,unsigned=0){}
static lv_obj_t*root=nullptr;static constexpr int SCREEN_W=1280,SCREEN_H=720;
static String currentPath,pendingDeletePath;static std::vector<std::string>pendingDeletePaths;
static void warnSystemFile(const String&){}
static lv_color_t C_BORDER(){return C_MUTED();}
static void confirmDeleteYes(lv_event_t*){CHECK(false);}
static lv_obj_t*makeButton(lv_obj_t*parent,const char*text,int w,int h){auto*b=lv_button_create(parent);lv_obj_set_size(b,w,h);auto*l=lv_label_create(b);lv_label_set_text(l,text);lv_obj_center(l);return b;}
struct FolderPreview{
 struct Entry{String path,name;bool directory;};
 std::vector<Entry>entries;String path;size_t count=0,offset=0;bool done=true,failed=false,dirty=false;unsigned begins=0;
 void cancel(){path="";entries.clear();dirty=false;done=true;}
 bool begin(const String&p,size_t first=0){++begins;path=p;offset=first;entries.clear();for(const auto&[name,dir]:filesystem)if(parentDirOf(name)==path&&name!=path)entries.push_back({name,baseNameOf(name),dir});count=entries.size();done=true;failed=false;dirty=true;return true;}
 void service(){}
}folderPreview;
static void serviceFolderPreview();static void cancelFolderPreview();
#define fileStyleRow productionFileStyleRow
#include "Tab5ExplorerRowStyle.inc"
#undef fileStyleRow
static unsigned rowStyleCalls=0;
static void fileStyleRow(lv_obj_t*row,bool selected,lv_color_t color){++rowStyleCalls;productionFileStyleRow(row,selected,color);}
#include "selection-extracted.inc"
namespace Tab5ContextMenu{constexpr size_t Capacity=32;struct Item{const char*title;const char*shortcut;void(*command)(void*);void*data;bool enabled;};std::vector<Item>last;bool show(lv_obj_t*,int,int,Item*items,size_t count){last.assign(items,items+count);return count>0&&count<=Capacity;}}
static void fileMenuCommand(void*);
#include "menus-extracted.inc"
static bool menuHas(FileCommand c){return std::any_of(Tab5ContextMenu::last.begin(),Tab5ContextMenu::last.end(),[&](const Tab5ContextMenu::Item&i){return uintptr_t(i.data)==uintptr_t(c)&&i.enabled;});}
static lv_display_t*display;static lv_indev_t*pointer;static lv_indev_data_t pointerState={};static unsigned flushes=0;static size_t flushedPixels=0;static std::vector<uint8_t>pixels(1280*720*3);
static void pump(unsigned milliseconds=20){now+=milliseconds;lv_tick_inc(milliseconds);lv_timer_handler();lv_obj_update_layout(filesPage);}
static void readPointer(lv_indev_t*,lv_indev_data_t*data){*data=pointerState;}
static lv_obj_t*row(const char*path,bool right){for(const auto&r:fileSelectionRows)if(r.path==path&&r.right==right)return r.object;return nullptr;}
static void pointerAt(lv_obj_t*object){lv_area_t a;lv_obj_get_coords(object,&a);pointerState.point={a.x1+(a.x2-a.x1)/2,a.y1+(a.y2-a.y1)/2};}
static void click(lv_obj_t*object){CHECK(object);lv_obj_scroll_to_view(object,LV_ANIM_OFF);pump();pointerAt(object);pointerState.state=LV_INDEV_STATE_PRESSED;lv_indev_read(pointer);pump(30);pointerState.state=LV_INDEV_STATE_RELEASED;lv_indev_read(pointer);pump(20);}
static void setup(){lv_init();display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size(),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*a,uint8_t*){++flushes;flushedPixels+=size_t(a->x2-a->x1+1)*(a->y2-a->y1+1);lv_display_flush_ready(d);});root=filesPage=lv_screen_active();lv_obj_set_style_pad_all(filesPage,0,0);lv_obj_remove_flag(filesPage,LV_OBJ_FLAG_SCROLLABLE);Tab5Focus.begin(filesPage,uiFont());Tab5Focus.theme(C_SELECTED(),C_TEXT(),C_SURFACE());fileList=lv_list_create(filesPage);lv_obj_set_pos(fileList,0,40);lv_obj_set_size(fileList,FILE_LEFT_W,650);folderPreviewHost=lv_obj_create(filesPage);lv_obj_set_pos(folderPreviewHost,400,40);lv_obj_set_size(folderPreviewHost,850,650);lv_obj_set_style_pad_all(folderPreviewHost,0,0);folderPreviewRows=lv_obj_create(folderPreviewHost);lv_obj_set_size(folderPreviewRows,lv_pct(100),lv_pct(100));lv_obj_set_flex_flow(folderPreviewRows,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(folderPreviewRows,0,0);filePreviewScroll=lv_obj_create(filesPage);lv_obj_set_pos(filePreviewScroll,400,40);lv_obj_set_size(filePreviewScroll,850,650);filePreviewLabel=lv_label_create(filePreviewScroll);lv_obj_set_width(filePreviewLabel,800);filePreviewImage=lv_obj_create(filePreviewScroll);lv_obj_set_size(filePreviewImage,80,80);lv_obj_remove_flag(filePreviewImage,LV_OBJ_FLAG_CLICKABLE);filePreviewTitle=lv_label_create(filesPage);folderPreviewPageLabel=lv_label_create(filesPage);filePreviewMeta=lv_label_create(filesPage);filePathLabel=lv_label_create(filesPage);fileOpenWithBtn=lv_button_create(filesPage);lv_obj_set_pos(fileOpenWithBtn,1100,0);pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,readPointer);lv_indev_add_event_cb(pointer,filePointerEvent,LV_EVENT_SHORT_CLICKED,nullptr);lv_indev_set_double_click_time(pointer,LV_INDEV_DEF_DOUBLE_CLICK_TIME);pointerState.state=LV_INDEV_STATE_RELEASED;pump();}
static void oldReset(){Tab5Focus.clear();lv_obj_clean(fileList);lv_obj_clean(folderPreviewRows);fileSelection.clear();fileSelectionRows.clear();fileTree.clear();fileListRevealPath="";fileTreeRevealPath="";fileClickGesture.valid=false;selectedPath="";selectedFileBtn=nullptr;fileActiveRight=false;fileMultiSelect=false;fileSelectionLongPress=false;fileContentDir="/";currentDir="/";fileTreeRoot="/";fileSearchQuery="";fileSearchRoot="";fileListOffset=0;writing::modifiers=0;fileOpens=0;filesystem.clear();filesystem["/"]=true;fileTree.root="/";scope=nullptr;pump(500);}
static void reset(){
 lv_indev_reset(pointer,nullptr);pointerState.state=LV_INDEV_STATE_RELEASED;fileTreeRequested=false;
 oldReset();clearFileContent();fileTree.root="";fileTreeRoot="";fileMetadataPath="";fileMetadataRevision=0;
 fileBytes.clear();currentPath="";pendingDeletePath="";pendingDeletePaths.clear();fsOpens=bytesRead=0;imagePreviews=0;folderPreview.begins=0;fileTree.begins=0;fileTree.pending=false;fileTree.asyncRefresh=false;
 sdReady=true;Tab5UsbStorage::connected=true;activePage=Page::Files;fileDirectoryFailed=false;pump(500);
}
static void clickPoint(lv_point_t point){pointerState.point=point;pointerState.state=LV_INDEV_STATE_PRESSED;lv_indev_read(pointer);pump(30);pointerState.state=LV_INDEV_STATE_RELEASED;lv_indev_read(pointer);pump(20);}
static lv_point_t center(lv_obj_t*object){CHECK(object);lv_obj_scroll_to_view(object,LV_ANIM_OFF);pump();pointerAt(object);return pointerState.point;}
static void treeEntry(const char*path,bool dir,uint16_t depth=0){filesystem[path]=dir;fileTree.rows.push_back({path,baseNameOf(path),dir,depth});}
static void browseTree(){fileMenuCommand(reinterpret_cast<void*>(uintptr_t(FileCommand::Sd)));pump();}
static void assertDisplayed(const char*path,FileContentMode mode){CHECK(fileDisplayedPath==path);CHECK(fileContentMode==mode);CHECK(std::string(lv_label_get_text(filePreviewTitle))==path);}
int main(){
 setup();reset();
 // Startup and idle services must leave both panes empty without touching storage.
 refreshFileList();serviceFileList();serviceFolderPreview();
 CHECK(!fileTreeRequested);CHECK(fileTree.begins==0);CHECK(folderPreview.begins==0);CHECK(fsOpens==0);
 CHECK(fileSelectionRows.empty());CHECK(lv_obj_get_child_count(fileList)==0);assertDisplayed("",FileContentMode::Empty);
 CHECK(lv_obj_has_flag(folderPreviewHost,LV_OBJ_FLAG_HIDDEN));CHECK(lv_obj_has_flag(filePreviewScroll,LV_OBJ_FLAG_HIDDEN));
 treeEntry("/Documents",true);treeEntry("/Images",true);filesystem["/Documents/a.txt"]=false;filesystem["/Images/photo.png"]=false;
 fileBytes["/Documents/a.txt"]="Un aperçu réel, avec contenu UTF-8.\nDeuxième ligne.";
 // Explicit tree navigation opens only left; first plain left click opens right immediately.
 browseTree();CHECK(fileTree.begins==1);CHECK(row("/Documents",false));assertDisplayed("",FileContentMode::Empty);
 click(row("/Documents",false));assertDisplayed("/Documents",FileContentMode::Directory);CHECK(row("/Documents/a.txt",true));CHECK(fileTreeRoot=="/");CHECK(fileOpens==0);
 // Left and right files render real production text, not metadata alone.
 click(row("/Documents/a.txt",true));assertDisplayed("/Documents/a.txt",FileContentMode::Preview);
 CHECK(std::string(lv_label_get_text(filePreviewLabel))==fileBytes["/Documents/a.txt"]);CHECK(fileContentDir=="/Documents");CHECK(fileOpens==0);
 fileParentNavigation();assertDisplayed("/Documents",FileContentMode::Directory);CHECK(row("/Documents/a.txt",true));CHECK(selectedPath.empty());
 treeEntry("/Documents/a.txt",false,1);startFileListPage(0);pump(600);
 click(row("/Documents/a.txt",false));assertDisplayed("/Documents/a.txt",FileContentMode::Preview);CHECK(fileOpens==0);
 fileParentNavigation();assertDisplayed("/Documents",FileContentMode::Directory);CHECK(fileActiveRight);CHECK(fileContextDirectory()=="/Documents");CHECK(fileGitDirectory()=="/Documents");
 click(row("/Images",false));assertDisplayed("/Images",FileContentMode::Directory);
 click(row("/Images/photo.png",true));assertDisplayed("/Images/photo.png",FileContentMode::Preview);CHECK(filePreviewImagePath=="/Images/photo.png");CHECK(imagePreviews==1);CHECK(fileOpens==0);
 // A repeat selection/preview of the same path must use cached metadata/content.
 const auto cachedOpens=fsOpens;fileSelectEntry("/Images/photo.png",false,nullptr,true);showFileSelectionContent();CHECK(fsOpens==cachedOpens);CHECK(imagePreviews==1);
 CHECK(showFileMenu(FileMenu::ImageOpenWith,400,200));CHECK(menuHas(FileCommand::ViewImage));CHECK(menuHas(FileCommand::EditImage));CHECK(menuHas(FileCommand::OpenWithOthers));

 // Root parents are captions/containers, not rows. Both SD and USB reveal must settle.
 for(bool usb:{false,true}){
   reset();const String base=usb?"/USB":"/";const String path=usb?"/USB/root.txt":"/root.txt";
   filesystem[base]=true;treeEntry(path.c_str(),false);fileBytes[path]="root file";
   fileMenuCommand(reinterpret_cast<void*>(uintptr_t(usb?FileCommand::Usb:FileCommand::Sd)));openFileContentDirectory(base);pump(600);
   const lv_point_t point=center(row(path.c_str(),true));clickPoint(point);assertDisplayed(path.c_str(),FileContentMode::Preview);clickPoint(point);
   CHECK(currentDir==base);CHECK(fileTreeRoot==base);CHECK(Tab5Focus.focused()==fileList);CHECK(fileTreeRevealPath.empty());CHECK(selectedPath==path);CHECK(!selectedIsDir);CHECK(fileOpens==0);assertDisplayed(path.c_str(),FileContentMode::Preview);
 }
 // Real LVGL double click at old coordinates after first release destroys its row.
 reset();treeEntry("/Folder",true);treeEntry("/Other",true);filesystem["/Folder/child.txt"]=false;fileBytes["/Folder/child.txt"]="child content";
 browseTree();openFileContentDirectory("/");pump(600);
 const lv_point_t folderPoint=center(row("/Folder",true));const auto beforeScans=folderPreview.begins;
 clickPoint(folderPoint);assertDisplayed("/Folder",FileContentMode::Directory);CHECK(row("/Folder",true)==nullptr);CHECK(row("/Folder/child.txt",true));CHECK(folderPreview.begins==beforeScans+1);
 clickPoint(folderPoint);assertDisplayed("/Folder",FileContentMode::Directory);CHECK(selectedPath=="/Folder");CHECK(Tab5Focus.focused()==row("/Folder",false));CHECK(fileTreeRevealPath.empty());CHECK(fileOpens==0);CHECK(folderPreview.begins==beforeScans+1);
 pump(600);serviceFileList();CHECK(folderPreview.begins==beforeScans+1);
 // File double click focuses its parent while keeping commands bound to the file.
 const lv_point_t filePoint=center(row("/Folder/child.txt",true));clickPoint(filePoint);assertDisplayed("/Folder/child.txt",FileContentMode::Preview);
 CHECK(std::string(lv_label_get_text(filePreviewLabel))=="child content");clickPoint(filePoint);
 assertDisplayed("/Folder/child.txt",FileContentMode::Preview);CHECK(Tab5Focus.focused()==row("/Folder",false));CHECK(selectedPath=="/Folder/child.txt");CHECK(!selectedIsDir);CHECK(fileSelection.paths==std::vector<std::string>{"/Folder/child.txt"});CHECK(fileActiveRight);CHECK(fileOpens==0);
 CHECK(showFileMenu(FileMenu::Context,400,200));CHECK(menuHas(FileCommand::Delete));CHECK(menuHas(FileCommand::Rename));CHECK(fileGitDirectory()=="/Folder");
 // Enter remains explicit application launch using real focus activation.
 treeEntry("/Folder/child.txt",false,1);startFileListPage(0);Tab5Focus.focusObject(row("/Folder/child.txt",false));writing::KeyEvent enter{};enter.key=writing::Key::Enter;CHECK(Tab5Focus.key(enter,filesPage));CHECK(fileOpens==1);CHECK(openedFile=="/Folder/child.txt");
 // Parent from any preview returns its containing folder, regardless of focused pane.
 fileParentNavigation();assertDisplayed("/Folder",FileContentMode::Directory);
 // Chevron changes only expansion, without changing selection or content.
 fileSelectEntry("/Other",true,row("/Other",false),false);const String contextBeforeChevron=selectedPath;pump(600);
 click(lv_obj_get_child(row("/Folder",false),2));CHECK(fileTree.expanded("/Folder"));CHECK(selectedPath==contextBeforeChevron);assertDisplayed("/Folder",FileContentMode::Directory);

 // Ctrl, Shift, touch-multi and contextual selection leave displayed content intact.
 reset();treeEntry("/A",true);treeEntry("/B",true);treeEntry("/C",true);browseTree();openFileContentDirectory("/");pump(600);
 writing::modifiers=0x01;click(row("/A",true));assertDisplayed("/",FileContentMode::Directory);CHECK(fileSelection.paths.size()==1);
 writing::modifiers=0x02;click(row("/C",true));CHECK(fileSelection.paths.size()==3);assertDisplayed("/",FileContentMode::Directory);writing::modifiers=0;
 fileMultiSelect=true;click(row("/B",true));CHECK(fileSelection.paths.size()==2);assertDisplayed("/",FileContentMode::Directory);fileMultiSelect=false;
 CHECK(fileContextMenuAt(lv_obj_get_child(row("/B",true),1),500,200));CHECK(selectedPath=="/B");assertDisplayed("/",FileContentMode::Directory);
 // A genuine held pointer creates touch selection; release cannot browse.
 pump(600);auto*held=row("/A",true);pointerAt(held);pointerState.state=LV_INDEV_STATE_PRESSED;lv_indev_read(pointer);pump(700);lv_indev_read(pointer);pointerState.state=LV_INDEV_STATE_RELEASED;lv_indev_read(pointer);pump();
 CHECK(fileMultiSelect);assertDisplayed("/",FileContentMode::Directory);CHECK(fileOpens==0);
 // Subsequent unrelated click cannot inherit an earlier double-click gesture.
 fileMultiSelect=false;pump(600);click(row("/B",true));assertDisplayed("/B",FileContentMode::Directory);CHECK(selectedPath=="/B");

 // Tree reveal is asynchronous, paginated, and never changes preview or action target.
 reset();treeEntry("/A",true);filesystem["/A/B"]=true;filesystem["/A/B/C"]=true;filesystem["/A/B/C/file.txt"]=false;fileBytes["/A/B/C/file.txt"]="deep";browseTree();
 fileSelectEntry("/A/B/C/file.txt",false,nullptr,true);showFileSelectionContent();fileRequestTreeReveal("/A/B/C");CHECK(fileTree.expanded("/A"));CHECK(fileTreeRevealPath=="/A/B/C");
 fileTree.pending=true;serviceFileTreeReveal();CHECK(!fileTree.expanded("/A/B"));fileTree.pending=false;treeEntry("/A/B",true,1);serviceFileTreeReveal();CHECK(fileTree.expanded("/A/B"));treeEntry("/A/B/C",true,2);startFileListPage(0);serviceFileTreeReveal();
 CHECK(fileTreeRevealPath.empty());CHECK(Tab5Focus.focused()==row("/A/B/C",false));CHECK(selectedPath=="/A/B/C/file.txt");assertDisplayed("/A/B/C/file.txt",FileContentMode::Preview);
 fileTreeRevealPath="/A/absent";writing::KeyEvent escape{};escape.key=writing::Key::Escape;CHECK(fileSelectionKey(escape));CHECK(fileTreeRevealPath.empty());assertDisplayed("/A/B/C/file.txt",FileContentMode::Preview);
 reset();for(unsigned i=0;i<400;++i){char p[32];snprintf(p,sizeof(p),"/D%03u",i);treeEntry(p,true);}browseTree();fileSelectEntry("/D260",true,nullptr,true);showFileSelectionContent();fileRequestTreeReveal("/D260");CHECK(fileListOffset==256);CHECK(Tab5Focus.focused()==row("/D260",false));CHECK(selectedPath=="/D260");assertDisplayed("/D260",FileContentMode::Directory);
 scope=folderPreviewHost;fileRequestTreeReveal("/D001");CHECK(fileTreeRevealPath=="/D001");CHECK(fileListOffset==256);fileSelectEntry("/D261",true,row("/D261",false),false);scope=nullptr;serviceFileTreeReveal();CHECK(fileTreeRevealPath.empty());CHECK(selectedPath=="/D261");CHECK(fileListOffset==256);

 // No all-row repaint, repeated filesystem probes, or animation at rest.
 rowStyleCalls=0;const auto opensBeforeIdle=fsOpens;for(unsigned i=0;i<200;++i)updateFileSelectionUi();CHECK(rowStyleCalls==0);CHECK(fsOpens==opensBeforeIdle);
 rowStyleCalls=0;fileSelectEntry("/D262",true,row("/D262",false),false);CHECK(rowStyleCalls==2);CHECK(fsOpens==opensBeforeIdle+1);
 pump(4000);pump(100);flushes=0;flushedPixels=0;for(unsigned i=0;i<25;++i){serviceFileTreeReveal();serviceFolderPreview();pump(20);}CHECK(flushes==0);CHECK(flushedPixels==0);
 // Text limit is byte-bounded and does not leave an incomplete UTF-8 sequence.
 reset();treeEntry("/large.txt",false);fileBytes["/large.txt"]=std::string(FILE_PREVIEW_BYTES-1,'x')+"é"+"suffix";browseTree();click(row("/large.txt",false));CHECK(bytesRead==FILE_PREVIEW_BYTES);CHECK(strlen(lv_label_get_text(filePreviewLabel))==FILE_PREVIEW_BYTES-1);assertDisplayed("/large.txt",FileContentMode::Preview);
 // Refresh preserves visible preview while a context-only selection differs.
 treeEntry("/other.txt",false);fileBytes["/other.txt"]="other";startFileListPage(0);fileSelectEntry("/other.txt",false,row("/other.txt",false),false);refreshFileList();CHECK(selectedPath=="/other.txt");assertDisplayed("/large.txt",FileContentMode::Preview);

 // In-flight refresh leaves old left rows alive until the complete replacement is ready.
 auto*stable=row("/other.txt",false);CHECK(stable);fileTree.asyncRefresh=true;refreshFileList();CHECK(row("/other.txt",false)==stable);CHECK(fileTree.busy());serviceFileList();CHECK(!fileTree.busy());CHECK(row("/other.txt",false));fileTree.asyncRefresh=false;
 // Removing visible entries and volumes clears stale titles, preview content and row contexts.
 filesystem.erase("/large.txt");refreshFileList();assertDisplayed("",FileContentMode::Empty);CHECK(std::string(lv_label_get_text(filePreviewLabel)).empty());
 sdReady=false;Tab5UsbStorage::connected=false;refreshFileList();CHECK(fileTreeRoot.empty());CHECK(!fileTree.rootPath()[0]);CHECK(fileSelectionRows.empty());CHECK(selectedPath.empty());CHECK(contexts.empty());assertDisplayed("",FileContentMode::Empty);
 const unsigned beginsBeforeRemount=fileTree.begins;sdReady=true;refreshFileList();CHECK(fileTree.begins==beginsBeforeRemount+1);CHECK(fileTreeRoot=="/");assertDisplayed("",FileContentMode::Empty);
 // Delete in an empty Explorer cannot silently target a document left open in Editor.
 reset();currentPath="/editor-open.txt";filesystem[currentPath]=false;deleteDocEvent(nullptr);CHECK(!fileDeleteOverlay);CHECK(pendingDeletePaths.empty());CHECK(filesystem.count(currentPath));
 // Opening and cancelling the actual confirmation preserves preview rows, path and scanner.
 treeEntry("/Folder",true);filesystem["/Folder/child.txt"]=false;browseTree();click(row("/Folder",false));
 auto*childBeforeDelete=row("/Folder/child.txt",true);const unsigned scansBeforeDelete=folderPreview.begins;fileTree.pending=true;
 deleteDocEvent(nullptr);CHECK(fileDeleteOverlay);CHECK(pendingDeletePaths==std::vector<std::string>{"/Folder"});CHECK(row("/Folder/child.txt",true)==childBeforeDelete);CHECK(folderPreview.path=="/Folder");CHECK(fileTree.pending);assertDisplayed("/Folder",FileContentMode::Directory);
 click(lv_obj_get_child(lv_obj_get_child(fileDeleteOverlay,0),1));CHECK(!fileDeleteOverlay);CHECK(pendingDeletePaths.empty());CHECK(row("/Folder/child.txt",true)==childBeforeDelete);CHECK(folderPreview.begins==scansBeforeDelete);CHECK(fileTree.pending);CHECK(selectedPath=="/Folder");assertDisplayed("/Folder",FileContentMode::Directory);fileTree.pending=false;
 // Escape and Annuler share the exact same dialog close helper in production.
 deleteDocEvent(nullptr);CHECK(fileDeleteOverlay);closeFileDeleteDialog();CHECK(!fileDeleteOverlay);CHECK(row("/Folder/child.txt",true)==childBeforeDelete);assertDisplayed("/Folder",FileContentMode::Directory);
 // Real production Navigation SD/USB branches are extracted and dispatched from the menu callback.
 reset();filesystem["/USB"]=true;treeEntry("/USB/Notes",true);fileMenuCommand(reinterpret_cast<void*>(uintptr_t(FileCommand::Usb)));CHECK(fileTreeRequested);CHECK(currentDir=="/USB");CHECK(fileTreeRoot=="/USB");CHECK(row("/USB/Notes",false));assertDisplayed("",FileContentMode::Empty);
 fileMenuCommand(reinterpret_cast<void*>(uintptr_t(FileCommand::Sd)));CHECK(currentDir=="/");assertDisplayed("",FileContentMode::Empty);
 Tab5Focus.clear();lv_obj_clean(filesPage);CHECK(contexts.empty());lv_indev_delete(pointer);lv_deinit();
 printf("{\"checks\":%u,\"passed\":true,\"lvgl\":\"9.6.0\",\"immediateFirstClick\":true,\"doubleClickDeletedTarget\":true,\"previewContentAndPath\":true,\"idleFlushes\":0,\"selectionRowWrites\":2,\"repeatedSelectionUpdates\":200}\n",checks);
 return 0;
}
