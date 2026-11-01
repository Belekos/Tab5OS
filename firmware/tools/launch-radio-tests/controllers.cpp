#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <lvgl.h>
#include "WriterView.h"
#include "WriterDocument.h"
#include "WriterSyntax.h"

static unsigned checks=0;
static void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}++checks;}
using String=std::string;
static std::vector<std::string> order;
static uint32_t millis(){return 100;}
namespace Tab5Python {
static bool active=false,error=false;
bool running(){return active;}bool failed(){return error;}
const char* runningPath(){return "/MicroPython/test.py";}
uint32_t outputRevision(){return 1;}String output(){return "test\n";}
}
enum class Tab5Tool {Python,Other};
enum class Page {Writing,Home,Files};
static Page activePage=Page::Writing;
static bool keyboardDockFocus=true,keyboardUiNavigation=false,keyboardVisible=true;
static void setKeyboardVisible(bool visible){keyboardVisible=visible;order.push_back("keyboard");}
namespace Tab5PythonReader {static bool opened=false;bool visible(){return opened;}void resume(){order.push_back("reader");}}
struct Focus {void focusInitial(lv_obj_t*){order.push_back("focus");}} Tab5Focus;
namespace Tab5TextInput {bool hasSelection(lv_obj_t*){return false;}}
class Tab5ToolsApp {
public:
 struct Impl {
  lv_obj_t* root=nullptr;lv_obj_t* output=nullptr;lv_obj_t* switchPrompt=nullptr;
  Tab5Tool tool=Tab5Tool::Other;
  bool pythonWasRunning=false;
  uint32_t consoleRevision=0;
  void open(Tab5Tool which,const char*,bool){root=lv_obj_create(lv_screen_active());output=lv_label_create(root);tool=which;order.push_back("open");}
  void scriptUI(){order.push_back("script");}
  void close(bool){lv_obj_delete(root);root=output=nullptr;order.push_back("close");}
  void note(const char*){}
 } state,*impl_=&state;
 bool resumePythonApp();
 lv_obj_t* scope(){return state.root;}
 void service(){auto* a=&state;if(!a->root)return;/* PYTHON_SERVICE */}
} Tab5Tools;
class Tab5WritingApp {
public:
 struct Impl {
  uint16_t fontSize=24;
  bool built=false,visible=false,split=false;
  writing::RichView view,refView;
  lv_obj_t *mainScroll=nullptr,*refScroll=nullptr;
 } state,*impl_=&state;
 uint16_t textSize_=24;
 void setTextSize(uint16_t pixels);
 void leaveEditing(){state.view.blur();order.push_back("leave");}
} Tab5Writing;
/* TEXT_SIZE */
/* RESUME_TOOLS */
/* RESUME_OS */

