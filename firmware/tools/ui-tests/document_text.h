namespace DocumentFontTest {
struct {uint8_t fontIndex=1;}settings;
lv_obj_t *editor=nullptr,*previewLabel=nullptr,*filePreviewLabel=nullptr,*fontPreview=nullptr,*fontValue=nullptr;
lv_font_t* docTtfFont=nullptr;bool smoothFontReady=false;unsigned settingsWrites=0;
lv_font_t* headingFont=nullptr;bool simulateRichFallback=false;
struct {void attach(lv_font_t*,uint16_t){}}Tab5Cjk;
struct {unsigned calls=0;uint16_t size=0;void setTextSize(uint16_t value){size=value;++calls;}}Tab5Writing;
static uint16_t fontSizeForIndex(uint8_t index){return Tab5DocumentText::pixels(index);}
static const lv_font_t* documentFont(){return docTtfFont;}
static const lv_font_t* readerFont(){return docTtfFont;}
static const lv_font_t* uiFont(){return LV_FONT_DEFAULT;}
static const lv_font_t* richFontFor(uint16_t px,uint8_t flags){check(px==24&&!flags,"interface heading requests one shared regular24px font");return simulateRichFallback?docTtfFont:headingFont;}
static void markSettingsDirty(){++settingsWrites;}
// These functions are extracted verbatim from the current sketch by run.py.
#include "document_font_integration.inc"
}
static void documentTextChecks(lv_obj_t* screen,const char* fontPath){
 using namespace DocumentFontTest;
 auto* file=std::fopen(fontPath,"rb");check(file!=nullptr,"shipped font readable");std::fseek(file,0,SEEK_END);const auto size=std::ftell(file);std::rewind(file);
 std::vector<unsigned char> data(size);check(std::fread(data.data(),1,data.size(),file)==data.size(),"shipped font loaded completely");std::fclose(file);
 docTtfFont=lv_tiny_ttf_create_data_ex(data.data(),data.size(),24,LV_FONT_KERNING_NORMAL,256);check(docTtfFont!=nullptr,"actual TinyTTF font created");smoothFontReady=true;
 headingFont=lv_tiny_ttf_create_data_ex(data.data(),data.size(),24,LV_FONT_KERNING_NORMAL,192);check(headingFont!=nullptr,"shared fixed heading font created");
 fontValue=lv_label_create(screen);fontPreview=lv_label_create(screen);lv_label_set_text(fontPreview,"Document");
 editor=lv_textarea_create(screen);lv_textarea_set_text(editor,"Document");
 previewLabel=lv_label_create(screen);lv_label_set_text(previewLabel,"Preview");
 filePreviewLabel=lv_label_create(screen);lv_label_set_text(filePreviewLabel,"File");
 auto* menu=lv_label_create(screen);lv_label_set_text(menu,"Menu");lv_obj_set_style_text_font(menu,uiHeadingFont(),0);
 int previousHeight=0;
 for(uint8_t index=0;index<4;++index){settings.fontIndex=index;applyEditorFont();lv_obj_update_layout(screen);
  const unsigned pixels=20+4*index;
  check(Tab5Writing.size==pixels,"Settings dispatches selected document size to current Writing app");
  char text[24];std::snprintf(text,sizeof(text),"%u px",pixels);check(!std::strcmp(lv_label_get_text(fontValue),text),"Settings value immediately reports actual selected pixels");
  for(auto* label:{fontPreview,previewLabel,filePreviewLabel})check(lv_obj_get_height(label)==docTtfFont->line_height,"same-pointer TinyTTF resize invalidates preview label measurement");
  check(docTtfFont->line_height<=44,"largest document font fits the visible Settings preview row");
  check(lv_obj_get_style_text_font(editor,LV_PART_MAIN)==docTtfFont,"legacy editor receives document font");
  check(lv_obj_get_height(fontPreview)>previousHeight,"Settings preview visibly grows for every supported size");previousHeight=lv_obj_get_height(fontPreview);
  check(lv_obj_get_style_text_font(menu,LV_PART_MAIN)==headingFont&&lv_obj_get_height(menu)==headingFont->line_height,"document size does not mutate fixed interface headings or their measured height");
 }
 auto calls=Tab5Writing.calls;auto writes=settingsWrites;fontPlusEvent(nullptr);
 check(Tab5Writing.calls==calls&&settingsWrites==writes,"maximum size button does not rebuild fonts or rewrite unchanged settings");
 fontMinusEvent(nullptr);check(settings.fontIndex==2&&Tab5Writing.size==28&&settingsWrites==writes+1,"minus applies and persists selected size immediately");
 settings.fontIndex=0;applyEditorFont();calls=Tab5Writing.calls;writes=settingsWrites;fontMinusEvent(nullptr);
 check(Tab5Writing.calls==calls&&settingsWrites==writes,"minimum size button avoids needless work");
 check(Tab5DocumentText::pixels(255)==32,"invalid saved size is bounded");
 simulateRichFallback=true;check(uiHeadingFont()==LV_FONT_DEFAULT,"low-memory or missing-TTF headings never borrow mutable reader font");simulateRichFallback=false;
 for(auto* object:{fontValue,fontPreview,editor,previewLabel,filePreviewLabel,menu})lv_obj_delete(object);
 fontValue=fontPreview=editor=previewLabel=filePreviewLabel=nullptr;
 lv_tiny_ttf_destroy(docTtfFont);docTtfFont=nullptr;smoothFontReady=false;
 lv_tiny_ttf_destroy(headingFont);headingFont=nullptr;
}
