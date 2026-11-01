#pragma once
#include <lvgl.h>
#include "WriterKeys.h"

// A UI-owned PIN dialog. The requesting application stays in place and its
// continuation is called only after the protected fields have been destroyed.
namespace Tab5VaultPrompt {
struct Hooks {
 const char* (*translate)(const char*)=nullptr;
 const lv_font_t* (*font)()=nullptr;
 uint32_t (*color)(unsigned)=nullptr; // background, surface, text, muted, accent
 void (*keyboard)(bool,lv_obj_t*)=nullptr;
};
using Done=void (*)(bool,void*);
void begin(Hooks hooks);
// Returns false if a prompt/job already owns the vault. A successful no-dialog
// request invokes done immediately; otherwise service invokes it on completion.
bool open(Done done=nullptr,void* context=nullptr);
void close();
lv_obj_t* scope();
bool key(const writing::KeyEvent& event);
void service();
}