struct Editor {
 struct Hooks {
  std::function<bool(const char*)> run;
  std::function<void()> resume;
 } hooks;
 writing::Document document;
 String path="/MicroPython/test.py";
 bool codeDraft=false,consoleVisible=false,canSave=true,saveDirty=false;
 void note(const char*){order.push_back("notice");}
 bool save(const char*,bool){order.push_back("save");if(canSave&&!saveDirty)document.markSaved(document.revision());return canSave;}
 void layout(){order.push_back("layout");}
 void serviceConsole(uint32_t,bool){order.push_back("console");}
 /* RUN_CODE */
};
static unsigned fonts=0;
static const lv_font_t* font(uint16_t pixels,uint8_t){++fonts;return pixels<=20?&lv_font_montserrat_20:&lv_font_montserrat_24;}
int main(){
 lv_init();lv_display_create(1280,720);
 Editor editor;editor.document.assign("print('hello')",14);
 editor.hooks.run=[](const char*){order.push_back("run");Tab5Python::active=true;return true;};
 editor.hooks.resume=resumePythonApp;
 editor.runCode();
 check(Tab5Tools.scope()!=nullptr,"editor run opens foreground automatically");
 check(order==std::vector<std::string>({"save","run","layout","console","leave","keyboard","open","script","reader","focus"}),"foreground switch follows editor layout and leaves editing before focus");
 check(activePage==Page::Writing,"launch keeps original editor underneath");
 check(!keyboardVisible&&!keyboardDockFocus&&keyboardUiNavigation,"keyboard hidden and UI navigation active");
 check(Tab5Tools.state.pythonWasRunning,"foreground tracks running state immediately");
 Tab5Python::active=false;Tab5Tools.service();
 check(Tab5Tools.scope()==nullptr&&activePage==Page::Writing,"fast normal completion dismisses app and returns to editor");
 for(int mode=0;mode<5;++mode){
  order.clear();Editor failed;failed.document.assign("x",1);failed.hooks=editor.hooks;
  if(mode==0)failed.path="/Documents/text.md";
  if(mode==1)failed.canSave=false;
  if(mode==2){failed.saveDirty=true;failed.document.replace(0,1,"y",1,0,1);}
  if(mode==3)failed.hooks.run=[](const char*){order.push_back("run");return false;};
  if(mode==4){std::string big(65537,'x');failed.document.assign(big.c_str(),big.size());}
  failed.runCode();check(Tab5Tools.scope()==nullptr,"failed run never changes foreground");
  check(std::find(order.begin(),order.end(),"keyboard")==order.end(),"failure preserves keyboard/focus");
 }
 Tab5Python::active=true;activePage=Page::Files;order.clear();resumePythonApp();
 check(activePage==Page::Files&&std::find(order.begin(),order.end(),"leave")==order.end(),"toolbar resume preserves noneditor page");
 Tab5Python::active=false;Tab5Tools.service();check(activePage==Page::Files&&Tab5Tools.scope()==nullptr,"completion returns to original menu");
 Tab5Python::active=true;Tab5PythonReader::opened=true;order.clear();resumePythonApp();
 check(std::find(order.begin(),order.end(),"focus")==order.end(),"reader foreground keeps its own focus");
 Tab5Python::active=false;Tab5Tools.service();Tab5PythonReader::opened=false;
 order.clear();resumePythonApp();check(order.empty(),"resume without running VM is inert");

 // The exact OS text-size setter executes against real Writer RichView and LVGL.
 Tab5WritingApp sizing;sizing.impl_=nullptr;sizing.setTextSize(32);
 check(sizing.textSize_==32,"setting can be stored before editor allocation");
 auto& s=Tab5Writing.state;Tab5Writing.setTextSize(28);
 check(s.fontSize==28&&fonts==0,"unbuilt editor stores setting without loading fonts");
 s.mainScroll=lv_obj_create(lv_screen_active());s.refScroll=lv_obj_create(lv_screen_active());
 lv_obj_set_size(s.mainScroll,500,200);lv_obj_set_size(s.refScroll,500,200);lv_obj_set_x(s.refScroll,600);
 s.view.begin(s.mainScroll);s.refView.begin(s.refScroll,true);s.view.font=font;s.refView.font=font;
 std::string text;for(int i=0;i<120;++i)text+="A paragraph with enough words to wrap and preserve a selected fragment.\n";
 s.view.load(text);s.refView.load(text);s.built=s.visible=s.split=true;
 s.view.move(22);s.view.move(40,true);s.refView.move(8);s.refView.move(17,true);
 auto selected=s.view.selected(),refSelected=s.refView.selected();
 writing::Document document;document.assign(text.c_str(),text.size());document.replace(0,1,"B",1,0,1);const auto history=document.historyBytes();
 lv_obj_update_layout(lv_screen_active());lv_obj_scroll_to_y(s.mainScroll,120,LV_ANIM_OFF);lv_obj_scroll_to_y(s.refScroll,80,LV_ANIM_OFF);
 const int top=lv_obj_get_scroll_y(s.mainScroll),refTop=lv_obj_get_scroll_y(s.refScroll);auto layouts=s.view.metrics().layouts;
 Tab5Writing.setTextSize(34);
 check(s.view.fontSize==34&&s.refView.fontSize==34&&s.view.metrics().layouts==layouts+1,"setting reflows both real document panes");
 check(s.view.selected()==selected&&s.refView.selected()==refSelected&&s.view.caret==40&&s.view.anchor==22,"selection and cursor preserved");
 check(lv_obj_get_scroll_y(s.mainScroll)==top&&lv_obj_get_scroll_y(s.refScroll)==refTop,"scroll positions preserved");
 check(document.historyBytes()==history&&document.canUndo(),"document undo state untouched");
 s.fontSize=s.view.fontSize=s.refView.fontSize=30;s.view.reflow(false);layouts=s.view.metrics().layouts;
 Tab5Writing.setTextSize(34);check(s.view.fontSize==30&&s.view.metrics().layouts==layouts,"unchanged settings preserve local editor zoom");
 s.visible=false;s.view.discardLayout();s.refView.discardLayout();layouts=s.view.metrics().layouts;auto beforeFonts=fonts;
 Tab5Writing.setTextSize(20);check(s.fontSize==20&&fonts==beforeFonts&&s.view.metrics().layouts==layouts,"hidden editor applies preference with no fonts or layout allocation");
 s.visible=true;s.view.resumeLayout();s.refView.resumeLayout();
 check(s.view.metrics().layouts==layouts+1&&s.view.selected()==selected,"visible return reflows new size and preserves selection");
 Tab5Writing.setTextSize(1);check(s.fontSize==18,"minimum text-size bound");Tab5Writing.setTextSize(200);check(s.fontSize==36,"maximum text-size bound");
 std::printf("PASS %u controller/layout checks; actual Writer/LVGL; simulated VM and window boundaries\n",checks);
 return 0;
}
