#include "Platform.h"
#include <lvgl.h>
extern const lv_font_t lv_font_montserrat_24=lv_font_montserrat_14;
#include "Tab5TextInput.cpp"
#include "Tab5UiStyle.cpp"
#define private public
#include "Tab5DirectoryView.h"
#undef private
#include "Tab5DirectoryView.cpp"
#include "Tab5FilePicker.cpp"
#include <cstdio>
#include <cassert>
static unsigned checks=0;
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);std::exit(1);}++checks;}
static void pump(Tab5DirectoryView& view){while(view.scanning()){unsigned reads=Fake::reads;view.service();check(Fake::reads-reads<=12,"bounded directory read slice");lv_obj_update_layout(lv_screen_active());}}
static lv_obj_t* symbol(lv_obj_t* root,const char* text){for(unsigned i=0;i<lv_obj_get_child_count(root);++i){auto* child=lv_obj_get_child(root,i);if(lv_obj_check_type(child,&lv_label_class)&&!strcmp(lv_label_get_text(child),text))return lv_obj_get_parent(child);if(auto* found=symbol(child,text))return found;}return nullptr;}
static void click(lv_obj_t* object){check(object!=nullptr,"mouse action exists");lv_obj_send_event(object,LV_EVENT_PRESSED,nullptr);lv_obj_send_event(object,LV_EVENT_CLICKED,nullptr);}
int main(){
 lv_init();lv_display_create(1280,720);auto* screen=lv_screen_active();Tab5Focus.begin(screen,LV_FONT_DEFAULT);
 uint32_t palette[]={0x101010,0x202020,0x303030,0x777777,0xffffff,0x999999,0x55aaff,0xaaaaaa,0x345678};Tab5UiStyle::configure(palette,12);
 for(unsigned i=0;i<1205;++i){char name[32];snprintf(name,sizeof(name),"file-%04u.elf",i);Fake::folders["/"].push_back({name,false});}
 Fake::folders["/USB"]={{"usb.elf",false}};Fake::folders["/next"]={{"child",true},{"last.elf",false}};Fake::folders["/next/child"]={{"nested.elf",false}};
 auto* list=lv_obj_create(screen);lv_obj_set_size(list,800,420);lv_obj_update_layout(screen);std::string selected;
 {
  Tab5DirectoryView view(list,LV_FONT_DEFAULT,nullptr,[](const char* path,bool,void* p){*static_cast<std::string*>(p)=path;},&selected);
  check(view.open("/"),"open root");check(view.scanning()&&view.count()==0,"opening doesn't enumerate synchronously");
  view.service();check(view.count()==12&&view.scanning(),"partial scan isn't marked complete");check(!strcmp(lv_label_get_text(view.empty_),"Chargement…"),"partial scan indicates loading");
  pump(view);check(view.count()==1205,"all files beyond former 16/40/80 caps");check(lv_obj_get_child_count(list)==30,"fixed 28 graphic rows and two markers");
  unsigned opens=Fake::opens,reads=Fake::reads;
  writing::KeyEvent end;end.key=writing::Key::End;check(view.key(end),"End accepted");lv_obj_update_layout(screen);check(view.selected_==1204&&lv_obj_get_scroll_y(list)>60000,"End reaches actual last file");
  writing::KeyEvent enter;enter.key=writing::Key::Enter;check(view.key(enter)&&selected=="/file-1204.elf","last file opens by Enter");
  lv_obj_scroll_to_y(list,0,LV_ANIM_OFF);lv_obj_update_layout(screen);check(view.selected_==1204,"selection survives recycled offscreen row");
  check(Fake::opens==opens&&Fake::reads==reads,"scroll never rescans filesystem");
  auto* pressed=view.slots_[0].row;lv_obj_send_event(pressed,LV_EVENT_PRESSED,nullptr);selected.clear();lv_obj_scroll_to_y(list,50000,LV_ANIM_OFF);lv_obj_update_layout(screen);lv_obj_send_event(pressed,LV_EVENT_CLICKED,nullptr);check(selected.empty(),"recycled pressed row cannot activate another file");
  writing::KeyEvent home;home.key=writing::Key::Home;check(view.key(home)&&view.selected_==0,"Home returns first logical row");
  lv_obj_send_event(view.slots_[0].row,LV_EVENT_CLICKED,nullptr);check(selected.empty(),"bare delayed click cannot activate an unpressed row");
  writing::KeyEvent space;space.key=writing::Key::Text;strcpy(space.text," ");check(view.key(space)&&selected=="/file-0000.elf","Space activates focused row without a pointer press");
  selected.clear();click(view.slots_[0].row);check(selected=="/file-0000.elf","valid pointer press activates the same row");
  writing::KeyEvent tab;tab.key=writing::Key::Tab;
  for(size_t i=1;i<=100;++i){check(view.key(tab),"Tab handled across recycled rows");lv_obj_update_layout(screen);check(view.selected_==i&&Tab5Focus.focused()!=nullptr,"Tab retains exact logical focus after scrolling");}
  tab.shift=true;
  for(size_t i=100;i>0;--i){check(view.key(tab),"ShiftTab handled across recycled rows");lv_obj_update_layout(screen);check(view.selected_==i-1&&Tab5Focus.focused()!=nullptr,"ShiftTab retains logical focus after scrolling");}
  check(!view.key(tab),"ShiftTab leaves list at first entry");view.key(end);tab.shift=false;check(!view.key(tab),"Tab leaves list at last entry");view.key(home);
  check(Fake::opens==opens&&Fake::reads==reads,"keyboard scrolling never rescans filesystem");
  palette[0]=0xf0f0f0;palette[1]=0xeeeeee;palette[4]=0x111111;Tab5UiStyle::configure(palette,4);view.service();
  check(lv_color_to_u32(lv_obj_get_style_bg_color(list,LV_PART_MAIN))==lv_color_to_u32(lv_color_hex(palette[0])),"list updates live theme");
  check(lv_color_to_u32(lv_obj_get_style_bg_color(view.slots_[0].row,LV_PART_MAIN))==lv_color_to_u32(lv_color_hex(palette[8])),"selected row theme survives update");
  view.open("/");view.service();view.open("/next");pump(view);check(view.count()==2&&!strcmp(view.model_[0].name,"child"),"changing folder cancels old stream");
  Fake::folders["/sort"]={{"beta",false},{"dirZ",true},{"Gamma",false},{"alpha",false},{"dirA",true}};
  Fake::metadata["/sort/alpha"]={900,300};Fake::metadata["/sort/beta"]={50,200};Fake::metadata["/sort/Gamma"]={400,100};
  Fake::metadata["/sort/dirA"]={0,10};Fake::metadata["/sort/dirZ"]={0,20};
  view.open("/sort");pump(view);view.focus(2);check(!strcmp(view.model_[view.selected_].name,"alpha"),"initial alphabetical selection");
  auto* sort=view.sortControl(screen,330);const char* orders[][3]={{"alpha","beta","Gamma"},{"Gamma","beta","alpha"},{"Gamma","beta","alpha"},{"alpha","beta","Gamma"},{"beta","Gamma","alpha"},{"alpha","Gamma","beta"}};
  const unsigned beforeSortOpens=Fake::opens,beforeSortReads=Fake::reads;
  for(unsigned order=0;order<6;++order){
    lv_dropdown_set_selected(sort,order);lv_obj_send_event(sort,LV_EVENT_VALUE_CHANGED,nullptr);lv_obj_update_layout(screen);
    check(Tab5FileSort::selected==static_cast<Tab5FileSort::Order>(order),"dropdown changes shared order");
    check(view.model_[0].directory&&view.model_[1].directory,"folders remain grouped before files in all orders");
    for(unsigned i=0;i<3;++i)check(!strcmp(view.model_[i+2].name,orders[order][i]),"global name/date/size ordering in both directions");
    check(view.selected_<view.count()&&!strcmp(view.model_[view.selected_].name,"alpha"),"sort retains selected logical file and focus");
  }
  check(Fake::reads==beforeSortReads&&Fake::opens==beforeSortOpens,"sort uses cached metadata without filesystem rescan");
  lv_dropdown_set_selected(sort,0);lv_obj_send_event(sort,LV_EVENT_VALUE_CHANGED,nullptr);
  auto* held=view.slots_[0].row;lv_obj_send_event(held,LV_EVENT_PRESSED,nullptr);selected.clear();lv_dropdown_set_selected(sort,1);lv_obj_send_event(sort,LV_EVENT_VALUE_CHANGED,nullptr);lv_obj_send_event(held,LV_EVENT_CLICKED,nullptr);check(selected.empty(),"sorting invalidates an old pointer press");
  Tab5FileSort::selected=Tab5FileSort::Order::NameAscending;view.service();
  view.open("/");view.service();check(Fake::handles==1,"one directory retained during scan");
 }
 check(Fake::handles==0,"destroy midscan closes directory");lv_obj_delete(list);lv_obj_update_layout(screen);
 auto* origin=lv_button_create(screen);Tab5Focus.focusObject(origin);
 check(Tab5FilePicker::open("/",".elf",false,[](const char*,void*){},nullptr),"open production picker");
 Tab5FilePicker::service();check(Fake::handles==1,"picker scans incrementally");
 auto* picker=Tab5FilePicker::scope();check(lv_color_to_u32(lv_obj_get_style_bg_color(picker,LV_PART_MAIN))==lv_color_to_u32(lv_color_hex(palette[0])),"picker root uses OS theme");
 check(Tab5FilePicker::state->view->sortControl_&&lv_dropdown_get_option_count(Tab5FilePicker::state->view->sortControl_)==6,"file picker exposes all six explicit sort options");
 click(symbol(picker,LV_SYMBOL_DRIVE));check(Tab5FilePicker::scope()!=picker,"storage popup has own keyboard scope");
 writing::KeyEvent escape;escape.key=writing::Key::Escape;check(Tab5FilePicker::key(escape)&&Tab5FilePicker::visible()&&Tab5FilePicker::scope()==picker,"Escape closes storage menu only");
 click(symbol(picker,LV_SYMBOL_DRIVE));click(symbol(Tab5FilePicker::scope(),"USB"));Tab5FilePicker::service();check(Tab5FilePicker::visible(),"mouse opens USB");
 Fake::usb=false;Tab5FilePicker::service();check(Tab5FilePicker::visible(),"USB unplug retains chooser");
 auto* heading=lv_obj_get_child(picker,0);check(!strcmp(lv_label_get_text(heading),"/"),"USB unplug returns to available SD");
 palette[0]=0x090909;palette[1]=0x252525;palette[4]=0xefefef;Tab5UiStyle::configure(palette,20);Tab5FilePicker::service();check(lv_color_to_u32(lv_obj_get_style_bg_color(picker,LV_PART_MAIN))==lv_color_to_u32(lv_color_hex(palette[0])),"picker theme changes after open");
 check(Tab5FilePicker::key(escape)&&!Tab5FilePicker::visible()&&Fake::handles==0,"Escape closes picker and pending scan");check(Tab5Focus.focused()==origin,"closing restores invoking control");
 printf("PASS %u checks: actual LVGL directory/picker, 1205 files, virtualization, bounded reads, cancellation, navigation, theme and media changes.\n",checks);
}
