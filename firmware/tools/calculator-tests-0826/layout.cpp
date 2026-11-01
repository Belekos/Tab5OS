#include "../../Tab5OS_Arduino_v0.8.26_Ecriture/Tab5CalculatorUi.h"
#include "../../Tab5OS_Arduino_v0.8.26_Ecriture/Tab5Calculator.h"
#include "../../Tab5OS_Arduino_v0.8.26_Ecriture/Tab5TextInput.h"
#include "../../Tab5OS_Arduino_v0.8.26_Ecriture/Tab5Focus.h"
#include "../../Tab5OS_Arduino_v0.8.26_Ecriture/Tab5CalculatorConversion.h"
#include "../../Tab5OS_Arduino_v0.8.26_Ecriture/Tab5UiStyle.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>
#include <vector>
#include <map>
static unsigned checks=0;
static lv_font_t* textFont=nullptr;
static void check(bool v,const char* text){if(!v){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}++checks;}
static lv_point_t touch{};static bool pressed=false;static lv_indev_t* pointer=nullptr;static lv_display_t* display=nullptr;
static std::vector<unsigned char> framebuffer(1280*720*3),drawbuffer(1280*720*3);
static Tab5CalculatorUi::Widgets ui;static double answer=0,memory=0;static int clicked=0;static std::string error;
static tab5calc::History history;static tab5calc::HistoryView historyView;static tab5calc::EntryState entry;static tab5calc::ConversionWindow conversion;
static void flush(lv_display_t* d,const lv_area_t* area,uint8_t* bytes){for(int y=area->y1;y<=area->y2;++y){size_t n=size_t(area->x2-area->x1+1)*3;std::memcpy(framebuffer.data()+(y*1280+area->x1)*3,bytes,n);bytes+=n;}lv_display_flush_ready(d);}
static lv_obj_t* label(lv_obj_t* parent,const char* text){auto* l=lv_label_create(parent);lv_label_set_text(l,text);return l;}
static void action(int id){clicked=id;error.clear();if(id==1){auto result=tab5calc::evaluate(lv_textarea_get_text(ui.input),lv_dropdown_get_selected(ui.angle)==0,answer,memory);if(!result){error=result.error;return;}history.append(lv_textarea_get_text(ui.input),result.value);answer=result.value;entry.accept(ui.input,answer);historyView.refresh(true);}
 else if(id==2)entry.recall(ui.input,"");
 else if(id==3){entry.result=false;writing::KeyEvent e;e.key=writing::Key::Backspace;Tab5TextInput::key(ui.input,e);}
 else if(id==4)memory=answer;else if(id==5)entry.insert(ui.input,"mem");else if(id==6)memory+=answer;else if(id==7)memory-=answer;else if(id==8)memory=0;
 else if(const char* text=Tab5CalculatorUi::insertion(id))entry.insert(ui.input,text);
}
static lv_obj_t* button(lv_obj_t* parent,const char* text,int id){auto* b=lv_button_create(parent);lv_obj_set_style_bg_color(b,lv_color_hex(0xf8fafb),0);lv_obj_set_style_border_color(b,lv_color_hex(0x607786),0);lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_shadow_width(b,0,0);lv_obj_set_style_text_color(b,lv_color_hex(0x17242c),0);lv_obj_set_style_radius(b,8,0);lv_obj_set_style_pad_all(b,4,0);auto* l=label(b,text);lv_obj_center(l);lv_obj_add_event_cb(b,[](lv_event_t* e){action(int(intptr_t(lv_event_get_user_data(e))));},LV_EVENT_CLICKED,(void*)intptr_t(id));return b;}
static void tap(lv_obj_t* b){lv_area_t a;lv_obj_get_coords(b,&a);touch={int32_t((a.x1+a.x2)/2),int32_t((a.y1+a.y2)/2)};pressed=true;lv_indev_read(pointer);lv_tick_inc(20);pressed=false;lv_indev_read(pointer);lv_tick_inc(20);}
static void tapAction(int id){for(size_t i=0;i<Tab5CalculatorUi::KeyCount;++i)if(Tab5CalculatorUi::Keys[i].action==id){clicked=0;tap(ui.keys[i]);check(clicked==id,"tap center activates intended calculator key");return;}check(false,"action exists");}
static void expect(double value){tapAction(1);check(error.empty()&&std::fabs(answer-value)<1e-9,"button expression evaluates correctly");}
static void image(const char* name){lv_obj_update_layout(lv_screen_active());lv_refr_now(display);std::ofstream f(name,std::ios::binary);f<<"P6\n1280 720\n255\n";for(size_t i=0;i<framebuffer.size();i+=3){f.put(char(framebuffer[i+2]));f.put(char(framebuffer[i+1]));f.put(char(framebuffer[i]));}}
static std::map<void*,size_t> allocations;static bool failAllocation=false;
static void* trackedAllocate(size_t bytes){if(failAllocation)return nullptr;void* result=std::malloc(bytes);if(result)allocations[result]=bytes;return result;}
static void trackedRelease(void* pointer){check(allocations.erase(pointer)==1,"history releases each owned allocation once");std::free(pointer);}
static void modelTests(){
 for(unsigned cycle=0;cycle<8;++cycle){tab5calc::History model(trackedAllocate,trackedRelease);for(unsigned n=0;n<500;++n){char expression[48];std::snprintf(expression,sizeof(expression),"%u/3",n);check(model.append(expression,n/3.),"history append succeeds");check(model.size()<=128&&model.textBytes()<=64*1024,"history bounded in count and bytes");}
  check(model.size()==128&&!std::strcmp(model.at(0)->expression,"372/3"),"only oldest entries evicted");const double precise=1.2345678901234567;check(model.append("1.2345678901234567",precise),"precise result saved");check(std::strtod(model.at(127)->result,nullptr)==precise,"history result round trips every double digit");
  failAllocation=true;check(!model.append("9+9",18),"allocation failure reported");check(model.size()==128&&model.at(127)->value==precise,"allocation failure preserves existing history");failAllocation=false;
  std::string longExpression(2048,'1');for(unsigned i=0;i<100;++i)check(model.append(longExpression.c_str(),i),"long expression accepted without truncation");check(model.textBytes()<=tab5calc::History::TextBudget&&model.size()==31,"byte budget evicts safely before count capacity");model.clear();check(allocations.empty(),"all history memory returned on close");}
}
static void historyTests(){
 entry.recall(ui.input,"1/3");action(1);entry.insert(ui.input,"*");entry.insert(ui.input,"3");action(1);check(answer==1,"continuation does not round prior result to 12 digits");entry.insert(ui.input,"7");check(!std::strcmp(lv_textarea_get_text(ui.input),"7"),"digit after result begins a new expression");action(1);entry.insert(ui.input,"+");entry.insert(ui.input,"5");action(1);check(answer==12,"operator after result chains from result");
 const unsigned before=history.size();entry.recall(ui.input,"1/0");action(1);check(!error.empty()&&history.size()==before&&answer==12,"invalid calculation leaves answer and history unchanged");
 entry.accept(ui.input,-2);entry.insert(ui.input,"^");entry.insert(ui.input,"2");action(1);check(answer==4,"negative result is grouped before exponent continuation");entry.accept(ui.input,-3);entry.insert(ui.input,Tab5CalculatorUi::keyboardToken("²"));action(1);check(answer==9,"physical square token chains from a negative result");
 history.clear();for(unsigned n=0;n<200;++n){char text[48];std::snprintf(text,sizeof(text),"%u+1",n);history.append(text,n+1.);}historyView.refresh(true);lv_obj_update_layout(ui.history);
 check(lv_obj_get_child_count(ui.history)==tab5calc::HistoryView::PoolSize+1,"history retains only fixed row pool for 128 calculations");check(lv_obj_get_scroll_y(ui.history)>10000,"history scroll reaches recent entries beyond pool");
 lv_obj_scroll_to_y(ui.history,0,LV_ANIM_OFF);historyView.refresh();lv_obj_update_layout(ui.history);auto* first=lv_obj_get_child(ui.history,1);auto* result=lv_obj_get_child(ui.history,2);tap(first);check(!std::strcmp(lv_textarea_get_text(ui.input),"72+1"),"selecting calculation copies full expression");tap(result);check(!std::strcmp(lv_textarea_get_text(ui.input),"73"),"selecting result copies numeric value");
 lv_obj_send_event(first,LV_EVENT_PRESSED,nullptr);entry.recall(ui.input,"unchanged");lv_obj_scroll_to_y(ui.history,4000,LV_ANIM_OFF);historyView.refresh();lv_obj_send_event(first,LV_EVENT_CLICKED,nullptr);check(!std::strcmp(lv_textarea_get_text(ui.input),"unchanged"),"press then recycled row never selects another calculation");
 Tab5Focus.focusObject(first);writing::KeyEvent key;key.key=writing::Key::End;check(historyView.key(key),"history End keyboard navigation handled");key.key=writing::Key::Enter;check(historyView.key(key)&&!std::strcmp(lv_textarea_get_text(ui.input),"200"),"keyboard can select final result beyond recycled pool");
 key.key=writing::Key::Home;historyView.key(key);key.key=writing::Key::Text;std::strcpy(key.text," ");check(historyView.key(key)&&!std::strcmp(lv_textarea_get_text(ui.input),"72+1"),"Space activates keyboard-selected expression without pointer press");
 for(unsigned line=1;line<256;++line){key.key=writing::Key::Tab;key.shift=false;check(historyView.key(key),"Tab moves to next logical history row");const auto* expected=history.at(line/2);const char* text=line%2?expected->result:expected->expression;check(!std::strcmp(lv_label_get_text(lv_obj_get_child(Tab5Focus.focused(),0)),text),"Tab preserves row identity across scrolling and pool recycling");}
 check(!historyView.key(key),"Tab at final history row can leave for next control");
 for(int line=254;line>=0;--line){key.key=writing::Key::Tab;key.shift=true;check(historyView.key(key),"Shift Tab moves to previous logical history row");const auto* expected=history.at(line/2);const char* text=line%2?expected->result:expected->expression;check(!std::strcmp(lv_label_get_text(lv_obj_get_child(Tab5Focus.focused(),0)),text),"Shift Tab preserves identity across recycled pool");}
 check(!historyView.key(key),"Shift Tab at first history row can leave for previous control");
 Tab5Focus.clear();history.clear();history.append("18.5*4",74);history.append("74-12",62);history.append("sqrt(144)",12);history.append("12/3",4);historyView.refresh(true);entry.accept(ui.input,4);
}
static void conversionTests(lv_obj_t* root){
 const uint32_t palette[]={0x12171c,0x202830,0x303b46,0x52687c,0xeaf3fb,0xa7bdcf,0x6699cc,0xffffff,0x334c65};Tab5UiStyle::configure(palette,9);double used=-1;unsigned closes=0;struct Context{double* used;unsigned* closes;} context{&used,&closes};tab5calc::ConversionWindow::Hooks hooks;
 hooks.color=[](unsigned role,void*)->uint32_t{const uint32_t map[]={0x12171c,0x202830,0xeaf3fb,0xa7bdcf,0x6699cc};return map[role<5?role:0];};hooks.style=[](lv_obj_t* object,void*){Tab5UiStyle::control(object);};hooks.use=[](double value,void* p){*static_cast<Context*>(p)->used=value;};hooks.closed=[](void* p){++*static_cast<Context*>(p)->closes;};hooks.context=&context;
 conversion.open(root,"1000",true,answer,memory,hooks);lv_obj_update_layout(root);check(Tab5Focus.focused()==conversion.input(),"conversion window initially focuses required value");check(lv_obj_has_state(conversion.useButton(),LV_STATE_DISABLED),"cannot reuse uncomputed conversion");writing::KeyEvent enter;enter.key=writing::Key::Enter;conversion.key(enter);check(!std::strcmp(lv_textarea_get_text(conversion.output()),"1"),"Enter converts mm to metres");check(!lv_obj_has_state(conversion.useButton(),LV_STATE_DISABLED),"valid result can be used in calculator");
 Tab5Focus.focusObject(conversion.output());Tab5Focus.finishEditing();writing::KeyEvent edit;edit.key=writing::Key::Text;edit.ctrl=true;std::strcpy(edit.text,"a");conversion.key(edit);std::strcpy(edit.text,"c");conversion.key(edit);check(!std::strcmp(Tab5TextInput::clipboard(),"1"),"Ctrl A/C copies read-only result without entering edit mode");Tab5Focus.focusObject(conversion.input());std::strcpy(edit.text,"a");conversion.key(edit);Tab5TextInput::copy("1500",4);std::strcpy(edit.text,"v");conversion.key(edit);check(!std::strcmp(lv_textarea_get_text(conversion.input()),"1500"),"Ctrl V replaces selection in keyboard-focused value field");std::strcpy(edit.text,"z");conversion.key(edit);check(!std::strcmp(lv_textarea_get_text(conversion.input()),"1000"),"Ctrl Z restores conversion value");conversion.calculate();
 check(lv_color_to_u32(lv_obj_get_style_bg_color(conversion.input(),LV_PART_MAIN))==lv_color_to_u32(lv_color_hex(0x202830)),"conversion input honors dark palette");lv_dropdown_open(conversion.from());auto* list=lv_dropdown_get_list(conversion.from());check(lv_color_to_u32(lv_obj_get_style_bg_color(list,LV_PART_MAIN))==lv_color_to_u32(lv_color_hex(0x202830)),"opened unit dropdown honors dark palette");lv_dropdown_close(conversion.from());
 lv_textarea_set_text(conversion.input(),"2000");check(lv_obj_has_state(conversion.useButton(),LV_STATE_DISABLED),"editing value invalidates stale result");conversion.calculate();tap(conversion.useButton());check(used==2&&closes==1&&!conversion.scope(),"Use inserts converted numeric result and closes modal");
 conversion.open(root,"4",true,answer,memory,hooks);lv_dropdown_set_selected(conversion.to(),9);conversion.calculate();check(lv_obj_has_state(conversion.useButton(),LV_STATE_DISABLED),"incompatible units cannot be used");writing::KeyEvent escape;escape.key=writing::Key::Escape;conversion.key(escape);check(!conversion.scope()&&closes==2,"Escape closes conversion without altering prior result");
 conversion.open(root,"1",true,answer,memory,hooks);lv_dropdown_open(conversion.from());conversion.key(escape);check(conversion.scope()&&!lv_dropdown_is_open(conversion.from()),"Escape dismisses active unit list before conversion window");conversion.key(escape);check(!conversion.scope(),"second Escape closes conversion window");
 conversion.open(root,"1000",true,answer,memory,hooks);conversion.calculate();image("calculator-conversion.ppm");conversion.close();
 for(unsigned i=0;i<20;++i){conversion.open(root,"1",true,answer,memory,hooks);conversion.close();}check(!conversion.scope(),"repeated modal close releases its LVGL subtree");
}
static void build(bool sidebar){
 lv_obj_clean(lv_screen_active());auto* screen=lv_screen_active();lv_obj_set_style_bg_color(screen,lv_color_hex(0xe6ecef),0);lv_obj_set_style_text_font(screen,textFont,0);
 auto* top=lv_obj_create(screen);lv_obj_set_size(top,1280,48);lv_obj_set_pos(top,0,0);lv_obj_remove_flag(top,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_radius(top,0,0);lv_obj_set_style_pad_all(top,8,0);auto* heading=label(top,"Tab5OS");lv_obj_set_style_text_font(heading,&lv_font_montserrat_24,0);
 const int dock=sidebar?64:0;if(sidebar){auto* bar=lv_obj_create(screen);lv_obj_set_pos(bar,0,48);lv_obj_set_size(bar,64,672);lv_obj_set_style_radius(bar,0,0);lv_obj_set_style_pad_all(bar,8,0);auto* home=label(bar,LV_SYMBOL_HOME);lv_obj_set_style_text_font(home,&lv_font_montserrat_24,0);}else lv_label_set_text(heading,LV_SYMBOL_HOME "   Tab5OS");
 auto* root=lv_obj_create(screen);lv_obj_set_pos(root,dock,48);lv_obj_set_size(root,1280-dock,672);lv_obj_set_style_pad_all(root,16,0);lv_obj_set_style_border_width(root,0,0);lv_obj_set_style_radius(root,0,0);lv_obj_set_style_bg_color(root,lv_color_hex(0xe6ecef),0);lv_obj_set_scroll_dir(root,LV_DIR_VER);lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLL_CHAIN);auto* title=label(root,"Calculatrice");lv_obj_set_pos(title,0,0);
 auto* close=button(root,LV_SYMBOL_CLOSE,0);lv_obj_set_size(close,48,44);lv_obj_set_style_text_font(lv_obj_get_child(close,0),&lv_font_montserrat_24,0);
 ui={};history.clear();entry.result=false;ui.input=lv_textarea_create(root);lv_textarea_set_one_line(ui.input,true);lv_textarea_set_max_length(ui.input,2048);lv_textarea_set_text(ui.input,"");Tab5TextInput::track(ui.input);ui.angle=lv_dropdown_create(root);lv_dropdown_set_options(ui.angle,"Degrés\nRadians");ui.status=label(root,"Prêt");ui.conversion=button(root,"Conversions",9);
 ui.history=historyView.create(root,history,[](const char* text,void*){entry.recall(ui.input,text);},[](lv_obj_t* object,bool result,void*){lv_obj_set_style_bg_opa(object,LV_OPA_TRANSP,0);lv_obj_set_style_text_color(object,lv_color_hex(result?0x17242c:0x506774),0);},nullptr);
 for(size_t i=0;i<Tab5CalculatorUi::KeyCount;++i){const auto& key=Tab5CalculatorUi::Keys[i];const auto* font=key.action==3?&lv_font_montserrat_24:textFont;ui.keys[i]=button(root,Tab5CalculatorUi::caption(key,font),key.action);lv_obj_set_style_text_font(lv_obj_get_child(ui.keys[i],0),font,0);}
 lv_obj_update_layout(root);int width=lv_obj_get_content_width(root),height=lv_obj_get_content_height(root);Tab5CalculatorUi::layout(ui,width,height);lv_obj_set_pos(close,width-48,0);lv_obj_update_layout(root);
 check(width==(sidebar?1184:1248)&&height==640,"actual 720p work-area dimensions");
 std::set<int> actions;lv_area_t parent;lv_obj_get_content_coords(root,&parent);int lowest=0,equalsBottom=0;
 for(size_t i=0;i<Tab5CalculatorUi::KeyCount;++i){auto* b=ui.keys[i];const auto& key=Tab5CalculatorUi::Keys[i];lv_area_t a;lv_obj_get_coords(b,&a);actions.insert(key.action);check(a.x1>=parent.x1&&a.x2<=parent.x2&&a.y1>=parent.y1&&a.y2<=parent.y2,"key entirely in visible work area");check(lv_obj_get_width(b)>=48&&lv_obj_get_height(b)>=44,"usable touch target");lowest=std::max(lowest,int(a.y2));if(key.action==1)equalsBottom=a.y2;
  for(size_t j=0;j<i;++j){lv_area_t other;lv_obj_get_coords(ui.keys[j],&other);check(a.x2<other.x1||other.x2<a.x1||a.y2<other.y1||other.y2<a.y1,"no key overlap");}
  if(key.action==2||key.action==3){lv_area_t quit;lv_obj_get_coords(close,&quit);check(a.y1-quit.y2>=140,"clear and backspace separated from quit");}
  tapAction(key.action);
 }
 check(actions.size()==40,"all calculator keys uniquely present");check(equalsBottom==lowest,"equals is on bottom row");for(int id=100;id<=131;++id)check(actions.count(id),"all previous functions and two existing engine functions present");check(lv_obj_get_scroll_bottom(root)==0,"no lower buttons clipped or scrolling at normal height");
 tapAction(2);tapAction(100);tapAction(122);tapAction(112);expect(8);
 tapAction(2);tapAction(126);tapAction(102);tapAction(121);expect(3);
 tapAction(2);tapAction(100);tapAction(129);tapAction(106);expect(3);
 tapAction(2);tapAction(130);tapAction(118);tapAction(121);expect(1);
 tapAction(2);tapAction(131);tapAction(115);tapAction(107);tapAction(121);expect(5);
 tapAction(2);tapAction(104);tapAction(114);tapAction(118);tapAction(121);expect(.5);
 lv_dropdown_set_selected(ui.angle,1);tapAction(2);tapAction(104);tapAction(124);tapAction(103);tapAction(113);tapAction(121);expect(1);lv_dropdown_set_selected(ui.angle,0);
 tapAction(4);tapAction(2);tapAction(5);tapAction(122);tapAction(113);expect(3);
 tapAction(2);tapAction(126);tapAction(102);tapAction(121);tapAction(122);tapAction(113);expect(5);
 historyTests();image(sidebar?"calculator-sidebar.ppm":"calculator-wide.ppm");conversionTests(root);
 Tab5CalculatorUi::layout(ui,width,380);lv_obj_update_layout(root);for(auto* key:ui.keys)check(lv_obj_get_height(key)>=44,"keyboard-open layout retains touch target height");
}
int main(int argc,char** argv){modelTests();lv_init();check(argc==2,"shipped font path provided");std::ifstream fontFile(argv[1],std::ios::binary);std::vector<unsigned char> fontBytes((std::istreambuf_iterator<char>(fontFile)),std::istreambuf_iterator<char>());textFont=lv_tiny_ttf_create_data(fontBytes.data(),fontBytes.size(),28);check(textFont!=nullptr,"shipped TTF font loaded");textFont->fallback=&lv_font_montserrat_24;display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,drawbuffer.data(),nullptr,drawbuffer.size(),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,flush);pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_mode(pointer,LV_INDEV_MODE_EVENT);lv_indev_set_read_cb(pointer,[](lv_indev_t*,lv_indev_data_t* data){data->point=touch;data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;});Tab5Focus.begin(lv_layer_top(),textFont);build(true);build(false);check(!std::strcmp(Tab5CalculatorUi::keyboardToken("√"),"sqrt("),"physical square-root key normalization");check(!std::strcmp(Tab5CalculatorUi::keyboardToken("%"),"%"),"physical remainder key retained");std::printf("PASS %u checks; actual LVGL geometry/touch, history and conversion, continuation, bounded memory release; 1280x720 sidebar + wide + modal captures\n",checks);}
