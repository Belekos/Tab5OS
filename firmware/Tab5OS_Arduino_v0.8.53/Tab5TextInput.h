#pragma once
#include <lvgl.h>
#include "WriterKeys.h"
namespace Tab5TextInput {
bool copy(const char* text, size_t length, bool markdown=false);
// Explicit password-manager copy; expires after 30 seconds or a vault lock.
bool copySecret(const char* text,size_t length);
void clearSecrets();
void serviceSecrets();
// Keep credentials out of edit history and ordinary copy even when revealed.
void protect(lv_obj_t* field);
bool protectedField(lv_obj_t* field);
const char* clipboard();
bool markdown();
void setReadOnly(lv_obj_t* field);
bool readOnly(lv_obj_t* field);
void selectable(lv_obj_t* label);
bool hasSelection(lv_obj_t* object);
bool copySelection(lv_obj_t* object);
// Bounded edit history, excluded from passwords and read-only reports.
void track(lv_obj_t* field);
bool canUndo(lv_obj_t* field);
bool canRedo(lv_obj_t* field);
bool insert(lv_obj_t*,const char*);
bool key(lv_obj_t* field,const writing::KeyEvent& event);
}
