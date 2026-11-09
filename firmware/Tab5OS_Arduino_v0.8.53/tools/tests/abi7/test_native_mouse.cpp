#include "Tab5AppApi.h"
#include "Tab5AppInputQueue.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <new>
#include <cstdio>
enum {LV_EVENT_PRESSED,LV_EVENT_PRESSING,LV_EVENT_RELEASED,LV_EVENT_PRESS_LOST};
struct lv_point_t{int x,y;};struct lv_indev_t{bool mouse;lv_point_t point;};struct lv_event_t{int code;};
static lv_indev_t mouseInput{true,{600,380}},touchInput{false,{600,380}};
static lv_indev_t* active=&touchInput;
int lv_event_get_code(lv_event_t* e){return e->code;}
lv_indev_t* lv_indev_active(){return active;}
void lv_indev_get_point(lv_indev_t* d,lv_point_t* p){*p=d->point;}
namespace Tab5Mouse{bool isInput(const lv_indev_t* d){return d&&d->mouse;}}
namespace Tab5Resources{struct Budget{size_t psram,psramBlock;};enum class Owner{Tools};}
struct Reservation{operator bool()const{return true;}void allocated(size_t,size_t){}};
Reservation tab5ReserveMemory(Tab5Resources::Owner,const Tab5Resources::Budget&,const char*){return {};}
enum{MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,portMAX_DELAY=0};
void* heap_caps_malloc(size_t n,int){return std::malloc(n);}void* heap_caps_calloc(size_t n,size_t size,int){return std::calloc(n,size);}
void heap_caps_free(void* p){std::free(p);}int inputLock(){return 0;}
void xSemaphoreTake(int,int){}void xSemaphoreGive(int){}
std::atomic<bool> ready{false},keyboardActive{false};
std::atomic<uint32_t> touchButtons{0},requests{0};
bool canvasMode=false,canvasMouseEvents=false,mouseInside=false,dirty=false,shownScreen=true;
uint16_t *front=nullptr,*back=nullptr;unsigned width=0,height=0,received=0,shown=0,lastStats=0;
int canvasLeft=100,canvasTop=80,canvasWidth=1000,canvasHeight=600;
Tab5AppInputQueue* input=nullptr;
bool visible(){return shownScreen;}
void queueInput(const tab5_app_input_event& e){if(input)input->push(e);}
#include "native_mouse.inc"
static void clear(){if(input){input->~Tab5AppInputQueue();heap_caps_free(input);}heap_caps_free(front);heap_caps_free(back);input=nullptr;front=back=nullptr;ready=false;}
int main(){
 assert(!beginMode(0,480,true));assert(!beginMode(801,480,true));assert(!beginMode(800,481,true));
 assert(!beginMode(TAB5_CANVAS_MOUSE_EVENTS|0x4000u|800u,480,true));
 assert(!beginMode(TAB5_CANVAS_MOUSE_EVENTS|800u,0,true));assert(!beginMode(TAB5_CANVAS_MOUSE_EVENTS|320u,200,false));
 assert(beginMode(800,480,true)&&width==800&&height==480&&!canvasMouseEvents);
 keyboardActive=true;assert(!mouse(600,380,4,5,1,0));tab5_app_input_event e{};assert(!input->pop(e));
 active=&mouseInput;lv_event_t pressed{LV_EVENT_PRESSED};canvasPointer(&pressed);
 assert(input->pop(e)&&e.type==TAB5_INPUT_DOWN&&e.x==400&&e.y==240&&(e.modifiers&TAB5_POINTER_MOUSE));clear();
 assert(beginMode(TAB5_CANVAS_MOUSE_EVENTS|800u,480,true)&&width==800&&height==480&&canvasMouseEvents);
 keyboardActive=true;canvasPointer(&pressed);assert(!input->pop(e)); // no duplicate USB click
 active=&touchInput;canvasPointer(&pressed);assert(input->pop(e)&&e.type==TAB5_INPUT_DOWN&&!e.modifiers);
 assert(mouse(600,380,14,-8,3,0));assert(input->pop(e)&&e.type==TAB5_INPUT_POINTER_POSITION&&e.x==400&&e.y==240&&e.key==3);
 assert(input->pop(e)&&e.type==TAB5_INPUT_POINTER_RELATIVE&&e.x==14&&e.y==-8&&e.key==3);
 assert(mouse(1099,380,200,0,1,0));assert(input->pop(e)&&e.x==799);
 assert(input->pop(e)&&e.type==TAB5_INPUT_POINTER_RELATIVE&&e.x==200); // no desktop clamp of raw delta
 assert(!mouse(1100,380,20,0,1,0));assert(input->pop(e)&&e.type==TAB5_INPUT_POINTER_POSITION&&e.key==0);
 assert(input->pop(e)&&e.type==TAB5_INPUT_POINTER_RELATIVE&&!e.key&&!e.x);
 keyboardActive=false;assert(!mouse(600,380,0,0,1,0)&&!input->pop(e));keyboardActive=true;shownScreen=false;
 assert(!mouse(600,380,0,0,1,0)&&!input->pop(e));shownScreen=true;
 clear();puts("Production native mouse callbacks: opt-in dimensions, legacy app isolation, touch/USB routing, raw deltas and release on leaving canvas passed.");
}
