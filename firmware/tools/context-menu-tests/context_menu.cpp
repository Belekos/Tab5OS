#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <lvgl.h>

static bool failNothrow = false;
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
  if (failNothrow) { failNothrow = false; return nullptr; }
  return std::malloc(n);
}
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }

#include "Tab5TextInput.cpp"
#include "Tab5UiStyle.cpp"
#include "Tab5ContextMenu.cpp"

static unsigned checks = 0, groups = 0, selected = 0;
static void check(bool value, const char* why) {
  ++checks;
  if (!value) { std::fprintf(stderr, "FAIL: %s (check %u)\n", why, checks); std::exit(1); }
}
static void group(const char* name) { ++groups; std::printf("CASE %s\n", name); }
static void action(void* amount) { selected += unsigned(reinterpret_cast<uintptr_t>(amount)); }
static writing::KeyEvent keyEvent(writing::Key key, char c=0, bool ctrl=false, bool shift=false, bool alt=false) {
  writing::KeyEvent e; e.key=key; e.text[0]=c; e.ctrl=ctrl; e.shift=shift; e.alt=alt; return e;
}
static void key(writing::Key key, char c=0, bool ctrl=false, bool shift=false, bool alt=false) {
  check(Tab5ContextMenu::key(keyEvent(key,c,ctrl,shift,alt)), "menu consumes key");
}
static lv_obj_t* panel() {
  auto* scope = Tab5ContextMenu::scope();
  check(scope && lv_obj_is_valid(scope), "live overlay");
  check(lv_obj_get_child_count(scope)==1, "one panel");
  return lv_obj_get_child(scope,0);
}
static lv_obj_t* button(size_t index) { return lv_obj_get_child(panel(), int(index)); }
static void click(size_t index) { check(lv_obj_send_event(button(index),LV_EVENT_CLICKED,nullptr)==LV_RESULT_OK,"click returns to event dispatcher"); }
static lv_obj_t* owner(lv_obj_t* screen) {
  auto* o=lv_obj_create(screen);lv_obj_set_size(o,500,320);
  auto* b=lv_button_create(o);lv_obj_set_size(b,100,40);Tab5Focus.focusObject(b);return o;
}
static bool hasTitle(const char* title) {
  auto* p=panel();
  for(uint32_t i=0;i<lv_obj_get_child_count(p);++i) {
    auto* b=lv_obj_get_child(p,i); auto* label=lv_obj_get_child(b,0);
    if(label && !std::strcmp(lv_label_get_text(label),title))return true;
  }
  return false;
}
static void selectTitle(const char* title) {
  auto* p=panel();
  for(uint32_t i=0;i<lv_obj_get_child_count(p);++i) {
    auto* b=lv_obj_get_child(p,i);auto* label=lv_obj_get_child(b,0);
    if(label&&!std::strcmp(lv_label_get_text(label),title)){click(i);return;}
  }
  check(false,"requested menu title exists");
}
static const lv_font_t* font() { return LV_FONT_DEFAULT; }
static const char* translate(const char* s) { return !std::strcmp(s,"First")?"Premier":s; }
static void refresh() { lv_obj_update_layout(lv_screen_active());lv_obj_update_layout(lv_layer_top()); }
struct Output { lv_obj_t* field; std::string model; unsigned calls=0; bool selectionCleared=false; };
static void clear(void* data) {
  auto& o=*static_cast<Output*>(data);++o.calls;
  auto* label=lv_obj_check_type(o.field,&lv_textarea_class)?lv_textarea_get_label(o.field):o.field;
  o.selectionCleared=lv_label_get_text_selection_start(label)==LV_DRAW_LABEL_NO_TXT_SEL && lv_label_get_text_selection_end(label)==LV_DRAW_LABEL_NO_TXT_SEL;
  o.model.clear();
  if(lv_obj_check_type(o.field,&lv_textarea_class))lv_textarea_set_text(o.field,o.model.c_str());
  else lv_label_set_text(o.field,o.model.c_str());
}
static lv_obj_t* reopenedOwner=nullptr;
static void reopen(void*) {
  const Tab5ContextMenu::Item items[]={{"Replacement","Ctrl+R",action,reinterpret_cast<void*>(16)}};
  check(Tab5ContextMenu::show(reopenedOwner,44,44,items,1),"callback opens replacement menu");
}
static void deleteOnFocus(lv_event_t* e) {
  auto** parent=static_cast<lv_obj_t**>(lv_event_get_user_data(e));
  auto* doomed=*parent;*parent=nullptr;if(doomed)lv_obj_delete(doomed);
}

