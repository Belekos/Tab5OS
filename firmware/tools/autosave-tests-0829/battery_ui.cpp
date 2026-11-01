// Exact production battery orchestration with display/sensor boundaries.
// Deliberately provides no editor or filesystem API: accidental forced saving
// from this include is a compile error, not merely a counter expectation.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include "Tab5BatteryGuard.h"
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct lv_obj_t {std::string text;bool deleted=false;};
static std::vector<std::unique_ptr<lv_obj_t>> objects;
static lv_obj_t layer;
static lv_obj_t* lv_layer_top(){return &layer;}
static lv_obj_t* lv_obj_create(lv_obj_t*){objects.emplace_back(new lv_obj_t);return objects.back().get();}
static lv_obj_t* lv_label_create(lv_obj_t* p){return lv_obj_create(p);}
static void lv_obj_delete(lv_obj_t* p){p->deleted=true;}
#define UI_NOOP(name) template<class... T> static void name(T...){}
UI_NOOP(lv_obj_remove_flag) UI_NOOP(lv_obj_set_height) UI_NOOP(lv_obj_set_flex_flow)
UI_NOOP(lv_obj_set_style_pad_all) UI_NOOP(lv_obj_set_style_pad_row) UI_NOOP(lv_obj_set_style_border_width)
UI_NOOP(lv_obj_set_style_radius) UI_NOOP(lv_obj_set_width) UI_NOOP(lv_label_set_long_mode)
UI_NOOP(lv_obj_align) UI_NOOP(lv_obj_set_style_bg_color) UI_NOOP(lv_obj_set_style_bg_opa)
UI_NOOP(lv_obj_set_style_border_color) UI_NOOP(lv_obj_set_style_text_color)
UI_NOOP(lv_obj_set_style_text_font) UI_NOOP(lv_obj_move_foreground)
static int lv_display_get_default(){return 0;}
static int lv_display_get_horizontal_resolution(int){return 1280;}
static int richFontFor(int,int){return 0;}
static int C_RAISED(){return 0;}static int C_ACTIVE(){return 0;}static int C_TEXT(){return 0;}
static const char* T(const char* t){return t;}
#define LV_PCT(x) (x)
enum {LV_OBJ_FLAG_SCROLLABLE,LV_OBJ_FLAG_CLICKABLE,LV_SIZE_CONTENT,LV_FLEX_FLOW_COLUMN,LV_LABEL_LONG_WRAP,LV_ALIGN_TOP_MID,LV_OPA_COVER};
static void setLabelIfChanged(lv_obj_t* p,const char* text){p->text=text;}
static unsigned toasts=0;
static void toast(const char*,uint32_t){++toasts;}
static struct {unsigned wakes=0;void wake(uint32_t){++wakes;}} screenIdle;
namespace Tab5Firmware {unsigned cancelled=0;void cancel(){++cancelled;}}
namespace Tab5GitView {unsigned cancelled=0;void cancel(){++cancelled;}}
namespace Tab5Terminal {unsigned cancelled=0;void stop(){++cancelled;}}
namespace Tab5Vault {unsigned cancelled=0;void cancel(){++cancelled;}}
namespace Tab5FileOperation {unsigned cancelled=0;void cancel(){++cancelled;}}
namespace Tab5BatteryGuard {
 static Result next;static bool confirmed=false;static unsigned attempts=0,off=0;
 Result service(uint32_t){return next;}
 const Result& status(){return next;}
 bool powerOffIfStillCritical(uint32_t){++attempts;if(confirmed)++off;return confirmed;}
}
#include "Tab5BatteryUi.inc"
static void event(Tab5BatteryGuard::Event e,unsigned seconds=0){Tab5BatteryGuard::next.event=e;Tab5BatteryGuard::next.seconds=seconds;serviceBatteryProtection(10000+seconds);}
int main(){
 using E=Tab5BatteryGuard::Event;
 event(E::Low);CHECK(toasts==1);CHECK(!batteryGuardBanner);CHECK(screenIdle.wakes==1);
 event(E::Countdown,30);CHECK(batteryGuardCycle);CHECK(batteryGuardBanner);CHECK(batteryGuardMessage);
 CHECK(batteryGuardMessage->text.find("30 s")!=std::string::npos);
 CHECK(Tab5Firmware::cancelled==1);CHECK(Tab5GitView::cancelled==1);CHECK(Tab5Terminal::cancelled==1);
 CHECK(Tab5Vault::cancelled==1);CHECK(Tab5FileOperation::cancelled==1);
 CHECK(objects.size()==2); // Banner and alert only; no forced-save status.
 event(E::Countdown,29);CHECK(Tab5Firmware::cancelled==1);CHECK(objects.size()==2);
 CHECK(batteryGuardMessage->text.find("29 s")!=std::string::npos);
 event(E::Cancelled);CHECK(!batteryGuardBanner);CHECK(!batteryGuardCycle);CHECK(toasts==2);
 event(E::None);CHECK(!batteryGuardBanner);
 event(E::Countdown,5);CHECK(Tab5Firmware::cancelled==2);
 event(E::Shutdown);CHECK(Tab5BatteryGuard::attempts==1);CHECK(Tab5BatteryGuard::off==0);
 CHECK(!batteryGuardBanner);CHECK(!batteryGuardCycle);
 event(E::Countdown,5);Tab5BatteryGuard::confirmed=true;event(E::Shutdown);
 CHECK(Tab5BatteryGuard::off==1);CHECK(Tab5BatteryGuard::attempts==2);
 std::printf("PASS %u checks\n",checks);
}
