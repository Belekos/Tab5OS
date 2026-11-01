#pragma once
#include "WriterKeys.h"
namespace writing {
// One UI-task instance. Reading it never consumes the controller's game state.
class GamepadNavigation {
 public:
 bool poll(uint32_t buttons,uint32_t now,bool enabled,KeyEvent& event);
 private:
 uint32_t held_=0,pending_=0,repeatAt_=0;
 bool enabled_=true;
};
bool gamepadPythonCaptured();
void gamepadPythonReset();
int gamepadPythonCall(int operation,int value,void* output,size_t capacity);
}
