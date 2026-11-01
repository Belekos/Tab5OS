#include "Tab5MapOverlay.h"
#include "Tab5MapMarkerProjection.h"
#include <esp_heap_caps.h>
#include <algorithm>
namespace Tab5MapOverlay {
namespace {
constexpr unsigned Capacity=128;
struct Point {int16_t x=0,y=0;uint8_t kind=0;bool visible=false;};
lv_obj_t* image=nullptr;Point* points=nullptr;double latitude=0,longitude=0;unsigned zoom=0;int width=0,height=0;uint32_t revision=0;bool dirty=true;
void invalidate(const Point& p){if(!image||!p.visible)return;lv_area_t a;lv_obj_get_coords(image,&a);a.x2=a.x1+p.x+14;a.y2=a.y1+p.y+14;a.x1+=p.x-14;a.y1+=p.y-24;lv_obj_invalidate_area(image,&a);}
void draw(lv_event_t* e){
 if(!points)return;auto* layer=lv_event_get_layer(e);lv_area_t imageArea;lv_obj_get_coords(image,&imageArea);
 for(unsigned i=0;i<Capacity;++i){const auto& p=points[i];if(!p.visible)continue;const int x=imageArea.x1+p.x,y=imageArea.y1+p.y;
  const uint32_t rgb=p.kind==uint8_t(Tab5MapMarkerKind::Mesh)?0x299956:p.kind==uint8_t(Tab5MapMarkerKind::Saved)?0xdba02f:0x368cc9;
  lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);line.color=lv_color_hex(0x172026);line.width=3;line.p1={lv_value_precise_t(x),lv_value_precise_t(y-10)};line.p2={lv_value_precise_t(x),lv_value_precise_t(y+3)};lv_draw_line(layer,&line);
  lv_draw_rect_dsc_t dot;lv_draw_rect_dsc_init(&dot);dot.radius=LV_RADIUS_CIRCLE;dot.bg_color=lv_color_hex(rgb);dot.bg_opa=LV_OPA_COVER;dot.border_width=2;dot.border_color=lv_color_white();dot.outline_width=1;dot.outline_color=lv_color_hex(0x172026);dot.outline_opa=LV_OPA_COVER;
  lv_area_t area={x-8,y-19,x+8,y-3};lv_draw_rect(layer,&dot,&area);
  dot.bg_color=lv_color_white();dot.border_width=0;dot.outline_width=0;lv_area_t center={x-2,y-13,x+2,y-9};lv_draw_rect(layer,&dot,&center);
 }
}
}
bool attach(lv_obj_t* value){
 if(image==value)return points!=nullptr;if(image)return false;
 points=static_cast<Point*>(heap_caps_calloc(Capacity,sizeof(Point),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!points)return false;image=value;dirty=true;
 lv_obj_add_event_cb(image,draw,LV_EVENT_DRAW_MAIN_END,nullptr);
 lv_obj_add_event_cb(image,[](lv_event_t*){image=nullptr;heap_caps_free(points);points=nullptr;},LV_EVENT_DELETE,nullptr);return true;
}
void viewport(double lat,double lon,unsigned z,int w,int h){if(lat!=latitude||lon!=longitude||z!=zoom||w!=width||h!=height){latitude=lat;longitude=lon;zoom=z;width=w;height=h;dirty=true;}refresh();}
void refresh(){
 if(!image||!points)return;const auto next=Tab5Map.markerRevision();if(!dirty&&next==revision)return;dirty=false;revision=next;
 const auto count=std::min(size_t(Capacity),Tab5Map.markerCount());
 for(unsigned i=0;i<Capacity;++i){Point p;Tab5MapMarker marker;int x=0,y=0;if(i<count&&Tab5Map.marker(i,marker)&&Tab5MapMarkerProjection::project(marker.latitude,marker.longitude,latitude,longitude,zoom,width,height,x,y)){p.x=x;p.y=y;p.kind=uint8_t(marker.kind);p.visible=true;}
  auto& old=points[i];if(old.x==p.x&&old.y==p.y&&old.kind==p.kind&&old.visible==p.visible)continue;invalidate(old);old=p;invalidate(old);
 }
}
bool hit(int x,int y,Tab5MapMarker& result){
 int nearest=24*24;bool found=false;const auto count=std::min(size_t(Capacity),Tab5Map.markerCount());
 for(size_t i=0;i<count;++i){Tab5MapMarker m;int px=0,py=0;if(!Tab5Map.marker(i,m)||!Tab5MapMarkerProjection::project(m.latitude,m.longitude,latitude,longitude,zoom,width,height,px,py))continue;const int dx=px-x,dy=py-10-y,d=dx*dx+dy*dy;if(d<=nearest){nearest=d;result=m;found=true;}}
 return found;
}
}
