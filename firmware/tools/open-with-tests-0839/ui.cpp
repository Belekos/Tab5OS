#include <lvgl.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Tab5Focus.h"
#include "Tab5UiStyle.h"
#include "Tab5TextInput.h"
#include "Tab5PythonArguments.h"
#define private public
#include "Tab5Tools.h"
#undef private
class String:public std::string {
 public:using std::string::string;using std::string::operator=;
 String(const std::string& s):std::string(s){} String()=default;
 bool isEmpty()const{return empty();}
 void toLowerCase(){for(char& c:*this)c=char(std::tolower(static_cast<unsigned char>(c)));}
 bool endsWith(const char* suffix)const{const size_t n=strlen(suffix);return size()>=n&&compare(size()-n,n,suffix)==0;}
};
static unsigned checks=0,shots=0;static std::string directory;
static void check(bool value,const char* message){++checks;if(!value){fprintf(stderr,"FAIL %u %s\n",checks,message);std::exit(1);}}
static std::vector<uint8_t> pixels(1280u*720u*3u),fontBytes;
static lv_font_t* face=nullptr;static lv_display_t* display=nullptr;static lv_obj_t* screen=nullptr;
static const lv_font_t* font(){return face;}
static void pump(){lv_tick_inc(20);lv_timer_handler();lv_obj_update_layout(screen);}
static void snapshot(unsigned size){lv_obj_invalidate(screen);lv_refr_now(display);auto name=directory+"/open-with-"+std::to_string(size)+".ppm";FILE* f=fopen(name.c_str(),"wb");check(f,"snapshot opens");fprintf(f,"P6\n1280 720\n255\n");for(size_t n=0;n<pixels.size();n+=3){uint8_t rgb[]={pixels[n+2],pixels[n+1],pixels[n]};fwrite(rgb,1,3,f);}fclose(f);++shots;}
namespace Tab5FilePicker {
 using Result=void(*)(const char*,void*);static Result callback=nullptr;static void* context=nullptr;
 static std::string initial,suffix;static unsigned opens=0;
 bool open(const char* path,const char* extension,bool folders,Result result,void* data){check(!folders,"application picker never selects folders");initial=path;suffix=extension;callback=result;context=data;++opens;return true;}
 void choose(const char* path){auto result=callback;auto data=context;callback=nullptr;context=nullptr;if(result)result(path,data);}
}
static struct FS {bool available=true;bool exists(const String&){return available;}}tab5FS;
namespace Tab5Python {
 static bool active=false;static std::string script,argument;static unsigned launches=0,stops=0;
 bool running(){return active;}
 const char* runningPath(){return active?script.c_str():"";}
 const char* runningArgument(){return active?argument.c_str():"";}
 bool runFile(const char* name,const char* arg){if(active)return false;script=name;argument=arg?arg:"";active=true;++launches;return true;}
 void stop(){++stops;active=false;}
 const char* status(){return "Python fixture";}
}
namespace Tab5PythonUI {void close(){}}
namespace Tab5PythonReader {void suspend(){}void resume(){}void close(){}}
namespace Tab5UsbPrinter {static uint32_t revisionValue=1;static bool available=false;uint32_t revision(){return revisionValue;}bool uri(const char* text){return text&&!strcmp(text,"usb://printer");}}
namespace Tab5Printers {
 struct Printer {String name,uri;bool secure;};static unsigned reads=0;static String remembered;
 std::vector<Printer> results(){++reads;std::vector<Printer> result{{"Network printer","ipp://192.0.2.7/ipp/print",false}};if(Tab5UsbPrinter::available)result.push_back({"Imprimante USB IPP","usb://printer",false});return result;}
 bool remember(const String& value){remembered=value;return true;}const char* status(){return "Printer fixture";}
}
static unsigned nativeCalls=0;static Tab5NativeFileApp nativeKind;static std::string nativePath;
struct Tab5ToolsApp::Impl {
 Tab5ToolsHooks hooks;lv_obj_t *parent=nullptr,*root=nullptr,*status=nullptr,*nativeMode=nullptr,*input=nullptr,*switchPrompt=nullptr,*scriptPanel=nullptr;
 String pendingPythonPath,pendingPythonArgument,openWithDocument;bool switchConfirmed=false,pythonWasRunning=false;Tab5Tool tool=Tab5Tool::Calculator;
 lv_obj_t *printers=nullptr,*uri=nullptr;std::vector<Tab5Printers::Printer> detected;String printerUri;uint32_t usbPrinterRevision=0;
 const char* tr(const char* text){return hooks.translate?hooks.translate(text):text;}
 uint32_t color(unsigned n){const uint32_t colors[]={0xf4f4f0,0xffffff,0x222824,0x69716a,0x427b57};return colors[n<5?n:0];}
 void note(const char* text){if(status)lv_label_set_text(status,tr(text));}
 void hideKeyboard(){}void scriptUI(){}void nativeModeChanged(){}
 void close(){Tab5Focus.clear();auto* old=root;root=nullptr;status=nativeMode=input=switchPrompt=scriptPanel=nullptr;pendingPythonPath="";pendingPythonArgument="";switchConfirmed=false;openWithDocument="";if(old)lv_obj_delete(old);}
 #include "members.inc"
 void open(Tab5Tool which,const char* selected,bool=false){close();tool=which;root=lv_obj_create(parent);lv_obj_set_pos(root,0,0);lv_obj_set_size(root,1216,672);lv_obj_set_style_pad_all(root,16,0);lv_obj_set_style_border_width(root,0,0);lv_obj_set_style_radius(root,0,0);lv_obj_set_style_bg_color(root,lv_color_hex(color(0)),0);lv_obj_set_style_text_color(root,lv_color_hex(color(2)),0);lv_obj_set_style_text_font(root,font(),0);lv_obj_set_scroll_dir(root,LV_DIR_VER);
  label(which==Tab5Tool::OpenWith?"Ouvrir avec…":"Application",0,0,990);status=label("Prêt",0,568,1120);
  if(which==Tab5Tool::OpenWith){
   #include "build.inc"
  }else if(which==Tab5Tool::Executable){nativeMode=dropdown("Demander\nSans fichier\nCe fichier",0,160,714);input=lv_textarea_create(root);lv_obj_set_pos(input,0,224);lv_obj_set_size(input,676,48);}
 }
 void act(int action){
  if(tool==Tab5Tool::Print){
   #include "printer-action.inc"
  }
  #include "actions.inc"
 }
};
Tab5ToolsApp Tab5Tools;
void Tab5ToolsApp::open(Tab5Tool tool,const char* path){impl_->open(tool,path);}
lv_obj_t* Tab5ToolsApp::scope()const{return impl_?impl_->root:nullptr;}
#include "public.inc"
static void finishedSwitch(Tab5ToolsApp::Impl* a){
 #include "switch.inc"
}
static void printerTick(Tab5ToolsApp::Impl* a){
 #include "printer-tick.inc"
}
static lv_obj_t* findButton(lv_obj_t* root,const char* label){for(unsigned i=0;i<lv_obj_get_child_count(root);++i){auto* o=lv_obj_get_child(root,i);if(lv_obj_check_type(o,&lv_button_class)&&!strcmp(lv_label_get_text(lv_obj_get_child(o,0)),label))return o;}return nullptr;}
static void click(lv_obj_t* o){check(o,"button exists");lv_obj_send_event(o,LV_EVENT_CLICKED,nullptr);pump();}
int main(int argc,char** argv){check(argc==3,"arguments");directory=argv[1];std::ifstream f(argv[2],std::ios::binary);fontBytes=std::vector<uint8_t>(std::istreambuf_iterator<char>(f),{});lv_init();display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size(),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);});screen=lv_obj_create(nullptr);lv_screen_load(screen);lv_obj_set_style_bg_color(screen,lv_color_hex(0xf4f4f0),0);auto* host=lv_obj_create(screen);lv_obj_set_pos(host,64,48);lv_obj_set_size(host,1216,672);lv_obj_set_style_pad_all(host,0,0);lv_obj_set_style_border_width(host,0,0);lv_obj_remove_flag(host,LV_OBJ_FLAG_SCROLLABLE);
 auto* a=Tab5Tools.impl_=new Tab5ToolsApp::Impl;a->parent=host;a->hooks.font=font;a->hooks.openNativeFile=[](Tab5NativeFileApp kind,const char* path){++nativeCalls;nativeKind=kind;nativePath=path;};
 for(unsigned size:{22u,24u}){face=lv_tiny_ttf_create_data(fontBytes.data(),fontBytes.size(),size);check(face,"shipped font loaded");face->fallback=size==22?&lv_font_montserrat_22:&lv_font_montserrat_24;Tab5Focus.begin(screen,face);Tab5Tools.openWith("/Documents/Étude.txt");pump();
  check(a->tool==Tab5Tool::OpenWith,"unrestricted chooser opened");check(lv_dropdown_get_option_count(a->nativeMode)==unsigned(Tab5NativeFileApp::Count),"all native applications offered");
  for(unsigned i=0;i<lv_obj_get_child_count(a->root);++i){auto* o=lv_obj_get_child(a->root,i);if(lv_obj_check_type(o,&lv_button_class)||lv_obj_check_type(o,&lv_dropdown_class)){lv_area_t r,p;lv_obj_get_coords(o,&r);lv_obj_get_coords(a->root,&p);check(r.x1>=p.x1&&r.x2<=p.x2&&r.y1>=p.y1&&r.y2<=p.y2,"control remains inside page");}}
  snapshot(size);a->close();lv_tiny_ttf_destroy(face);face=nullptr;
 }
 face=lv_tiny_ttf_create_data(fontBytes.data(),fontBytes.size(),22);face->fallback=&lv_font_montserrat_22;Tab5Focus.begin(screen,face);
 for(unsigned i=0;i<unsigned(Tab5NativeFileApp::Count);++i){Tab5Tools.openWith("/Documents/unknown.bin");lv_dropdown_set_selected(a->nativeMode,i);const unsigned before=nativeCalls;click(findButton(a->root,"Ouvrir"));check(nativeCalls==before+1&&nativeKind==Tab5NativeFileApp(i)&&nativePath=="/Documents/unknown.bin","unknown extension explicitly dispatched to chosen native application");check(!a->root,"chooser closed before native application dispatch");}
 Tab5Tools.openWith("/Documents/keep.json");tab5FS.available=false;click(findButton(a->root,"Ouvrir"));check(a->root&&!strcmp(lv_label_get_text(a->status),"Fichier inaccessible"),"removed document leaves chooser with an error");tab5FS.available=true;
 Tab5Tools.openWith("/USB/Documents/data.bin");click(findButton(a->root,"Choisir une application ELF…"));check(Tab5FilePicker::initial=="/Applications"&&Tab5FilePicker::suffix==".elf","ELF picker filtered");Tab5FilePicker::choose("/Applications/custom.elf");check(a->tool==Tab5Tool::Executable&&std::string(lv_textarea_get_text(a->input))=="usb:/Documents/data.bin","ELF config retains document argument");check(lv_dropdown_get_selected(a->nativeMode)==unsigned(Tab5LaunchMode::Ask),"ELF explicit argument not overridden by stored no-file mode");
 Tab5Python::active=false;Tab5Tools.openWith("/Documents/L'été.txt");click(findButton(a->root,"Choisir un script Python…"));check(Tab5FilePicker::initial=="/MicroPython"&&Tab5FilePicker::suffix==".py","Python picker filtered");Tab5FilePicker::choose("/MicroPython/custom.py");check(Tab5Python::active&&Tab5Python::script=="/MicroPython/custom.py"&&Tab5Python::argument=="/Documents/L'été.txt","Python receives the stable selected document");
 unsigned launches=Tab5Python::launches;check(Tab5Tools.openPythonApp("/MicroPython/custom.py","/Documents/L'été.txt")&&Tab5Python::launches==launches&&!a->switchPrompt,"same script and document resume without restart");
 check(Tab5Tools.openPythonApp("/MicroPython/custom.py","/USB/new.txt")&&a->switchPrompt,"same script with a different document requests switch");check(a->pendingPythonArgument=="/USB/new.txt","pending document retained");a->cancelSwitch();check(Tab5Python::argument=="/Documents/L'été.txt"&&a->pendingPythonArgument.empty(),"cancel leaves running document and clears pending argument");
 check(Tab5Tools.openPythonApp("/MicroPython/custom.py","/USB/new.txt"),"switch offered again");a->confirmSwitch();finishedSwitch(a);check(Tab5Python::argument=="/USB/new.txt"&&Tab5Python::launches==launches+1,"confirmed switch retains document after panel destruction");
 char path[261];check(Tab5PythonArguments::copy(nullptr,path,sizeof(path))&&!path[0],"no document allowed");check(Tab5PythonArguments::copy("usb:/Folder/L'été.txt",path,sizeof(path))&&!strcmp(path,"/USB/Folder/L'été.txt"),"USB and apostrophes normalized without code escaping");
 for(const char* invalid:{"/../a","/Folder/./a","relative.txt","/a\nb","/a//b","sd:/USB/data"})check(!Tab5PythonArguments::copy(invalid,path,sizeof(path)),"unsafe path rejected");
 std::string maximum="/"+std::string(259,'a');check(Tab5PythonArguments::copy(maximum.c_str(),path,sizeof(path)),"260-byte document accepted");maximum+='a';check(!Tab5PythonArguments::copy(maximum.c_str(),path,sizeof(path)),"261-byte document rejected");
 a->open(Tab5Tool::Print,nullptr);a->printers=a->dropdown("",0,54,924);a->uri=lv_textarea_create(a->root);lv_textarea_set_text(a->uri,"ipp://192.0.2.7/ipp/print");a->refreshPrinters();
 check(lv_dropdown_get_option_count(a->printers)==1&&lv_dropdown_get_selected(a->printers)==0,"network printer initially selected");
 unsigned reads=Tab5Printers::reads;for(unsigned i=0;i<200;++i)printerTick(a);check(Tab5Printers::reads==reads,"200 idle printer ticks do not rebuild the list");
 Tab5UsbPrinter::available=true;++Tab5UsbPrinter::revisionValue;printerTick(a);check(Tab5Printers::reads==reads+1&&lv_dropdown_get_option_count(a->printers)==2,"USB arrival refreshes the list once");
 check(!strcmp(lv_textarea_get_text(a->uri),"ipp://192.0.2.7/ipp/print")&&lv_dropdown_get_selected(a->printers)==0,"USB arrival preserves the selected network printer");
 check(strstr(lv_dropdown_get_options(a->printers),"Imprimante USB IPP · USB"),"USB endpoint has a translated distinct caption");
 lv_dropdown_set_selected(a->printers,1);a->act(24);check(!strcmp(lv_textarea_get_text(a->uri),"usb://printer")&&Tab5Printers::remembered=="usb://printer","explicit USB selection updates the target");
 Tab5UsbPrinter::available=false;++Tab5UsbPrinter::revisionValue;printerTick(a);check(lv_dropdown_get_option_count(a->printers)==1&&!strstr(lv_dropdown_get_options(a->printers),"USB"),"removed USB printer disappears from choices");
 check(!strcmp(lv_textarea_get_text(a->uri),"usb://printer"),"unplug does not silently redirect the job to a network printer");
 reads=Tab5Printers::reads;printerTick(a);check(Tab5Printers::reads==reads,"consumed removal revision does not rebuild again");
 lv_textarea_set_text(a->uri,"");Tab5UsbPrinter::available=true;++Tab5UsbPrinter::revisionValue;printerTick(a);check(!*lv_textarea_get_text(a->uri),"USB hotplug does not select a destination without user action");
 a->tool=Tab5Tool::OpenWith;reads=Tab5Printers::reads;++Tab5UsbPrinter::revisionValue;printerTick(a);check(Tab5Printers::reads==reads,"printer updates do not rebuild another tool");
 a->close();delete a;Tab5Tools.impl_=nullptr;lv_tiny_ttf_destroy(face);printf("{\"checks\":%u,\"captures\":%u,\"passed\":true}\n",checks,shots);
}
