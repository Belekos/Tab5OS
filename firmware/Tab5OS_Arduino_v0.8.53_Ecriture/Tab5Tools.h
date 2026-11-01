#pragma once
#include <lvgl.h>
#include "WriterKeys.h"
#include "Tab5OpenWith.h"
enum class Tab5Tool { Calculator, Python, Print, Vault, Keys, Firmware, Executable, Theme, Terminal, Storage, Sdr, AppPanel, Secrets, OpenWith };
struct Tab5ToolsHooks {
 const char* (*translate)(const char*)=nullptr;
 const lv_font_t* (*font)()=nullptr;
 const lv_font_t* (*richFont)(uint16_t,uint8_t)=nullptr;
 bool (*imageLoad)(const char*,int,int,lv_image_dsc_t*,void**)=nullptr;
 void (*imageFree)(void*)=nullptr;
 uint32_t (*color)(unsigned)=nullptr; // background, surface, text, muted, accent
 void (*keyboard)(bool,lv_obj_t*)=nullptr;
 void (*browse)(const char*)=nullptr;
 void (*themePalette)(uint32_t*)=nullptr;
 void (*themesChanged)()=nullptr;
 size_t (*themeCount)()=nullptr;
 const char* (*themeName)(size_t)=nullptr;
 const char* (*themeId)(size_t)=nullptr; // Empty for an immutable built-in.
 bool (*themeRead)(size_t,uint32_t*)=nullptr;
 void (*themeReload)()=nullptr;
 void (*themeApply)(const char* id)=nullptr;
 void (*editSource)(const char*)=nullptr;
 bool (*runExecutable)(const char* path,const char* argument)=nullptr;
 void (*openNativeFile)(Tab5NativeFileApp app,const char* path)=nullptr;
 bool (*beginMapPrint)(unsigned,unsigned,bool,unsigned)=nullptr;
 bool (*sdMounted)()=nullptr;
 const char* (*sdAction)(bool eject)=nullptr; // UI-owned safe mount/eject
};
class Tab5ToolsApp {
 public:
 void clearSecrets();
 void begin(lv_obj_t* parent,Tab5ToolsHooks hooks);
 void open(Tab5Tool tool,const char* path=nullptr);
 // Settings-owned panel, independent of the current application window.
 void mountStorage(lv_obj_t* parent);
 void openMapPrint();
 void openWith(const char* document);
 bool launchExecutable(const char* path,bool configure=false);
 bool openPythonApp(const char* path,const char* argument=nullptr);
 bool resumePythonApp();
 bool pythonForeground()const;
 // Navigation releases LVGL objects while a light Python task keeps its model.
 void suspend();
 void close();
 void resize(int height);
 void service();
 bool key(const writing::KeyEvent& e);
 bool isInput(lv_obj_t*)const;void insert(const char*);void touchKey(writing::Key);
 lv_obj_t* scope()const;
  lv_obj_t* modalScope()const;
 private:struct Impl;Impl* impl_=nullptr;Impl* storage_=nullptr;
};
extern Tab5ToolsApp Tab5Tools;

void tab5RequestNativeTheme();