int main() {
  lv_init();lv_display_create(1280,720);auto* screen=lv_screen_active();Tab5Focus.begin(screen,LV_FONT_DEFAULT);
  const uint32_t colors[]={0x101010,0x202020,0x303030,0x404040,0xf0f0f0,0xa0a0a0,0x4488aa,0xff0000,0x557799};
  Tab5UiStyle::configure(colors,18);Tab5ContextMenu::configure(font,translate);
  auto* base=owner(screen);
  const Tab5ContextMenu::Item items[]={
    {"Unavailable","Ctrl+D",action,reinterpret_cast<void*>(100),false},
    {"First","Ctrl+C",action,reinterpret_cast<void*>(1)},
    {"Second","Ctrl+Maj+L",action,reinterpret_cast<void*>(2)},
    {"Refresh","F5",action,reinterpret_cast<void*>(4)},
    {"No callback",nullptr,nullptr,nullptr,true}};

  group("mouse-deferred-and-theme");
  check(Tab5ContextMenu::show(base,1279,719,items,5),"open menu");refresh();
  check(Tab5Focus.focused()==button(1),"first enabled action initially selected");
  check(hasTitle("Premier"),"translation callback used");
  lv_area_t a;lv_obj_get_coords(panel(),&a);
  check(a.x1>=8&&a.y1>=8&&a.x2<=1271&&a.y2<=711,"menu kept inside screen margins");
  check(lv_obj_get_style_radius(panel(),LV_PART_MAIN)==18,"menu uses OS radius");
  check(lv_color_to_u32(lv_obj_get_style_bg_color(panel(),LV_PART_MAIN))==lv_color_to_u32(lv_color_hex(colors[1])),"menu uses OS surface colour");
  check(lv_obj_has_state(button(0),LV_STATE_DISABLED)&&lv_obj_has_state(button(4),LV_STATE_DISABLED),"unavailable and null actions disabled");
  click(1);check(selected==0&&Tab5ContextMenu::scope(),"click only queues action");
  Tab5ContextMenu::service();check(selected==1&&!Tab5ContextMenu::scope(),"service closes before action");
  check(Tab5Focus.contains(base,Tab5Focus.focused()),"focus returned to owner");

  group("keyboard-shortcuts-and-navigation");
  check(Tab5ContextMenu::show(base,-100,-100,items,5),"open clamped top-left");
  key(writing::Key::Text,'d',true);Tab5ContextMenu::service();check(selected==1&&Tab5ContextMenu::scope(),"disabled shortcut never executes");
  key(writing::Key::Text,'l',true);Tab5ContextMenu::service();check(selected==1&&Tab5ContextMenu::scope(),"shift modifier must match");
  key(writing::Key::Text,'l',true,true);check(selected==1,"shortcut deferred");Tab5ContextMenu::service();check(selected==3,"Ctrl Shift L activates");
  check(Tab5ContextMenu::show(base,10,10,items,5),"open navigation menu");key(writing::Key::Down);
  check(Tab5Focus.focused()==button(2),"down selects next action");key(writing::Key::Enter);check(selected==3,"Enter deferred");Tab5ContextMenu::service();check(selected==5,"Enter runs focused action");
  check(Tab5ContextMenu::show(base,10,10,items,5),"open F5 menu");key(writing::Key::F5);Tab5ContextMenu::service();check(selected==9,"function key activates");
  check(Tab5ContextMenu::show(base,10,10,items,5),"open Meta menu");auto meta=keyEvent(writing::Key::Text,'c');meta.meta=true;check(Tab5ContextMenu::key(meta),"Meta shortcut handled");Tab5ContextMenu::service();check(selected==10,"Meta aliases Ctrl");

  group("outside-escape-and-owner-deletion");
  check(Tab5ContextMenu::show(base,10,10,items,5),"open outside test");auto* overlay=Tab5ContextMenu::scope();
  lv_obj_send_event(overlay,LV_EVENT_CLICKED,nullptr);check(Tab5ContextMenu::scope()==overlay,"outside click closes after dispatcher returns");Tab5ContextMenu::service();check(!Tab5ContextMenu::scope()&&selected==10,"outside dismiss cancels action");
  check(Tab5ContextMenu::show(base,10,10,items,5),"open Escape test");key(writing::Key::Escape);Tab5ContextMenu::service();check(!Tab5ContextMenu::scope()&&selected==10,"Escape dismisses");
  auto* doomed=owner(screen);check(Tab5ContextMenu::show(doomed,10,10,items,5),"open doomed owner");click(1);lv_obj_delete(doomed);Tab5ContextMenu::service();check(!Tab5ContextMenu::scope()&&selected==10,"deleted owner cancels queued callback");
  doomed=owner(screen);auto* originalFocus=lv_obj_get_child(doomed,0);check(Tab5ContextMenu::show(doomed,10,10,items,5),"open restore deletion");
  lv_obj_add_event_cb(originalFocus,deleteOnFocus,LV_EVENT_FOCUSED,&doomed);click(1);Tab5ContextMenu::service();
  check(doomed==nullptr&&!Tab5ContextMenu::scope()&&selected==10,"owner deleted during focus restoration cancels callback");

  group("callback-reopens-menu-and-local-items");
  reopenedOwner=base;
  {const Tab5ContextMenu::Item local[]={{"Reopen",nullptr,reopen}};check(Tab5ContextMenu::show(base,10,10,local,1),"local item array accepted");}
  click(0);Tab5ContextMenu::service();check(Tab5ContextMenu::scope()&&hasTitle("Replacement"),"replacement menu survives previous action cleanup");
  key(writing::Key::Text,'r',true);Tab5ContextMenu::service();check(selected==26&&!Tab5ContextMenu::scope(),"replacement action runs");

  group("textarea-copy-cut-paste-undo-selection");
  auto* input=lv_textarea_create(screen);lv_textarea_set_text(input,"bonjour 漢字");Tab5TextInput::track(input);Tab5Focus.focusObject(input);
  check(Tab5ContextMenu::text(lv_textarea_get_label(input),15,15),"inner textarea label resolves to field");
  key(writing::Key::Text,'a',true);Tab5ContextMenu::service();check(Tab5TextInput::hasSelection(input),"select all through context menu");
  check(Tab5ContextMenu::text(input,15,15),"open selection menu");check(hasTitle("Copier")&&hasTitle("Couper"),"editable copy and cut commands present");
  selectTitle("Copier");Tab5ContextMenu::service();check(!std::strcmp(Tab5TextInput::clipboard(),"bonjour 漢字"),"UTF8 selection copied");
  check(Tab5ContextMenu::text(input,15,15),"open cut menu");key(writing::Key::Text,'x',true);Tab5ContextMenu::service();check(!*lv_textarea_get_text(input),"selection cut");
  check(Tab5ContextMenu::text(input,15,15),"open paste menu");key(writing::Key::Text,'v',true);Tab5ContextMenu::service();check(!std::strcmp(lv_textarea_get_text(input),"bonjour 漢字"),"clipboard pasted");
  check(Tab5ContextMenu::text(input,15,15),"open undo menu");key(writing::Key::Text,'z',true);Tab5ContextMenu::service();check(!*lv_textarea_get_text(input),"context undo");
  check(Tab5ContextMenu::text(input,15,15),"open redo menu");key(writing::Key::Text,'y',true);Tab5ContextMenu::service();check(!std::strcmp(lv_textarea_get_text(input),"bonjour 漢字"),"context redo");
  check(Tab5ContextMenu::text(input,15,15),"open alternate redo preparation");key(writing::Key::Text,'z',true);Tab5ContextMenu::service();
  check(Tab5ContextMenu::text(input,15,15),"open Ctrl Shift Z redo menu");key(writing::Key::Text,'z',true,true);Tab5ContextMenu::service();
  check(!std::strcmp(lv_textarea_get_text(input),"bonjour 漢字")&&!Tab5ContextMenu::scope(),"Ctrl Shift Z remains an alternate redo shortcut in context menu");

  group("readonly-console-clear-and-bind-lifetime");
  auto* output=lv_textarea_create(screen);Output model{output,"rapport 日本語"};lv_textarea_set_text(output,model.model.c_str());Tab5TextInput::setReadOnly(output);
  check(Tab5ContextMenu::bindOutput(output,clear,&model),"clear model binding attached");auto events=lv_obj_get_event_count(output);
  check(Tab5ContextMenu::bindOutput(output,clear,&model)&&lv_obj_get_event_count(output)==events,"rebinding does not retain another callback");
  check(Tab5ContextMenu::text(output,12,12),"read-only output menu");check(hasTitle("Copier")&&hasTitle("Effacer la sortie")&&!hasTitle("Couper")&&!hasTitle("Coller"),"read-only menu has only applicable actions");
  key(writing::Key::Text,'a',true);Tab5ContextMenu::service();check(Tab5TextInput::hasSelection(output),"output selectable");
  check(Tab5ContextMenu::text(output,12,12),"clear selected output menu");key(writing::Key::Text,'l',true);check(model.calls==0,"clear deferred");Tab5ContextMenu::service();
  check(model.calls==1&&model.model.empty()&&!*lv_textarea_get_text(output)&&model.selectionCleared,"clear empties backing model and selection");
  lv_textarea_set_text(output,model.model.c_str());check(!*lv_textarea_get_text(output),"refresh cannot restore old model text");
  check(Tab5ContextMenu::text(output,12,12),"empty output menu");key(writing::Key::Text,'l',true);Tab5ContextMenu::service();check(model.calls==1,"empty clear action disabled");Tab5ContextMenu::close();
  auto* plain=lv_label_create(screen);lv_label_set_text(plain,"plain");check(!Tab5ContextMenu::text(plain,5,5),"ordinary display label not an edit target");
  lv_obj_add_flag(plain,LV_OBJ_FLAG_USER_1);Output labelModel{plain,"plain"};check(Tab5ContextMenu::bindOutput(plain,clear,&labelModel),"label output binding");
  check(Tab5ContextMenu::text(plain,5,5),"selectable output label menu");key(writing::Key::Text,'l',true);Tab5ContextMenu::service();check(labelModel.calls==1&&!std::strcmp(lv_label_get_text(plain),""),"label model cleared");

  group("password-and-disabled-field");
  auto* secret=lv_textarea_create(screen);lv_textarea_set_password_mode(secret,true);lv_textarea_set_text(secret,"secret");
  check(Tab5ContextMenu::text(secret,10,10),"password supports paste and selection");check(!hasTitle("Copier")&&!hasTitle("Couper")&&!hasTitle("Annuler"),"password never exposes copy cut or history");Tab5ContextMenu::close();
  Tab5TextInput::protect(secret);lv_textarea_set_password_mode(secret,false);
  check(Tab5ContextMenu::text(secret,10,10),"revealed protected secret still offers input menu");
  check(!hasTitle("Copier")&&!hasTitle("Couper")&&!hasTitle("Annuler")&&!hasTitle("Rétablir"),"revealing a secret cannot expose ordinary clipboard or history commands");
  check(hasTitle("Coller")&&hasTitle("Tout sélectionner"),"protected secret retains paste and select for editing");Tab5ContextMenu::close();
  lv_obj_add_state(secret,LV_STATE_DISABLED);check(!Tab5ContextMenu::text(secret,10,10),"disabled field does not open menu");

  group("bounded-menu-invalid-input-and-binding-OOM");
  check(!Tab5ContextMenu::show(nullptr,0,0,items,5),"null owner rejected");check(!Tab5ContextMenu::show(base,0,0,nullptr,5),"null items rejected");check(!Tab5ContextMenu::show(base,0,0,items,0),"empty menu rejected");
  Tab5ContextMenu::Item many[17];for(auto& i:many)i=items[1];check(!Tab5ContextMenu::show(base,0,0,many,17),"menu bound enforced");
  check(Tab5ContextMenu::show(base,1279,719,many,16),"max count allowed");refresh();lv_obj_get_coords(panel(),&a);check(a.y1>=8&&a.y2<=711,"long menu fits screen with scrolling");check(lv_obj_get_scroll_dir(panel())==LV_DIR_VER,"long menu scrolls vertically");Tab5ContextMenu::close();
  auto* oom=lv_label_create(screen);lv_label_set_text(oom,"OOM");Output oomModel{oom,"OOM"};const auto beforeEvents=lv_obj_get_event_count(oom);
  failNothrow=true;check(!Tab5ContextMenu::bindOutput(oom,clear,&oomModel),"binding OOM reported");check(!failNothrow&&lv_obj_get_event_count(oom)==beforeEvents&&!Tab5ContextMenu::clearOutput(oom),"binding OOM leaves no partial registration");
  check(Tab5ContextMenu::bindOutput(oom,clear,&oomModel)&&Tab5ContextMenu::clearOutput(oom)&&oomModel.calls==1,"binding works after allocation failure");

  group("repeated-menu-output-lifecycle");
  for(unsigned cycle=0;cycle<512;++cycle) {
    auto* o=lv_textarea_create(screen);lv_textarea_set_text(o,"cycle");Tab5TextInput::setReadOnly(o);Output value{o,"cycle"};
    check(Tab5ContextMenu::bindOutput(o,clear,&value),"cycle output binding");check(Tab5ContextMenu::text(o,40,40),"cycle menu");
    if(cycle%2){key(writing::Key::Text,'l',true);Tab5ContextMenu::service();check(value.calls==1,"cycle clears model");lv_obj_delete(o);}
    else{key(writing::Key::Text,'l',true);lv_obj_delete(o);Tab5ContextMenu::service();check(value.calls==0,"cycle cancels deleted model");}
    check(!Tab5ContextMenu::scope(),"cycle overlay released");
  }
  lv_obj_delete(oom);lv_obj_delete(secret);lv_obj_delete(plain);lv_obj_delete(output);lv_obj_delete(input);lv_obj_delete(base);
  check(Tab5TextInput::historyBytes==0,"all editable histories released");
  check(!Tab5ContextMenu::key(keyEvent(writing::Key::Escape)),"closed menu leaves keyboard to OS");
  std::printf("PASS %u context-menu checks; groups=%u; cycles=512; history_bytes=%zu; LVGL=%d.%d.%d; physical_device=false\n",checks,groups,Tab5TextInput::historyBytes,LVGL_VERSION_MAJOR,LVGL_VERSION_MINOR,LVGL_VERSION_PATCH);
  return 0;
}
