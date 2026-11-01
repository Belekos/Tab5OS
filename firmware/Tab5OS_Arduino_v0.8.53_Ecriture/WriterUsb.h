#pragma once
#include "WriterKeys.h"
#include "WriterMouse.h"
#include "WriterGamepad.h"
// Started globally during OS setup. Call keyboard/mouse poll only from the UI task.
namespace writing {
bool usbBegin();
bool usbHighSpeed();bool usbHighSpeedActive();bool usbHighSpeed(bool enabled);
bool usbConnected();
// Latest published HID modifier bits, merged across keyboards without consuming events.
uint8_t usbModifiers();
bool usbMouseConnected();
uint32_t usbGameButtons();
// Non-consuming controller snapshot, excluding the physical keyboard.
uint32_t usbGamepadButtons();
bool usbGamepadActivity();
bool usbMousePoll(MouseReport& report);
const char* usbStatus();
bool usbPoll(KeyEvent& event, uint32_t now);
void usbFrench(bool enabled);
void usbMapping(const KeyMapping* data,size_t count);
}
