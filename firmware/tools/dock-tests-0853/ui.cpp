// Production widgets/controllers; only peripheral/catalogue and app boundaries are substituted.
#include <lvgl.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <string>
#include <vector>
#include "Tab5OSTypes.h"
#include "Tab5Icons.h"
#include "Tab5Focus.h"
#include "Tab5UiStyle.h"
#include "Tab5AppCatalogue.h"
#include "Tab5TopBarLayout.h"
#include "Tab5ShellLayout.h"
#include "WriterUsb.h"

namespace {
unsigned checks=0, flushes=0, dispatches=0, scanCancels=0, iconAttempts=0;
uint32_t now=1000;
lv_display_t* display=nullptr;
lv_font_t* font=nullptr;lv_font_t* headingFont=nullptr;
std::vector<unsigned char> fontBytes, framebuffer(1280*720*3), drawbuf(1280*48*3);
std::string toastText, launchedPath;
unsigned launchedMenu=0;
void check(bool value,const char* why){++checks;if(!value){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
}
namespace writing {
std::deque<MouseReport> reports;
bool usbMouseConnected(){return true;}
bool usbMousePoll(MouseReport& r){if(reports.empty())return false;r=reports.front();reports.pop_front();return true;}
}
#include "Tab5Mouse.cpp"

namespace Tab5AppCatalogue {
std::vector<App> apps;
std::vector<uint64_t> customIcons;
Choices config;
uint32_t generation=1;
bool allAppsRequested=false;
unsigned count(){return apps.size();}
const App* at(unsigned index){return index<apps.size()?&apps[index]:nullptr;}
unsigned ordered(unsigned* indices,bool onlyPinned){allAppsRequested=!onlyPinned;for(unsigned i=0;i<apps.size();++i)indices[i]=i;return apps.size();}
uint8_t icon(unsigned index){return index<apps.size()&&apps[index].key==keyFor("/Applications/Custom.app.elf")?16:0;}
void endScan(){++scanCancels;}
bool attachIcon(lv_obj_t* container,unsigned index){
 ++iconAttempts;const auto* app=at(index);
 if(!app||std::find(customIcons.begin(),customIcons.end(),app->key)==customIcons.end())return false;
 // Successful ICO decode is the boundary: model its documented replacing ownership.
 lv_obj_clean(container);auto* mark=lv_label_create(container);lv_label_set_text(mark,LV_SYMBOL_IMAGE);
 lv_obj_set_style_text_font(mark,&lv_font_montserrat_32,0);lv_obj_center(mark);return true;
}
uint32_t revision(){return generation;}
bool addBuiltin(const char* id,const char* title,uint8_t action,uint8_t symbol){
 App app;snprintf(app.id,sizeof(app.id),"%s",id);snprintf(app.title,sizeof(app.title),"%s",title);
 app.key=keyFor(id);app.action=action;app.symbol=symbol;apps.push_back(app);return true;
}
void add(const char* path,const char* title,Kind kind){addBuiltin(path,title,0,0);apps.back().kind=kind;}
}

static constexpr int SCREEN_W=1280,SCREEN_H=720,PAGE_H=672,TOP_H=48,DOCK_W=64;
static constexpr unsigned DOCK_BUTTON_COUNT=Tab5Shell::DockCount,SETTINGS_SECTION_COUNT=5;
static int WORK_W=1216,MAP_VIEW_W=1200;
static struct {uint8_t cornerRadius=7;} settings;
enum class Page:uint8_t{Home,Files,Editor,Writing,Preview,Audio,Map,Settings,ImageEditor};
static Page activePage=Page::Home;
static HomeAction activeTool=HomeAction::Home;
static uint64_t dockLaunchedKey=0;
static lv_obj_t *root=nullptr,*dock=nullptr,*pageHost=nullptr,*homePage=nullptr,*settingsPage=nullptr;
static lv_obj_t *topHomeButton=nullptr,*topPythonResume=nullptr,*titleLabel=nullptr,*dateLabel=nullptr,*topAudioIndicator=nullptr;
static lv_obj_t *sidebarSettingsOverlay=nullptr,*homeDefaultGrid=nullptr,*mapCanvas=nullptr;
static lv_obj_t *dockButtons[DOCK_BUTTON_COUNT]{},*settingsSectionButtons[SETTINGS_SECTION_COUNT]{};
static uint32_t mapGeneration=1;
static bool keyboardVisible=false,keyboardDockFocus=false,keyboardUiNavigation=true;
static bool homeCustomize=false,homeAppsScanning=false,homeAppsDirty=false;
static unsigned homeMenu=0;
static struct ToolBoundary {
 lv_obj_t* object=nullptr; bool python=false;
 lv_obj_t* scope(){return object;}
 bool openPythonApp(const char* path){++dispatches;python=true;launchedPath=path;object=homePage;return true;}
 void launchExecutable(const char* path){++dispatches;python=false;launchedPath=path;object=homePage;}
 void resize(int){}
} Tab5Tools;
static struct {void resize(int){}} Tab5Writing;
static struct {void beginGeneration(uint32_t){}} Tab5Map;
namespace Tab5GitView {lv_obj_t* scope(){return nullptr;}}
namespace Tab5MeshBadge {lv_obj_t* object(){return nullptr;}}
static void freeMapCanvas(){}
static void renderMapViewport(bool){}
static int keyboardHeight(){return 220;}
static void setKeyboardVisible(bool b){keyboardVisible=b;}
static void homeRequestRender(){}
static void toast(const char* message,unsigned){toastText=message;}
static void homeOpenMenu(unsigned menu){launchedMenu=menu;}
static void homeOpenCatalogue(bool){++dispatches;}
static void openHomeAction(HomeAction action){++dispatches;activeTool=action;dockLaunchedKey=0;}
static const char* T(const char* text){return text;}
static const lv_font_t* uiFont(){return font;}
static const lv_font_t* uiHeadingFont(){return headingFont;}
static lv_color_t C_BG(){return lv_color_hex(Tab5UiStyle::color(0));}
static lv_color_t C_SURFACE(){return lv_color_hex(Tab5UiStyle::color(1));}
static lv_color_t C_RAISED(){return lv_color_hex(Tab5UiStyle::color(2));}
static lv_color_t C_BORDER(){return lv_color_hex(Tab5UiStyle::color(3));}
static lv_color_t C_TEXT(){return lv_color_hex(Tab5UiStyle::color(4));}
static lv_color_t C_MUTED(){return lv_color_hex(Tab5UiStyle::color(5));}
static lv_color_t C_ACTIVE(){return lv_color_hex(Tab5UiStyle::color(6));}
static void shellRefreshDock();
static void shellRefreshSidebar();
static void shellDockPositions();
static void shellLaunchDock(unsigned);
static void updateNavState();
static void dockEvent(lv_event_t*);
static const Tab5AppCatalogue::App* shellAppForKey(uint64_t);
#include "helpers.inc"
#include "home.inc"
#include "main.inc"
#include "Tab5ShellDock.h"
#include "Tab5ShellUi.h"

namespace {
void paint(){lv_obj_update_layout(root);lv_obj_update_layout(lv_layer_top());lv_refr_now(display);}
void shot(const std::string& name){paint();std::ofstream out(name,std::ios::binary);out<<"P6\n1280 720\n255\n";for(size_t i=0;i<framebuffer.size();i+=3){out.put(framebuffer[i+2]);out.put(framebuffer[i+1]);out.put(framebuffer[i]);}}
lv_point_t center(lv_obj_t* object){lv_area_t a;lv_obj_get_coords(object,&a);return{(a.x1+a.x2)/2,(a.y1+a.y2)/2};}
void mouse(int x,int y,uint8_t buttons=0){writing::MouseReport r{};r.x=x-Tab5Mouse::x;r.y=y-Tab5Mouse::y;r.buttons=buttons;writing::reports.push_back(r);Tab5Mouse::service();paint();}
void click(lv_obj_t* object){lv_obj_scroll_to_view_recursive(object,LV_ANIM_OFF);paint();const auto p=center(object);mouse(p.x,p.y,1);mouse(p.x,p.y,0);}
bool key(writing::Key value,lv_obj_t* scope){Tab5Focus.keyboardMode();Tab5UiStyle::keyboardMode();writing::KeyEvent e{};e.key=value;const bool result=Tab5Focus.key(e,scope);paint();return result;}
lv_obj_t* picker(uint64_t wanted){for(unsigned i=0;i<lv_obj_get_child_count(sidebarItems);++i){auto* b=lv_obj_get_child(sidebarItems,i);if(!lv_obj_check_type(b,&lv_button_class))continue;auto* p=static_cast<uint64_t*>(lv_obj_get_user_data(b));if(p&&*p==wanted)return b;}return nullptr;}
void setChoices(std::initializer_list<const char*> ids){Tab5Shell::choices=Tab5Shell::Choices{};while(Tab5Shell::choices.count)Tab5Shell::choices.remove(0);for(const char* id:ids)check(Tab5Shell::choices.add(Tab5AppCatalogue::keyFor(id),Tab5Shell::capacity(PAGE_H)),"test setup choice added");shellRefreshDock();applyShellLayout();paint();}
unsigned objectCount(lv_obj_t* object){unsigned result=1;for(unsigned i=0;i<lv_obj_get_child_count(object);++i)result+=objectCount(lv_obj_get_child(object,i));return result;}
void populate(){
 const char* ids[]={"files","editor","reader","audio","map","calculator","settings","terminal","alarm","sdr","image-editor","meshtastic"};
 const char* titles[]={"Explorateur","Éditeur","eBook","Média","Cartes","Calculatrice","Réglages","Terminal","Réveil","RTL-SDR","Dessin","Meshtastic"};
 const HomeAction actions[]={HomeAction::Files,HomeAction::Editor,HomeAction::Reader,HomeAction::Audio,HomeAction::Map,HomeAction::Calculator,HomeAction::Settings,HomeAction::Terminal,HomeAction::Alarm,HomeAction::Sdr,HomeAction::ImageEditor,HomeAction::Meshtastic};
 const uint8_t symbols[]={1,2,10,3,4,11,5,12,16,3,7,15};
 for(unsigned i=0;i<12;++i)Tab5AppCatalogue::addBuiltin(ids[i],titles[i],uint8_t(actions[i]),symbols[i]);
 Tab5AppCatalogue::addBuiltin("elf-apps","Apps ELF",254,6);Tab5AppCatalogue::addBuiltin("python-apps","Apps Python",253,24);
 Tab5AppCatalogue::add("/Applications/Game.app.elf","Morpion",Tab5AppCatalogue::Kind::Executable);
 Tab5AppCatalogue::add("/Applications/Custom.app.elf","Custom icon",Tab5AppCatalogue::Kind::Executable);
 Tab5AppCatalogue::add("/Python/Demo.py","Horloge",Tab5AppCatalogue::Kind::Script);
 Tab5AppCatalogue::add("/Python/Long.py","Application dont le nom est volontairement très long pour contrôler les limites du sélecteur",Tab5AppCatalogue::Kind::Script);
}
}
int main(int argc,char** argv){
 check(argc==3,"font and evidence directory provided");
 std::ifstream input(argv[1],std::ios::binary);fontBytes.assign(std::istreambuf_iterator<char>(input),{});check(!fontBytes.empty(),"real delivered OS font loaded");
 lv_init();lv_tick_set_cb([](){return now;});
 display=lv_display_create(SCREEN_W,SCREEN_H);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);
 lv_display_set_buffers(display,drawbuf.data(),nullptr,drawbuf.size(),LV_DISPLAY_RENDER_MODE_PARTIAL);
 lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t* a,uint8_t* data){++flushes;size_t bytes=(a->x2-a->x1+1)*3,stride=lv_draw_buf_width_to_stride(a->x2-a->x1+1,LV_COLOR_FORMAT_RGB888);for(int y=a->y1;y<=a->y2;++y)memcpy(framebuffer.data()+(size_t(y)*SCREEN_W+a->x1)*3,data+size_t(y-a->y1)*stride,bytes);lv_display_flush_ready(d);});
 font=lv_tiny_ttf_create_data(fontBytes.data(),fontBytes.size(),20);check(font,"OS font created");font->fallback=&lv_font_montserrat_20;headingFont=lv_tiny_ttf_create_data(fontBytes.data(),fontBytes.size(),24);check(headingFont,"heading font created");headingFont->fallback=&lv_font_montserrat_24;
 root=lv_screen_active();lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_text_font(root,font,0);
 pageHost=lv_obj_create(root);lv_obj_set_size(pageHost,1216,PAGE_H);lv_obj_set_pos(pageHost,64,TOP_H);lv_obj_remove_style_all(pageHost);
 homePage=lv_obj_create(pageHost);settingsPage=lv_obj_create(pageHost);lv_obj_remove_flag(homePage,LV_OBJ_FLAG_SCROLLABLE);
 topHomeButton=lv_button_create(root);lv_obj_set_size(topHomeButton,52,40);lv_obj_set_pos(topHomeButton,6,4);
 Tab5Focus.begin(lv_layer_top(),font);Tab5Focus.theme(C_ACTIVE(),C_TEXT(),C_SURFACE());
 createDock();check(lv_obj_has_state(dockButtons[1],LV_STATE_DISABLED),"initial unresolved native shortcut is unavailable safely");
 populate();shellCatalogueChanged();
 check(!lv_obj_has_state(dockButtons[1],LV_STATE_DISABLED),"initial catalogue publication resolves native shortcuts");
 Tab5Mouse::begin(display);Tab5Mouse::service();paint();
 check(Tab5Shell::capacity(PAGE_H)==10,"actual screen supports ten 56px targets without scrolling");
 setChoices({"files","editor","calculator","settings","alarm","image-editor","meshtastic","/Applications/Game.app.elf","/Python/Demo.py","/Applications/Custom.app.elf"});
 check(!lv_obj_has_flag(dock,LV_OBJ_FLAG_SCROLLABLE),"dock has no scroll surface");
 for(unsigned i=0;i<10;++i){lv_area_t b,d;lv_obj_get_coords(dockButtons[i],&b);lv_obj_get_coords(dock,&d);check(b.y1>=d.y1&&b.y2<=d.y2&&b.x1>=d.x1&&b.x2<=d.x2,"every slot fits dock rectangle");check(!lv_obj_has_flag(dockButtons[i],LV_OBJ_FLAG_HIDDEN),"all ten slots visible");}
 for(unsigned i=0;i<7;++i)check(lv_obj_get_child_count(dockButtons[i])==1,"native dock preserves icon-only presentation");
 check(lv_obj_get_child_count(dockButtons[7])==2,"external default has short caption");
 check(lv_obj_get_child_count(dockButtons[9])==1,"personalized external icon has no overlapping caption");
 check(!strcmp(lv_label_get_text(lv_obj_get_child(dockButtons[7],1)),"Mor"),"external fallback caption shows readable prefix instead of ellipsis");char utf8[16];shellDockCaption(utf8,"Éditeur");check(!strcmp(utf8,"Édi"),"caption retains complete accented UTF-8 characters");shot(std::string(argv[2])+"/dock-ten-light.ppm");
 const uint64_t elf=Tab5AppCatalogue::keyFor("/Applications/Game.app.elf"),py=Tab5AppCatalogue::keyFor("/Python/Demo.py");
 const unsigned before=dispatches;auto p=center(dockButtons[7]);mouse(p.x,p.y);check(dispatches==before,"hover does not launch app");
 check(lv_obj_has_state(dockButtons[7],LV_STATE_HOVERED),"pointer highlights external dock button");
 std::reverse(Tab5AppCatalogue::apps.begin(),Tab5AppCatalogue::apps.end());
 click(dockButtons[7]);check(launchedPath=="/Applications/Game.app.elf"&&!Tab5Tools.python,"mouse launch resolves stable ELF key after reorder");
 Tab5Tools.object=nullptr;Tab5Focus.focusObject(dockButtons[8]);check(key(writing::Key::Enter,root),"keyboard activates dock shortcut");check(launchedPath=="/Python/Demo.py"&&Tab5Tools.python,"keyboard launches correct Python path");
 Tab5Tools.object=nullptr;
 auto it=std::find_if(Tab5AppCatalogue::apps.begin(),Tab5AppCatalogue::apps.end(),[&](const auto& app){return app.key==elf;});const auto saved=*it;Tab5Focus.focusObject(dockButtons[7]);Tab5AppCatalogue::apps.erase(it);shellCatalogueChanged();check(Tab5Focus.focused()&&!lv_obj_has_state(Tab5Focus.focused(),LV_STATE_DISABLED),"removed focused app restores an enabled dock target");
 check(Tab5Shell::choices.keys[7]==elf&&lv_obj_has_state(dockButtons[7],LV_STATE_DISABLED),"removed media retains shortcut and disables unavailable launch");check(key(writing::Key::Tab,dock),"Tab continues after focused app disappears");check(!lv_obj_has_state(Tab5Focus.focused(),LV_STATE_DISABLED),"Tab skips unavailable shortcut");unsigned continued=dispatches;check(key(writing::Key::Enter,dock)&&dispatches==continued+1,"Enter remains functional after media removal");
 const unsigned stopped=dispatches;shellLaunchDock(7);check(dispatches==stopped&&toastText=="Application indisponible","missing key cannot launch a reused catalogue index");
 Tab5AppCatalogue::apps.push_back(saved);shellCatalogueChanged();check(!lv_obj_has_state(dockButtons[7],LV_STATE_DISABLED),"remount restores same key without replacing preferences");
 Tab5AppCatalogue::customIcons.push_back(elf);const unsigned startIcon=iconAttempts;shellServiceDockIcon();check(iconAttempts==startIcon+1,"one catalogue icon load per service pass");check(lv_obj_has_flag(lv_obj_get_child(dockButtons[7],1),LV_OBJ_FLAG_HIDDEN),"decoded external icon hides fallback caption");
 click(dockButtons[4]);check(activeTool==HomeAction::Alarm,"native action available via dock");
 Tab5Tools.object=nullptr;activePage=Page::Settings;
 showSidebarSettings();paint();check(lv_obj_has_state(sidebarAdd,LV_STATE_DISABLED),"full dock disables add affordance");
 auto* row=lv_obj_get_child(sidebarItems,9);click(lv_obj_get_child(row,3));check(Tab5Shell::choices.count==9,"mouse removes selected shortcut");
 click(sidebarAdd);check(sidebarPicking&&Tab5AppCatalogue::allAppsRequested,"picker requests all installed apps including unpinned");
 auto* pythonRow=picker(Tab5AppCatalogue::keyFor("/Python/Long.py"));check(pythonRow,"unselected Python app is available");paint();check(lv_obj_get_width(lv_obj_get_child(pythonRow,0))==990&&lv_obj_get_height(lv_obj_get_child(pythonRow,0))==lv_font_get_line_height(font),"long picker caption stays on one bounded line");
 Tab5Focus.focusObject(pythonRow);std::reverse(Tab5AppCatalogue::apps.begin(),Tab5AppCatalogue::apps.end());shellCatalogueChanged();
 check(Tab5Focus.focused()==picker(Tab5AppCatalogue::keyFor("/Python/Long.py")),"catalogue refresh preserves keyboard picker focus by key");
 shot(std::string(argv[2])+"/picker-light.ppm");
 check(key(writing::Key::Enter,sidebarSettingsOverlay),"Enter activates focused picker row");
 check(Tab5Shell::choices.count==10&&Tab5Shell::choices.keys[9]==Tab5AppCatalogue::keyFor("/Python/Long.py"),"picker inserts current stable Python key");
 const auto full=Tab5Shell::choices;sidebarAddKey(Tab5AppCatalogue::keyFor("/Python/Other.py"));check(!memcmp(&full,&Tab5Shell::choices,sizeof(full)),"unavailable app cannot modify full preferences");
 sidebarAddKey(Tab5AppCatalogue::keyFor("reader"));check(!memcmp(&full,&Tab5Shell::choices,sizeof(full))&&toastText.find("Bandeau complet")==0,"eleventh available shortcut rejected without data loss");
 row=lv_obj_get_child(sidebarItems,9);click(lv_obj_get_child(row,1));check(Tab5Shell::choices.keys[8]==full.keys[9],"mouse moves shortcut upward with key identity");
 check(preferences_test::blobs.count("apps-v2"),"UI changes save versioned preferences");
 closeSidebarSettings();check(!sidebarSettingsOverlay&&!sidebarItems,"close clears overlay handles");
 Tab5Shell::choices.remove(9);shellRefreshDock();showSidebarSettings();click(sidebarAdd);
 auto* nativeRow=picker(Tab5AppCatalogue::keyFor("reader"));check(nativeRow,"unselected native is offered by picker");click(nativeRow);check(Tab5Shell::choices.count==10&&Tab5Shell::choices.keys[9]==Tab5AppCatalogue::keyFor("reader"),"mouse picks a native app");closeSidebarSettings();
 Tab5Shell::choices.remove(9);shellRefreshDock();showSidebarSettings();click(sidebarAdd);
 auto* elfRow=picker(Tab5AppCatalogue::keyFor("/Applications/Custom.app.elf"));check(elfRow,"unselected ELF is offered by picker");click(elfRow);check(Tab5Shell::choices.count==10&&Tab5Shell::choices.keys[9]==Tab5AppCatalogue::keyFor("/Applications/Custom.app.elf"),"mouse picks an ELF app");closeSidebarSettings();
 Tab5Shell::choices=full;shellRefreshDock();
 const unsigned objects=objectCount(root);
 for(unsigned n=0;n<30;++n){showSidebarSettings();Tab5Shell::choices.remove(9);shellRefreshSidebar();click(sidebarAdd);check(sidebarPicking,"repeated picker opens");shellRefreshSidebar();closeSidebarSettings();Tab5Shell::choices=full;shellRefreshDock();}
 check(objectCount(root)==objects,"repeated picker and dock cycles leave stable LVGL object count");
 const uint32_t dark[]={0x101214,0x1c2024,0x292f34,0x63717a,0xf3f4f5,0xaab4bc,0x507b9a,0xffffff,0x365b78};Tab5UiStyle::configure(dark,7);lv_obj_set_style_bg_color(root,C_BG(),0);shellRefreshDock();Tab5UiStyle::tree(root);shot(std::string(argv[2])+"/dock-ten-dark.ppm");
 showSidebarSettings();Tab5Shell::choices.remove(9);shellRefreshSidebar();click(sidebarAdd);shot(std::string(argv[2])+"/picker-dark.ppm");closeSidebarSettings();
 setChoices({});activePage=Page::Settings;shellTopLayout();check(lv_obj_has_flag(dock,LV_OBJ_FLAG_HIDDEN)&&!lv_obj_has_flag(topHomeButton,LV_OBJ_FLAG_HIDDEN),"empty dock preserves top Home access");
 setChoices({"home","files"});activePage=Page::Home;shellDockPositions();check(lv_obj_has_flag(dockButtons[0],LV_OBJ_FLAG_HIDDEN),"Home hidden only on actual main home");
 homeMenu=1;shellDockPositions();check(!lv_obj_has_flag(dockButtons[0],LV_OBJ_FLAG_HIDDEN),"Home shortcut visible from ELF/Python libraries");
 paint();unsigned idle=flushes;for(unsigned i=0;i<100;++i){now+=16;Tab5Mouse::service();paint();}check(flushes==idle,"idle widgets introduce no redraw loop");
 Tab5Focus.clear();Tab5UiStyle::pointerLeave();lv_obj_clean(root);check(objectCount(root)==1,"root teardown retires all widget subtrees");
 printf("{\"passed\":true,\"checks\":%u,\"cycles\":30,\"dockCapacity\":10,\"flushes\":%u,\"launches\":%u,\"catalogueIconLoads\":%u}\n",checks,flushes,dispatches,iconAttempts);
}
