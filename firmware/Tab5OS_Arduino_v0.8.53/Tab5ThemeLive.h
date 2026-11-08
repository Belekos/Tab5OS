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
// Record a known palette role for a local style, including when roles share RGB.
// Tracks the role without rebuilding the object; released with it. Roles: 0-8.
void bind(lv_obj_t* object,lv_style_prop_t property,lv_style_selector_t selector,unsigned role);
// User colour samples own their colours, including descendants of a preview.
// Both automatic recolouring and the generic control-style pass honor this marker.
void preserve(lv_obj_t* object);
bool preserved(lv_obj_t* object);
}
