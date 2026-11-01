#pragma once
#include <lvgl.h>
#include <stdint.h>

// Native controls share the OS palette, including their checked and focused
// parts. Plain panels and app-specific semantic button fills stay untouched.
namespace Tab5UiStyle {
void configure(const uint32_t* palette9,uint8_t cornerRadius,uint32_t switchOff=UINT32_MAX,uint32_t switchOn=UINT32_MAX,uint32_t knobOff=UINT32_MAX,uint32_t knobOn=UINT32_MAX);
void switchColors(lv_obj_t* object,uint32_t trackOff,uint32_t trackOn,uint32_t knobOff,uint32_t knobOn);
uint32_t color(unsigned role);
uint8_t radius();
void control(lv_obj_t* object);
// Standard buttons opt in to all state colors; semantic app fills stay explicit.
void button(lv_obj_t* object);
// Highlight states only: keeps app-defined normal/semantic fills.
void selection(lv_obj_t* object);
void tree(lv_obj_t* root);
// Called only for physical pointer/keyboard events. Hover never changes a value.
void pointer(lv_indev_t* input,int x,int y,uint8_t buttons);
void keyboardMode();
void pointerLeave();
// Register once: touching a control is a press, not persistent mouse hover.
void touchInput(lv_indev_t* input);
}
