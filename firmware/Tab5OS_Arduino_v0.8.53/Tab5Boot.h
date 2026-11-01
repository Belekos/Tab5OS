#pragma once
#include "Tab5Serial.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include "Tab5Diagnostics.h"

// Direct rendering ends before the first LVGL refresh. No heap or task is needed.
class Tab5BootStatus {
 public:
  enum Stage : uint8_t { Hardware, Storage, Settings, Language, Keyboard, Usb, Audio, Video,
                        Maps, Clock, Graphics, Fonts, Interface, Integrity, Count };
  void begin(uint32_t started) {
    started_=started; visible_=true; total_=0;
    for(unsigned i=0;i<Count;++i) {
      state_[i]=0; elapsed_[i]=0; internalUsed_[i]=externalUsed_[i]=0;
    }
    drawFrame();
  }
  void start(Stage stage) {
    current_=stage; since_=stage==Hardware?started_:millis(); state_[stage]=1;
    tab5DiagnosticMark(0,names_[stage],"",1);
    beforeInternal_=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    beforeExternal_=heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    drawStage(stage);
  }
  void finish(bool ok=true) {
    elapsed_[current_]=millis()-since_; state_[current_]=ok?2:3;
    internalUsed_[current_]=int32_t(beforeInternal_)-int32_t(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    externalUsed_[current_]=int32_t(beforeExternal_)-int32_t(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    Tab5DebugSerial.printf("[BOOT] %s: %s (%lu ms)\n",names_[current_],ok?"OK":"CHECK",(unsigned long)elapsed_[current_]);
    Tab5DebugSerial.printf("[BOOT][MEM] %s: internal delta=%ld B; PSRAM delta=%ld B (positive=allocated)\n",
      names_[current_],long(internalUsed_[current_]),long(externalUsed_[current_]));
    drawStage(current_); drawProgress();
  }
  uint32_t duration(Stage s)const{return elapsed_[s];}
  int32_t internalUsed(Stage s)const{return internalUsed_[s];}
  int32_t psramUsed(Stage s)const{return externalUsed_[s];}
  void ready() {
    total_=millis()-started_; visible_=false;
    tab5DiagnosticMark(0,"Home ready","",0);
    Tab5DebugSerial.printf("[BOOT] Home ready in %lu ms\n",(unsigned long)total_);
  }
  uint32_t totalMs() const { return total_; }
  bool healthy() const { return state_[Integrity]==2; }
  const char* slowestName() const { return names_[slowest()]; }
  uint32_t slowestMs() const { return elapsed_[slowest()]; }
 private:
  static constexpr uint32_t Background=0x111111, Foreground=0xDCDCDC,
                            Muted=0x787878, Rule=0x343434, Warning=0xC3AA83;
  unsigned slowest() const { unsigned index=0; for(unsigned i=1;i<Count;++i)if(elapsed_[i]>elapsed_[index])index=i;return index; }
  int rowPitch()const{return (M5.Display.height()-260)/Count;}
  void drawFrame() {
    if(!visible_)return;
    auto& d=M5.Display;
    d.startWrite();
    d.fillScreen(Background); d.setTextSize(3); d.setTextColor(Foreground,Background);
    d.setCursor(56,32); d.print("Tab5OS");
    d.setTextSize(2); d.setTextColor(Muted,Background);
    d.setCursor(56,76); d.print("Starting");
    d.fillRect(56,108,d.width()-112,1,Rule);
    d.endWrite();
    for(unsigned i=0;i<Count;++i)drawStage(i);
    drawProgress();
  }
  void drawStage(unsigned stage) {
    if(!visible_)return;
    auto& d=M5.Display;
    const int y=130+int(stage)*rowPitch();
    const uint32_t color=state_[stage]==3?Warning:state_[stage]?Foreground:Muted;
    d.startWrite();
    d.fillRect(56,y,d.width()-112,rowPitch(),Background);
    d.fillRect(56,y+5,6,6,color);
    d.setTextSize(2); d.setTextColor(color,Background);
    d.setCursor(80,y); d.print(names_[stage]);
    d.setCursor(d.width()-384,y);
    d.print(state_[stage]==3?"Check":state_[stage]==2?"Ready":state_[stage]==1?"Starting":"Waiting");
    if(state_[stage]>=2) {
      d.setTextColor(Muted,Background); d.setCursor(d.width()-156,y);
      d.printf("%lu ms",(unsigned long)elapsed_[stage]);
    }
    d.endWrite();
  }
  void drawProgress() {
    if(!visible_)return;
    auto& d=M5.Display;
    unsigned complete=0,checks=0;
    for(unsigned i=0;i<Count;++i){if(state_[i]>=2)++complete;if(state_[i]==3)++checks;}
    d.startWrite();
    d.fillRect(56,d.height()-96,d.width()-112,3,Rule);
    if(complete)d.fillRect(56,d.height()-96,int((d.width()-112)*complete/Count),3,Foreground);
    d.fillRect(56,d.height()-64,d.width()-112,22,Background);
    d.setTextSize(2); d.setTextColor(checks?Warning:Muted,Background); d.setCursor(56,d.height()-64);
    d.printf("%u / %u",complete,unsigned(Count));
    if(checks)d.printf("    %u need attention",checks);
    d.endWrite();
  }
  static constexpr const char* names_[Count]={"Display and hardware","SD card","Preferences","Interface language","Keyboard layout","USB keyboard",
    "Audio service","Video service","Map service","Clock","LVGL display","Fonts","Interface","Heap integrity"};
  uint8_t state_[Count]{}; uint32_t elapsed_[Count]{};
  int32_t internalUsed_[Count]{},externalUsed_[Count]{};
  uint32_t beforeInternal_=0,beforeExternal_=0;
  Stage current_=Hardware; uint32_t started_=0,since_=0,total_=0; bool visible_=false;
};
