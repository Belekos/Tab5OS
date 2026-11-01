#pragma once
#include <lvgl.h>
#include <stdint.h>

namespace Tab5ThemeLive {
// Update existing local colour styles in place. Objects, input state, content,
// scroll offsets and callbacks survive the change. Shared application styles
// (the writing editor) are refreshed by their existing appearance hook.
// Palette order: bg, surface, raised, border, text, muted, active, cursor,
// selected. Call only from the LVGL/UI task.
void apply(lv_obj_t* root,const uint32_t* oldPalette,const uint32_t* newPalette);
}
