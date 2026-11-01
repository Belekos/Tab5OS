#include <lvgl.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "Tab5Focus.h"
#include "Tab5UiStyle.h"
#include "Tab5Icons.h"
#include "Tab5ImageFormats.h"
#include "WriterUsb.h"
struct String:std::string{
 using std::string::string;using std::string::operator=;
 String(const std::string& text):std::string(text){};String()=default;
};
static unsigned checks=0,frames=0,flushes=0;
static void check(bool ok,const char* message){++checks;if(!ok){fprintf(stderr,"FAIL %u: %s\n",checks,message);std::exit(1);}}
static std::vector<uint8_t> pixels(1280u*720u*3u),fontData;
static lv_font_t* font=nullptr;static lv_display_t* display;static lv_obj_t* screen;
static std::string shotDir;
static uint32_t now=1000;
namespace writing {
std::deque<MouseReport> reports;
bool usbMouseConnected(){return true;}
bool usbMousePoll(MouseReport& report){if(reports.empty())return false;report=reports.front();reports.pop_front();return true;}
}
#include "Tab5Mouse.cpp"
static lv_obj_t *pageHost=nullptr,*filesPage=nullptr,*filePathLabel=nullptr,*fileList=nullptr;
static lv_obj_t *filePreviewTitle=nullptr,*filePreviewMeta=nullptr,*filePreviewScroll=nullptr,*filePreviewLabel=nullptr,*filePreviewImage=nullptr;
static lv_obj_t *folderPreviewHost=nullptr,*folderPreviewRows=nullptr,*folderPreviewPrevious=nullptr,*folderPreviewNext=nullptr,*folderPreviewPageLabel=nullptr;
static lv_obj_t *fileActionBar=nullptr,*fileOpenBtn=nullptr,*fileDrawBtn=nullptr,*fileReadBtn=nullptr,*fileExecuteBtn=nullptr,*fileGitBtn=nullptr,*fileOpenWithBtn=nullptr,*fileGitHost=nullptr;
static lv_obj_t *filePaneDivider=nullptr,*fileParentButton=nullptr,*fileActionsMenuBtn=nullptr,*fileUsbButton=nullptr,*fileSearchClear=nullptr;
static lv_obj_t *fileRenameBtn=nullptr,*deleteDocBtn=nullptr,*fileInfoBtn=nullptr,*filePrintBtn=nullptr,*fileVaultBtn=nullptr,*fileCopyBtn=nullptr,*fileCutBtn=nullptr,*filePasteBtn=nullptr,*fileHiddenBtn=nullptr,*fileHideSelectedBtn=nullptr;
static constexpr int WORK_W=1216,PAGE_H=672,FILE_LEFT_W=380;
static struct {int cornerRadius=8;} settings;
static const uint32_t light[]={0xf4f4f0,0xffffff,0xe8e9e6,0xb9bcb6,0x222824,0x69716a,0x427b57,0xffffff,0xd9eadc};
static const uint32_t dark[]={0x13191f,0x20262c,0x30383e,0x708080,0xe9eeee,0xa9b1b3,0x77ad99,0x101616,0x36574d};
static uint32_t palette[9];
static lv_color_t C_BG(){return lv_color_hex(palette[0]);}static lv_color_t C_SURFACE(){return lv_color_hex(palette[1]);}
static lv_color_t C_RAISED(){return lv_color_hex(palette[2]);}static lv_color_t C_BORDER(){return lv_color_hex(palette[3]);}
static lv_color_t C_TEXT(){return lv_color_hex(palette[4]);}static lv_color_t C_MUTED(){return lv_color_hex(palette[5]);}
static lv_color_t C_ACTIVE(){return lv_color_hex(palette[6]);}static lv_color_t C_SELECTED(){return lv_color_hex(palette[8]);}
static const lv_font_t*uiFont(){return font;}static const lv_font_t*uiHeadingFont(){return font;}static const lv_font_t*documentFont(){return font;}
static const char*T(const char* text){return text;}
namespace Tab5Shell {static void width(lv_obj_t*,int){}}
static bool selectedIsDir=false,selectedIsText=false,selectedIsImage=false,selectedIsAudio=false,selectedIsVideo=false,fileMultiSelect=false,fileSelectAllDirectory=false,fileSelectAllSearch=false,busy=false;
static String selectedPath,currentDir="/",fileContentDir="/",fileDisplayedPath="/",clipboardPath;
static struct {std::vector<std::string> paths;} fileSelection;
static std::vector<std::string> fileClipboardPaths;
static struct {bool active=false;} filePasteBatch;
enum class FileContentMode{Directory,Search,Preview};static FileContentMode fileContentMode=FileContentMode::Directory;
static bool pathStorageReady(const String&){return true;}
static bool selectedFolderIsGit(){return false;}
static String parentDirOf(const String& path){auto pos=path.rfind('/');return pos==0||pos==String::npos?String("/"):String(path.substr(0,pos));}
static bool isImageProject(const String& path){return Tab5ImageFormats::kind(path.c_str())==Tab5ImageFormats::Kind::Project;}
static bool isEpubExtension(const String& path){return Tab5ImageFormats::extension(path.c_str(),".epub");}
static bool isPythonExtension(const String& path){return Tab5ImageFormats::extension(path.c_str(),".py");}
static bool isShellExtension(const String& path){return Tab5ImageFormats::extension(path.c_str(),".sh");}
static bool isExecutableExtension(const String& path){return Tab5ImageFormats::extension(path.c_str(),".elf");}
static bool isFirmwareExtension(const String& path){return Tab5ImageFormats::extension(path.c_str(),".bin");}
namespace Tab5GitView{bool busy(){return ::busy;}lv_obj_t*scope(){return nullptr;}void context(const char*,const char*){}}
namespace Tab5Git{bool busy(){return false;}}
namespace Tab5FileOperation{bool busy(){return false;}}
namespace Tab5Files{bool showHidden=false;}
namespace Tab5HiddenFiles{bool hidden(const char*){return false;}}
static void setLabelIfChanged(lv_obj_t* object,const char* value){if(object&&strcmp(lv_label_get_text(object),value))lv_label_set_text(object,value);}
#include "state.inc"
static unsigned viewer=0,draw=0,openWith=0,legacy=0;
static std::string viewedPath,drawnPath;
static void startImageViewer(const String& path){++viewer;viewedPath=path;}
static void openImageEditor(const char* path){++draw;drawnPath=path;}
#include "image-eligible.inc"
#include "command.inc"
static void selectedOpenEvent(lv_event_t*){++legacy;}
static void folderPreviewPageEvent(lv_event_t*){}static void selectedReadEvent(lv_event_t*){}
static void selectedExecuteEvent(lv_event_t*){}static void openFileGit(){}
static void openSelectedWith(lv_obj_t*){++openWith;}
static void upFolderEvent(lv_event_t*){}static void fileMenuEvent(lv_event_t*){}static void fileSearchEvent(lv_event_t*){}
enum class FileMenu {Create,Edit,Actions,View,Sort,Git,Context,ImageOpenWith};
#include "helpers.inc"
#include "Tab5ExplorerUi.inc"
#include "Tab5ExplorerRowStyle.inc"
static void paint(){now+=20;lv_obj_update_layout(screen);lv_obj_update_layout(lv_layer_top());lv_refr_now(display);}
static void move(int x,int y,uint8_t buttons=0){writing::MouseReport report{};report.x=x-Tab5Mouse::x;report.y=y-Tab5Mouse::y;report.buttons=buttons;writing::reports.push_back(report);Tab5Mouse::service();paint();}
static void click(lv_obj_t* object){check(object,"click target");lv_area_t area;lv_obj_get_coords(object,&area);int x=(area.x1+area.x2)/2,y=(area.y1+area.y2)/2;move(x,y);move(x,y,1);move(x,y,0);}
static bool visible(lv_obj_t* object){return object&&!lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN);}
static void snapshot(const char* name){paint();std::string path=shotDir+"/"+name+".ppm";FILE* out=fopen(path.c_str(),"wb");check(out,"snapshot opens");fprintf(out,"P6\n1280 720\n255\n");for(size_t i=0;i<pixels.size();i+=3){uint8_t rgb[]={pixels[i+2],pixels[i+1],pixels[i]};fwrite(rgb,1,3,out);}fclose(out);++frames;}
static void select(const char* path,bool multiple=false,bool directory=false){
 selectedPath=path;selectedIsDir=directory;selectedIsImage=!directory&&Tab5ImageFormats::supported(path)&&!isImageProject(selectedPath);
 selectedIsText=Tab5ImageFormats::extension(path,".txt");selectedIsAudio=selectedIsVideo=false;
 fileSelection.paths={path};if(multiple)fileSelection.paths.push_back("/Pictures/other.png");
 updateFileActionState();paint();
}
static unsigned visibleActions(){unsigned count=0;for(uint32_t i=0;i<lv_obj_get_child_count(fileActionBar);++i)count+=visible(lv_obj_get_child(fileActionBar,i));return count;}
int main(int argc,char**argv){
 check(argc==3,"arguments");shotDir=argv[1];std::ifstream input(argv[2],std::ios::binary);fontData={std::istreambuf_iterator<char>(input),{}};
 lv_init();lv_tick_set_cb([](){return now;});font=lv_tiny_ttf_create_data(fontData.data(),fontData.size(),22);check(font,"real OS font");font->fallback=&lv_font_montserrat_22;
 display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size(),LV_DISPLAY_RENDER_MODE_FULL);
 lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*){++flushes;lv_display_flush_ready(d);});screen=lv_screen_active();lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_text_font(screen,font,0);
 Tab5Focus.begin(lv_layer_top(),font);Tab5Mouse::begin(display);Tab5Mouse::service();
 for(unsigned theme=0;theme<2;++theme){
  std::copy(theme?std::begin(dark):std::begin(light),theme?std::end(dark):std::end(light),palette);
  lv_display_set_theme(display,lv_theme_mono_init(display,palette[0]<0x808080,font));
  Tab5UiStyle::configure(palette,8);Tab5Focus.theme(C_SELECTED(),C_TEXT(),C_SURFACE());lv_obj_set_style_bg_color(screen,C_BG(),0);lv_obj_set_style_text_color(screen,C_TEXT(),0);
  pageHost=lv_obj_create(screen);lv_obj_set_pos(pageHost,64,48);lv_obj_set_size(pageHost,WORK_W,PAGE_H);lv_obj_set_style_pad_all(pageHost,0,0);lv_obj_set_style_border_width(pageHost,0,0);lv_obj_remove_flag(pageHost,LV_OBJ_FLAG_SCROLLABLE);
  selectedPath="";fileSelection.paths.clear();createFilePage();paint();
  lv_obj_remove_flag(folderPreviewHost,LV_OBJ_FLAG_HIDDEN);lv_label_set_text(filePreviewTitle,"/Pictures");lv_label_set_text(filePreviewMeta,"6 éléments");
  auto* first=fileCreateContentRow(folderPreviewRows,"Paysage.png",false,C_TEXT());fileStyleRow(first,true,C_TEXT());
  for(const char* path:{"/Pictures/Paysage.png","/Pictures/photo.JPG","/Pictures/photo.jpeg","/Pictures/image.bmp","/Pictures/test.WEBP","/Pictures/drawing.t5i"}){
   select(path);check(visible(fileOpenBtn)&&visible(fileOpenWithBtn),"image viewer and open-with present");
#ifdef BASELINE
   check(!fileDrawBtn&&visibleActions()==2,"baseline lacks drawing quick action");check(!strcmp(lv_label_get_text(lv_obj_get_child(fileOpenBtn,0)),LV_SYMBOL_IMAGE),"baseline viewer uses same picture symbol as Drawing");
#else
   check(visible(fileDrawBtn)&&visibleActions()==3,"three distinct quick image actions");
   check(!strcmp(lv_label_get_text(lv_obj_get_child(fileOpenBtn,0)),LV_SYMBOL_EYE_OPEN),"viewer has eye symbol");
   check(lv_obj_get_child_count(fileDrawBtn)>=2,"Drawing vector exists beside empty label");
   unsigned beforeViewer=viewer,beforeDraw=draw,beforeWith=openWith;click(fileOpenBtn);check(viewer==beforeViewer+1&&draw==beforeDraw&&viewedPath==path,"eye routes to real ViewImage branch exactly once");
   click(fileDrawBtn);check(draw==beforeDraw+1&&drawnPath==path,"brush routes to real EditImage branch exactly once");
   click(fileOpenWithBtn);check(openWith==beforeWith+1,"open-with remains available");
#endif
   lv_area_t bar,row;lv_obj_get_coords(fileActionBar,&bar);lv_obj_get_coords(first,&row);
#ifdef BASELINE
   check(row.y1-bar.y2-1==0,"baseline action bar touches first row");
#else
   check(row.y1-bar.y2-1>=8,"eight pixel breathing space before first selected row");
   check(lv_obj_get_width(fileDrawBtn)>=44&&lv_obj_get_height(fileDrawBtn)>=44,"drawing touch target retained");
   lv_area_t previous{};bool seen=false;
   for(uint32_t i=0;i<lv_obj_get_child_count(fileActionBar);++i){auto* item=lv_obj_get_child(fileActionBar,i);if(!visible(item))continue;lv_area_t area;lv_obj_get_coords(item,&area);check(area.x1>=bar.x1&&area.x2<=bar.x2,"action stays inside toolbar");if(seen)check(area.x1-previous.x2-1>=8,"action icons keep horizontal gap");previous=area;seen=true;}
#endif
  }
  snapshot(theme?"images-dark":"images-light");
#ifndef BASELINE
  busy=true;updateFileActionState();paint();check(lv_obj_has_state(fileDrawBtn,LV_STATE_DISABLED),"Drawing disabled while file work is busy");unsigned before=draw;click(fileDrawBtn);check(draw==before,"busy Draw does not launch");
  check(lv_obj_has_state(fileOpenBtn,LV_STATE_DISABLED),"image viewer disabled while file work is busy");before=viewer;click(fileOpenBtn);check(viewer==before,"busy viewer does not launch");busy=false;
  select("/Pictures/Paysage.png",true);check(!visible(fileOpenBtn)&&!visible(fileDrawBtn)&&!visible(fileOpenWithBtn),"multiple selection hides file quick actions");
  select("/Pictures",false,true);check(!visible(fileOpenBtn)&&!visible(fileDrawBtn),"folder does not advertise drawing or viewer");
  select("/Documents/note.txt");check(visible(fileOpenBtn)&&visible(fileReadBtn)&&!visible(fileDrawBtn),"text retains own actions");unsigned old=legacy;click(fileOpenBtn);check(legacy==old+1,"non-image original open callback preserved");
  select("/Pictures/not-an-image.xyz");check(!visible(fileOpenBtn)&&!visible(fileDrawBtn)&&visible(fileOpenWithBtn),"unsupported file retains open-with without false image actions");
#endif
  Tab5Focus.clear();lv_obj_delete(pageHost);pageHost=nullptr;paint();
 }
 printf("{\"passed\":true,\"checks\":%u,\"frames\":%u,\"viewerCalls\":%u,\"drawingCalls\":%u,\"openWithCalls\":%u}\n",checks,frames,viewer,draw,openWith);
}
