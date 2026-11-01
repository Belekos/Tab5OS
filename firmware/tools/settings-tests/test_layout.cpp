#include <lvgl.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include "Tab5SettingsLayout.h"
#include "Tab5ShellLayout.h"
#include "Tab5SerialCore.h"
namespace Tab5Serial{Config config(){return {};}Stats stats(){return {};}}
#include "settings-translations.inc"
// Only text history is unrelated to these Settings widgets; real global styles
// are included and switch visuals are resolved by LVGL itself.
#define TAB5_TEXT_INPUT_H
namespace Tab5TextInput{void track(lv_obj_t*){}}
#include "Tab5UiStyle.h"
// Tab5UiStyle includes the history declarations; its track symbol above is the
// only text-input boundary referenced by the native-control style function.
#include "Tab5UiStyle.cpp"
class String:public std::string{public:using std::string::string;String(const std::string& s):std::string(s){} };
static int WORK_W=1216;static constexpr int PAGE_H=672,SETTINGS_SECTION_COUNT=4;
static lv_obj_t* pageHost=nullptr,*settingsPage=nullptr;
static lv_obj_t* settingsSectionButtons[4]{},*settingsSectionPanels[4]{};
static uint8_t activeSettingsSection=0;static bool settingsStorageMounted=false;
static lv_obj_t *brightnessValue=nullptr,*brightnessSlider=nullptr,*fontValue=nullptr,*fontPreview=nullptr,*themeDropdown=nullptr;
static lv_obj_t *autosaveSwitch=nullptr,*autosaveDelayValue=nullptr,*autosaveDelaySlider=nullptr,*rtcValue=nullptr,*timeSetButton=nullptr,*ntpSwitch=nullptr,*timeZoneDropdown=nullptr;
static lv_obj_t *wifiPowerSwitch=nullptr,*wifiSettingsStatus=nullptr,*languageDropdown=nullptr,*keyboardLayoutDropdown=nullptr,*keyboardLanguageSwitch=nullptr,*documentsRootButton=nullptr;
static lv_obj_t *fileShareLanButton=nullptr,*fileShareDirectButton=nullptr,*fileShareStopButton=nullptr,*fileShareStatusLabel=nullptr;
static lv_obj_t *settingsPadLabel=nullptr,*settingsPadConfigure=nullptr,*settingsPadCancel=nullptr;
static struct{struct{unsigned speed=100,scroll=3;bool leftHanded=false,natural=false;}mouse;unsigned screenSleepIndex=1,brightness=200,cornerRadius=12,autosaveDelaySec=5,timeZoneIndex=0;bool wifiPowerSave=false,batteryInstalled=true,autosave=true,ntpEnabled=true;String documentsRoot="/Documents";}settings;
static struct{bool wifiEnabled(){return true;}}Tab5Audio;
static bool fallback=false;
static struct{size_t count(){return languageCount;}const char* name(size_t){return "English / Français";}unsigned selectedIndex(){return 0;}bool fontFallbackActive(){return fallback;}}Tab5Locale;
static struct{size_t count(){return 2;}const char* name(size_t i){return i?"Français AZERTY":"English QWERTY";}unsigned selectedIndex(){return 0;}bool automatic(){return true;}}Tab5Keyboards;
static struct{void describe(lv_obj_t*,const char*){}}Tab5Focus;
static const char* T(const char* s){auto it=translations.find(s);return it==translations.end()?s:it->second.c_str();}
static lv_font_t* font=nullptr;
static const lv_font_t* uiFont(){return font;}static const lv_font_t* uiHeadingFont(){return font;}static const lv_font_t* documentFont(){return font;}
namespace Tab5DocumentText{void apply(lv_obj_t* object,const lv_font_t* f){lv_obj_set_style_text_font(object,f,0);}}
static lv_color_t C_BG(){return lv_color_hex(Tab5UiStyle::color(0));}static lv_color_t C_SURFACE(){return lv_color_hex(Tab5UiStyle::color(1));}
static lv_color_t C_RAISED(){return lv_color_hex(Tab5UiStyle::color(2));}static lv_color_t C_BORDER(){return lv_color_hex(Tab5UiStyle::color(3));}
static lv_color_t C_TEXT(){return lv_color_hex(Tab5UiStyle::color(4));}static lv_color_t C_MUTED(){return lv_color_hex(Tab5UiStyle::color(5));}static lv_color_t C_ACTIVE(){return lv_color_hex(Tab5UiStyle::color(6));}
static void refreshThemeChoices(){lv_dropdown_set_options(themeDropdown,"Graphite\nAmbre\nSauge");}
static void refreshTimeSettingsUi(){}static void updateClockLabels(bool){lv_label_set_text(rtcValue,"15/10/2026 19:45:12");}
static void refreshWifiSettingsUi(bool){lv_label_set_text(wifiSettingsStatus,T("Connecté"));}
static void updateFileShareUi(uint32_t,bool){lv_obj_add_flag(fileShareStatusLabel,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(fileShareStopButton,LV_OBJ_FLAG_HIDDEN);}
static void setButtonEnabled(lv_obj_t* object,bool value){if(value)lv_obj_remove_state(object,LV_STATE_DISABLED);else lv_obj_add_state(object,LV_STATE_DISABLED);}
static uint32_t millis(){return 0;}
static void showSettingsSection(uint8_t section){for(unsigned i=0;i<4;++i){if(i==section)lv_obj_remove_flag(settingsSectionPanels[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(settingsSectionPanels[i],LV_OBJ_FLAG_HIDDEN);}}
static struct{const char* label;}TIME_ZONES[]={{"UTC"},{"Paris"}};static constexpr size_t TIME_ZONE_COUNT=2;
static lv_obj_t* makeButton(lv_obj_t*,const char*,int,int);
static lv_obj_t* makeIconButton(lv_obj_t* p,const char* symbol,int w,int h){return makeButton(p,symbol,w,h);}
#include "settings-production.inc"
static unsigned checks=0;static unsigned lang=0;
static void check(bool result,const char* what){if(!result){fprintf(stderr,"FAIL language%u width%d: %s\n",lang,WORK_W,what);exit(1);}++checks;}
static bool visible(lv_obj_t* o){return !lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN);}
static void geometry(lv_obj_t* parent){
 lv_area_t outer;lv_obj_get_content_coords(parent,&outer);
 for(unsigned i=0;i<lv_obj_get_child_count(parent);++i){auto* child=lv_obj_get_child(parent,i);if(!visible(child))continue;
  lv_area_t bounds;lv_obj_get_coords(child,&bounds);
  // A vertically scrolling page can contain rows below the viewport. Every
  // horizontal edge, and all children of non-scrolling rows/cards, must fit.
  bool horizontal=bounds.x1>=outer.x1-1&&bounds.x2<=outer.x2+1;
  if(!horizontal){const char* text=lv_obj_check_type(child,&lv_label_class)?lv_label_get_text(child):"<widget>";fprintf(stderr,"bounds %d..%d parent %d..%d text %s\n",bounds.x1,bounds.x2,outer.x1,outer.x2,text);}
  check(horizontal,"control or translated label fits horizontal parent content");
  if(!lv_obj_has_flag(parent,LV_OBJ_FLAG_SCROLLABLE)){
   if(!(bounds.y1>=outer.y1-1&&bounds.y2<=outer.y2+1))fprintf(stderr,"vertical %d..%d parent %d..%d\n",bounds.y1,bounds.y2,outer.y1,outer.y2);
   check(bounds.y1>=outer.y1-1&&bounds.y2<=outer.y2+1,"non-scrolling row/card encloses every child");
  }
  if(lv_obj_check_type(child,&lv_switch_class)){
   check(lv_obj_get_style_border_width(child,LV_PART_KNOB)==2,"OFF knob has explicit2px border");
   lv_obj_add_state(child,LV_STATE_CHECKED);check(lv_obj_get_style_border_width(child,LV_PART_KNOB)==2,"ON knob has explicit2px border");
   lv_obj_remove_state(child,LV_STATE_CHECKED);
  }
  if(lv_obj_check_type(child,&lv_button_class)||lv_obj_check_type(child,&lv_dropdown_class))check(lv_obj_get_height(child)>=48,"action and choice retains48px target");
  geometry(child);
 }
}
int main(int argc,char** argv){
 check(argc==2,"font path supplied");FILE* file=fopen(argv[1],"rb");check(file!=nullptr,"shipped UI font found");fseek(file,0,SEEK_END);size_t size=ftell(file);rewind(file);std::vector<unsigned char> bytes(size);check(fread(bytes.data(),1,size,file)==size,"complete font read");fclose(file);
 lv_init();lv_display_create(1280,720);font=lv_tiny_ttf_create_data_ex(bytes.data(),bytes.size(),24,LV_FONT_KERNING_NORMAL,192);check(font!=nullptr,"real24px font");
 uint32_t palette[]={0x101010,0x202020,0x303030,0x666666,0xffffff,0xaaaaaa,0x357ec7,0xffffff,0x445566};Tab5UiStyle::configure(palette,12);
 unsigned maximumInterface=0,maximumSystem=0;
 for(lang=0;lang<languageCount;++lang)for(bool missingFont:{false,true}){
  language(lang);fallback=missingFont;WORK_W=1216;Tab5Shell::workspaceWidth=WORK_W;
  pageHost=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(pageHost);lv_obj_set_size(pageHost,WORK_W,PAGE_H);lv_obj_set_style_text_font(pageHost,font,0);
  createSettingsPage();
  for(int width:{1216,1280,1216}){
   WORK_W=width;Tab5Shell::apply(width);lv_obj_set_width(pageHost,width);
   const int tabWidth=(WORK_W-32-14*3)/4;for(unsigned i=0;i<4;++i){lv_obj_set_width(settingsSectionButtons[i],tabWidth);lv_obj_set_x(settingsSectionButtons[i],16+i*(tabWidth+14));}
   for(unsigned section=0;section<3;++section){showSettingsSection(section);lv_obj_update_layout(pageHost);geometry(settingsSectionPanels[section]);
    auto* panel=settingsSectionPanels[section];auto* last=lv_obj_get_child(panel,lv_obj_get_child_count(panel)-1);unsigned extent=lv_obj_get_y(last)+lv_obj_get_height(last);
    if(section==0)maximumInterface=std::max(maximumInterface,extent);if(section==1)maximumSystem=std::max(maximumSystem,extent);
    if(lang<2)printf("layout lang%u w%d section%u extent%u\n",lang,width,section,extent);
    check(extent<1100,"localized content remains bounded and scrollable");
   }
  }
  // Actual status grows a card only when there is something useful to show.
  lv_label_set_text(settingsPadLabel,"USB gamepad - HID - 2DC8:6001/0\nA - START - UP\nReports:123456 / 0 - USB errors:0 / 0\n9 B - 01 00 00 00 80 80 80 80 00");
  showSettingsSection(0);lv_obj_update_layout(pageHost);geometry(settingsSectionPanels[0]);
  check(lv_obj_get_height(lv_obj_get_parent(settingsPadLabel))>180,"connected4line gamepad report grows its card without clipping");
  lv_obj_remove_flag(fileShareStatusLabel,LV_OBJ_FLAG_HIDDEN);lv_label_set_text(fileShareStatusLabel,"Network: Tab5\nhttp://192.0.2.1 - code:123456\nRead/write -15 minutes");showSettingsSection(1);lv_obj_update_layout(pageHost);geometry(settingsSectionPanels[1]);
  lv_obj_delete(pageHost);pageHost=nullptr;settingsPage=nullptr;
 }
 lv_tiny_ttf_destroy(font);
 printf("PASS %u checks; actual Settings creation + real LVGL24px font;14languages,1216/1280px and live resize; max interface%u/system%u; no hardware test.\n",checks,maximumInterface,maximumSystem);return 0;
}
