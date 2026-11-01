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
 void trim(){auto a=find_first_not_of(" \t\r\n"),b=find_last_not_of(" \t\r\n");*this=a==npos?String():String(substr(a,b-a+1));}
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
static lv_obj_t*dialogOverlay=nullptr,*filePopupOverlay=nullptr,*fileDeleteOverlay=nullptr,*fileOperationOverlay=nullptr,*scope=nullptr,*gitModalScope=nullptr;
static bool selectedIsDir=false,selectedIsText=false,selectedIsImage=false,selectedIsAudio=false,selectedIsVideo=false;
static String currentDir="/",fileContentDir="/",fileTreeRoot="/",selectedPath,fileSearchQuery,fileSearchRoot,fileListRevealPath,fileSelectionScope,fileSelectAllRoot;
static String fileTreeRevealPath;
static FileContentMode fileContentMode=FileContentMode::Empty;
static String fileDisplayedPath,fileMetadataPath,filePreviewImagePath;
static uint32_t fileDisplayedRevision=0,fileMetadataRevision=0;
static size_t fileSelectedBytes=0;static bool fileTreeRequested=false;
static bool fileSessionStarted=false,fileSessionRestorePending=false,fileSearchNeedsRestart=false;
static int32_t fileSavedTreeScroll=0,fileSavedContentScroll=0;
static size_t fileSavedContentOffset=0,fileSearchOffset=0;
static String fileSavedFocusPath;static bool fileSavedFocusRight=false;
static void beginFileSearch(const String&,const String&);static void renderFileSearchPage(size_t);static void restoreFileSessionPosition();
static void showFileSelectionContent(bool force=false);static void restoreFileContent();static void clearFileContent();
static lv_obj_t*filePreviewTitle,*filePreviewLabel,*filePreviewScroll,*filePreviewImage,*folderPreviewPageLabel,*folderPreviewPrevious,*folderPreviewNext;
static unsigned imagePreviews=0;
static void showFilePreviewImage(){++imagePreviews;filePreviewImagePath=selectedPath;lv_obj_add_flag(filePreviewLabel,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(filePreviewImage,LV_OBJ_FLAG_HIDDEN);}
static bool fileActiveRight=false,fileMultiSelect=false,fileSelectionLongPress=false,fileDirectoryFailed=false,fileSelectAllSearch=false,sdReady=true;
static size_t fileListOffset=0,fileListShown=0;static uint32_t fileKnownRevision=0;
static std::vector<std::string>fileClipboardPaths;static String clipboardPath;static bool clipboardCut=false;
static struct {bool active=false;}filePasteBatch;
enum class Page{Files,Editor,Other};static Page activePage=Page::Files;
static lv_obj_t*keyboardScope(){return fileDeleteOverlay?fileDeleteOverlay:gitModalScope?gitModalScope:scope?scope:filesPage;}
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
static bool isExistingDirectory(const String&p){return filesystem.count(p)&&filesystem.at(p);}static bool pathStorageReady(const String&);static unsigned fsRevision=42;static unsigned tab5FsRevision(){return fsRevision;}
namespace Tab5UsbStorage{static bool connected=true;bool mounted(){return connected;}bool path(const char*p){return std::string(p)=="/USB"||std::string(p).rfind("/USB/",0)==0;}}
static bool pathStorageReady(const String&p){return Tab5UsbStorage::path(p.c_str())?Tab5UsbStorage::mounted():sdReady;}
namespace Tab5GitView{bool busy(){return false;}lv_obj_t*modalScope(){return gitModalScope;}lv_obj_t*scope(){return gitModalScope;}}
namespace Tab5Git{bool busy(){return false;}}
namespace Tab5FileOperation{bool busy(){return false;}}
namespace Tab5Files{bool showHidden=false;bool userWritable(const char*p){return p&&*p&&std::string(p)!="/"&&std::string(p)!="/USB";}}
struct Tools{lv_obj_t*scope(){return nullptr;}void openWith(const char*){}}Tab5Tools;
static bool gitRepository=false;static bool selectedFolderIsGit(){return gitRepository;}static bool isEpubExtension(const String&){return false;}static bool isPythonExtension(const String&p){return Tab5ImageFormats::extension(p.c_str(),".py");}static bool isShellExtension(const String&){return false;}static bool isExecutableExtension(const String&){return false;}static bool isFirmwareExtension(const String&){return false;}
struct Tree{
 struct Row{std::string path,name;bool directory=false;uint16_t depth=0;bool volume=false,expandable=true;};
 std::vector<Row>rows;std::set<std::string>expandedPaths;String root="/";bool pending=false,changed=false,asyncRefresh=false,volumeMode=false,sdMounted=false,usbMounted=false,hints=false;unsigned begins=0,refreshes=0;
 bool rowIndex(const String&p,size_t&i)const{for(i=0;i<rows.size();++i)if(rows[i].path==p)return true;return false;}
 size_t rowCount()const{return rows.size();}Row row(size_t i)const{return rows.at(i);}bool busy()const{return pending;}bool dirty()const{return changed;}void clearDirty(){changed=false;}bool failed()const{return false;}
 bool expanded(const char*p)const{return expandedPaths.count(p);}bool expanded(const String&p)const{return expanded(p.c_str());}
 bool expand(const char*p){expandedPaths.insert(p);changed=true;return true;}bool expand(const String&p){return expand(p.c_str());}
 bool collapse(const char*p){expandedPaths.erase(p);changed=true;return true;}bool collapse(const String&p){return collapse(p.c_str());}bool toggle(const String&p){return expanded(p)?collapse(p):expand(p);}
 bool volumes()const{return volumeMode;}bool hasSubdirectories(const String&p)const{for(auto&r:rows)if(r.path==p)return r.expandable;return false;}bool hasSubdirectories(const char*p)const{return hasSubdirectories(String(p));}
 void requestChildProbe(const String&){}void requestChildProbe(const char*){}bool hintsDirty()const{return hints;}void clearHintsDirty(){hints=false;}bool probing()const{return false;}
 bool syncVolumes(bool sd){bool changedVolumes=sdMounted!=sd||usbMounted!=Tab5UsbStorage::mounted();sdMounted=sd;usbMounted=Tab5UsbStorage::mounted();
  rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const Row&r){return (r.path=="/"&&!sd)||(r.path=="/USB"&&!usbMounted);}),rows.end());
  if(sd&&std::none_of(rows.begin(),rows.end(),[](const Row&r){return r.path=="/";}))rows.insert(rows.begin(),{"/","SD",true,0,true,true});
  if(usbMounted&&std::none_of(rows.begin(),rows.end(),[](const Row&r){return r.path=="/USB";}))rows.push_back({"/USB","USB",true,0,true,true});changed|=changedVolumes;return changedVolumes;}
 bool beginVolumes(bool sd){++begins;root="/";volumeMode=true;changed=true;syncVolumes(sd);if(sd)expandedPaths.insert("/");return true;}
 void clear(){rows.clear();expandedPaths.clear();root="";volumeMode=false;sdMounted=usbMounted=false;}void cancel(){pending=false;}void refresh(){++refreshes;changed=true;pending=asyncRefresh;}bool begin(const String&p){++begins;root=p;changed=true;return true;}const char*rootPath()const{return root.c_str();}void service(){pending=false;}
}fileTree;
struct Searcher{
 bool active=false,partial=false;String root,query;size_t found=0;unsigned begins=0;
 bool busy()const{return active;}bool incomplete()const{return partial;}size_t count()const{return found;}
 void cancel(){active=false;}void begin(const String&r,const String&q){++begins;root=r;query=q;active=true;found=0;partial=false;}
 template<class Fn>void step(Fn visitor){for(auto&[path,dir]:filesystem)if(path!=root&&(root=="/"||String(path).startsWith(root+"/"))&&baseNameOf(path).find(query)!=String::npos){if(!visitor(String(path),baseNameOf(path),dir)){partial=true;break;}++found;}active=false;}
}fileSearcher;
static void setTopActionVisible(lv_obj_t*,bool){}
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
struct Tab5FolderPreview{static constexpr size_t PageSize=48;};
struct FolderPreview{
 struct Entry{String path,name;bool directory;};
 std::vector<Entry>entries;String path;size_t count=0,offset=0;bool done=true,failed=false,dirty=false;unsigned begins=0;
 void cancel(){path="";entries.clear();dirty=false;done=true;}
 bool begin(const String&p,size_t first=0){++begins;path=p;offset=first;entries.clear();std::vector<Entry>all;for(const auto&[name,dir]:filesystem)if(parentDirOf(name)==path&&name!=path)all.push_back({name,baseNameOf(name),dir});count=all.size();if(offset>=count&&offset)offset=count?((count-1)/48)*48:0;for(size_t i=offset;i<count&&entries.size()<48;++i)entries.push_back(all[i]);done=true;failed=false;dirty=true;return true;}
 void service(){}
}folderPreview;
static void serviceFolderPreview();static void cancelFolderPreview();static void folderPreviewPageEvent(lv_event_t*);
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
static void setup(){lv_init();display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size(),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*a,uint8_t*){++flushes;flushedPixels+=size_t(a->x2-a->x1+1)*(a->y2-a->y1+1);lv_display_flush_ready(d);});root=filesPage=lv_screen_active();lv_obj_set_style_pad_all(filesPage,0,0);lv_obj_remove_flag(filesPage,LV_OBJ_FLAG_SCROLLABLE);Tab5Focus.begin(filesPage,uiFont());Tab5Focus.theme(C_SELECTED(),C_TEXT(),C_SURFACE());fileList=lv_list_create(filesPage);lv_obj_set_pos(fileList,0,40);lv_obj_set_size(fileList,FILE_LEFT_W,650);folderPreviewHost=lv_obj_create(filesPage);lv_obj_set_pos(folderPreviewHost,400,40);lv_obj_set_size(folderPreviewHost,850,650);lv_obj_set_style_pad_all(folderPreviewHost,0,0);folderPreviewRows=lv_obj_create(folderPreviewHost);lv_obj_set_size(folderPreviewRows,lv_pct(100),lv_pct(100));lv_obj_set_flex_flow(folderPreviewRows,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(folderPreviewRows,0,0);filePreviewScroll=lv_obj_create(filesPage);lv_obj_set_pos(filePreviewScroll,400,40);lv_obj_set_size(filePreviewScroll,850,650);filePreviewLabel=lv_label_create(filePreviewScroll);lv_obj_set_width(filePreviewLabel,800);filePreviewImage=lv_obj_create(filePreviewScroll);lv_obj_set_size(filePreviewImage,80,80);lv_obj_remove_flag(filePreviewImage,LV_OBJ_FLAG_CLICKABLE);filePreviewTitle=lv_label_create(filesPage);folderPreviewPageLabel=lv_label_create(filesPage);filePreviewMeta=lv_label_create(filesPage);filePathLabel=lv_label_create(filesPage);fileOpenWithBtn=lv_button_create(filesPage);lv_obj_set_pos(fileOpenWithBtn,1100,0);folderPreviewNext=lv_button_create(filesPage);lv_obj_add_flag(folderPreviewNext,LV_OBJ_FLAG_HIDDEN);lv_obj_add_event_cb(folderPreviewNext,folderPreviewPageEvent,LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(1)));pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,readPointer);lv_indev_set_double_click_time(pointer,LV_INDEV_DEF_DOUBLE_CLICK_TIME);pointerState.state=LV_INDEV_STATE_RELEASED;pump();}
static void oldReset(){Tab5Focus.clear();lv_obj_clean(fileList);lv_obj_clean(folderPreviewRows);fileSelection.clear();fileSelectionRows.clear();fileTree.clear();fileListRevealPath="";fileTreeRevealPath="";selectedPath="";selectedFileBtn=nullptr;fileActiveRight=false;fileMultiSelect=false;fileSelectionLongPress=false;fileContentDir="/";currentDir="/";fileTreeRoot="/";fileSearchQuery="";fileSearchRoot="";fileListOffset=0;writing::modifiers=0;fileOpens=0;filesystem.clear();filesystem["/"]=true;fileTree.root="/";scope=nullptr;pump(500);}
static void reset(){
 lv_indev_reset(pointer,nullptr);pointerState.state=LV_INDEV_STATE_RELEASED;fileTreeRequested=false;
 oldReset();fileSessionStarted=fileSessionRestorePending=fileSearchNeedsRestart=false;fileSavedTreeScroll=fileSavedContentScroll=0;fileSavedContentOffset=fileSearchOffset=0;fileSavedFocusPath="";fileSavedFocusRight=false;fileSearcher.begins=0;clearFileContent();fileTree.root="";fileTreeRoot="";fileMetadataPath="";fileMetadataRevision=0;
 fileBytes.clear();fsRevision=42;gitModalScope=nullptr;currentPath="";pendingDeletePath="";pendingDeletePaths.clear();fsOpens=bytesRead=0;imagePreviews=0;folderPreview.begins=0;fileTree.begins=0;fileTree.pending=false;fileTree.asyncRefresh=false;
 sdReady=true;Tab5UsbStorage::connected=true;filesystem["/USB"]=true;gitRepository=false;clipboardPath="";clipboardCut=false;fileClipboardPaths.clear();activePage=Page::Files;fileDirectoryFailed=false;pump(500);
}
static void clickPoint(lv_point_t point){pointerState.point=point;pointerState.state=LV_INDEV_STATE_PRESSED;lv_indev_read(pointer);pump(30);pointerState.state=LV_INDEV_STATE_RELEASED;lv_indev_read(pointer);pump(20);}
static lv_point_t center(lv_obj_t*object){CHECK(object);lv_obj_scroll_to_view(object,LV_ANIM_OFF);pump();pointerAt(object);return pointerState.point;}
static void treeEntry(const char*path,bool dir,uint16_t depth=0){filesystem[path]=dir;fileTree.rows.push_back({path,baseNameOf(path),dir,depth});}
static void browseTree(){ensureFileSession();serviceFileList();pump();}
static void assertDisplayed(const char*path,FileContentMode mode){if(fileDisplayedPath!=path||fileContentMode!=mode)fprintf(stderr,"DISPLAY expected=%s mode=%u got=%s mode=%u selection=%s\n",path,unsigned(mode),fileDisplayedPath.c_str(),unsigned(fileContentMode),selectedPath.c_str());CHECK(fileDisplayedPath==path);CHECK(fileContentMode==mode);CHECK(std::string(lv_label_get_text(filePreviewTitle))==path);}
int main(){
 setup();reset();
 treeEntry("/Documents",true,1);treeEntry("/Images",true,1);
 filesystem["/Documents/a.txt"]=false;fileBytes["/Documents/a.txt"]="Text selected, then explicitly opened.";
 filesystem["/Images/photo.png"]=false;
 // The first entry opens the SD volume; subsequent entries keep the session.
 browseTree();CHECK(fileSessionStarted);CHECK(fileTree.volumes());CHECK(fileTree.begins==1);
 CHECK(row("/",false));CHECK(row("/USB",false));CHECK(row("/Documents",false));assertDisplayed("/",FileContentMode::Directory);
 CHECK(row("/Documents",true));CHECK(fileTree.expanded("/"));
 const unsigned initialBegins=fileTree.begins;const unsigned initialScans=folderPreview.begins;
 ensureFileSession();CHECK(fileTree.begins==initialBegins);CHECK(folderPreview.begins==initialScans);assertDisplayed("/",FileContentMode::Directory);
 // Left folder click navigates right. Left contains no file fixture/model rows.
 click(row("/Documents",false));assertDisplayed("/Documents",FileContentMode::Directory);CHECK(row("/Documents/a.txt",true));CHECK(!row("/Documents/a.txt",false));CHECK(fileOpens==0);
 // Right single click changes selection only: same row, content and path survive.
 pump(600);auto*textRow=row("/Documents/a.txt",true);const auto scansBeforeSingle=folderPreview.begins;const lv_point_t textPoint=center(textRow);
 clickPoint(textPoint);CHECK(selectedPath=="/Documents/a.txt");CHECK(!selectedIsDir);CHECK(fileActiveRight);CHECK(row("/Documents/a.txt",true)==textRow);
 assertDisplayed("/Documents",FileContentMode::Directory);CHECK(folderPreview.begins==scansBeforeSingle);CHECK(fileOpens==0);CHECK(imagePreviews==0);gitRepository=true;CHECK(!fileShowGitAction());gitRepository=false;
 // Native LVGL double click opens the selected file exactly once.
 clickPoint(textPoint);CHECK(fileOpens==1);CHECK(openedFile=="/Documents/a.txt");CHECK(selectedPath=="/Documents/a.txt");
 assertDisplayed("/Documents",FileContentMode::Directory);CHECK(row("/Documents/a.txt",true)==textRow);
 CHECK(showFileMenu(FileMenu::Edit,400,200));CHECK(menuHas(FileCommand::Rename));CHECK(menuHas(FileCommand::Delete));
 CHECK(showFileMenu(FileMenu::Actions,400,200));CHECK(!menuHas(FileCommand::Rename));CHECK(!menuHas(FileCommand::Delete));CHECK(!menuHas(FileCommand::GitMenu));
 // An image follows the same selection/open contract, without single-click decode.
 click(row("/Images",false));pump(600);auto*imageRow=row("/Images/photo.png",true);const auto imagePoint=center(imageRow);
 clickPoint(imagePoint);CHECK(imagePreviews==0);CHECK(fileOpens==1);assertDisplayed("/Images",FileContentMode::Directory);CHECK(row("/Images/photo.png",true)==imageRow);
 CHECK(showFileMenu(FileMenu::ImageOpenWith,400,200));CHECK(menuHas(FileCommand::ViewImage));CHECK(menuHas(FileCommand::EditImage));CHECK(menuHas(FileCommand::OpenWithOthers));
 clickPoint(imagePoint);CHECK(fileOpens==2);CHECK(openedFile=="/Images/photo.png");CHECK(imagePreviews==0);
 // Session reentry keeps the browsed directory and the user selection.
 const String savedSelection=selectedPath;const unsigned scansBeforeReturn=folderPreview.begins;
 leaveFileSessionForTest();activePage=Page::Files;ensureFileSession();restoreFileSessionPosition();CHECK(fileTree.begins==initialBegins);CHECK(folderPreview.begins==scansBeforeReturn);CHECK(selectedPath==savedSelection);assertDisplayed("/Images",FileContentMode::Directory);

 // Native folder double click opens once, then reveals the folder on the left.
 reset();treeEntry("/Folder",true,1);treeEntry("/Other",true,1);filesystem["/Folder/child.txt"]=false;fileBytes["/Folder/child.txt"]="child";browseTree();pump(600);
 auto*folderRow=row("/Folder",true);const lv_point_t folderPoint=center(folderRow);const unsigned scansBeforeFolder=folderPreview.begins;
 clickPoint(folderPoint);CHECK(row("/Folder",true)==folderRow);CHECK(selectedPath=="/Folder");assertDisplayed("/",FileContentMode::Directory);CHECK(folderPreview.begins==scansBeforeFolder);
 clickPoint(folderPoint);assertDisplayed("/Folder",FileContentMode::Directory);CHECK(row("/Folder",true)==nullptr);CHECK(row("/Folder/child.txt",true));CHECK(folderPreview.begins==scansBeforeFolder+1);
 serviceFileTreeReveal();CHECK(Tab5Focus.focused()==row("/Folder",false));CHECK(selectedPath=="/Folder");CHECK(fileTreeRevealPath.empty());CHECK(fileOpens==0);
 // Enter uses the real focus activation and opens the focused right file.
 auto*child=row("/Folder/child.txt",true);Tab5Focus.focusObject(child);writing::KeyEvent enter{};enter.key=writing::Key::Enter;
 CHECK(Tab5Focus.key(enter,filesPage));CHECK(fileOpens==1);CHECK(openedFile=="/Folder/child.txt");
 // Chevron expands only, without changing the selected object or right view.
 fileSelectEntry("/Other",true,row("/Other",false),false);const String contextBeforeChevron=selectedPath;pump(600);
 auto*chevron=lv_obj_get_child(row("/Folder",false),2);CHECK(chevron);click(chevron);CHECK(fileTree.expanded("/Folder"));CHECK(selectedPath==contextBeforeChevron);assertDisplayed("/Folder",FileContentMode::Directory);
 // The left folder with no known subfolders must not display a chevron.
 fileTree.rows.push_back({"/Leaf","Leaf",true,1,false,false});filesystem["/Leaf"]=true;startFileListPage(0);pump();CHECK(row("/Leaf",false));CHECK(lv_obj_has_flag(lv_obj_get_child(row("/Leaf",false),2),LV_OBJ_FLAG_HIDDEN));
 // Explicit Parent uses the visible directory and gives commands that destination.
 fileParentNavigation();assertDisplayed("/",FileContentMode::Directory);CHECK(fileActiveRight);CHECK(fileContextDirectory()=="/");CHECK(fileGitDirectory()=="/");

 // Selection modifiers, touch multi-select and context menus never navigate.
 reset();treeEntry("/A",true,1);treeEntry("/B",true,1);treeEntry("/C",true,1);browseTree();pump(600);
 writing::modifiers=0x01;click(row("/A",true));assertDisplayed("/",FileContentMode::Directory);CHECK(fileSelection.paths.size()==1);
 writing::modifiers=0x02;click(row("/C",true));CHECK(fileSelection.paths.size()==3);assertDisplayed("/",FileContentMode::Directory);writing::modifiers=0;
 fileMultiSelect=true;click(row("/B",true));CHECK(fileSelection.paths.size()==2);assertDisplayed("/",FileContentMode::Directory);fileMultiSelect=false;
 CHECK(fileContextMenuAt(lv_obj_get_child(row("/B",true),1),500,200));CHECK(selectedPath=="/B");assertDisplayed("/",FileContentMode::Directory);
 pump(600);auto*held=row("/A",true);pointerAt(held);pointerState.state=LV_INDEV_STATE_PRESSED;lv_indev_read(pointer);pump(700);lv_indev_read(pointer);pointerState.state=LV_INDEV_STATE_RELEASED;lv_indev_read(pointer);pump();
 CHECK(fileMultiSelect);assertDisplayed("/",FileContentMode::Directory);CHECK(fileOpens==0);
 fileMultiSelect=false;pump(600);click(row("/B",true));assertDisplayed("/",FileContentMode::Directory);CHECK(selectedPath=="/B");CHECK(fileOpens==0);
 // Git is a single contextual submenu with distinct ordinary/repository choices.
 gitRepository=false;CHECK(!fileShowGitAction());CHECK(showFileMenu(FileMenu::Actions,400,200));CHECK(menuHas(FileCommand::GitMenu));CHECK(!menuHas(FileCommand::Git));CHECK(!menuHas(FileCommand::InitGit));CHECK(!menuHas(FileCommand::Rename));CHECK(!menuHas(FileCommand::Delete));
 fileMenuCommand(reinterpret_cast<void*>(uintptr_t(FileCommand::GitMenu)));CHECK(menuHas(FileCommand::InitGit));CHECK(menuHas(FileCommand::NewGit));CHECK(menuHas(FileCommand::ImportGit));CHECK(!menuHas(FileCommand::Git));
 gitRepository=true;CHECK(fileShowGitAction());CHECK(showFileMenu(FileMenu::Git,400,200));CHECK(menuHas(FileCommand::Git));CHECK(!menuHas(FileCommand::InitGit));CHECK(!menuHas(FileCommand::NewGit));CHECK(!menuHas(FileCommand::ImportGit));gitRepository=false;
 CHECK(showFileMenu(FileMenu::Edit,400,200));CHECK(menuHas(FileCommand::Copy));CHECK(menuHas(FileCommand::Cut));CHECK(menuHas(FileCommand::Rename));CHECK(menuHas(FileCommand::Delete));CHECK(!menuHas(FileCommand::GitMenu));
 CHECK(showFileMenu(FileMenu::Create,400,200));CHECK(menuHas(FileCommand::NewDocument));CHECK(menuHas(FileCommand::NewFolder));CHECK(!menuHas(FileCommand::NewGit));CHECK(!menuHas(FileCommand::ImportGit));
 fileSelectEntry("/A",true,row("/A",true),true,true);CHECK(fileSelection.paths.size()==2);CHECK(showFileMenu(FileMenu::Actions,400,200));CHECK(!menuHas(FileCommand::GitMenu));gitRepository=true;CHECK(!fileShowGitAction());gitRepository=false;
 fileSelectEntry("/",true,row("/",false),false);CHECK(showFileMenu(FileMenu::Edit,400,200));CHECK(!menuHas(FileCommand::Delete));CHECK(!menuHas(FileCommand::Rename));CHECK(!menuHas(FileCommand::Cut));CHECK(!menuHas(FileCommand::Copy));
 fileClipboardPaths={"/kept.txt"};clipboardPath="/kept.txt";clipboardCut=false;
 copySelectedEvent(nullptr);CHECK(fileClipboardPaths==std::vector<std::string>{"/kept.txt"});CHECK(clipboardPath=="/kept.txt");CHECK(!clipboardCut);
 cutSelectedEvent(nullptr);CHECK(fileClipboardPaths==std::vector<std::string>{"/kept.txt"});CHECK(clipboardPath=="/kept.txt");CHECK(!clipboardCut);
 deleteDocEvent(nullptr);CHECK(!fileDeleteOverlay);

 // Asynchronous tree reveal and pagination do not retarget the selected file.
 reset();treeEntry("/A",true,1);filesystem["/A/B"]=true;filesystem["/A/B/C"]=true;filesystem["/A/B/C/file.txt"]=false;fileBytes["/A/B/C/file.txt"]="deep";browseTree();
 fileSelectEntry("/A/B/C/file.txt",false,nullptr,true);fileRequestTreeReveal("/A/B/C");CHECK(fileTree.expanded("/A"));CHECK(fileTreeRevealPath=="/A/B/C");
 fileTree.pending=true;serviceFileTreeReveal();CHECK(!fileTree.expanded("/A/B"));fileTree.pending=false;treeEntry("/A/B",true,2);serviceFileTreeReveal();CHECK(fileTree.expanded("/A/B"));treeEntry("/A/B/C",true,3);startFileListPage(0);serviceFileTreeReveal();
 CHECK(fileTreeRevealPath.empty());CHECK(Tab5Focus.focused()==row("/A/B/C",false));CHECK(selectedPath=="/A/B/C/file.txt");assertDisplayed("/",FileContentMode::Directory);
 fileTreeRevealPath="/A/absent";writing::KeyEvent escape{};escape.key=writing::Key::Escape;CHECK(fileSelectionKey(escape));CHECK(fileTreeRevealPath.empty());assertDisplayed("/",FileContentMode::Directory);
 reset();for(unsigned i=0;i<400;++i){char p[32];snprintf(p,sizeof(p),"/D%03u",i);treeEntry(p,true,1);}browseTree();fileSelectEntry("/D260",true,nullptr,true);openFileContentDirectory("/D260");fileRequestTreeReveal("/D260");CHECK(fileListOffset==256);CHECK(Tab5Focus.focused()==row("/D260",false));CHECK(selectedPath=="/D260");assertDisplayed("/D260",FileContentMode::Directory);
 scope=folderPreviewHost;fileRequestTreeReveal("/D001");CHECK(fileTreeRevealPath=="/D001");CHECK(fileListOffset==256);fileSelectEntry("/D261",true,row("/D261",false),false);scope=nullptr;serviceFileTreeReveal();CHECK(fileTreeRevealPath.empty());CHECK(selectedPath=="/D261");CHECK(fileListOffset==256);
 // No all-row repaint, repeated filesystem probes, or animation at rest.
 rowStyleCalls=0;const auto opensBeforeIdle=fsOpens;for(unsigned i=0;i<200;++i)updateFileSelectionUi();CHECK(rowStyleCalls==0);CHECK(fsOpens==opensBeforeIdle);
 rowStyleCalls=0;fileSelectEntry("/D262",true,row("/D262",false),false);CHECK(rowStyleCalls==2);CHECK(fsOpens==opensBeforeIdle+1);
 pump(4000);pump(100);flushes=0;flushedPixels=0;for(unsigned i=0;i<25;++i){serviceFileTreeReveal();serviceFolderPreview();pump(20);}CHECK(flushes==0);CHECK(flushedPixels==0);
 // Session initialization never rebuilds an existing paginated/expanded tree.
 const auto returnBegins=fileTree.begins;const auto returnScans=folderPreview.begins;const auto savedOffset=fileListOffset;const String savedView=fileDisplayedPath;
 leaveFileSessionForTest();activePage=Page::Files;ensureFileSession();restoreFileSessionPosition();CHECK(fileTree.begins==returnBegins);CHECK(folderPreview.begins==returnScans);CHECK(fileListOffset==savedOffset);CHECK(fileDisplayedPath==savedView);
 // Bounded preview helper remains safe for explicit preview callers/restoration.
 reset();filesystem["/large.txt"]=false;fileBytes["/large.txt"]=std::string(FILE_PREVIEW_BYTES-1,'x')+"é"+"suffix";browseTree();fileSelectEntry("/large.txt",false,row("/large.txt",true),true);const unsigned beforeText=bytesRead;showFileSelectionContent();CHECK(bytesRead-beforeText==FILE_PREVIEW_BYTES);CHECK(strlen(lv_label_get_text(filePreviewLabel))==FILE_PREVIEW_BYTES-1);assertDisplayed("/large.txt",FileContentMode::Preview);
 fileParentNavigation();assertDisplayed("/",FileContentMode::Directory);
 // Delete without Explorer selection never targets a document opened elsewhere.
 reset();currentPath="/editor-open.txt";filesystem[currentPath]=false;deleteDocEvent(nullptr);CHECK(!fileDeleteOverlay);CHECK(pendingDeletePaths.empty());CHECK(filesystem.count(currentPath));
 // Cancelling the real confirmation keeps rows, displayed path and active scanner.
 treeEntry("/Folder",true,1);filesystem["/Folder/child.txt"]=false;browseTree();click(row("/Folder",false));
 auto*childBeforeDelete=row("/Folder/child.txt",true);const unsigned scansBeforeDelete=folderPreview.begins;fileTree.pending=true;
 deleteDocEvent(nullptr);CHECK(fileDeleteOverlay);CHECK(pendingDeletePaths==std::vector<std::string>{"/Folder"});CHECK(row("/Folder/child.txt",true)==childBeforeDelete);CHECK(folderPreview.path=="/Folder");CHECK(fileTree.pending);assertDisplayed("/Folder",FileContentMode::Directory);
 click(lv_obj_get_child(lv_obj_get_child(fileDeleteOverlay,0),1));CHECK(!fileDeleteOverlay);CHECK(pendingDeletePaths.empty());CHECK(row("/Folder/child.txt",true)==childBeforeDelete);CHECK(folderPreview.begins==scansBeforeDelete);CHECK(fileTree.pending);CHECK(selectedPath=="/Folder");assertDisplayed("/Folder",FileContentMode::Directory);fileTree.pending=false;
 deleteDocEvent(nullptr);CHECK(fileDeleteOverlay);closeFileDeleteDialog();CHECK(!fileDeleteOverlay);CHECK(row("/Folder/child.txt",true)==childBeforeDelete);assertDisplayed("/Folder",FileContentMode::Directory);
 // Search results belong to the right pane; left stays a directory-only tree.
 reset();treeEntry("/Documents",true,1);treeEntry("/Documents/report-folder",true,2);treeEntry("/Elsewhere",true,1);
 filesystem["/Documents/report.txt"]=false;fileBytes["/Documents/report.txt"]="searched text";
 filesystem["/Documents/report-folder/child.txt"]=false;fileBytes["/Documents/report-folder/child.txt"]="child";
 browseTree();auto*leftDocument=row("/Documents",false);beginFileSearch("/Documents","report");CHECK(fileSearcher.busy());assertDisplayed("/Documents",FileContentMode::Search);
 serviceFileSearch();CHECK(!fileSearcher.busy());CHECK(row("/Documents",false)==leftDocument);CHECK(!row("/Documents/report.txt",false));CHECK(row("/Documents/report.txt",true));CHECK(row("/Documents/report-folder",true));
 pump(600);auto*result=row("/Documents/report.txt",true);const auto resultPoint=center(result);clickPoint(resultPoint);CHECK(row("/Documents/report.txt",true)==result);CHECK(selectedPath=="/Documents/report.txt");CHECK(fileSearchQuery=="report");assertDisplayed("/Documents",FileContentMode::Search);CHECK(fileOpens==0);
 clickPoint(resultPoint);CHECK(fileOpens==1);CHECK(openedFile=="/Documents/report.txt");assertDisplayed("/Documents",FileContentMode::Search);
 const unsigned searchesBeforeReturn=fileSearcher.begins;leaveFileSessionForTest();activePage=Page::Files;ensureFileSession();restoreFileSessionPosition();CHECK(fileSearcher.begins==searchesBeforeReturn);CHECK(fileSearchQuery=="report");CHECK(row("/Documents/report.txt",true)==result);CHECK(selectedPath=="/Documents/report.txt");assertDisplayed("/Documents",FileContentMode::Search);
 // Opening a result folder leaves Search completely; refresh cannot resurrect it.
 pump(600);const auto resultFolderPoint=center(row("/Documents/report-folder",true));clickPoint(resultFolderPoint);assertDisplayed("/Documents",FileContentMode::Search);clickPoint(resultFolderPoint);assertDisplayed("/Documents/report-folder",FileContentMode::Directory);CHECK(fileSearchQuery.empty());CHECK(row("/Documents/report-folder/child.txt",true));refreshFileList();assertDisplayed("/Documents/report-folder",FileContentMode::Directory);CHECK(fileSearchQuery.empty());
 beginFileSearch("/Documents","report");serviceFileSearch();click(row("/Elsewhere",false));assertDisplayed("/Elsewhere",FileContentMode::Directory);CHECK(fileSearchQuery.empty());
 beginFileSearch("/Documents","report");serviceFileSearch();fileParentNavigation();assertDisplayed("/Documents",FileContentMode::Directory);CHECK(fileSearchQuery.empty());
 // An interrupted search restarts only that query after returning, not the tree.
 beginFileSearch("/Documents","report");CHECK(fileSearcher.busy());const auto beforePausedTree=fileTree.begins;const auto beforePausedSearch=fileSearcher.begins;
 leaveFileSessionForTest();CHECK(fileSearchNeedsRestart);CHECK(!fileSearcher.busy());activePage=Page::Files;ensureFileSession();CHECK(fileSearcher.begins==beforePausedSearch+1);CHECK(fileTree.begins==beforePausedTree);CHECK(fileSearchQuery=="report");serviceFileSearch();restoreFileSessionPosition();CHECK(row("/Documents/report.txt",true));assertDisplayed("/Documents",FileContentMode::Search);
 // Search pagination and focus are remembered while the OS session remains live.
 for(unsigned i=0;i<65;++i){char p[64];snprintf(p,sizeof(p),"/Documents/report-%02u.txt",i);filesystem[p]=false;fileBytes[p]="report";}
 beginFileSearch("/Documents","report");serviceFileSearch();CHECK(fileDirectoryModel.size()>48);renderFileSearchPage(48);CHECK(fileSearchOffset==48);
 const String rememberedSearchPath=fileSelectionRows.back().path.c_str();auto*rememberedSearchRow=row(rememberedSearchPath.c_str(),true);CHECK(rememberedSearchRow);fileSelectEntry(rememberedSearchPath,false,rememberedSearchRow,true);Tab5Focus.focusObject(rememberedSearchRow);
 const auto completedSearchBegins=fileSearcher.begins;leaveFileSessionForTest();activePage=Page::Files;ensureFileSession();restoreFileSessionPosition();CHECK(fileSearchOffset==48);CHECK(fileSearcher.begins==completedSearchBegins);CHECK(selectedPath==rememberedSearchPath);CHECK(Tab5Focus.focused()==rememberedSearchRow);
 fileClearSearch();assertDisplayed("/Documents",FileContentMode::Directory);CHECK(fileSearchQuery.empty());
 // A modal Git pane must preserve the directory page, scroll, and previous focus.
 reset();treeEntry("/Folder",true,1);for(unsigned i=0;i<50;++i){char p[32];snprintf(p,sizeof(p),"/Tree%02u",i);treeEntry(p,true,1);}
 for(unsigned i=0;i<90;++i){char p[64];snprintf(p,sizeof(p),"/Folder/file-%02u.txt",i);filesystem[p]=false;fileBytes[p]="item";}
 browseTree();click(row("/Folder",false));lv_obj_send_event(folderPreviewNext,LV_EVENT_CLICKED,nullptr);CHECK(folderPreview.offset==48);CHECK(row("/Folder/file-48.txt",true));
 auto*sessionRow=row("/Folder/file-65.txt",true);fileSelectEntry("/Folder",true,row("/Folder",false),false);gitRepository=true;CHECK(fileShowGitAction());Tab5Focus.focusObject(sessionRow);pump();lv_obj_scroll_to_y(fileList,140,LV_ANIM_OFF);lv_obj_scroll_to_y(folderPreviewRows,220,LV_ANIM_OFF);pump();
 rememberFileSession();const auto savedTreeScroll=fileSavedTreeScroll,savedContentScroll=fileSavedContentScroll;CHECK(savedTreeScroll>0);CHECK(savedContentScroll>0);CHECK(fileSavedContentOffset==48);CHECK(fileSavedFocusPath=="/Folder/file-65.txt");
 gitModalScope=lv_obj_create(filesPage);cancelFolderPreview();Tab5Focus.focusObject(gitModalScope);rememberFileSession();restoreFileSessionPosition();CHECK(fileSessionRestorePending);CHECK(fileSavedContentOffset==48);CHECK(fileSavedTreeScroll==savedTreeScroll);CHECK(fileSavedContentScroll==savedContentScroll);
 {auto*modal=gitModalScope;gitModalScope=nullptr;lv_obj_delete(modal);}restoreFileContent();restoreFileSessionPosition();CHECK(selectedPath=="/Folder");CHECK(folderPreview.offset==48);CHECK(row("/Folder/file-65.txt",true));CHECK(Tab5Focus.focused()==row("/Folder/file-65.txt",true));CHECK(lv_obj_get_scroll_y(fileList)==savedTreeScroll);CHECK(lv_obj_get_scroll_y(folderPreviewRows)==savedContentScroll);CHECK(!fileSessionRestorePending);
 // After UI cache release plus external FS change, restoring cannot overwrite the snapshot with empty widgets.
 leaveFileSessionForTest();const auto purgeTreeScroll=fileSavedTreeScroll,purgeContentScroll=fileSavedContentScroll;const auto purgeOffset=fileSavedContentOffset;const String purgeFocus=fileSavedFocusPath;
 lv_obj_clean(fileList);cancelFolderPreview();++fsRevision;activePage=Page::Files;ensureFileSession();serviceFileList();serviceFolderPreview();restoreFileSessionPosition();
 CHECK(folderPreview.offset==purgeOffset);CHECK(lv_obj_get_scroll_y(fileList)==purgeTreeScroll);CHECK(lv_obj_get_scroll_y(folderPreviewRows)==purgeContentScroll);CHECK(Tab5Focus.focused()==row(purgeFocus.c_str(),true));assertDisplayed("/Folder",FileContentMode::Directory);
 // Root rows route to their own support; USB disappears when absent.
 reset();Tab5UsbStorage::connected=false;treeEntry("/Notes",true,1);browseTree();CHECK(row("/",false));CHECK(!row("/USB",false));assertDisplayed("/",FileContentMode::Directory);
 Tab5UsbStorage::connected=true;filesystem["/USB"]=true;filesystem["/USB/Notes"]=true;refreshFileList();serviceFileList();CHECK(row("/USB",false));click(row("/USB",false));assertDisplayed("/USB",FileContentMode::Directory);CHECK(row("/USB/Notes",true));CHECK(fileContextDirectory()=="/USB");
 Tab5Focus.clear();lv_obj_clean(filesPage);CHECK(contexts.empty());lv_indev_delete(pointer);lv_deinit();
 printf("{\"checks\":%u,\"passed\":true,\"lvgl\":\"9.6.0\",\"rightSingleSelectOnly\":true,\"nativeDoubleOpen\":true,\"leftFolderClickNavigates\":true,\"sessionNavigationRetained\":true,\"gitSubmenuContextual\":true,\"idleFlushes\":0,\"selectionRowWrites\":2,\"repeatedSelectionUpdates\":200}\n",checks);
 return 0;
}
