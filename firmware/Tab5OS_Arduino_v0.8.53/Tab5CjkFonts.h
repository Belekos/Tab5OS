#pragma once
#include <Arduino.h>
#include <lvgl.h>

// Only the selected CJK repertoire is resident. Latin/Arabic fonts remain the
// primary face, preserving symbols, existing documents and Arabic shaping.
class Tab5CjkFonts {
 public:
  bool select(const String& language,const String& keyboard);
  bool availableFor(const String& id)const;
  // Styled faces can omit scripts present in the regular face (notably
  // Arabic in the supplied oblique fonts). Keep that regular face before
  // the language/keyboard repertoires and the final symbol bitmap.
  void attach(lv_font_t* primary,uint16_t pixels,lv_font_t* regular=nullptr);
  void detach(lv_font_t* primary);
  size_t residentBytes()const{return _bytes[0]+_bytes[1];}
 private:
  struct Parent {lv_font_t* font=nullptr;uint16_t pixels=20;lv_font_t* regular=nullptr;};
  struct Face {lv_font_t* font=nullptr;uint16_t pixels=20;uint8_t bank=0;};
  static const char* repertoire(const String& id);
  lv_font_t* face(uint16_t pixels,uint8_t bank=0);
  void reattach();
  String _id[2];uint8_t* _data[2]={};size_t _bytes[2]={};
  Parent _parents[80];size_t _parentCount=0;
  Face _faces[64];size_t _faceCount=0;
};
extern Tab5CjkFonts Tab5Cjk;
