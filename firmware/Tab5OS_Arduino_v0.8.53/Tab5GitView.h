#pragma once
#include <lvgl.h>
#include "WriterKeys.h"
namespace Tab5GitView {
struct Hooks {
 const char* (*translate)(const char*)=nullptr;
 const lv_font_t* (*font)()=nullptr;
 void (*keyboard)(bool,lv_obj_t*)=nullptr;
 bool (*terminal)(const char* directory)=nullptr;
 void (*changed)()=nullptr;
 void (*closed)()=nullptr;
 // Dialogs span the OS surface; the Git view itself lives in Explorer.
 lv_obj_t* dialogParent=nullptr;
 void (*directoryChanged)(const char*)=nullptr;
};
enum class Start { Browse, Create, Import, Initialize };
bool open(lv_obj_t* parent,const char* logicalDirectory,Hooks hooks,Start start=Start::Browse,const char* selectedPath=nullptr);
// Coalesce Explorer navigation until the current worker has released its state.
void context(const char* logicalDirectory,const char* selectedPath=nullptr);
lv_obj_t* modalScope();
void close();
void service();
void resize(int availableHeight);
bool key(const writing::KeyEvent& event);
lv_obj_t* scope();
bool busy();
// Cooperative cancellation only: safe from power-management polling, no
// widget destruction, blocking wait, or forced task deletion.
void cancel();
void clearOutput();
}
