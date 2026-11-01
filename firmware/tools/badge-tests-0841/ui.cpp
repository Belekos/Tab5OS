#include <lvgl.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "Tab5MeshBadge.h"
#include "Tab5Meshtastic.h"
#include "Tab5UiStyle.h"
#include "Tab5Focus.h"
#include "Tab5ShellLayout.h"
#include "Tab5TopBarLayout.h"
static unsigned checks=0,invalidations=0,flushes=0,clicks=0,layouts=0;static bool observe=false;
static void check(bool good,const char*why){++checks;if(!good){fprintf(stderr,"FAIL %u: %s\n",checks,why);exit(1);}}
namespace Tab5Meshtastic {
namespace M=Tab5MeshModel;
#include "badge-status.inc"
struct Context{Status status=Status::Idle;bool ready=false,stop=false;unsigned pairAction=0;M::History*history=nullptr;};static Context*state=nullptr;static M::Preferences preferences;static std::atomic<bool>running{false};struct Guard{};
#include "badge-model.inc"
}
static constexpr int SCREEN_W=1280;
static lv_obj_t*screen,*topBar,*topHomeButton,*topPythonResume,*titleLabel,*dateLabel,*topAudioIndicator;static lv_display_t*display;
enum class Page{Home,Settings};static Page activePage=Page::Settings;static unsigned homeMenu=0;static bool homeCustomize=false;
static struct{lv_obj_t*scope(){return nullptr;}}Tab5Tools;namespace Tab5GitView{lv_obj_t*scope(){return nullptr;}}
static void setTopActionVisible(lv_obj_t*o,bool visible){if(visible)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
static void shellDockPositions(){++layouts;}
#include "shell-top.inc"
static const char*T(const char*s){return s;}
#include "badge-service.inc"
static std::vector<uint8_t>pixels(1280*720*3),drawBuffer(1280*100*3),fontData;static lv_font_t*font;static std::string shots;
static void pump(){lv_tick_inc(30);lv_timer_handler();lv_obj_update_layout(screen);}
static void capture(const std::string&name){lv_obj_invalidate(screen);lv_refr_now(display);FILE*f=fopen((shots+"/"+name+".ppm").c_str(),"wb");check(f,"capture opens");fprintf(f,"P6\n1280 720\n255\n");for(size_t i=0;i<pixels.size();i+=3){uint8_t rgb[]={pixels[i+2],pixels[i+1],pixels[i]};fwrite(rgb,1,3,f);}fclose(f);}
static size_t colored(uint32_t value,const lv_area_t&a){size_t n=0;for(int y=a.y1;y<=a.y2;++y)for(int x=a.x1;x<=a.x2;++x){size_t i=(size_t(y)*1280+x)*3;uint32_t c=uint32_t(pixels[i])|uint32_t(pixels[i+1])<<8|uint32_t(pixels[i+2])<<16;n+=c==value;}return n;}
static void titleBounds(bool shown){lv_area_t title,badge;lv_obj_get_coords(titleLabel,&title);lv_obj_get_coords(Tab5MeshBadge::object(),&badge);if(!(title.y1>=0&&title.y2<48&&title.x2<550))fprintf(stderr,"title=(%d,%d)-(%d,%d), text=%s, fontHeight=%d, width=%d\n",title.x1,title.y1,title.x2,title.y2,lv_label_get_text(titleLabel),lv_font_get_line_height(font),lv_obj_get_width(titleLabel));check(title.y1>=0&&title.y2<48&&title.x2<550,"page title stays within top-bar reservation");if(shown)check(title.x1>badge.x2,"active badge and page title do not overlap");check(lv_obj_get_width(titleLabel)==532-lv_obj_get_x(titleLabel),"title width follows its actual left anchor");}
int main(int argc,char**argv){check(argc==3,"output and font supplied");shots=argv[1];std::ifstream fontFile(argv[2],std::ios::binary);fontData.assign(std::istreambuf_iterator<char>(fontFile),{});lv_init();display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,drawBuffer.data(),nullptr,drawBuffer.size(),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*a,uint8_t*data){const unsigned width=lv_area_get_width(a),stride=lv_draw_buf_width_to_stride(width,LV_COLOR_FORMAT_RGB888);for(int y=a->y1;y<=a->y2;++y)memcpy(pixels.data()+(size_t(y)*1280+a->x1)*3,data+size_t(y-a->y1)*stride,width*3);++flushes;lv_display_flush_ready(d);});lv_display_add_event_cb(display,[](lv_event_t*){if(observe)++invalidations;},LV_EVENT_INVALIDATE_AREA,nullptr);
 screen=lv_obj_create(nullptr);lv_screen_load(screen);lv_obj_remove_style_all(screen);lv_obj_set_size(screen,1280,720);topBar=lv_obj_create(screen);lv_obj_remove_style_all(topBar);lv_obj_set_size(topBar,1280,48);topHomeButton=lv_button_create(topBar);lv_obj_set_size(topHomeButton,42,36);lv_obj_set_pos(topHomeButton,6,6);topPythonResume=lv_button_create(topBar);lv_obj_set_size(topPythonResume,52,36);titleLabel=lv_label_create(topBar);lv_label_set_long_mode(titleLabel,LV_LABEL_LONG_DOT);dateLabel=lv_label_create(topBar);lv_label_set_text(dateLabel,"24.10.2026 23:59");topAudioIndicator=lv_label_create(topBar);lv_obj_set_pos(topAudioIndicator,550,6);lv_label_set_text(topAudioIndicator,"");auto*badge=Tab5MeshBadge::create(topBar,[](lv_event_t*){++clicks;});check(lv_obj_has_flag(badge,LV_OBJ_FLAG_HIDDEN),"new badge begins hidden");
 using namespace Tab5Meshtastic;M::History history;Context fixture;fixture.history=&history;state=&fixture;preferences.knownCount=1;running.store(true);
 for(unsigned size:{22u,24u}){font=lv_tiny_ttf_create_data(fontData.data(),fontData.size(),size);check(font,"shipped TTF loads");font->fallback=size==22?&lv_font_montserrat_22:&lv_font_montserrat_24;lv_obj_set_style_text_font(topBar,font,0);Tab5Focus.begin(screen,font);
 for(bool dark:{false,true})for(bool german:{false,true}){uint32_t light[]={0xf4f4f0,0xffffff,0xe8e9e6,0xb9bcb6,0x222824,0x69716a,0x427b57,0xffffff,0xd9eadc};uint32_t night[]={0x101316,0x191e23,0x252c33,0x48545e,0xeaf0f4,0x929da6,0x3e7657,0xffffff,0x284835};auto*palette=dark?night:light;Tab5UiStyle::configure(palette,8);lv_obj_set_style_bg_color(screen,lv_color_hex(palette[0]),0);lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);lv_obj_set_style_text_color(topBar,lv_color_hex(palette[4]),0);Tab5UiStyle::button(badge);lv_label_set_text(titleLabel,german?"Einstellungen · Übersicht":"Réglages · Vue d’ensemble");
  for(bool dock:{false,true}){Tab5Shell::choices.enabled=dock;shellTopLayout();fixture.ready=true;fixture.stop=false;running.store(true);history.messageCount=0;
   for(auto status:{Status::Idle,Status::Starting,Status::Scanning,Status::Connecting,Status::Pairing,Status::Discovering,Status::Configuring,Status::Stopping,Status::Failed}){fixture.status=status;serviceBadge();pump();check(!lv_obj_is_visible(badge),"badge hidden unless the actual session status is Ready, even with a known device");titleBounds(false);}
   const auto widthWithout=lv_obj_get_width(titleLabel);fixture.status=Status::Ready;serviceBadge();pump();check(lv_obj_is_visible(badge),"ready running radio shows badge");titleBounds(true);check(lv_obj_get_width(titleLabel)<widthWithout,"page title yields only the active badge width");lv_area_t area;lv_obj_get_coords(badge,&area);lv_refr_now(display);check(colored(0x299956,area)>25,"active badge is green");
   history.messageCount=1;history.messages[0].unread=true;serviceBadge();pump();lv_refr_now(display);lv_area_t envelope={area.x1+39,area.y1+11,area.x1+58,area.y1+26};check(colored(palette[4],envelope)>15,"unread message envelope appears inside same badge");
   pump();observe=true;invalidations=0;auto oldLayouts=layouts,oldFlushes=flushes;for(unsigned i=0;i<100;++i){serviceBadge();pump();}check(!invalidations&&flushes==oldFlushes&&layouts==oldLayouts,"unchanged badge/revision causes no invalidation, flush or shell relayout");observe=false;
   lv_obj_send_event(badge,LV_EVENT_CLICKED,nullptr);check(clicks>0,"badge click invokes open action");
   if(dock){std::string suffix=std::string(german?"de-":"fr-")+(dark?"dark-":"light-")+std::to_string(size);capture("badge-unread-"+suffix);}
   history.messages[0].unread=false;serviceBadge();pump();lv_refr_now(display);check(colored(palette[4],envelope)==0,"read messages remove envelope");
   for(int reason=0;reason<3;++reason){fixture.ready=reason!=0;fixture.stop=reason==1;running.store(reason!=2);serviceBadge();pump();check(!lv_obj_is_visible(badge),"not configured, stopping, or stopped session hides badge even with Ready status");titleBounds(false);check(lv_obj_get_width(titleLabel)==widthWithout,"hidden badge returns title width");fixture.ready=true;fixture.stop=false;running.store(true);serviceBadge();pump();}
   running.store(false);serviceBadge();pump();if(dock){std::string suffix=std::string(german?"de-":"fr-")+(dark?"dark-":"light-")+std::to_string(size);capture("badge-inactive-hidden-"+suffix);}running.store(true);
  }
 }Tab5Focus.clear();lv_obj_set_style_text_font(topBar,&lv_font_montserrat_22,0);lv_tiny_ttf_destroy(font);}
 state=nullptr;serviceBadge();pump();check(!lv_obj_is_visible(badge),"absent Mesh context remains hidden");lv_obj_delete(badge);check(!Tab5MeshBadge::object(),"badge teardown clears global pointer");Tab5MeshBadge::update(true,true,1,"Meshtastic");lv_deinit();printf("{\"passed\":true,\"checks\":%u,\"captures\":16,\"fonts\":[22,24],\"themes\":[\"light\",\"dark\"],\"languages\":[\"fr\",\"de\"]}\n",checks);
}
