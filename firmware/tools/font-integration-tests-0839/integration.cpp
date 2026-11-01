#include <lvgl.h>
#include "Arduino.h"
#include "font_test_platform.h"
#include "Tab5FontDefaults.h"
#include "Tab5CjkFonts.h"
#include "Tab5DocumentText.h"
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <climits>
static unsigned checks=0;
static void check(bool b){++checks;if(!b){std::cerr<<"failure check "<<checks<<"\n";std::abort();}}
static void file(const char* path,const std::string& host){std::ifstream f(host,std::ios::binary);files[path].bytes={std::istreambuf_iterator<char>(f),{}};check(files[path].bytes.size()>4096);}
static constexpr const char* FONT_FILE="/System/font.ttf";
static constexpr const char* FONT_DIR="/System/fonts";
static constexpr const char* FONT_BOLD_FILE="/System/fonts/bold.ttf";
static constexpr const char* FONT_ITALIC_FILE="/System/fonts/italic.ttf";
static constexpr const char* FONT_BOLDITALIC_FILE="/System/fonts/bolditalic.ttf";
struct AppSettings {uint8_t fontIndex=1;char interfaceFontPath[261]{},documentFontPath[261]{};}settings;
static bool sdReady=true,smoothFontReady=false,configuredFontWarning=false;
static uint8_t* ttfData=nullptr;static size_t ttfDataSize=0;
static lv_font_t *uiTtfFont=nullptr,*docTtfFont=nullptr,*keyTtfFont=nullptr,*readerTtfFont=nullptr;
static Tab5FontDefaults::Sources configuredFontSources;
static lv_font_t *uiStockFont=nullptr,*docStockFont=nullptr,*keyStockFont=nullptr,*readerStockFont=nullptr,*headingTtfFont=nullptr,*headingStockFont=nullptr;
struct RichFontBlob{uint8_t* data=nullptr;size_t size=0;};
struct RichFontCacheEntry{uint16_t px=0;uint8_t variant=0;lv_font_t* font=nullptr;};
static RichFontBlob richFontBlob[4];static RichFontCacheEntry richFontCache[28];static lv_font_t* richStockFonts[28]{};
static uint8_t richFontCacheCount=0;static bool richFontSourceTried[4]{};
namespace epub_render {constexpr unsigned TXT_BOLD=1,TXT_ITALIC=2;}
struct Locale{String selectedId(){return "en-US";}}Tab5Locale,Tab5Keyboards;
static unsigned createCalls=0,failCreate=0;static std::vector<const void*> createdSources;
static lv_font_t* createFace(const void* data,size_t bytes,int32_t pixels,lv_font_kerning_t kerning,uint32_t cache){
 ++createCalls;createdSources.push_back(data);if(createCalls==failCreate)return nullptr;
 return lv_tiny_ttf_create_data_ex(data,bytes,pixels,kerning,cache);
}
static lv_obj_t* defaultFontButtons[2]{};static unsigned settingsMarks=0;static std::string lastToast;static bool keyboard=false;
static const char* T(const char* s){return s;}static void toast(const char* s,unsigned=0){lastToast=s?s:"";}static void markSettingsDirty(){++settingsMarks;}static void setKeyboardVisible(bool b){keyboard=b;}
namespace Tab5FilePicker{
using Result=void(*)(const char*,void*);static Result callback=nullptr;static void* context=nullptr;static std::string directory,suffix;
static bool open(const char* d,const char* s,bool folders,Result cb,void* c){check(!folders);directory=d;suffix=s;callback=cb;context=c;return true;}
}
#define lv_tiny_ttf_create_data_ex createFace
#include "helpers.inc"
#undef lv_tiny_ttf_create_data_ex
static bool glyph(lv_font_t* font,uint32_t point){lv_font_glyph_dsc_t d{};return lv_font_get_glyph_dsc(font,&d,point,0)&&!d.is_placeholder;}
static void destroy(lv_font_t* font){if(font){Tab5Cjk.detach(font);lv_tiny_ttf_destroy(font);}}
int main(int argc,char** argv){
 check(argc==2);const std::string sd=argv[1];file(FONT_FILE,sd+"/System/font.ttf");
 for(const char* name:{"bold.ttf","italic.ttf","bolditalic.ttf","cjk-ja.ttf","cjk-ko.ttf","cjk-zh.ttf"})file((std::string(FONT_DIR)+"/"+name).c_str(),sd+"/System/fonts/"+name);
 for(const char* name:{"cjk-ja.ttf","cjk-ko.ttf","cjk-zh.ttf"}){const auto& data=files[std::string(FONT_DIR)+"/"+name].bytes;check(Tab5FontDefaults::validate(data.data(),data.size()));}
 file("/Custom/UI.ttf",sd+"/System/fonts/italic.ttf");file("/Custom/Document.ttf",sd+"/System/fonts/bold.ttf");
 std::strcpy(settings.interfaceFontPath,"/Custom/UI.ttf");std::strcpy(settings.documentFontPath,"/Custom/Document.ttf");
 lv_init();check(lv_display_create(1280,720));
 check(loadSmoothFonts());check(smoothFontReady);check(!configuredFontWarning);check(uiTtfFont&&docTtfFont&&uiStockFont&&docStockFont);
 check(uiTtfFont->fallback==uiStockFont&&docTtfFont->fallback==docStockFont);check(uiFont()==uiTtfFont&&documentFont()==docTtfFont);
 check(createCalls==4);check(createdSources[1]==configuredFontSources.custom(Tab5FontDefaults::Role::Interface)->data);check(createdSources[3]==configuredFontSources.custom(Tab5FontDefaults::Role::Document)->data);
 check(!keyTtfFont&&!readerTtfFont&&!headingTtfFont);
 check(ensureKeyFont());check(ensureReaderFont());check(keyFont()==keyTtfFont&&readerFont()==readerTtfFont);
 auto calls=createCalls;check(ensureKeyFont()&&ensureReaderFont());check(createCalls==calls);
 check(uiHeadingFont()==headingTtfFont&&headingTtfFont);check(createdSources.back()==configuredFontSources.custom(Tab5FontDefaults::Role::Interface)->data);
 calls=createCalls;check(uiHeadingFont()==headingTtfFont);check(createCalls==calls);
 for(auto* face:{uiTtfFont,docTtfFont,keyTtfFont,readerTtfFont,headingTtfFont})for(uint32_t point:{uint32_t('A'),uint32_t(0xe9),uint32_t(0x03a9),uint32_t(0x0416),uint32_t(0x0627),uint32_t(0xfe8d)})check(glyph(face,point));
 for(unsigned px:{20u,24u,28u,32u}){resizeConfiguredFont(docTtfFont,docStockFont,px);check(docTtfFont->line_height==docStockFont->line_height);check(docTtfFont->fallback==docStockFont);check(glyph(docTtfFont,0x0627));}
 resizeConfiguredFont(readerTtfFont,readerStockFont,28);check(readerTtfFont->line_height==readerStockFont->line_height);
 auto* regular=const_cast<lv_font_t*>(richFontFor(24,0));check(regular&&regular!=readerTtfFont);check(regular->fallback==richStockFonts[0]);check(createdSources.back()==configuredFontSources.custom(Tab5FontDefaults::Role::Document)->data);
 check(richFontFor(24,0)==regular);auto* styled=const_cast<lv_font_t*>(richFontFor(24,epub_render::TXT_ITALIC));check(styled->fallback==regular);check(glyph(styled,0x0627));
 for(unsigned px=14;px<50&&richFontCacheCount<28;++px)(void)richFontFor(px,0);
 check(richFontCacheCount==28);calls=createCalls;check(richFontFor(58,0));check(createCalls==calls);
 check(Tab5Cjk.select("ja-JP","ko-KR"));
 for(unsigned i=0;i<richFontCacheCount;++i){auto* face=richFontCache[i].font;check(glyph(face,0x4e00));check(glyph(face,0xac00));check(glyph(face,0x0627));}
 check(Tab5Cjk.select("en-US","en-US"));
 for(unsigned i=0;i<richFontCacheCount;++i){auto* face=richFontCache[i].font;check(!glyph(face,0x4e00));check(glyph(face,0x0627));}
 // Failure of custom face creation retains the live standard face.
 configuredFontWarning=false;lv_font_t* fallback=nullptr;failCreate=createCalls+2;auto* chosen=configuredFont(Tab5FontDefaults::Role::Interface,20,96,fallback);check(chosen&&!fallback&&configuredFontWarning);check(glyph(chosen,0x0627));destroy(chosen);
 failCreate=createCalls+1;fallback=uiStockFont;chosen=configuredFont(Tab5FontDefaults::Role::Interface,20,96,fallback);check(!chosen&&!fallback);failCreate=0;
 // Real LVGL button events dispatch the production settings callbacks.
 for(unsigned role=0;role<2;++role){defaultFontButtons[role]=lv_button_create(lv_screen_active());lv_label_create(defaultFontButtons[role]);lv_obj_add_event_cb(defaultFontButtons[role],chooseDefaultFont,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(role)));}
 refreshDefaultFontButtons();check(std::string(lv_label_get_text(lv_obj_get_child(defaultFontButtons[0],0)))=="UI.ttf");
 auto oldUI=uiTtfFont;keyboard=true;lv_obj_send_event(defaultFontButtons[0],LV_EVENT_CLICKED,nullptr);check(!keyboard&&Tab5FilePicker::directory==FONT_DIR&&Tab5FilePicker::suffix==".ttf");
 Tab5FilePicker::callback(FONT_BOLD_FILE,Tab5FilePicker::context);check(std::string(settings.interfaceFontPath)==FONT_BOLD_FILE);check(settingsMarks==1&&uiTtfFont==oldUI);check(lastToast=="Redémarrez pour appliquer les polices");
 Tab5FilePicker::callback("/Missing.ttf",Tab5FilePicker::context);check(settingsMarks==1&&std::string(settings.interfaceFontPath)==FONT_BOLD_FILE);
 files["/LargeA.ttf"]=files[FONT_FILE];files["/LargeA.ttf"].bytes.resize(5u*1024u*1024u);files["/LargeB.ttf"]=files["/LargeA.ttf"];
 std::strcpy(settings.documentFontPath,"/LargeB.ttf");Tab5FilePicker::callback("/LargeA.ttf",Tab5FilePicker::context);
 check(settingsMarks==1&&std::string(settings.interfaceFontPath)==FONT_BOLD_FILE);check(lastToast=="Les deux polices dépassent le budget mémoire");
 std::strcpy(settings.documentFontPath,"/Missing.ttf");Tab5FilePicker::callback("/LargeA.ttf",Tab5FilePicker::context);check(settingsMarks==2&&std::string(settings.interfaceFontPath)=="/LargeA.ttf");check(uiTtfFont==oldUI);
 lv_obj_send_event(defaultFontButtons[1],LV_EVENT_CLICKED,nullptr);Tab5FilePicker::callback("/LargeA.ttf",Tab5FilePicker::context);check(settingsMarks==3&&std::string(settings.documentFontPath)=="/LargeA.ttf");
 auto* reset=lv_button_create(lv_screen_active());lv_obj_add_event_cb(reset,resetDefaultFont,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(1)));lv_obj_send_event(reset,LV_EVENT_CLICKED,nullptr);check(!*settings.documentFontPath&&settingsMarks==4);check(docTtfFont&&docTtfFont->fallback==docStockFont);check(std::string(lv_label_get_text(lv_obj_get_child(defaultFontButtons[1],0)))=="Police standard");
 // Explicit main-setting code is exercised above; no reboot or GUI object replacement occurs.
 std::set<lv_font_t*> fonts{uiTtfFont,docTtfFont,keyTtfFont,readerTtfFont,headingTtfFont,uiStockFont,docStockFont,keyStockFont,readerStockFont,headingStockFont};for(unsigned i=0;i<richFontCacheCount;++i){fonts.insert(richFontCache[i].font);if(richStockFonts[i])fonts.insert(richStockFonts[i]);}for(auto* f:fonts)destroy(f);
 for(unsigned i=1;i<4;++i)heap_caps_free(richFontBlob[i].data);heap_caps_free(ttfData);lv_deinit();
 std::cout<<"PASS "<<checks<<" production font integration checks\n";
}
