#include <lvgl.h>
#include <deque>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "WriterUsb.h"
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"mouse check line %d: %s\n",__LINE__,#x);abort();}}while(0)
namespace writing {
static std::deque<MouseReport> queue;static bool attached=true;
bool usbMouseConnected(){return attached;}
bool usbMousePoll(MouseReport& r){if(queue.empty())return false;r=queue.front();queue.pop_front();return true;}
}
#include "Tab5Mouse.cpp"
struct Event {int x,y;uint8_t buttons;int wheel;};
static std::vector<Event> events;
static bool pointer(int x,int y,uint8_t buttons,int wheel){events.push_back({x,y,buttons,wheel});return true;}
static void report(uint8_t buttons,int16_t dx=0,int16_t dy=0,int16_t wheel=0){writing::MouseReport r;r.buttons=buttons;r.x=dx;r.y=dy;r.wheel=wheel;writing::queue.push_back(r);}
int main(){
 lv_init();auto* display=lv_display_create(1280,720);Tab5Mouse::begin(display);Tab5Mouse::pointerEvents(pointer);Tab5Mouse::service();events.clear();
 const int start=Tab5Mouse::x;
 for(unsigned n=0;n<32;++n)report(0,1);CHECK(Tab5Mouse::service());CHECK(Tab5Mouse::x==start+32);const auto hoverDispatches=events.size();
#ifdef TAB5_DISPLAY_BASELINE
 CHECK(hoverDispatches==32);
#else
 CHECK(hoverDispatches==1);
#endif
 // Clamp after EACH report, not after summing deltas; a bounce at an edge matters.
 events.clear();Tab5Mouse::x=1278;report(0,100);report(0,-10);Tab5Mouse::service();CHECK(Tab5Mouse::x==1269);CHECK(events.back().x==1269);
 Tab5Mouse::Config cfg;cfg.speed=25;Tab5Mouse::configure(cfg);Tab5Mouse::x=100;Tab5Mouse::fractionX=0;
 for(unsigned n=0;n<4;++n)report(0,1);Tab5Mouse::service();CHECK(Tab5Mouse::x==101&&Tab5Mouse::fractionX==0);
 cfg.speed=100;Tab5Mouse::configure(cfg);Tab5Mouse::x=100;events.clear();
 // Hover must be dispatched at its own location BEFORE a click changes it.
 report(0,3);report(0,4);report(1,5);report(1,1);report(1,2);report(1,0,0,2);report(0,-1);report(0,1);report(0,1);Tab5Mouse::service();
 std::vector<Event> interactive;for(const auto& e:events)if(e.buttons||e.wheel)interactive.push_back(e);
 CHECK(interactive.size()==4);CHECK(interactive[0].x==112&&interactive[0].buttons==1);
 CHECK(interactive[1].x==113&&interactive[2].x==115&&interactive[3].x==115&&interactive[3].wheel==2);
 bool releaseSeen=false;for(const auto& e:events)if(e.x==114&&!e.buttons&&!e.wheel)releaseSeen=true;CHECK(releaseSeen);CHECK(events.back().x==116&&!events.back().buttons);
#ifndef TAB5_DISPLAY_BASELINE
 CHECK(events.size()==7&&events[0].x==107&&events[1].buttons==1);
#endif
 // Continuous primary drag keeps every point, including repeated held state.
 events.clear();report(1);for(unsigned n=0;n<20;++n)report(1,1,n%2?1:-1);report(0);Tab5Mouse::service();CHECK(events.size()==22);
 for(unsigned n=1;n<=20;++n)CHECK(events[n].x==116+int(n)&&events[n].buttons==1);CHECK(events.back().buttons==0);
 // Preserve the bounded service budget: the 33rd report waits for the next call.
 events.clear();for(unsigned n=0;n<33;++n)report(0,1);Tab5Mouse::service();CHECK(writing::queue.size()==1);Tab5Mouse::service();CHECK(writing::queue.empty());
 // Retain an isolated neutral report: the application hook can cancel a stale scope.
 events.clear();report(0);CHECK(Tab5Mouse::service());CHECK(events.size()==1);
 // Disconnect must release a held pointer once and discard stale queue state.
 report(1);Tab5Mouse::service();events.clear();writing::attached=false;report(1);Tab5Mouse::service();CHECK(events.size()==1&&events[0].buttons==0&&writing::queue.empty());Tab5Mouse::service();CHECK(events.size()==1);
 printf("mouse hover_reports=32 hover_dispatches=%zu drag_points=20 checks=%u\n",hoverDispatches,checks);
 lv_deinit();
}
