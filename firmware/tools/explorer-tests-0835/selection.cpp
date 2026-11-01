#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Tab5DirectoryModel.h"
#include "Tab5FileSelection.h"
#include "Tab5ImageFormats.h"
struct String:std::string{using std::string::string;String(const std::string& s):std::string(s){};String()=default;};
struct lv_obj_t{bool valid=true;lv_obj_t* parent=nullptr;std::vector<lv_obj_t*> children;String path;};
struct lv_event_t{};struct lv_area_t{int x1=1,y1=1,x2=10,y2=10;};
constexpr int LV_ANIM_OFF=0,LV_EVENT_CLICKED=1;const char* LV_SYMBOL_LEFT="left";const char* LV_SYMBOL_RIGHT="right";
unsigned checks=0;void check(bool value){++checks;if(!value){fprintf(stderr,"check %u failed\n",checks);std::abort();}}
std::vector<std::unique_ptr<lv_obj_t>> objects;
lv_obj_t* object(lv_obj_t* parent=nullptr){objects.emplace_back(new lv_obj_t);auto* p=objects.back().get();p->parent=parent;if(parent)parent->children.push_back(p);return p;}
void lv_obj_clean(lv_obj_t* parent){for(auto* c:parent->children)c->valid=false;parent->children.clear();}
bool lv_obj_is_valid(lv_obj_t* p){return p&&p->valid;}
size_t lv_obj_get_child_count(lv_obj_t* p){return p->children.size();}
lv_obj_t* lv_obj_get_child(lv_obj_t* p,size_t i){return p->children.at(i);}
void lv_obj_get_coords(lv_obj_t*,lv_area_t*){}
lv_obj_t* lastScrolled=nullptr;void lv_obj_scroll_to_view(lv_obj_t* p,int){lastScrolled=p;}
lv_obj_t* lv_list_add_button(lv_obj_t* p,const char*,const char*){return object(p);}
void lv_list_add_text(lv_obj_t*,const char*){}
void lv_obj_add_event_cb(lv_obj_t*,void(*)(lv_event_t*),int,void*){}
void lv_label_set_text(lv_obj_t*,const char*){}
const char* T(const char* s){return s;}
lv_obj_t* filesPage=object();lv_obj_t* fileList=object(filesPage);lv_obj_t* filePathLabel=object();
lv_obj_t* selectedFileBtn=nullptr;lv_obj_t* fileSearchClear=nullptr;lv_obj_t* fileOpenWithBtn=object(filesPage);
lv_obj_t* scope=filesPage;
lv_obj_t* keyboardScope(){return scope;}
struct Focus{lv_obj_t* value=nullptr;void clear(){value=nullptr;}lv_obj_t* focused(){return value;}bool contains(lv_obj_t* p,lv_obj_t* c){for(;c;c=c->parent)if(p==c)return true;return false;}void focusObject(lv_obj_t* p){value=p;}void focusFirst(lv_obj_t* p){value=p->children.empty()?nullptr:p->children[0];}}Tab5Focus;
String currentDir="/",selectedPath,fileSearchQuery,fileSearchRoot,fileSelectionScope,fileListRevealPath;
bool selectedIsDir=false,selectedIsImage=false,selectedIsText=false,selectedIsAudio=false,selectedIsVideo=false;
bool fileMultiSelect=false,fileSelectionFocusOnly=false,fileDirectoryFailed=false,keyboardUiNavigation=false,fileListNeedsFocus=false;
bool sdReady=true,fileSelectAllSearch=false;
size_t fileListOffset=0,fileListSeen=0,fileListShown=0;constexpr size_t FILE_LIST_PAGE=128;constexpr int FILE_LEFT_W=350;
int32_t fileRefreshScroll=-1;unsigned fileKnownRevision=0;
Tab5DirectoryModel fileDirectoryModel;Tab5FileSelection fileSelection;
struct Entry{String path;bool directory;};std::unordered_map<std::string,bool> filesystem;
struct File{bool open=false,directory=false;explicit operator bool()const{return open;}bool isDirectory()const{return directory;}void close(){open=false;}}fileListDir,fileSelectAllDirectory;
struct Fs{bool exists(const char* p){return filesystem.count(p);}File open(const char* p){auto i=filesystem.find(p);return i==filesystem.end()?File{}:File{true,i->second};}}tab5FS;
namespace Tab5UsbStorage{bool mounted(){return true;}}
struct Searcher{bool active=false;bool busy(){return active;}void cancel(){active=false;}void begin(const String&,const String&){active=true;}}fileSearcher;
struct Paste{bool active=false;}filePasteBatch;
struct Row{std::string path;bool directory;lv_obj_t* object;};using FileSelectionRow=Row;std::vector<Row> fileSelectionRows;
struct Preview{std::vector<Entry> entries;}folderPreview;
lv_obj_t* folderPreviewRows=object(filesPage);
String parentDirOf(const String& p){auto slash=p.rfind('/');return slash==std::string::npos||slash==0?String("/"):String(p.substr(0,slash));}
String baseNameOf(const String& p){return p.substr(p.rfind('/')+1);}
String joinPath(const String& d,const String& n){return d=="/"?String("/"+n):String(d+"/"+n);}
String tailPath(const char* p,void*,int){return p;}void* uiFont(){return nullptr;}
unsigned tab5FsRevision(){return 42;}bool isExistingDirectory(const String& p){return filesystem.count(p)&&filesystem.at(p);}
bool pathStorageReady(const String&){return true;}
void setTopActionVisible(lv_obj_t*,bool){}void updateFileSearchCaption(){}
unsigned previews=0,actions=0;String previewPath;
void updateFileActionState(){++actions;}
void updateFileSelectionUi(){check(fileSelection.paths.size()==1);check(selectedPath==fileSelection.paths.front());}
void previewSelectedFile(){++previews;previewPath=selectedPath;selectedIsImage=Tab5ImageFormats::supported(selectedPath.c_str());}
void clearFilePreview(){fileListRevealPath="";fileSelection.clear();fileSelectAllDirectory.close();fileSelectAllSearch=false;selectedPath="";selectedIsDir=false;selectedIsImage=false;selectedFileBtn=nullptr;folderPreview.entries.clear();lv_obj_clean(folderPreviewRows);}
bool addFileRow(const String& full,const String&,bool directory){auto* row=object(fileList);row->path=full;fileSelectionRows.push_back({full,directory,row});if(full==selectedPath)selectedFileBtn=row;return true;}
void fileListPageEvent(lv_event_t*){}
static void refreshFileList();static void startFileListPage(size_t);
#include "selection-extracted.inc"
// Menu eligibility and command contents are the production implementation.
namespace Tab5GitView{bool busy(){return false;}lv_obj_t* modalScope(){return nullptr;}}
namespace Tab5Git{bool busy(){return false;}}
namespace Tab5FileOperation{bool busy(){return false;}}
namespace Tab5Files{bool showHidden=false;}
struct Tools{unsigned opened=0;lv_obj_t* scope(){return nullptr;}void openWith(const char*){++opened;}}Tab5Tools;
String fileGitDirectory(){return selectedIsDir?selectedPath:parentDirOf(selectedPath);}
bool selectedFolderIsGit(){return false;}
bool isEpubExtension(const String&){return false;}bool isPythonExtension(const String& p){return Tab5ImageFormats::extension(p.c_str(),".py");}bool isShellExtension(const String&){return false;}bool isExecutableExtension(const String&){return false;}bool isFirmwareExtension(const String&){return false;}
std::vector<std::string> fileClipboardPaths;String clipboardPath;
namespace Tab5ContextMenu{constexpr size_t Capacity=32;struct Item{const char* title;const char* shortcut;void(*command)(void*);void* data;bool enabled;};std::vector<Item> last;bool show(lv_obj_t*,int,int,Item* items,size_t count){last.assign(items,items+count);return count>0&&count<=Capacity;}}
void fileMenuCommand(void*){}void setKeyboardVisible(bool){}
enum class Page{Files,Other};Page activePage=Page::Files;
lv_obj_t *dialogOverlay=nullptr,*filePopupOverlay=nullptr,*fileDeleteOverlay=nullptr,*fileOperationOverlay=nullptr;
#include "menus-extracted.inc"
bool menuHas(FileCommand command){return std::any_of(Tab5ContextMenu::last.begin(),Tab5ContextMenu::last.end(),[&](const Tab5ContextMenu::Item& i){return reinterpret_cast<uintptr_t>(i.data)==static_cast<uintptr_t>(command)&&i.enabled;});}
void fillDirectory(unsigned count,const char* extension=".png"){
  for(unsigned i=0;i<count;++i){char name[64];snprintf(name,sizeof(name),"image-%04u%s",i,extension);check(fileDirectoryModel.add(name,false));filesystem[joinPath(currentDir,name)]=false;}
  fileDirectoryModel.sort(Tab5FileSort::Order::NameAscending);
}
void ready(){fileListDir.close();startFileListPage(fileListOffset);}
int main(){
  filesystem["/"]=true;filesystem["/Images"]=true;filesystem["/USB"]=true;filesystem["/USB/My pictures"]=true;
  filesystem["/Images/image-0260.png"]=false;currentDir="/";selectedPath="/Images";fileSelection.add("/Images");fileMultiSelect=true;
  folderPreview.entries.push_back({"/Images/image-0260.png",false});
  const String& aliased=folderPreview.entries[0].path;
  selectFolderPreviewEntry(aliased,false);
  check(currentDir=="/Images");check(fileSearchQuery.empty());check(fileSelectionScope=="/Images\n");check(!fileMultiSelect);check(fileSelection.paths==std::vector<std::string>{"/Images/image-0260.png"});
  check(fileSelection.anchor=="/Images/image-0260.png");check(selectedPath==previewPath);check(!selectedIsDir);check(selectedIsImage);check(bool(fileListDir));check(!selectedFileBtn);check(previews==1);
  fillDirectory(400);ready();check(fileListOffset==256);check(selectedFileBtn&&selectedFileBtn->path==selectedPath);check(Tab5Focus.focused()==selectedFileBtn);check(lastScrolled==selectedFileBtn);check(fileListRevealPath.empty());check(fileListShown==128);
  const auto initialPreviews=previews;focusRevealedFile();focusRevealedFile();check(previews==initialPreviews);
  check(showFileMenu(FileMenu::Context,0,0));for(auto c:{FileCommand::Copy,FileCommand::Cut,FileCommand::Rename,FileCommand::Delete,FileCommand::Info,FileCommand::ViewImage,FileCommand::EditImage,FileCommand::OpenWith})check(menuHas(c));check(fileGitDirectory()=="/Images");
  check(showFileMenu(FileMenu::ImageOpenWith,0,0));check(Tab5ContextMenu::last.size()==3);check(menuHas(FileCommand::ViewImage));check(menuHas(FileCommand::EditImage));check(menuHas(FileCommand::OpenWithOthers));
  for(const char* p:{"/x.JPG","/x.jPeG","/x.PNG","/x.BMP","/x.WEBP","/x.T5I"}){selectedPath=p;selectedIsImage=false;check(fileCanEditImage());check(Tab5ImageFormats::supported(p));}
  for(const char* p:{"/x.gif","/x.jpg.txt","/x.png/file","/x","/x.svg"}){selectedPath=p;selectedIsImage=false;check(!fileCanEditImage());check(!Tab5ImageFormats::supported(p));}
  selectedPath="/Images/test.py";selectedIsText=true;selectedIsImage=false;fileSelection.paths={selectedPath};fileClipboardPaths={"/clipboard.txt"};
  check(showFileMenu(FileMenu::Context,0,0));check(Tab5ContextMenu::last.size()==17);check(menuHas(FileCommand::Execute));check(menuHas(FileCommand::Read));check(menuHas(FileCommand::Paste));
  selectedIsText=false;fileClipboardPaths.clear();fileSelection.paths={"/Images/image-0260.png"};
  using K=Tab5ImageFormats::Kind;
  const uint8_t png[]={137,80,78,71,13,10,26,10};const uint8_t jpg[]={255,216};
  check(Tab5ImageFormats::sniff(png,sizeof(png))==K::Png);check(Tab5ImageFormats::sniff(jpg,sizeof(jpg))==K::Jpeg);
  check(Tab5ImageFormats::sniff(reinterpret_cast<const uint8_t*>("BM"),2)==K::Bmp);
  check(Tab5ImageFormats::sniff(reinterpret_cast<const uint8_t*>("T5IMG001"),8)==K::Project);
  check(Tab5ImageFormats::sniff(reinterpret_cast<const uint8_t*>("RIFF1234WEBP"),12)==K::Webp);
  check(Tab5ImageFormats::sniff(reinterpret_cast<const uint8_t*>("RIFF1234WAVE"),12)==K::Unknown);
  check(Tab5ImageFormats::sniff(png,4)==K::Unknown);check(Tab5ImageFormats::sniff(nullptr,12)==K::Unknown);
  selectedPath="/Images/image-0260.png";fileSelection.add("/Images/another.png");check(!showFileMenu(FileMenu::ImageOpenWith,0,0));fileSelection.paths.resize(1);
  // A context-menu overlay defers focus, without losing the real selection.
  filesystem["/USB/My pictures/a photo.bmp"]=false;currentDir="/";fileSearchQuery="old query";fileSearchRoot="/";
  selectFolderPreviewEntry("/USB/My pictures/a photo.bmp",false);check(currentDir=="/USB/My pictures");check(fileSearchQuery.empty());check(fileSearcher.busy()==false);
  check(fileDirectoryModel.add("a photo.bmp",false));scope=object();ready();check(!Tab5Focus.focused());check(fileListRevealPath==selectedPath);scope=filesPage;focusRevealedFile();check(Tab5Focus.focused()==selectedFileBtn);check(fileListRevealPath.empty());
  // Navigating/selecting elsewhere before asynchronous completion cancels focus.
  selectFolderPreviewEntry("/Images/image-0260.png",false);selectedPath="/Images/other.png";focusRevealedFile();check(fileListRevealPath.empty());
  // Right click on the right pane selects that path before deriving commands.
  fileListDir.close();currentDir="/";selectedPath="/Images";selectedIsDir=true;fileSelection.clear();fileSelection.add("/Images");
  folderPreview.entries.push_back({"/Images/image-0260.png",false});auto* rightRow=object(folderPreviewRows);auto* icon=object(rightRow);
  check(fileContextMenuAt(icon,20,20));check(selectedPath=="/Images/image-0260.png");check(currentDir=="/Images");check(fileSelection.paths==std::vector<std::string>{selectedPath});check(menuHas(FileCommand::Delete));check(menuHas(FileCommand::EditImage));
  // Repeated opening of a folder preview replaces—not accumulates—selection.
  for(unsigned i=0;i<200;++i){selectFolderPreviewEntry("/USB/My pictures/a photo.bmp",false);check(fileSelection.paths.size()==1);check(fileSelection.paths[0]=="/USB/My pictures/a photo.bmp");check(fileDirectoryModel.add("a photo.bmp",false));ready();check(Tab5Focus.focused()==selectedFileBtn);}
  selectFolderPreviewEntry("/Images/image-0260.png",false);ready();check(fileListRevealPath.empty());
  fileDirectoryModel.clear();fileSelectionRows.clear();objects.clear();
  printf("{\"checks\":%u,\"repeatedSelections\":200,\"passed\":true}\n",checks);
}
