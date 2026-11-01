#include <lvgl.h>
#include "WriterView.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
using namespace writing;
static unsigned checks=0,frames=0,flushes=0;static std::string shotDir;static std::vector<uint8_t> pixels(1280u*720u*3u),fontData;static lv_font_t *font22,*font24;static lv_display_t* display;static lv_obj_t*screen;
static void check(bool v,const char* why){++checks;if(!v){fprintf(stderr,"FAIL %u: %s\n",checks,why);exit(1);}}
static const lv_font_t* font(uint16_t size,uint8_t){return size>=24?font24:font22;}
static void pump(){lv_tick_inc(20);lv_timer_handler();lv_obj_update_layout(screen);}
static void snapshot(const std::string&name){lv_obj_invalidate(screen);lv_refr_now(display);FILE*f=fopen((shotDir+"/"+name+".ppm").c_str(),"wb");check(f,"screenshot opens");fprintf(f,"P6\n1280 720\n255\n");for(size_t i=0;i<pixels.size();i+=3){uint8_t rgb[]={pixels[i+2],pixels[i+1],pixels[i]};fwrite(rgb,1,3,f);}fclose(f);++frames;}
static void label(lv_obj_t*p,const char*text,int x,int y){auto*l=lv_label_create(p);lv_label_set_text(l,text);lv_obj_set_style_text_font(l,font22,0);lv_obj_set_pos(l,x,y);}
static void render(unsigned size,bool dark){
 lv_obj_clean(screen);const uint32_t surface=dark?0x1e282b:0xffffff,background=dark?0x151f22:0xf4f4f0,foreground=dark?0xf1f3f5:0x222824;
 lv_obj_set_style_bg_color(screen,lv_color_hex(background),0);lv_obj_set_style_text_color(screen,lv_color_hex(foreground),0);label(screen,"Éditeur · C, Python et JSON",20,12);label(screen,"Coloration propre à chaque langage · source UTF-8 préservée",20,48);
 const char* names[]={"C · program.c","Python · script.py","JSON · settings.json"};
 const char* paths[]={"program.c","script.py","settings.json"};
 const char* source[]={"#include <stdint.h>\n#define LIMIT 16\n\n/* Mesure du capteur\n   sur deux lignes */\nuint32_t sample = 42;\n\nint read_value(void) {\n    printf(\"été\\n\");\n    return sample;\n}\n", "@cache\ndef greet(name: str):\n    \"\"\"Texte multiligne\n    français et Unicode\n    \"\"\"\n    if name is None:\n        return False\n    print(len(name))\n    return f\"été: {name}\"\n", "{\n  \"name\": \"été\",\n  \"enabled\": true,\n  \"count\": 12.5e2,\n  \"empty\": null,\n  \"labels\": [\n    \"return\",\n    \"class\",\n    \"def\"\n  ]\n}\n"};
 std::vector<std::unique_ptr<RichView>> views;
 for(unsigned i=0;i<3;++i){auto*panel=lv_obj_create(screen);lv_obj_set_pos(panel,16+int(i)*421,96);lv_obj_set_size(panel,407,606);lv_obj_set_style_bg_color(panel,lv_color_hex(surface),0);lv_obj_set_style_text_color(panel,lv_color_hex(foreground),0);lv_obj_set_style_border_width(panel,1,0);lv_obj_set_style_border_color(panel,lv_color_hex(dark?0x526765:0xb9bcb6),0);lv_obj_set_style_pad_all(panel,12,0);lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);label(panel,names[i],0,0);auto*content=lv_obj_create(panel);lv_obj_remove_style_all(content);lv_obj_set_pos(content,0,45);lv_obj_set_size(content,381,530);lv_obj_add_flag(content,LV_OBJ_FLAG_SCROLLABLE);auto v=std::make_unique<RichView>();v->font=font;v->fontSize=size;v->sourceMode=true;v->documentPath=paths[i];v->syntax=syntaxLanguage(paths[i]);v->begin(content,true);ViewTheme theme;theme.foreground=foreground;theme.surface=surface;v->setTheme(theme);v->load(source[i],true);views.push_back(std::move(v));}
 pump();snapshot(std::string(dark?"dark-":"light-")+std::to_string(size));
 for(unsigned i=0;i<views.size();++i){auto&v=*views[i];check(v.text.markdown()==source[i],"actual rendered RichView preserves source bytes");check(v.metrics().lastDrawGlyphs>80&&v.metrics().lastDrawRuns>8,"actual LVGL draw uses colored glyph runs");const auto layouts=v.metrics().layouts;auto*glyphs=v.text.lines[0].glyphs.data();v.caret=7;v.anchor=3;v.syntaxReady=false;v.recolorSyntax();check(v.text.glyph(0).foreground==NoColor,"unresolved page is rendered plain, never guessed");check(v.caret==7&&v.anchor==3&&v.metrics().layouts==layouts&&v.text.lines[0].glyphs.data()==glyphs,"background recolor preserves selection and layout allocations");v.syntaxReady=true;v.recolorSyntax();check(v.metrics().layouts==layouts,"context arrival does not reflow text");v.load(source[i],true);check(v.metrics().layouts==layouts,"unchanged source and language use geometry cache");}
 // A page beginning in a comment must not promote its first word to a keyword.
 auto&v=*views[0];v.syntaxInitial.mode=SyntaxState::BlockComment;v.syntaxInitial.depth=1;v.load("return value;\n*/ int main(void) { return 0; }",true);check(v.text.glyph(0).foreground==syntaxColor(SyntaxToken::Comment,dark),"production view receives previous page lexical state");pump();
 for(auto&v:views)lv_obj_delete(v->object);
 views.clear();lv_obj_clean(screen);
}
int main(int argc,char**argv){check(argc==3,"arguments");shotDir=argv[1];std::ifstream f(argv[2],std::ios::binary);fontData.assign(std::istreambuf_iterator<char>(f),{});lv_init();font22=lv_tiny_ttf_create_data(fontData.data(),fontData.size(),22);font24=lv_tiny_ttf_create_data(fontData.data(),fontData.size(),24);check(font22&&font24,"shipped TTF loaded at22 and24px");font22->fallback=&lv_font_montserrat_22;font24->fallback=&lv_font_montserrat_24;display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size(),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(display,[](lv_display_t*d,const lv_area_t*,uint8_t*){++flushes;lv_display_flush_ready(d);});screen=lv_obj_create(nullptr);lv_obj_set_style_pad_all(screen,0,0);lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);lv_screen_load(screen);for(auto size:{22u,24u})for(bool dark:{true,false})render(size,dark);lv_obj_delete(screen);lv_tiny_ttf_destroy(font22);lv_tiny_ttf_destroy(font24);lv_deinit();printf("{\"passed\":true,\"checks\":%u,\"frames\":%u,\"fonts\":[22,24]}\n",checks,frames);}
