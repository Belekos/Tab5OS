#pragma once
#include <lvgl.h>
#include <stdint.h>
namespace Tab5ThemePreview {
// The wire order is the existing JSON COLOR_KEYS order.
enum Role {Bg, Surface, Raised, Border, Text, Muted, Active, Cursor, Selected, Pointer, PointerOutline, SwitchOff, SwitchOn, SwitchKnobOff, SwitchKnobOn, FileIcon, FolderIcon, HiddenFileIcon, HiddenFolderIcon, Count};
const char* roleName(unsigned role);
lv_obj_t* create(lv_obj_t* parent);
void update(lv_obj_t* preview, const uint32_t* palette,unsigned count=9);
}
