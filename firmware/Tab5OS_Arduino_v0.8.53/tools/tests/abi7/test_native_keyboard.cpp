#include "Tab5AppApi.h"
#include "Tab5KeyboardState.h"
#include "Tab5AppInputQueue.h"
#include "WriterKeys.h"
#include <assert.h>
#include <atomic>
#include <mutex>
#include <stdio.h>
#include <thread>
#define taskENTER_CRITICAL(guard) (guard)->lock()
#define taskEXIT_CRITICAL(guard) (guard)->unlock()
namespace writing {
Tab5KeyboardState<8> physicalKeys;
std::mutex physicalKeysLock;
// The real USB snapshot wrapper, with a host mutex replacing its portMUX.
#include "usb_snapshot.inc"
}
enum {LV_EVENT_SCREEN_LOADED,LV_EVENT_SCREEN_UNLOAD_START,LV_EVENT_SCREEN_UNLOADED,LV_EVENT_DELETE,LV_OBJ_FLAG_HIDDEN};
struct lv_obj_t {bool hidden=true;};
struct lv_event_t {int code;lv_obj_t* target;lv_obj_t* current;};
int lv_event_get_code(lv_event_t* event){return event->code;}
lv_obj_t* lv_event_get_target_obj(lv_event_t* event){return event->target;}
lv_obj_t* lv_event_get_current_target_obj(lv_event_t* event){return event->current;}
bool lv_obj_has_flag(lv_obj_t* object,int){return object->hidden;}
bool canvasMode=false;
std::atomic<bool> keyboardActive{false};
std::atomic<uint32_t> requests{0};
uint8_t (*getVolume)()=nullptr;
uint8_t unmutedVolume=150;
lv_obj_t controlPanel;lv_obj_t* controls=&controlPanel;
unsigned stops=0,toggles=0;int changedVolume=0;
void toggleControls(){++toggles;controls->hidden=!controls->hidden;}
void changeVolume(int value){changedVolume=value;}
void tab5ExecutableStop(){++stops;}
Tab5AppInputQueue queue;
void queueInput(const tab5_app_input_event& event){queue.push(event);}
// Real LVGL visibility handler, worker snapshot callback and ABI2/6 key router.
#include "native_keyboard.inc"

int main(){
 using namespace writing;
 uint8_t held[32]{};held[41/8]|=1u<<(41%8);
 physicalKeys.update(0,held,0x05,true);
 tab5_app_keyboard_state state{};state.size=sizeof(state);
 assert(keyboardState(&state)==1&&!state.connected&&!state.modifiers);
 lv_obj_t screen,child;lv_event_t event{LV_EVENT_SCREEN_LOADED,&screen,&screen};
 canvasMode=true;keyboardScreenEvent(&event);
 assert(keyboardState(&state)==1&&state.connected&&state.modifiers==0x05&&!memcmp(state.keys,held,32));
 event={LV_EVENT_DELETE,&child,&screen};keyboardScreenEvent(&event);
 assert(keyboardState(&state)==1&&state.connected); // Ignore bubbled child events.
 event={LV_EVENT_SCREEN_UNLOAD_START,&screen,&screen};keyboardScreenEvent(&event);
 assert(keyboardState(&state)==1&&!state.connected&&!state.modifiers);
 for(auto key:state.keys)assert(!key);
 event.code=LV_EVENT_SCREEN_LOADED;keyboardScreenEvent(&event);assert(keyboardState(&state)==1&&state.connected);
 event.code=LV_EVENT_SCREEN_UNLOADED;keyboardScreenEvent(&event);assert(keyboardState(&state)==1&&!state.connected);
 canvasMode=false;event.code=LV_EVENT_SCREEN_LOADED;keyboardScreenEvent(&event);assert(keyboardState(&state)==1&&!state.connected);
 state.size=sizeof(state)-1;assert(keyboardState(&state)==-1&&keyboardState(nullptr)==-1);
 // The old canvas queue still carries Escape/text; game Escape still stops.
 KeyEvent keyEvent{};keyEvent.key=Key::Escape;canvasMode=true;key(keyEvent);
 tab5_app_input_event input{};assert(queue.pop(input)&&input.type==TAB5_INPUT_KEY&&input.key==TAB5_KEY_ESCAPE&&!stops);
 keyEvent.key=Key::Text;strcpy(keyEvent.text,"x");keyEvent.ctrl=true;keyEvent.alt=true;key(keyEvent);
 assert(queue.pop(input)&&input.modifiers==(TAB5_MOD_CTRL|TAB5_MOD_ALT)&&!strcmp(input.text,"x"));
 keyEvent.key=Key::F1;key(keyEvent);assert(!toggles&&!queue.pop(input));
 canvasMode=false;keyEvent.key=Key::Escape;key(keyEvent);assert(stops==1);
 controls->hidden=false;key(keyEvent);assert(stops==1&&toggles==1&&controls->hidden);
 keyEvent.key=Key::PageUp;key(keyEvent);assert(changedVolume==163);
 keyEvent.key=Key::Text;strcpy(keyEvent.text,"s");keyEvent.ctrl=true;keyEvent.alt=false;key(keyEvent);assert(requests.load()==(1u<<9));
 // Coherent publication while the USB callback and ELF worker run together.
 std::atomic<bool> finished{false};
 std::thread producer([&]{for(unsigned n=0;n<20000;++n){uint8_t bytes[32];memset(bytes,n&1?0x55:0xaa,32);
  std::lock_guard<std::mutex> guard(physicalKeysLock);physicalKeys.update(0,bytes,bytes[0],true);
 }finished=true;});
 unsigned snapshots=0;
 do{
  uint8_t bytes[32],modifiers=0;assert(usbKeyboardState(true,bytes,modifiers));
  if(modifiers==0x55||modifiers==0xaa)for(auto value:bytes)assert(value==modifiers);
  ++snapshots;
 }while(!finished.load());
 producer.join();assert(snapshots>0);
 puts("Production keyboard callbacks: coherent concurrent reports, LVGL focus transitions, ABI6 Escape/text and legacy game controls passed.");
}
