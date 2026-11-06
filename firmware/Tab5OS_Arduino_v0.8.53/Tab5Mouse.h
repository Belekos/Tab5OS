#pragma once
#include <lvgl.h>
namespace Tab5Mouse {
struct Config {uint16_t speed=100;uint8_t scroll=3;bool leftHanded=false,natural=false;};
void begin(lv_display_t*);
void configure(const Config&);
void palette(uint32_t pointer,uint32_t outline);
void pageScroll(void (*callback)(int));
void contextMenu(void (*callback)(lv_obj_t* hit,int x,int y));
// Buttons use logical primary=1, secondary=2 and middle=4 after handedness.
// Returning true consumes secondary/wheel handling; LVGL owns primary clicks.
void pointerEvents(bool (*callback)(int x,int y,uint8_t buttons,int wheel));
// Identifies mouse-generated LVGL events without conflating touch or keyboard.
bool isInput(const lv_indev_t*);
bool service();
}
