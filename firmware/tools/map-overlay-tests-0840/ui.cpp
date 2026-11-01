#include <lvgl.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include "Tab5MapOverlay.h"
#include "Tab5MapMarkerProjection.h"
#include "Tab5MeshBadge.h"
#include "Tab5UiStyle.h"
#include "Tab5Focus.h"
MockMap Tab5Map;bool failAllocation=false;size_t allocatedBytes=0,allocationCalls=0;
unsigned checks=0,invalidations=0,maximumInvalidation=0,clicks=0;bool observe=false;
void check(bool value,const char*msg){++checks;if(!value){fprintf(stderr,"FAIL %u: %s\n",checks,msg);exit(1);}}
static std::vector<uint8_t> pixels(1280*720*3),drawBuffer(1280*100*3);lv_display_t* display;lv_obj_t* screen;
void pump(){lv_tick_inc(30);lv_timer_handler();lv_obj_update_layout(screen);}
void snapshot(const std::string& path){lv_obj_invalidate(screen);lv_refr_now(display);FILE*f=fopen(path.c_str(),"wb");check(f,"capture");fprintf(f,"P6\n1280 720\n255\n");for(size_t i=0;i<pixels.size();i+=3){uint8_t rgb[]={pixels[i+2],pixels[i+1],pixels[i]};fwrite(rgb,1,3,f);}fclose(f);}
size_t colored(uint32_t color,int x0,int y0,int x1,int y1){size_t count=0;for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){size_t i=(size_t(y)*1280+x)*3;uint32_t v=uint32_t(pixels[i])|uint32_t(pixels[i+1])<<8|uint32_t(pixels[i+2])<<16;if(v==color)++count;}return count;}
void projections(){using Tab5MapMarkerProjection::project;int x=0,y=0;
 check(project(0,0,0,0,8,1000,600,x,y)&&x==500&&y==300,"equator center");
 check(project(0,-179.9,0,179.9,3,1000,600,x,y)&&x>500&&x<510,"dateline shortest positive path");
 check(project(0,179.9,0,-179.9,3,1000,600,x,y)&&x<500&&x>490,"dateline shortest negative path");
 check(project(90,0,90,0,4,1000,600,x,y)&&x==500&&y==300,"pole clamp stable");
 for(double v:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){check(!project(v,0,0,0,4,1000,600,x,y),"bad latitude rejected");check(!project(0,v,0,0,4,1000,600,x,y),"bad longitude rejected");check(!project(0,0,v,0,4,1000,600,x,y),"bad center rejected");}
 check(!project(91,0,0,0,4,1000,600,x,y),"range latitude");check(!project(0,181,0,0,4,1000,600,x,y),"range longitude");check(!project(0,0,0,0,25,1000,600,x,y),"zoom bound");check(!project(0,0,0,0,4,0,600,x,y),"empty viewport");check(!project(80,0,0,0,20,1000,600,x,y),"far point clipped");
 for(unsigned z=0;z<25;++z)for(int lat=-85;lat<=85;++lat)for(int lon=-180;lon<=180;lon+=15){check(project(lat,lon,lat,lon,z,1132,600,x,y)&&x==566&&y==300,"all legal center positions");}
}
int main(int argc,char**argv){check(argc==2,"output directory");projections();lv_init();display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,drawBuffer.data(),nullptr,drawBuffer.size(),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*a,uint8_t* data){const unsigned width=lv_area_get_width(a),stride=lv_draw_buf_width_to_stride(width,LV_COLOR_FORMAT_RGB888);for(int y=a->y1;y<=a->y2;++y)memcpy(pixels.data()+(size_t(y)*1280+a->x1)*3,data+size_t(y-a->y1)*stride,width*3);lv_display_flush_ready(d);});
 lv_display_add_event_cb(display,[](lv_event_t*e){if(!observe)return;auto*a=static_cast<lv_area_t*>(lv_event_get_param(e));++invalidations;maximumInvalidation=std::max(maximumInvalidation,unsigned(lv_area_get_width(a)*lv_area_get_height(a)));},LV_EVENT_INVALIDATE_AREA,nullptr);
 screen=lv_obj_create(nullptr);lv_screen_load(screen);lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);Tab5Focus.begin(screen,&lv_font_montserrat_22);
 auto* map=lv_obj_create(screen);lv_obj_remove_style_all(map);lv_obj_set_pos(map,70,80);lv_obj_set_size(map,1132,600);lv_obj_set_style_bg_color(map,lv_color_hex(0x29333b),0);lv_obj_set_style_bg_opa(map,LV_OPA_COVER,0);
 auto* badge=Tab5MeshBadge::create(screen,[](lv_event_t*){++clicks;});lv_obj_set_pos(badge,80,6);
 failAllocation=true;check(!Tab5MapOverlay::attach(map)&&allocatedBytes==0,"OOM safely leaves plain map");failAllocation=false;check(Tab5MapOverlay::attach(map)&&allocatedBytes<=1024,"bounded PSRAM cache");const auto allocations=allocationCalls;check(Tab5MapOverlay::attach(map)&&allocationCalls==allocations,"repeated attach no extra allocation");
 Tab5MapMarker marker;marker.latitude=48;marker.longitude=2;marker.node=42;marker.kind=Tab5MapMarkerKind::Mesh;strcpy(marker.name,"Mesh 42");Tab5Map.markers.push_back(marker);marker.longitude=1.95;marker.kind=Tab5MapMarkerKind::Saved;Tab5Map.markers.push_back(marker);marker.longitude=2.05;marker.kind=Tab5MapMarkerKind::Search;Tab5Map.markers.push_back(marker);
 Tab5MapOverlay::viewport(48,2,12,1132,600);pump();Tab5MapMarker found;check(Tab5MapOverlay::hit(566,290,found)&&found.node==42&&found.kind==Tab5MapMarkerKind::Mesh,"center pin hit precise");check(!Tab5MapOverlay::hit(20,20,found),"empty point misses");
 const uint32_t palettes[][9]={{0xf4f4f0,0xffffff,0xe8e9e6,0xb9bcb6,0x222824,0x69716a,0x427b57,0xffffff,0xd9eadc},{0x101316,0x191e23,0x252c33,0x48545e,0xeaf0f4,0x929da6,0x3e7657,0xffffff,0x284835}};
 for(unsigned theme=0;theme<2;++theme){Tab5UiStyle::configure(palettes[theme],8);Tab5UiStyle::button(badge);lv_obj_set_style_bg_color(screen,lv_color_hex(palettes[theme][0]),0);Tab5MeshBadge::update(true,true,2,"Meshtastic · Nouveau message");pump();snapshot(std::string(argv[1])+"/markers-"+std::to_string(theme)+".ppm");check(colored(0x299956,70,80,1202,680)>50,"mesh marker visible green");check(colored(0xdba02f,70,80,1202,680)>50,"saved marker visible amber");check(colored(0x368cc9,70,80,1202,680)>50,"search marker visible blue");check(colored(0x299956,80,6,144,42)>25,"active badge green");check(colored(palettes[theme][4],119,16,139,32)>15,"unread envelope in same badge");}
 pump();observe=true;unsigned reads=Tab5Map.reads;for(unsigned n=0;n<100;++n){Tab5MapOverlay::refresh();Tab5MeshBadge::update(true,true,2,nullptr);}check(invalidations==0&&reads==Tab5Map.reads,"idle overlay and badge cause no redraw or point queries");
 Tab5Map.markers[0].longitude+=0.001;++Tab5Map.revision;Tab5MapOverlay::refresh();check(invalidations>0&&maximumInvalidation<2000,"moving point invalidates only marker bounds");check(allocationCalls==allocations,"refresh no allocations");observe=false;pump();
 Tab5Map.markers.clear();++Tab5Map.revision;Tab5MapOverlay::refresh();pump();snapshot(std::string(argv[1])+"/markers-cleared.ppm");check(!Tab5MapOverlay::hit(566,290,found)&&colored(0x299956,70,80,1202,680)==0,"removed markers leave no ghost");
 Tab5MeshBadge::update(false,true,0,"Meshtastic");pump();snapshot(std::string(argv[1])+"/badge-ready.ppm");check(colored(palettes[1][5],80,6,144,42)>25&&colored(0x299956,80,6,144,42)==0,"inactive ready badge gray");check(colored(palettes[1][4],119,16,139,32)==0,"read state removes envelope");lv_obj_send_event(badge,LV_EVENT_CLICKED,nullptr);check(clicks==1,"badge invokes open action");
 Tab5Focus.clear();lv_obj_delete(map);check(allocatedBytes==0,"overlay deletion releases cache");lv_obj_delete(badge);check(!Tab5MeshBadge::object(),"badge deletion clears object");lv_deinit();printf("{\"passed\":true,\"checks\":%u,\"maxMovingPinInvalidationPixels\":%u}\n",checks,maximumInvalidation);
}
