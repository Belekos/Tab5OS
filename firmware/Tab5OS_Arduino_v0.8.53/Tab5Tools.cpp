#include "Tab5UsbStorage.h"
#include "Tab5UsbFormatUi.h"
#include "Tab5UsbPrinter.h"
#include "WriterView.h"
#include "Tab5SdrUi.h"
#include "Tab5AudioDriver.h"
#include "Tab5VolumeFS.h"
#include "Tab5ThemePreview.h"
#include "Tab5ThemeLive.h"
#include "Tab5ThemeTone.h"
#include "Tab5FilePicker.h"
#include "Tab5Terminal.h"
#include "Tab5AppPanel.h"
#include "Tab5Diagnostics.h"
#include "Tab5UiStyle.h"
#include "Tab5ThemeSchema.h"
#include "Tab5ThemeChoices.h"
#include "Tab5TextInput.h"
#include "Tab5ContextMenu.h"
#include "Tab5Json.h"
#include "Tab5JsonFile.h"
#include "Tab5AtomicFile.h"
#include "Tab5Tools.h"
#include "Tab5SystemResources.h"
#include "Tab5FileApplications.h"
#include "Tab5Firmware.h"
#include "Tab5BatteryGuard.h"
#include "Tab5Executable.h"
#include "Tab5LaunchProfile.h"
#include "Tab5Calculator.h"
#include "Tab5CalculatorUi.h"
#include "Tab5CalculatorConversion.h"
#include "Tab5Python.h"
#include "Tab5PythonUI.h"
#include "Tab5PythonReader.h"
#include "Tab5Storage.h"
#ifdef ARDUINO
#include <esp_heap_caps.h>
#else
#include <cstdlib>
#endif
#include "Tab5Print.h"
#include "Tab5PrintProtocol.h"
#include "Tab5PrintText.h"
#include "Tab5PrinterDiscovery.h"
#include "Tab5KeyStore.h"
#include "Tab5Secrets.h"
#include "Tab5Environment.h"
#include "Tab5MapPrint.h"
#include "Tab5Vault.h"
#include "Tab5VaultCore.h"
#include "Tab5FilePolicy.h"
#include "Tab5Focus.h"
#include "WriterUsb.h"
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <new>
#include <vector>
#include <atomic>
static std::atomic<bool> nativeThemeRequested{false};
void tab5RequestNativeTheme(){nativeThemeRequested.store(true);}
static char* allocateScript(size_t bytes){
#ifdef ARDUINO
 return static_cast<char*>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#else
 return static_cast<char*>(std::malloc(bytes));
#endif
}
static void freeScript(void* data){
#ifdef ARDUINO
 heap_caps_free(data);
#else
 std::free(data);
#endif
}
static unsigned utf8Characters(const char* text){unsigned n=0;for(const unsigned char* p=reinterpret_cast<const unsigned char*>(text);*p;++p)if((*p&0xc0)!=0x80)++n;return n;}
struct Tab5ToolsApp::Impl {
 lv_obj_t* consoleClear=nullptr;
 void bindConsole(int x,int y){
  Tab5ContextMenu::bindOutput(output,[](void* data){
   auto* self=static_cast<Impl*>(data);
   if(self->tool==Tab5Tool::Terminal)Tab5Terminal::clearOutput();
   else if(self->tool==Tab5Tool::Python)Tab5Python::clearOutput();
   else if(self->tool==Tab5Tool::Executable)tab5ExecutableClearOutput();
   self->consoleRevision=UINT32_MAX;
   if(self->output){
    if(lv_obj_check_type(self->output,&lv_textarea_class))lv_textarea_set_text(self->output,"");
    else lv_label_set_text(self->output,"");
    lv_obj_scroll_to_y(self->output,0,LV_ANIM_OFF);
   }
   if(self->consoleClear)lv_obj_add_flag(self->consoleClear,LV_OBJ_FLAG_HIDDEN);
  },this);
  consoleClear=button(LV_SYMBOL_TRASH,x,y,52,212);
  lv_obj_set_style_text_font(lv_obj_get_child(consoleClear,0),&lv_font_montserrat_24,0);
  Tab5Focus.describe(consoleClear,tr("Effacer la sortie · Ctrl+L"));
  lv_obj_add_flag(consoleClear,LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(consoleClear,[](lv_event_t* e){auto* self=static_cast<Impl*>(lv_event_get_user_data(e));if(self->consoleClear==lv_event_get_target_obj(e))self->consoleClear=nullptr;},LV_EVENT_DELETE,this);
 }
 uint32_t terminalPaint=0,panelPaint=0;
 Tab5ToolsHooks hooks;lv_obj_t *parent=nullptr,*root=nullptr,*input=nullptr,*path=nullptr,*output=nullptr,*status=nullptr,*uri=nullptr,*paper=nullptr,*dpi=nullptr,*landscape=nullptr,*password=nullptr,*confirmation=nullptr,*destination=nullptr,*fromUnit=nullptr,*toUnit=nullptr,*angle=nullptr;
 lv_obj_t *editButtons[6]{},*panelMode=nullptr,*panelValueLabel=nullptr,*panelSave=nullptr;
 lv_obj_t *activeField=nullptr,*scale=nullptr,*pageLabel=nullptr,*printers=nullptr,*keyList=nullptr,*keyName=nullptr,*trustButton=nullptr,*trustHint=nullptr,*tlsButton=nullptr;
 lv_obj_t *printSection=nullptr,*printScopeHint=nullptr,*printPrevious=nullptr,*printNext=nullptr;
 static constexpr unsigned SectionPageSize=48;
 tab5print::TextSection* printSections=nullptr;tab5print::MarkdownOutline printOutline;
 File printOutlineFile;Tab5StorageUse* printOutlineLease=nullptr;
 unsigned sectionPage=0,sectionCount=0,sectionTotal=0;uint32_t sectionFileSize=0;
 bool sectionDirty=false,sectionScanning=false;
 uint32_t themeColors[Tab5ThemePreview::Count]{};String themeDeleteId;bool themeDeleteArmed=false;lv_obj_t* themeDeleteButton=nullptr;unsigned themeRole=0;int pendingThemeRole=-1;bool themeUpdating=false;
 lv_obj_t *themeTone=nullptr,*themeToneValue=nullptr;uint32_t themeToneBase=0;
 lv_obj_t *themePreview=nullptr,*themeRoles=nullptr,*themeHex=nullptr,*themeSwatch=nullptr,*themeSliders[3]{},*themeLoad=nullptr,*nativeRecent=nullptr;
 lv_obj_t *nativeRun=nullptr,*nativeStop=nullptr,*nativeConsole=nullptr,*nativeMode=nullptr,*nativeFile=nullptr,*nativeFolder=nullptr;
 lv_obj_t *firmwareInstall=nullptr,*firmwareRestart=nullptr,*firmwareProgress=nullptr,*firmwareCancel=nullptr,*firmwareVerify=nullptr;
 lv_obj_t *storageMount=nullptr,*storageEject=nullptr,*storageOpen=nullptr,*storageRepair=nullptr,*storageCancel=nullptr,*storageProgress=nullptr;
 lv_obj_t *storagePanel=nullptr,*storageUsbCard=nullptr,*storageUsbInfo=nullptr,*storageRepairCard=nullptr;
 bool storageEmbedded=false,storageRepairRequested=false;uint32_t storageLast=0;
 lv_obj_t* scriptPanel=nullptr;lv_obj_t* scriptWidgets[Tab5PythonUI::MaxWidgets]{};
 lv_obj_t *toolHeading=nullptr,*toolClose=nullptr,*toolKeyboard=nullptr,*scriptHeading=nullptr,*scriptKeyboard=nullptr,*scriptStop=nullptr,*scriptHide=nullptr;
 struct LayoutAnchor{lv_obj_t* object;int16_t x,width;uint8_t mode;};
 LayoutAnchor layoutAnchors[128]{};unsigned layoutAnchorCount=0;int layoutWidth=0;
 uint32_t scriptVersions[Tab5PythonUI::MaxWidgets]{},scriptGeneration=0,consoleRevision=UINT32_MAX;
 std::vector<lv_obj_t*> scriptHidden;
 std::vector<Tab5Printers::Printer> detected;std::vector<Tab5KeyStore::Info> keys;
 bool scriptLauncher=false;
 lv_obj_t *pythonResume=nullptr,*switchPrompt=nullptr;String pendingPythonPath,pendingPythonArgument;bool switchConfirmed=false;
 String openWithDocument;
 bool vaultGate=false,vaultChanging=false,vaultUnlocking=false;String vaultSelection;
 bool firmwareUpdates=false;bool scanning=false,estimateDirty=false,estimateWaiting=false,keyJob=false,lockedFile=false,legacyFile=false;uint32_t estimateAt=0;
 unsigned mapFallbacks=0;bool mapMode=false,mapPreparing=false,printUpdates=false,vaultUpdates=false,pythonWasRunning=false;Tab5Print::Options mapOptions;
  Tab5CalculatorUi::Widgets calculator;int calculatorHeight=0;
  tab5calc::History calculatorHistory{[](size_t bytes)->void*{return allocateScript(bytes);},freeScript};
  tab5calc::HistoryView calculatorHistoryView;tab5calc::EntryState calculatorEntry;tab5calc::ConversionWindow calculatorConversion;
  Tab5Tool tool=Tab5Tool::Calculator;bool editing=false;double answer=0,memory=0;uint32_t last=0,nativeLast=0;String lastOutput;String printerUri;
 const char* tr(const char* s){return hooks.translate?hooks.translate(s):s;}
 uint32_t color(unsigned n){return hooks.color?hooks.color(n):n==2?0xffffff:0x202428;}
 void note(const char* text){if(status&&strcmp(lv_label_get_text(status),tr(text)))lv_label_set_text(status,tr(text));}
 lv_obj_t* label(const char* text,int x,int y,int width){auto* l=lv_label_create(root);lv_obj_set_pos(l,x,y);lv_obj_set_width(l,width);lv_label_set_text(l,tr(text));lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);Tab5TextInput::selectable(l);return l;}
 lv_obj_t* button(const char* text,int x,int y,int width,int action){auto* b=lv_button_create(root);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,width,44);lv_obj_set_style_pad_all(b,4,0);lv_obj_set_style_bg_color(b,lv_color_hex(color(1)),0);lv_obj_set_style_text_color(b,lv_color_hex(color(2)),0);lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_border_color(b,lv_color_hex(color(4)),0);auto* l=lv_label_create(b);lv_label_set_text(l,tr(text));lv_obj_center(l);auto* context=new std::pair<Impl*,int>(this,action);
  lv_obj_add_event_cb(b,[](lv_event_t* e){auto pair=*static_cast<std::pair<Impl*,int>*>(lv_event_get_user_data(e));pair.first->act(pair.second);},LV_EVENT_CLICKED,context);
  lv_obj_add_event_cb(b,[](lv_event_t* e){delete static_cast<std::pair<Impl*,int>*>(lv_event_get_user_data(e));},LV_EVENT_DELETE,context);Tab5UiStyle::control(b);return b;}
 void erasePassword(lv_obj_t* field){
  if(!field||!lv_obj_is_valid(field)||!lv_obj_check_type(field,&lv_textarea_class)||(!lv_textarea_get_password_mode(field)&&!Tab5TextInput::protectedField(field)))return;
  // LVGL 9.6 stores password text in its owned mutable pwd_tmp buffer.
  auto* text=const_cast<char*>(lv_textarea_get_text(field));if(text)tab5vault::wipe(text,strlen(text));lv_textarea_set_text(field,"");
 }
 void hideKeyboard(){editing=false;Tab5Focus.finishEditing();if(hooks.keyboard)hooks.keyboard(false,nullptr);}
 void inputKeyboard(lv_obj_t* f,bool manual=false){
  if(!f||!lv_obj_is_valid(f)||lv_obj_has_state(f,LV_STATE_DISABLED))return;
  if(Tab5TextInput::readOnly(f)){Tab5Focus.editObject(f);activeField=f;editing=true;if(hooks.keyboard)hooks.keyboard(false,f);return;}
  if(activeField&&activeField!=f&&lv_obj_is_valid(activeField))lv_obj_remove_state(activeField,LV_STATE_FOCUSED);
  Tab5Focus.editObject(f);activeField=f;editing=true;
  if(hooks.keyboard)hooks.keyboard(manual||!writing::usbConnected(),f);
 }
 void changedPrint(){estimateDirty=true;estimateAt=millis();if(pageLabel)lv_label_set_text(pageLabel,tr("Pages estimées : …"));}
 void stopOutline(){printOutlineFile.close();delete printOutlineLease;printOutlineLease=nullptr;sectionScanning=false;}
 void changedPrintPath(){stopOutline();sectionPage=sectionCount=sectionTotal=0;sectionFileSize=0;sectionDirty=true;changedPrint();if(printSection){lv_dropdown_set_options(printSection,tr("Document entier"));lv_obj_add_state(printSection,LV_STATE_DISABLED);}}
 bool markdownPath(){String name=lv_textarea_get_text(path);name.toLowerCase();return name.endsWith(".md")||name.endsWith(".markdown");}
 void finishOutline(bool success){
  stopOutline();if(!printSection)return;
  if(!success){sectionCount=sectionTotal=sectionFileSize=0;lv_label_set_text(printScopeHint,tr("Plan indisponible · document entier"));}
  else {
   String choices=tr("Document entier");
   for(unsigned i=0;i<sectionCount;++i){auto& section=printSections[i];if(!section.end)section.end=sectionFileSize;
    choices+='\n';for(unsigned level=1;level<section.level;++level)choices+="  ";choices+=section.title[0]?section.title:tr("Sans titre");}
   lv_dropdown_set_options(printSection,choices.c_str());lv_dropdown_set_selected(printSection,0);
   if(sectionTotal){String hint=String(tr("Titres : "))+String(sectionPage*SectionPageSize+1)+"–"+String(sectionPage*SectionPageSize+sectionCount)+" / "+String(sectionTotal)+" · "+tr("Sous-sections incluses");lv_label_set_text(printScopeHint,hint.c_str());}
   else lv_label_set_text(printScopeHint,tr("Document entier · aucun titre Markdown"));
  }
  if(sectionCount)lv_obj_remove_state(printSection,LV_STATE_DISABLED);else lv_obj_add_state(printSection,LV_STATE_DISABLED);
  if(sectionPage&&success)lv_obj_remove_state(printPrevious,LV_STATE_DISABLED);else lv_obj_add_state(printPrevious,LV_STATE_DISABLED);
  if(success&&(sectionPage+1)*SectionPageSize<sectionTotal)lv_obj_remove_state(printNext,LV_STATE_DISABLED);else lv_obj_add_state(printNext,LV_STATE_DISABLED);
  changedPrint();
 }
 void serviceOutline(){
  if(!printSection||mapMode)return;
  if(sectionDirty){
   if(millis()-estimateAt<400||Tab5Print::busy())return;
   sectionDirty=false;stopOutline();sectionCount=sectionTotal=sectionFileSize=0;printOutline.reset();
   lv_obj_add_state(printPrevious,LV_STATE_DISABLED);lv_obj_add_state(printNext,LV_STATE_DISABLED);
   lv_dropdown_set_options(printSection,tr("Document entier"));lv_obj_add_state(printSection,LV_STATE_DISABLED);
   if(!markdownPath()){lv_label_set_text(printScopeHint,tr("Document entier"));return;}
   if(!printSections){printSections=reinterpret_cast<tab5print::TextSection*>(allocateScript(sizeof(tab5print::TextSection)*SectionPageSize));if(printSections)for(unsigned i=0;i<SectionPageSize;++i)new(printSections+i)tab5print::TextSection;}
   if(!printSections){finishOutline(false);return;}
   printOutlineLease=new Tab5StorageUse();
   if(!*printOutlineLease){finishOutline(false);return;}
   printOutlineFile=tab5FS.open(lv_textarea_get_text(path));
   if(!printOutlineFile||printOutlineFile.isDirectory()||printOutlineFile.size()>2*1024*1024){finishOutline(false);return;}
   sectionFileSize=printOutlineFile.size();sectionScanning=true;lv_label_set_text(printScopeHint,tr("Lecture des titres…"));
  }
  if(!sectionScanning)return;
  auto heading=[&](const tab5print::TextSection& next){tab5print::closeSections(printSections,sectionCount,next);
   if(sectionTotal>=sectionPage*SectionPageSize&&sectionCount<SectionPageSize)printSections[sectionCount++]=next;
   ++sectionTotal;
  };
  char bytes[1024];uint32_t began=millis();unsigned remaining=32768;
  while(remaining&&millis()-began<8&&printOutline.position()<sectionFileSize){
   size_t count=printOutlineFile.read(reinterpret_cast<uint8_t*>(bytes),std::min<size_t>(sizeof(bytes),sectionFileSize-printOutline.position()));
   if(!count){finishOutline(false);return;}printOutline.feed(bytes,count,heading);remaining-=count;
  }
  if(printOutline.position()==sectionFileSize){printOutline.finish(heading);finishOutline(true);}
 }
 void captureLayout(){
  layoutAnchorCount=0;layoutWidth=0;lv_obj_update_layout(root);
   if(tool==Tab5Tool::Calculator){calculatorHeight=0;layoutTools();return;}
  // These native tool pages are authored on a 1128 px content grid. Capture
  // only their direct, absolute controls once, after all page-specific moves.
  // Storage's flex panel and script canvases have their own layout contracts.
  for(unsigned i=0;i<lv_obj_get_child_count(root)&&layoutAnchorCount<128;++i){
   auto* o=lv_obj_get_child(root,i);if(o==toolHeading||o==toolClose||o==toolKeyboard||o==storagePanel||o==scriptPanel)continue;
   int x=lv_obj_get_style_x(o,LV_PART_MAIN),w=lv_obj_get_style_width(o,LV_PART_MAIN);if(LV_COORD_IS_PCT(w)||LV_COORD_IS_PCT(x)||w==LV_SIZE_CONTENT)continue;
   const bool labelObject=lv_obj_check_type(o,&lv_label_class);
   bool stretch=(labelObject&&w>=1000)||(o==nativeConsole)||(o==firmwareProgress);
   if(tool==Tab5Tool::Terminal||tool==Tab5Tool::AppPanel)stretch|=o==input||o==output||o==status;
   if(tool==Tab5Tool::Executable)stretch|=o==input||o==path||o==nativeMode||o==nativeRecent;
   if((lv_obj_check_type(o,&lv_textarea_class)||lv_obj_check_type(o,&lv_dropdown_class)||labelObject)&&x+w>=1100)stretch=true;
   if(o==output&&tool==Tab5Tool::Sdr)stretch=true;
   if(tool==Tab5Tool::Calculator&&x>=700)stretch=false;
   // Small right-hand commands move as a group; they never get stretched.
   const bool move=!stretch&&x>=600;
   if(stretch||move)layoutAnchors[layoutAnchorCount++]={o,int16_t(x),int16_t(w),uint8_t(stretch?1:2)};
  }
  layoutTools();
 }
 void layoutScript(){
  if(!scriptPanel)return;lv_obj_update_layout(scriptPanel);int width=lv_obj_get_content_width(scriptPanel);
  if(scriptHeading)lv_obj_set_width(scriptHeading,std::max(100,width-242));
  if(scriptStop)lv_obj_set_x(scriptStop,width-216);
  if(scriptKeyboard)lv_obj_set_x(scriptKeyboard,width-144);
  if(scriptHide)lv_obj_set_x(scriptHide,width-72);
  const int inset=std::max(0,(width-1100)/2);
  for(unsigned i=0;i<Tab5PythonUI::MaxWidgets;++i)if(scriptWidgets[i]){Tab5PythonUI::Widget widget;if(Tab5PythonUI::read(i+1,widget,scriptGeneration))lv_obj_set_x(scriptWidgets[i],widget.x+inset);}
 }
 void layoutTools(){
  if(!root)return;lv_obj_update_layout(root);const int width=lv_obj_get_content_width(root);
   const int height=lv_obj_get_content_height(root);
   if(width==layoutWidth&&(tool!=Tab5Tool::Calculator||height==calculatorHeight))return;layoutWidth=width;const int delta=width-1128;
  if(toolHeading)lv_obj_set_width(toolHeading,std::max(100,width-(toolKeyboard?152:76)));
  if(toolClose)lv_obj_set_x(toolClose,width-48);
  if(toolKeyboard)lv_obj_set_x(toolKeyboard,width-120);
   if(tool==Tab5Tool::Calculator){calculatorHeight=height;Tab5CalculatorUi::layout(calculator,width,height);if(toolHeading)lv_obj_set_width(toolHeading,std::max(100,width-376));calculatorHistoryView.refresh();return;}
  for(unsigned i=0;i<layoutAnchorCount;++i){const auto& a=layoutAnchors[i];if(!a.object||!lv_obj_is_valid(a.object))continue;
   if(a.mode==1)lv_obj_set_width(a.object,std::max(32,std::min(width-int(a.x),int(a.width)+delta)));else lv_obj_set_x(a.object,std::min(width-int(a.width),int(a.x)+delta));
  }
  if(nativeConsole&&output&&lv_obj_get_parent(output)==nativeConsole)lv_obj_set_width(output,lv_pct(100));
  layoutScript();
 }
 void resize(int height){if(!root)return;const int oldHeight=lv_obj_get_height(root),available=lv_obj_get_content_height(parent);height=std::min(height,available);lv_obj_set_height(root,height);lv_obj_update_layout(root);layoutTools();
  if(height<available&&activeField&&lv_obj_is_valid(activeField))lv_obj_scroll_to_view(activeField,LV_ANIM_OFF);
  if(height>=available&&oldHeight<available)lv_obj_scroll_to(root,0,0,LV_ANIM_OFF);
 }
 lv_obj_t* field(const char* value,int x,int y,int width,int height=48,bool oneLine=true){auto* f=lv_textarea_create(root);lv_obj_set_pos(f,x,y);lv_obj_set_size(f,width,height);lv_textarea_set_one_line(f,oneLine);lv_obj_set_style_pad_top(f,4,0);lv_obj_set_style_pad_bottom(f,4,0);lv_obj_set_style_pad_left(f,10,0);lv_obj_set_style_pad_right(f,10,0);lv_textarea_set_max_length(f,oneLine?260:65536);lv_textarea_set_text(f,value);lv_obj_set_style_bg_color(f,lv_color_hex(color(1)),0);lv_obj_set_style_text_color(f,lv_color_hex(color(2)),0);lv_obj_set_style_border_color(f,lv_color_hex(color(4)),0);
  lv_textarea_set_text_selection(f,true);lv_obj_set_style_outline_width(f,2,LV_STATE_FOCUSED);lv_obj_set_style_outline_color(f,lv_color_hex(color(4)),LV_STATE_FOCUSED);lv_obj_set_style_outline_pad(f,1,LV_STATE_FOCUSED);lv_obj_set_style_border_width(f,1,0);
  lv_obj_set_style_text_color(f,lv_color_hex(color(2)),LV_STATE_FOCUSED);
  lv_obj_add_event_cb(f,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->inputKeyboard(lv_event_get_target_obj(e));},LV_EVENT_CLICKED,this);
  lv_obj_add_event_cb(f,[](lv_event_t* e){auto* self=static_cast<Impl*>(lv_event_get_user_data(e));auto* field=lv_event_get_target_obj(e);if(self->securityTool())self->securitySubmit(field);else if(!lv_obj_has_flag(field,LV_OBJ_FLAG_USER_4))self->hideKeyboard();},LV_EVENT_READY,this);
  lv_obj_add_event_cb(f,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->activeField=lv_event_get_target_obj(e);},LV_EVENT_FOCUSED,this);Tab5UiStyle::control(f);return f;}
 lv_obj_t* dropdown(const char* choices,int x,int y,int width){auto* d=lv_dropdown_create(root);lv_obj_set_pos(d,x,y);lv_obj_set_width(d,width);lv_obj_set_style_pad_top(d,6,0);lv_obj_set_style_pad_bottom(d,6,0);lv_dropdown_set_options(d,tr(choices));lv_obj_set_style_outline_width(d,0,LV_STATE_FOCUSED);auto* list=lv_dropdown_get_list(d);lv_obj_set_style_bg_color(list,lv_color_hex(color(1)),0);lv_obj_set_style_text_color(list,lv_color_hex(color(2)),0);lv_obj_set_style_bg_color(list,lv_color_hex(color(4)),LV_PART_SELECTED);lv_obj_set_style_bg_opa(list,LV_OPA_COVER,LV_PART_SELECTED);lv_obj_set_style_text_color(list,lv_color_hex(color(2)),LV_PART_SELECTED);if(hooks.font)lv_obj_set_style_text_font(list,hooks.font(),0);lv_obj_set_style_bg_color(d,lv_color_hex(color(1)),0);lv_obj_set_style_text_color(d,lv_color_hex(color(2)),0);Tab5UiStyle::control(d);return d;}
 void dismissScript(){
  if(!scriptPanel)return;hideKeyboard();activeField=nullptr;Tab5Focus.clear();lv_obj_delete(scriptPanel);scriptPanel=nullptr;scriptHeading=scriptKeyboard=scriptStop=scriptHide=nullptr;
  for(auto* o:scriptHidden)if(lv_obj_is_valid(o))lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);
  scriptHidden.clear();memset(scriptWidgets,0,sizeof(scriptWidgets));memset(scriptVersions,0,sizeof(scriptVersions));
 }
 void scriptUI(){
  uint32_t generation;char title[192];bool opened=Tab5PythonUI::opened(generation,title);
  if(!opened){dismissScript();return;}
  if(scriptPanel&&generation!=scriptGeneration)dismissScript();
  if(!scriptPanel){
   hideKeyboard();activeField=nullptr;Tab5Focus.clear();scriptGeneration=generation;
   for(unsigned i=0;i<lv_obj_get_child_count(root);++i){auto* o=lv_obj_get_child(root,i);if(!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)){scriptHidden.push_back(o);lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}}
   scriptPanel=lv_obj_create(root);lv_obj_set_user_data(scriptPanel,(void*)uintptr_t(scriptGeneration));lv_obj_remove_style_all(scriptPanel);lv_obj_set_pos(scriptPanel,0,0);lv_obj_set_size(scriptPanel,lv_pct(100),600);lv_obj_scroll_to(root,0,0,LV_ANIM_OFF);
   lv_obj_set_style_bg_opa(scriptPanel,LV_OPA_COVER,0);lv_obj_set_style_bg_color(scriptPanel,lv_color_hex(color(0)),0);
   lv_obj_set_style_text_color(scriptPanel,lv_color_hex(color(2)),0);lv_obj_remove_flag(scriptPanel,LV_OBJ_FLAG_SCROLLABLE);
   auto* heading=scriptHeading=lv_label_create(scriptPanel);lv_label_set_text(heading,title);lv_obj_set_width(heading,850);lv_obj_set_pos(heading,12,8);lv_label_set_long_mode(heading,LV_LABEL_LONG_DOT);
   auto* kb=scriptKeyboard=lv_button_create(scriptPanel);lv_obj_set_pos(kb,986,0);lv_obj_set_size(kb,60,44);
   lv_obj_set_style_bg_color(kb,lv_color_hex(color(1)),0);lv_obj_set_style_text_color(kb,lv_color_hex(color(2)),0);
   auto* kbIcon=lv_label_create(kb);lv_label_set_text(kbIcon,LV_SYMBOL_KEYBOARD);lv_obj_center(kbIcon);
   lv_obj_add_event_cb(kb,[](lv_event_t* e){auto* self=static_cast<Impl*>(lv_event_get_user_data(e));
    if(self->editing)self->hideKeyboard();else {
     if(!self->activeField)for(auto* widget:self->scriptWidgets)if(widget&&lv_obj_check_type(widget,&lv_textarea_class)){self->activeField=widget;break;}
     if(self->activeField)self->inputKeyboard(self->activeField,true);
    }
   },LV_EVENT_CLICKED,this);
   auto* stop=scriptStop=lv_button_create(scriptPanel);lv_obj_set_pos(stop,914,0);lv_obj_set_size(stop,60,44);
   lv_obj_set_style_bg_color(stop,lv_color_hex(color(1)),0);lv_obj_set_style_text_color(stop,lv_color_hex(color(2)),0);
   auto* stopIcon=lv_label_create(stop);lv_label_set_text(stopIcon,LV_SYMBOL_CLOSE);lv_obj_center(stopIcon);
   Tab5Focus.describe(stop,tr("Quitter · Échap"));
   lv_obj_add_event_cb(stop,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->act(13);},LV_EVENT_CLICKED,this);
   auto* exit=scriptHide=lv_button_create(scriptPanel);lv_obj_set_pos(exit,1058,0);lv_obj_set_size(exit,60,44);
   lv_obj_set_style_bg_color(exit,lv_color_hex(color(1)),0);lv_obj_set_style_text_color(exit,lv_color_hex(color(2)),0);lv_obj_set_style_outline_width(exit,0,LV_STATE_FOCUSED);
   auto* icon=lv_label_create(exit);lv_label_set_text(icon,LV_SYMBOL_DOWN);lv_obj_center(icon);
   Tab5Focus.describe(exit,tr("Masquer · Ctrl+Alt+P pour reprendre"));
   lv_obj_add_event_cb(exit,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->close(true);},LV_EVENT_CLICKED,this);
   layoutScript();
  }
  for(unsigned i=0;i<Tab5PythonUI::MaxWidgets;++i){Tab5PythonUI::Widget w;if(!Tab5PythonUI::read(i+1,w,scriptGeneration)||w.revision==scriptVersions[i])continue;
   auto*& o=scriptWidgets[i];if(!o){
    if(w.type==4){o=field(w.text,0,0,w.w,w.h);lv_obj_set_parent(o,scriptPanel);lv_textarea_set_max_length(o,255);
     lv_obj_add_event_cb(o,[](lv_event_t* e){unsigned id=uintptr_t(lv_event_get_user_data(e));Tab5PythonUI::input(id,lv_textarea_get_text(lv_event_get_target_obj(e)),uintptr_t(lv_obj_get_user_data(lv_obj_get_parent(lv_event_get_target_obj(e)))));},LV_EVENT_VALUE_CHANGED,(void*)uintptr_t(i+1));
     lv_obj_add_event_cb(o,[](lv_event_t* e){auto* object=lv_event_get_target_obj(e);const unsigned id=uintptr_t(lv_event_get_user_data(e));const uint32_t generation=uintptr_t(lv_obj_get_user_data(lv_obj_get_parent(object)));Tab5PythonUI::input(id,lv_textarea_get_text(object),generation);Tab5PythonUI::event(id,1,generation);},LV_EVENT_READY,(void*)uintptr_t(i+1));
    }else if(w.type==5){o=dropdown(w.text,0,0,w.w);lv_obj_set_parent(o,scriptPanel);
     lv_obj_add_event_cb(o,[](lv_event_t* e){Tab5PythonUI::event(uintptr_t(lv_event_get_user_data(e)),lv_dropdown_get_selected(lv_event_get_target_obj(e)),uintptr_t(lv_obj_get_user_data(lv_obj_get_parent(lv_event_get_target_obj(e)))));},LV_EVENT_VALUE_CHANGED,(void*)uintptr_t(i+1));
    }else if(w.type==7){o=Tab5ThemePreview::create(scriptPanel);
     lv_obj_add_event_cb(o,[](lv_event_t* e){auto* root=lv_event_get_current_target_obj(e);if(lv_event_get_target_obj(e)!=root||!lv_event_get_param(e))return;unsigned role=*static_cast<unsigned*>(lv_event_get_param(e));Tab5PythonUI::event(uintptr_t(lv_event_get_user_data(e)),role,uintptr_t(lv_obj_get_user_data(lv_obj_get_parent(root))));},LV_EVENT_VALUE_CHANGED,(void*)uintptr_t(i+1));
    }
    else o=(w.type==1||w.type==6)?lv_label_create(scriptPanel):w.type==2?lv_button_create(scriptPanel):lv_slider_create(scriptPanel);
    if(w.type==1||w.type==6)Tab5TextInput::selectable(o);
    lv_obj_update_layout(scriptPanel);const int inset=std::max(0,(int(lv_obj_get_content_width(scriptPanel))-1100)/2);lv_obj_set_pos(o,w.x+inset,w.y+64);lv_obj_set_size(o,w.w,w.h);
    lv_obj_set_style_text_color(o,lv_color_hex(color(2)),0);lv_obj_set_style_outline_width(o,0,LV_STATE_FOCUSED);
    if(w.type==2){lv_obj_set_style_pad_all(o,4,0);lv_obj_set_style_bg_color(o,lv_color_hex(color(1)),0);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,lv_color_hex(color(3)),0);lv_label_create(o);
     lv_obj_add_event_cb(o,[](lv_event_t* e){Tab5PythonUI::event(uintptr_t(lv_event_get_user_data(e)),1,uintptr_t(lv_obj_get_user_data(lv_obj_get_parent(lv_event_get_target_obj(e)))));},LV_EVENT_CLICKED,(void*)uintptr_t(i+1));}
    if(w.type==3){lv_slider_set_range(o,0,w.maximum);lv_obj_set_style_bg_color(o,lv_color_hex(color(1)),0);lv_obj_set_style_bg_color(o,lv_color_hex(color(4)),LV_PART_INDICATOR);lv_obj_set_style_bg_color(o,lv_color_hex(color(3)),LV_PART_KNOB);
     lv_obj_add_event_cb(o,[](lv_event_t* e){Tab5PythonUI::event(uintptr_t(lv_event_get_user_data(e)),lv_slider_get_value(lv_event_get_target_obj(e)),uintptr_t(lv_obj_get_user_data(lv_obj_get_parent(lv_event_get_target_obj(e)))));},LV_EVENT_VALUE_CHANGED,(void*)uintptr_t(i+1));}
   }
   if(w.type==1)lv_label_set_text(o,w.text);
   if(w.type==2){auto* l=lv_obj_get_child(o,0);lv_obj_set_width(l,w.w-18);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_label_set_text(l,w.text);lv_obj_center(l);}
   if(w.type==3)lv_slider_set_value(o,w.value,LV_ANIM_OFF);
   if(w.type==4){if(w.value==1)lv_obj_add_flag(o,LV_OBJ_FLAG_USER_4);else lv_obj_remove_flag(o,LV_OBJ_FLAG_USER_4);if(strcmp(lv_textarea_get_text(o),w.text))lv_textarea_set_text(o,w.text);}
   if(w.type==5){if(strcmp(lv_dropdown_get_options(o),w.text))lv_dropdown_set_options(o,w.text);lv_dropdown_set_selected(o,w.value);}
   if(w.type==7)Tab5ThemePreview::update(o,w.palette);
   if(w.type==6){lv_label_set_text(o,w.text);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_bg_color(o,lv_color_hex(w.background),0);lv_obj_set_style_text_color(o,lv_color_hex(w.foreground),0);lv_obj_set_style_pad_all(o,12,0);}

   scriptVersions[i]=w.revision;
  }
 }
 void cancelSwitch(){
  auto* old=switchPrompt;switchPrompt=nullptr;pendingPythonPath="";pendingPythonArgument="";switchConfirmed=false;
  if(old)lv_obj_delete(old);Tab5Focus.clear();if(root)Tab5Focus.focusFirst(scriptPanel?scriptPanel:root);
 }
 void confirmSwitch(){
  if(!pendingPythonPath.length())return;switchConfirmed=true;
  Tab5PythonReader::close();Tab5PythonUI::close();Tab5Python::stop();
  if(switchPrompt&&lv_obj_get_child_count(switchPrompt)){
   auto* panel=lv_obj_get_child(switchPrompt,0);
   for(uint32_t i=0;i<lv_obj_get_child_count(panel);++i){auto* child=lv_obj_get_child(panel,i);if(lv_obj_check_type(child,&lv_button_class))lv_obj_add_state(child,LV_STATE_DISABLED);}
  }
 }
 void askSwitch(const char* next,const char* argument=nullptr){
  if(switchConfirmed)return;cancelSwitch();pendingPythonPath=next;pendingPythonArgument=argument?argument:"";
  hideKeyboard();switchPrompt=lv_obj_create(root);lv_obj_remove_style_all(switchPrompt);
  lv_obj_set_size(switchPrompt,LV_PCT(100),LV_PCT(100));lv_obj_set_pos(switchPrompt,0,0);
  lv_obj_set_style_bg_color(switchPrompt,lv_color_black(),0);lv_obj_set_style_bg_opa(switchPrompt,LV_OPA_70,0);lv_obj_remove_flag(switchPrompt,LV_OBJ_FLAG_SCROLLABLE);
  auto* panel=lv_obj_create(switchPrompt);lv_obj_set_size(panel,720,238);lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel,lv_color_hex(color(1)),0);lv_obj_set_style_text_color(panel,lv_color_hex(color(2)),0);
  lv_obj_set_style_bg_opa(panel,LV_OPA_COVER,0);lv_obj_set_style_pad_all(panel,20,0);lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
  auto* prompt=lv_label_create(panel);lv_obj_set_width(prompt,670);lv_label_set_long_mode(prompt,LV_LABEL_LONG_WRAP);
  lv_label_set_text(prompt,tr("Une application Python est en cours. L’arrêter pour ouvrir l’autre ?"));
  auto* name=lv_label_create(panel);lv_obj_set_width(name,670);lv_obj_set_pos(name,0,78);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
  const char* file=strrchr(next,'/');String caption=file?file+1:next;
  if(argument&&*argument){const char* document=strrchr(argument,'/');caption+=" · ";caption+=document?document+1:argument;}
  lv_label_set_text(name,caption.c_str());
  auto* keep=lv_button_create(panel);lv_obj_set_size(keep,300,48);lv_obj_set_pos(keep,0,146);
  auto* label=lv_label_create(keep);lv_label_set_text(label,tr("Revenir à l’application"));lv_obj_center(label);
  lv_obj_add_event_cb(keep,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->cancelSwitch();},LV_EVENT_CLICKED,this);
  auto* change=lv_button_create(panel);lv_obj_set_size(change,340,48);lv_obj_set_pos(change,322,146);
  label=lv_label_create(change);lv_label_set_text(label,tr("Arrêter et ouvrir"));lv_obj_center(label);
  lv_obj_add_event_cb(change,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->confirmSwitch();},LV_EVENT_CLICKED,this);
  Tab5Focus.clear();Tab5Focus.focusObject(keep);
 }
 void rememberPrinter(){
  if(!uri)return;printerUri=lv_textarea_get_text(uri);
  tab5print::Endpoint e;if(tab5print::endpoint(printerUri.c_str(),e)&&!Tab5Printers::remember(printerUri))note(Tab5Printers::status());
 }
 void printerSelection(){
  if(!printers||!uri)return;
  String selected=lv_textarea_get_text(uri);lv_dropdown_set_text(printers,nullptr);
  for(unsigned i=0;i<detected.size();++i)if(detected[i].uri==selected){lv_dropdown_set_selected(printers,i);return;}
  if(selected.length())lv_dropdown_set_text(printers,tr("Adresse manuelle"));
  else if(!detected.empty())lv_dropdown_set_text(printers,tr("Choisir une imprimante"));
 }
 uint32_t usbPrinterRevision=0;
 void refreshPrinters(bool chooseFirst=false){
  usbPrinterRevision=Tab5UsbPrinter::revision();
  detected=Tab5Printers::results();String choices;
  for(auto& printer:detected){if(choices.length())choices+='\n';const bool usb=Tab5UsbPrinter::uri(printer.uri.c_str());choices+=(usb?String(tr("Imprimante USB IPP")):printer.name)+(usb?" · USB":printer.secure?" · IPPS":" · IPP");}
  lv_dropdown_set_options(printers,choices.length()?choices.c_str():tr("Aucune imprimante détectée"));
  if(chooseFirst&&!detected.empty()&&!strlen(lv_textarea_get_text(uri))){lv_dropdown_set_selected(printers,0);act(24);}
  printerSelection();
 }
 void close(bool preservePython=true){
 calculatorConversion.close(false);calculatorHistory.clear();calculatorEntry.result=false;
 if(tool==Tab5Tool::Sdr)Tab5SdrUi::close();Tab5FilePicker::close();if(tool==Tab5Tool::Terminal)Tab5Terminal::close();if(tool==Tab5Tool::AppPanel)Tab5AppPanel::close();Tab5PythonReader::suspend();if(!root)return;if(tool==Tab5Tool::Firmware&&Tab5Firmware::busy()){note("Attendre la fin ou annuler l’installation");return;}if(tool==Tab5Tool::Print)rememberPrinter();stopOutline();freeScript(printSections);printSections=nullptr;printSection=printScopeHint=printPrevious=printNext=nullptr;sectionDirty=false;pendingPythonPath="";switchConfirmed=false;switchPrompt=pythonResume=nullptr;if(tool==Tab5Tool::Python&&!preservePython){Tab5PythonUI::close();Tab5Python::stop();}dismissScript();if(mapPreparing){Tab5MapPrint::cancel();mapPreparing=false;}if(hooks.keyboard)hooks.keyboard(false,nullptr);editing=false;activeField=nullptr;memset(editButtons,0,sizeof(editButtons));panelMode=panelValueLabel=panelSave=nullptr;Tab5Focus.clear();resetSecurity();auto* old=root;root=nullptr;nativeRun=nativeStop=nativeConsole=nativeRecent=nativeMode=nativeFile=nativeFolder=nullptr;firmwareCancel=firmwareVerify=nullptr;themePreview=themeRoles=themeHex=themeSwatch=themeLoad=nullptr;scale=pageLabel=printers=keyList=keyName=trustButton=trustHint=tlsButton=nullptr;input=path=output=status=uri=paper=dpi=landscape=password=confirmation=destination=fromUnit=toUnit=angle=nullptr;lv_obj_delete(old);lastOutput=String();std::vector<Tab5Printers::Printer>().swap(detected);std::vector<Tab5KeyStore::Info>().swap(keys);}
 bool launchScript(const char* name,const char* argument=nullptr){
  hideKeyboard();bool started=Tab5Python::runFile(name,argument);
  pythonWasRunning=started;if(!started)note(Tab5Python::status());return started;
 }

 bool readJson(const char* file,JsonDocument& doc){if(!tab5AtomicRecover(file))return false;errno=0;File f=tab5FS.open(file);if(!f)return errno==ENOENT;if(f.size()>32768)return false;Tab5JsonFileReader reader(f);return !deserializeJson(doc,reader);}
 bool writeJson(const char* file,JsonDocument& doc){size_t n=measureJson(doc);if(n>32768)return false;char* bytes=allocateScript(n+1);if(!bytes)return false;serializeJson(doc,bytes,n+1);bool ok=tab5AtomicWrite(file,bytes,n);freeScript(bytes);return ok;}
 static const char* roleKey(unsigned i){return tab5theme::roleKey(i);}
 void themeToneReset(){themeToneBase=themeColors[themeRole];if(themeTone)lv_slider_set_value(themeTone,0,LV_ANIM_OFF);if(themeToneValue)lv_label_set_text(themeToneValue,"0");}
 void themeToneChanged(){
  if(themeUpdating||!themeTone)return;const int amount=lv_slider_get_value(themeTone);themeColors[themeRole]=tab5theme::tone(themeToneBase,amount);
  char value[12];snprintf(value,sizeof(value),"%+d",amount);lv_label_set_text(themeToneValue,value);themeRefresh();
 }
 void themeRefresh(){themeUpdating=true;char hex[8];snprintf(hex,sizeof(hex),"#%06lX",static_cast<unsigned long>(themeColors[themeRole]&0xffffff));lv_textarea_set_text(themeHex,hex);lv_dropdown_set_selected(themeRoles,themeRole);for(unsigned i=0;i<3;++i)lv_slider_set_value(themeSliders[i],(themeColors[themeRole]>>(16-i*8))&255,LV_ANIM_OFF);lv_obj_set_style_bg_color(themeSwatch,lv_color_hex(themeColors[themeRole]),0);Tab5ThemePreview::update(themePreview,themeColors,Tab5ThemePreview::Count);themeUpdating=false;}
 bool themeReadHex(){const char* text=lv_textarea_get_text(themeHex);if(strlen(text)!=7||text[0]!='#'){note("Couleur : #RRGGBB");return false;}for(unsigned i=1;i<7;++i)if(!isxdigit(text[i])){note("Couleur : #RRGGBB");return false;}themeColors[themeRole]=strtoul(text+1,nullptr,16);themeToneReset();return true;}
 void themeSelect(unsigned role){if(role>=Tab5ThemePreview::Count||themeUpdating)return;if(!themeReadHex()){themeUpdating=true;lv_dropdown_set_selected(themeRoles,themeRole);themeUpdating=false;return;}themeRole=role;themeToneReset();themeRefresh();}
 void themeChoices(){
  if(!themeLoad)return;unsigned selected=lv_dropdown_get_selected(themeLoad);String selectedId=themeDeleteId;
  if(hooks.themeReload)hooks.themeReload();
  const size_t count=hooks.themeCount?hooks.themeCount():0;
  Tab5ThemeChoices::populate(themeLoad,count,[&](size_t i){return hooks.themeName(i);},[&](const char* name){return tr(name);},"Thème actuel");
  if(selectedId.length()){selected=0;for(size_t i=0;i<count;++i)if(selectedId==hooks.themeId(i)){selected=unsigned(i)+1;break;}}
  lv_dropdown_set_selected(themeLoad,selected<=count?selected:0);
  themeDeleteArmed=false;if(themeDeleteButton){if(selectedId.length()&&selected)lv_obj_remove_state(themeDeleteButton,LV_STATE_DISABLED);else lv_obj_add_state(themeDeleteButton,LV_STATE_DISABLED);}
  Tab5UiStyle::control(themeLoad);
 }
 void themeLoadSelected(){
  const unsigned selected=lv_dropdown_get_selected(themeLoad);
  if(!selected){if(hooks.themePalette)hooks.themePalette(themeColors);themeDeleteId="";}
  else{
   if(!hooks.themeRead||!hooks.themeRead(selected-1,themeColors))return;
   const char* id=hooks.themeId?hooks.themeId(selected-1):"";themeDeleteId=id;
   lv_textarea_set_text(input,hooks.themeName?tr(hooks.themeName(selected-1)):"");
   // Built-ins are templates: saving creates a personal theme, never edits firmware data.
   lv_textarea_set_text(path,*id?id:"custom");
  }
  themeDeleteArmed=false;if(themeDeleteId.length())lv_obj_remove_state(themeDeleteButton,LV_STATE_DISABLED);else lv_obj_add_state(themeDeleteButton,LV_STATE_DISABLED);
  themeToneReset();themeRefresh();
 }
 void themeSave(){
  if(!themeReadHex())return;const char* id=lv_textarea_get_text(path);const char* name=lv_textarea_get_text(input);
  if(!*id||strlen(id)>23||strspn(id,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")!=strlen(id)){note("Identifiant invalide");return;}
  if(!*name||strlen(name)>63||strchr(name,'\n')){note("Nom du thème invalide");return;}
  Tab5StorageUse lease;if(!lease){note("Carte SD indisponible");return;}JsonDocument doc(tab5JsonAllocator());if(!readJson("/System/themes.json",doc)){note("Fichier de thèmes invalide");return;}
  doc["version"]=1;if(doc["themes"].isNull())doc["themes"].to<JsonArray>();auto list=doc["themes"].as<JsonArray>();JsonObject found;
  for(JsonObject item:list)if(!strcmp(item["id"]|"",id)){found=item;break;}
  if(found.isNull()){if(list.size()>=16){note("Maximum : 16 thèmes personnels");return;}found=list.add<JsonObject>();}
   found["id"]=id;found["name"]=name;tab5theme::writePalette(themeColors,[&](const char* key,char* hex){found[key]=hex;});
  if(!writeJson("/System/themes.json",doc)){note("Enregistrement impossible");return;}if(hooks.themesChanged)hooks.themesChanged();if(hooks.themeApply)hooks.themeApply(id);themeChoices();themeRefresh();note("Thème enregistré");
 }
 void themeDelete(){
  if(!themeDeleteId.length())return;
  if(!themeDeleteArmed){themeDeleteArmed=true;note("Supprimer ce thème ? Appuyez à nouveau pour confirmer.");return;}
  themeDeleteArmed=false;Tab5StorageUse lease;if(!lease){note("Carte SD indisponible");return;}
  JsonDocument doc(tab5JsonAllocator());if(!readJson("/System/themes.json",doc)){note("Fichier de thèmes invalide");return;}
  auto list=doc["themes"].as<JsonArray>();int found=-1;
  for(unsigned i=0;i<list.size();++i)if(themeDeleteId==String(list[i]["id"]|"")){if(found>=0){note("Fichier de thèmes invalide");return;}found=i;}
  if(found<0){themeChoices();return;}list.remove(found);
  if(!writeJson("/System/themes.json",doc)){note("Enregistrement impossible");return;}
  if(hooks.themesChanged)hooks.themesChanged();themeChoices();note("Thème supprimé");
 }
 void buildTheme(){
  themeDeleteId="";themeDeleteArmed=false;themeDeleteButton=nullptr;themeTone=themeToneValue=nullptr;pendingThemeRole=-1;themeRole=0;if(hooks.themePalette)hooks.themePalette(themeColors);
  label("Nom du thème",0,58,300);label("Identifiant",610,58,300);
  input=field(tr("Mon thème"),0,94,590);lv_textarea_set_max_length(input,63);path=field("custom",610,94,510);lv_textarea_set_max_length(path,23);
  themeLoad=dropdown("Thème actuel",0,158,410);themeChoices();
  lv_obj_add_event_cb(themeLoad,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->themeLoadSelected();},LV_EVENT_VALUE_CHANGED,this);
  lv_obj_add_event_cb(themeLoad,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->themeChoices();},LV_EVENT_READY,this);
   String roles;for(auto* title:{"Fond","Surface","Relief","Bordure","Texte","Texte secondaire","Actif","Curseur","Sélection","Pointeur","Contour du pointeur","Interrupteur : fond inactif","Interrupteur : fond actif","Interrupteur : bouton inactif","Interrupteur : bouton actif"}){if(roles.length())roles+='\n';roles+=tr(title);}themeRoles=dropdown(roles.c_str(),426,158,380);
  lv_obj_add_event_cb(themeRoles,[](lv_event_t* e){auto* a=static_cast<Impl*>(lv_event_get_user_data(e));a->themeSelect(lv_dropdown_get_selected(a->themeRoles));},LV_EVENT_VALUE_CHANGED,this);
  themeHex=field("#000000",822,158,220);lv_textarea_set_max_length(themeHex,7);
  themeSwatch=lv_obj_create(root);Tab5ThemeLive::preserve(themeSwatch);lv_obj_set_pos(themeSwatch,1060,158);lv_obj_set_size(themeSwatch,48,48);lv_obj_remove_flag(themeSwatch,LV_OBJ_FLAG_SCROLLABLE);
  for(unsigned i=0;i<3;++i){label(i==0?"R":i==1?"G":"B",i*270,224,30);auto* slider=themeSliders[i]=lv_slider_create(root);lv_obj_set_pos(slider,i*270+36,230);lv_obj_set_size(slider,204,24);lv_slider_set_range(slider,0,255);lv_obj_add_event_cb(slider,[](lv_event_t* e){auto* a=static_cast<Impl*>(lv_event_get_user_data(e));if(a->themeUpdating)return;uint32_t value=0;for(auto* s:a->themeSliders)value=(value<<8)|lv_slider_get_value(s);a->themeColors[a->themeRole]=value;a->themeToneReset();a->themeRefresh();},LV_EVENT_VALUE_CHANGED,this);}
  button("Valider couleur",826,218,284,202);
  label("Noir",0,282,88);label("Blanc",698,282,98);
  themeTone=lv_slider_create(root);lv_obj_set_pos(themeTone,108,288);lv_obj_set_size(themeTone,500,24);lv_slider_set_range(themeTone,-100,100);lv_slider_set_mode(themeTone,LV_SLIDER_MODE_SYMMETRICAL);
  lv_obj_set_ext_click_area(themeTone,16);lv_obj_add_event_cb(themeTone,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->themeToneChanged();},LV_EVENT_VALUE_CHANGED,this);
  themeToneValue=label("0",626,282,70);button("Enregistrer",826,338,284,203);themeDeleteButton=button("Supprimer",604,338,202,204);lv_obj_add_state(themeDeleteButton,LV_STATE_DISABLED);
  themePreview=Tab5ThemePreview::create(root);lv_obj_set_pos(themePreview,0,398);lv_obj_set_size(themePreview,1120,266);
  lv_obj_add_event_cb(themePreview,[](lv_event_t* e){if(lv_event_get_target_obj(e)!=lv_event_get_current_target_obj(e)||!lv_event_get_param(e))return;auto* a=static_cast<Impl*>(lv_event_get_user_data(e));unsigned role=*static_cast<unsigned*>(lv_event_get_param(e));a->pendingThemeRole=role;},LV_EVENT_VALUE_CHANGED,this);
  lv_obj_set_pos(status,0,342);lv_obj_set_width(status,584);themeToneReset();themeRefresh();
 }
 bool nativeHistory(bool favorite=false){
  Tab5StorageUse lease;if(!lease)return false;JsonDocument doc(tab5JsonAllocator());if(!readJson("/System/native-apps.json",doc))return false;
  auto items=doc["recent"].is<JsonArray>()?doc["recent"].as<JsonArray>():doc["recent"].to<JsonArray>();
  const char* app=lv_textarea_get_text(path);const char* arg=lv_textarea_get_text(input);if(!*app)return false;
  bool wasFavorite=favorite;for(unsigned i=0;i<items.size();){JsonObject row=items[i];if(!strcmp(row["app"]|"",app)&&!strcmp(row["file"]|"",arg)){wasFavorite|=row["favorite"]|false;items.remove(i);}else ++i;}
  if(items.size()>=24){bool removed=false;for(unsigned i=0;i<items.size();++i)if(!(items[i]["favorite"]|false)){items.remove(i);removed=true;break;}if(!removed)items.remove(0);}
  auto row=items.add<JsonObject>();row["app"]=app;row["file"]=arg;row["favorite"]=wasFavorite;bool ok=writeJson("/System/native-apps.json",doc);if(ok)nativeChoices();return ok;
 }
 void nativeChoices(){if(!nativeRecent)return;Tab5StorageUse lease;if(!lease)return;JsonDocument doc(tab5JsonAllocator());String list=tr("Récents et favoris");if(readJson("/System/native-apps.json",doc)){auto items=doc["recent"].as<JsonArray>();for(unsigned k=items.size();k;k--){auto row=items[k-1];String text=row["file"]|"";if(!text.length())text=row["app"]|"";text=text.substring(text.lastIndexOf('/')+1);list+='\n';if(row["favorite"]|false)list+="* ";list+=text;}}lv_dropdown_set_options(nativeRecent,list.c_str());}
 void nativeRecall(unsigned chosen){Tab5StorageUse lease;if(!lease)return;JsonDocument doc(tab5JsonAllocator());if(!readJson("/System/native-apps.json",doc))return;auto items=doc["recent"].as<JsonArray>();if(!chosen||chosen>items.size())return;auto row=items[items.size()-chosen];lv_textarea_set_text(path,row["app"]|"");lv_textarea_set_text(input,row["file"]|"");if(nativeMode)lv_dropdown_set_selected(nativeMode,0);nativeModeChanged();}
 void nativeModeChanged(){
  const bool show=!nativeMode||lv_dropdown_get_selected(nativeMode)!=unsigned(Tab5LaunchMode::None);
  for(auto* o:{input,nativeFile,nativeFolder})if(o){if(show)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
 }
 void loadNativeProfile(){
  if(!nativeMode||!path||!input)return;Tab5LaunchProfile profile;tab5LaunchProfileLoad(lv_textarea_get_text(path),profile);
  lv_dropdown_set_selected(nativeMode,unsigned(profile.mode));char shown[266]{};if(*profile.argument)tab5LaunchDisplayPath(profile.argument,shown,sizeof(shown));lv_textarea_set_text(input,shown);nativeModeChanged();
 }
 bool readNativeProfile(Tab5LaunchProfile& profile){
  profile.mode=Tab5LaunchMode(lv_dropdown_get_selected(nativeMode));profile.argument[0]=0;
  if(profile.mode==Tab5LaunchMode::File&&!tab5LaunchPath(lv_textarea_get_text(input),profile.argument,sizeof(profile.argument))){note("Choisir le fichier à ouvrir");return false;}return true;
 }
 void pick(lv_obj_t* target,bool app=false,bool folder=false){
  hideKeyboard();activeField=target;char internal[261];const char* initial=lv_textarea_get_text(target);if(tab5LaunchPath(initial,internal,sizeof(internal)))initial=internal;
  Tab5FilePicker::open(app?"/Applications":initial,app?".elf":"",folder,[](const char* chosen,void* data){auto* a=static_cast<Impl*>(data);if(a->activeField&&lv_obj_is_valid(a->activeField)){char shown[266];lv_textarea_set_text(a->activeField,tab5LaunchDisplayPath(chosen,shown,sizeof(shown))?shown:chosen);if(a->activeField==a->path&&a->tool==Tab5Tool::Executable){String document=lv_textarea_get_text(a->input);a->loadNativeProfile();if(document.length()){lv_dropdown_set_selected(a->nativeMode,0);lv_textarea_set_text(a->input,document.c_str());a->nativeModeChanged();}}}},this);
 }
 bool pythonShortcut(const writing::KeyEvent& e){
  using K=writing::Key;if(!scriptPanel||editing||e.ctrl||e.meta||e.alt)return false;
  auto* focused=Tab5Focus.focused();if(focused&&lv_obj_is_valid(focused)&&lv_obj_check_type(focused,&lv_textarea_class))return false;
  unsigned wanted=e.key==K::Escape||e.key==K::Backspace?1:e.key==K::Left||e.key==K::PageUp?2:e.key==K::Right||e.key==K::PageDown?3:e.key==K::Enter?4:e.key==K::Delete?5:e.key==K::Up?6:e.key==K::Down?7:0;
  if(!wanted)return false;if(wanted==4&&focused&&lv_obj_is_valid(focused)&&lv_obj_check_type(focused,&lv_button_class))return false;
  // Explicit bindings win. Generation guards prevent events reaching a replaced page.
  for(unsigned i=0;i<Tab5PythonUI::MaxWidgets;++i){Tab5PythonUI::Widget w;if(Tab5PythonUI::read(i+1,w,scriptGeneration)&&w.type==2&&w.shortcut==wanted){Tab5PythonUI::event(i+1,1,scriptGeneration);return true;}}
  if(wanted>3)return false;
  for(unsigned i=0;i<Tab5PythonUI::MaxWidgets;++i){Tab5PythonUI::Widget w;if(!Tab5PythonUI::read(i+1,w,scriptGeneration)||w.type!=2)continue;
   bool match=wanted==1?(!strcmp(w.text,tr("Retour"))||!strcmp(w.text,tr("Annuler"))):wanted==2?(!strcmp(w.text,"<")||!strcmp(w.text,tr("Précédent"))):(!strcmp(w.text,">")||!strcmp(w.text,tr("Suivant")));
   if(match){Tab5PythonUI::event(i+1,1,scriptGeneration);return true;}
  }return false;
 }


 void calculate(){auto r=tab5calc::evaluate(lv_textarea_get_text(input),lv_dropdown_get_selected(angle)==0,answer,memory);if(!r){note(r.error);return;}bool saved=calculatorHistory.append(lv_textarea_get_text(input),r.value);answer=r.value;calculatorEntry.accept(input,answer);calculatorHistoryView.refresh(true);note(saved?"Prêt":"Mémoire insuffisante");}
 void calculatorInsert(const char* text){calculatorEntry.insert(input,text);activeField=input;}
 void calculatorConvert(){
  hideKeyboard();Tab5Focus.remember(root);tab5calc::ConversionWindow::Hooks h;
  h.color=[](unsigned role,void* context){return static_cast<Impl*>(context)->color(role);};h.translate=[](const char* text,void* context){return static_cast<Impl*>(context)->tr(text);};
  h.style=[](lv_obj_t* object,void*){Tab5UiStyle::control(object);};h.edit=[](lv_obj_t* object,void* context){static_cast<Impl*>(context)->inputKeyboard(object);};
  h.closed=[](void* context){auto* self=static_cast<Impl*>(context);self->hideKeyboard();self->activeField=self->input;Tab5Focus.restore(self->root);};
  h.use=[](double value,void* context){auto* self=static_cast<Impl*>(context);self->answer=value;self->calculatorEntry.accept(self->input,value);Tab5Focus.focusObject(self->input);};h.context=this;
  calculatorConversion.open(root,lv_textarea_get_text(input),lv_dropdown_get_selected(angle)==0,answer,memory,h);
 }
 Tab5Print::Options printOptions(){Tab5Print::Options o;o.uri=lv_textarea_get_text(uri);o.path=lv_textarea_get_text(path);o.paper=lv_dropdown_get_selected(paper);o.dpi=lv_dropdown_get_selected(dpi)==0?150:lv_dropdown_get_selected(dpi)==1?300:600;o.landscape=lv_dropdown_get_selected(landscape)==1;o.scalePercent=unsigned(atoi(lv_textarea_get_text(scale)));if(printSection&&!mapMode&&!sectionDirty&&!sectionScanning){unsigned selected=lv_dropdown_get_selected(printSection);if(selected&&selected<=sectionCount){const auto& section=printSections[selected-1];o.textStart=section.begin;o.textEnd=section.end;o.textSourceSize=sectionFileSize;}}return o;}
 void refreshKeys(){if(!keyList||!lv_obj_is_valid(keyList))return;unsigned old=lv_dropdown_get_selected(keyList);String previous=old<keys.size()?keys[old].id:String();keys=Tab5KeyStore::list();String choices;
  for(auto& key:keys){if(choices.length())choices+='\n';choices+=key.name+" · "+key.id.substring(0,8);}
  if(!choices.length())choices=tr("Aucune clé — créer dans Réglages");lv_dropdown_set_options(keyList,choices.c_str());
  for(unsigned i=0;i<keys.size();++i)if(keys[i].id==previous){lv_dropdown_set_selected(keyList,i);break;}
 }
 unsigned selectedKey(){unsigned i=lv_dropdown_get_selected(keyList);return i<keys.size()?keys[i].slot:Tab5KeyStore::MaxKeys;}
 String uniqueCopy(const String& source,bool decrypt){
  String candidate=decrypt?source.substring(0,source.length()-4):source+".t5v";
  if(!tab5FS.exists(candidate)&&!tab5FS.exists(candidate+".tmp"))return candidate;
  int dot=candidate.lastIndexOf('.');if(dot<candidate.lastIndexOf('/'))dot=candidate.length();
  for(unsigned n=1;n<10000;++n){String copy=candidate.substring(0,dot)+"-"+String(n)+candidate.substring(dot);if(!tab5FS.exists(copy)&&!tab5FS.exists(copy+".tmp"))return copy;}return "";
 }
 static void visible(lv_obj_t* object,bool show){if(!object)return;if(show)lv_obj_remove_flag(object,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(object,LV_OBJ_FLAG_HIDDEN);}
 static void enabled(lv_obj_t* object,bool yes){if(!object)return;if(yes)lv_obj_remove_state(object,LV_STATE_DISABLED);else lv_obj_add_state(object,LV_STATE_DISABLED);}
 lv_obj_t* storageCard(){
  auto* card=lv_obj_create(storagePanel);lv_obj_set_width(card,lv_pct(100));lv_obj_set_height(card,LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(card,18,0);lv_obj_set_style_pad_row(card,14,0);lv_obj_set_style_border_width(card,1,0);
  lv_obj_set_style_border_color(card,lv_color_hex(color(3)),0);lv_obj_set_style_bg_color(card,lv_color_hex(color(1)),0);
  lv_obj_set_style_radius(card,12,0);lv_obj_set_flex_flow(card,LV_FLEX_FLOW_COLUMN);lv_obj_remove_flag(card,LV_OBJ_FLAG_SCROLLABLE);return card;
 }
 lv_obj_t* storageLabel(lv_obj_t* host,const char* text){
  auto* object=lv_label_create(host);lv_obj_set_width(object,lv_pct(100));lv_label_set_long_mode(object,LV_LABEL_LONG_WRAP);lv_label_set_text(object,tr(text));return object;
 }
 lv_obj_t* storageRow(lv_obj_t* host){
  auto* row=lv_obj_create(host);lv_obj_remove_style_all(row);lv_obj_set_width(row,lv_pct(100));lv_obj_set_height(row,LV_SIZE_CONTENT);
  lv_obj_set_style_pad_column(row,12,0);lv_obj_set_style_pad_row(row,12,0);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW_WRAP);return row;
 }
 lv_obj_t* storageButton(lv_obj_t* host,const char* text,int action){
  auto* object=button(text,0,0,240,action);lv_obj_set_parent(object,host);lv_obj_set_height(object,LV_SIZE_CONTENT);
  lv_obj_set_style_pad_ver(object,12,0);lv_obj_set_flex_grow(object,1);lv_obj_set_style_min_width(object,220,0);
  auto* caption=lv_obj_get_child(object,0);lv_obj_set_width(caption,lv_pct(100));lv_label_set_long_mode(caption,LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);return object;
 }
 void buildStorage(){
  storageRepairRequested=false;storageLast=0;storagePanel=lv_obj_create(root);lv_obj_remove_style_all(storagePanel);
  lv_obj_set_pos(storagePanel,0,storageEmbedded?0:58);lv_obj_set_width(storagePanel,lv_pct(100));lv_obj_set_height(storagePanel,LV_SIZE_CONTENT);
  lv_obj_set_style_pad_row(storagePanel,16,0);lv_obj_set_flex_flow(storagePanel,LV_FLEX_FLOW_COLUMN);
  auto* sd=storageCard();output=storageLabel(sd,"");auto* sdActions=storageRow(sd);
  storageMount=storageButton(sdActions,"Reconnecter la microSD",226);storageEject=storageButton(sdActions,"Éjecter",227);storageOpen=storageButton(sdActions,"Ouvrir",223);
  storageUsbCard=storageCard();storageUsbInfo=storageLabel(storageUsbCard,"");auto* usbActions=storageRow(storageUsbCard);
  nativeRun=storageButton(usbActions,"Connecter",220);nativeStop=storageButton(usbActions,"Éjecter",221);nativeRecent=storageButton(usbActions,"Ouvrir",222);
  auto* usbOptions=storageRow(storageUsbCard);path=storageButton(usbOptions,"",224);
  storageButton(usbOptions,"Formater…",230);
  auto* usbPort=storageCard();storageLabel(usbPort,"Port USB");uri=storageButton(storageRow(usbPort),"",225);
  auto* repair=storageRepairCard=storageCard();storageLabel(repair,"Ressources système");auto* repairActions=storageRow(repair);
  storageRepair=storageButton(repairActions,"Restaurer les fichiers manquants",228);storageCancel=storageButton(repairActions,"Annuler",229);
  storageProgress=storageLabel(repair,"");if(!status)status=storageLabel(storagePanel,"");else{lv_obj_set_parent(status,storagePanel);lv_label_set_text(status,"");}
  lv_obj_set_width(status,lv_pct(100));lv_obj_set_style_text_color(status,lv_color_hex(color(3)),0);serviceStorage(true);
 }
 void serviceStorage(bool force=false){
  if(!root||tool!=Tab5Tool::Storage||(!force&&(!lv_obj_is_visible(root)||millis()-storageLast<180)))return;storageLast=millis();
  const bool sd=hooks.sdMounted&&hooks.sdMounted(),usb=Tab5UsbStorage::attached()||Tab5UsbStorage::mounted();
  const bool mounted=Tab5UsbStorage::mounted(),pending=Tab5UsbStorage::pending(),repair=Tab5SystemResources::busy();
  char information[256];if(sd)snprintf(information,sizeof(information),tr("microSD · %llu Mio"),static_cast<unsigned long long>(SD.cardSize()/(1024*1024)));
  else snprintf(information,sizeof(information),"%s",tr("Carte SD non détectée"));
  if(strcmp(lv_label_get_text(output),information))lv_label_set_text(output,information);
  visible(storageMount,!sd);enabled(storageMount,!repair);visible(storageEject,sd);enabled(storageEject,!repair);visible(storageOpen,sd);
  visible(storageUsbCard,usb);
  if(usb){
   snprintf(information,sizeof(information),tr("USB · %s · %llu Mio"),tr(Tab5UsbStorage::status()),static_cast<unsigned long long>(Tab5UsbStorage::capacityBytes()/(1024*1024)));
   if(strcmp(lv_label_get_text(storageUsbInfo),information))lv_label_set_text(storageUsbInfo,information);
   visible(nativeRun,!mounted);enabled(nativeRun,!pending);visible(nativeStop,mounted);enabled(nativeStop,!pending);visible(nativeRecent,mounted);enabled(nativeRecent,!pending);
   auto* automatic=lv_obj_get_child(path,0);const char* automaticText=tr(Tab5UsbStorage::automatic()?"Connexion automatique : oui":"Connexion automatique : non");if(strcmp(lv_label_get_text(automatic),automaticText))lv_label_set_text(automatic,automaticText);
  }
  auto* mode=lv_obj_get_child(uri,0);const char* modeText=tr(writing::usbHighSpeed()?"USB rapide":"USB compatible hubs");if(strcmp(lv_label_get_text(mode),modeText))lv_label_set_text(mode,modeText);
  visible(storageRepairCard,sd||repair);visible(storageRepair,!repair);enabled(storageRepair,sd);visible(storageCancel,repair);visible(storageProgress,repair||storageRepairRequested);
  if(repair||storageRepairRequested){snprintf(information,sizeof(information),"%s\n%u / %u · %u %s",tr(Tab5SystemResources::status()),unsigned(Tab5SystemResources::checked()),unsigned(Tab5SystemResources::total()),unsigned(Tab5SystemResources::created()),tr("fichiers recréés"));if(strcmp(lv_label_get_text(storageProgress),information))lv_label_set_text(storageProgress,information);}
  if(writing::usbHighSpeed()!=writing::usbHighSpeedActive())note("Redémarrez pour appliquer le mode USB");
  else if(!strcmp(lv_label_get_text(status),tr("Redémarrez pour appliquer le mode USB")))lv_label_set_text(status,"");
  visible(status,*lv_label_get_text(status));
 }
 void editToolbar(int x,int y){
   const char* names[]={"Undo","Redo",LV_SYMBOL_CUT,LV_SYMBOL_COPY,LV_SYMBOL_PASTE,LV_SYMBOL_LIST};
   const char* hints[]={"Annuler · Ctrl+Z","Rétablir · Ctrl+Y","Couper la sélection · Ctrl+X","Copier la sélection · Ctrl+C","Coller · Ctrl+V","Tout sélectionner · Ctrl+A"};
   for(unsigned i=0;i<6;++i){editButtons[i]=button(names[i],x,y,i<2?72:52,360+i);Tab5Focus.describe(editButtons[i],tr(hints[i]));x+=i<2?80:60;}
  }
  void editState(){
   if(!editButtons[0])return;auto* target=activeField&&lv_obj_is_valid(activeField)?activeField:input;
   bool field=target&&lv_obj_check_type(target,&lv_textarea_class),readonly=!field||Tab5TextInput::readOnly(target),selection=Tab5TextInput::hasSelection(target);
   enabled(editButtons[0],field&&!readonly&&Tab5TextInput::canUndo(target));enabled(editButtons[1],field&&!readonly&&Tab5TextInput::canRedo(target));
   enabled(editButtons[2],!readonly&&selection);enabled(editButtons[3],selection);enabled(editButtons[4],!readonly&&*Tab5TextInput::clipboard());enabled(editButtons[5],field&&*lv_textarea_get_text(target));
  }
  void editAction(unsigned action){
   auto* target=activeField&&lv_obj_is_valid(activeField)?activeField:input;if(!target)return;
   writing::KeyEvent e;e.key=writing::Key::Text;e.ctrl=true;e.text[0]="zyxcva"[action];Tab5TextInput::key(target,e);Tab5Focus.focusObject(target);editState();
  }
  void panelModeChanged(){
   const auto* spec=Tab5AppPanel::specification();if(!spec||!panelMode)return;
   const unsigned selected=lv_dropdown_get_selected(panelMode);const bool value=(spec->valueModes&(1u<<selected))!=0;
   visible(path,value);visible(panelValueLabel,value);lv_textarea_set_placeholder_text(input,tr((spec->alternateModes&(1u<<selected))?spec->alternateHint:spec->targetHint));
  }
  void clearPanel(){
   Tab5AppPanel::clear();if(output)lv_textarea_set_text(output,"");consoleRevision=Tab5AppPanel::revision();
  }
  void savePanel(){
   if(Tab5AppPanel::busy())return;
   Tab5FilePicker::open("/","",true,[](const char* folder,void* context){
    auto* self=static_cast<Impl*>(context);if(!folder||!self->root||self->tool!=Tab5Tool::AppPanel)return;
    Tab5StorageUse lease;if(!lease){self->note("Stockage indisponible");return;}
    size_t reportLength=0;const char* report=Tab5AppPanel::completedReport(reportLength);if(!report||!reportLength)return;String directory=folder;while(directory.endsWith("/")&&directory.length()>1)directory.remove(directory.length()-1);
    String name;unsigned serial=0;do{name=directory+(directory=="/"?"":"/")+"report-"+String(millis())+"-"+String(serial++)+".txt";}while(tab5FS.exists(name)&&serial<1000);
    if(tab5FS.exists(name)||!tab5AtomicWrite(name.c_str(),report,reportLength)){self->note("Enregistrement impossible");return;}tab5FsChanged();self->note((String(self->tr("Rapport enregistré : "))+name).c_str());
   },this);
  }
  void chooseOpenWith(bool python){
   if(openWithDocument.isEmpty())return;
   hideKeyboard();
   if(!Tab5FilePicker::open(python?"/MicroPython":"/Applications",python?".py":".elf",false,
    [](const char* chosen,void* data){
     auto* self=static_cast<Impl*>(data);if(!self->root||self->tool!=Tab5Tool::OpenWith)return;
     String application=chosen,document=self->openWithDocument;
     String lower=application;lower.toLowerCase();
     if(lower.endsWith(".py")){Tab5Tools.openPythonApp(application.c_str(),document.c_str());return;}
     self->open(Tab5Tool::Executable,application.c_str());
     if(self->root&&self->tool==Tab5Tool::Executable&&self->input){
      char shown[266];const bool display=tab5LaunchDisplayPath(document.c_str(),shown,sizeof(shown));
      lv_textarea_set_text(self->input,display?shown:document.c_str());
      lv_dropdown_set_selected(self->nativeMode,unsigned(Tab5LaunchMode::Ask));self->nativeModeChanged();
     }
    },this))note("Sélecteur de fichiers indisponible");
  }
  void act(int action){
   if(action==212){Tab5ContextMenu::clearOutput(output);return;}
   if(action>=360&&action<366){editAction(action-360);return;}
   if(tool==Tab5Tool::AppPanel&&action>=350&&action<=353){
    if(action==350){hideKeyboard();if(Tab5AppPanel::submit(lv_dropdown_get_selected(panelMode),lv_textarea_get_text(input),lv_textarea_get_text(path)))note("Analyse en cours…");else note("Impossible de démarrer l’analyse");}
    if(action==351)Tab5AppPanel::stop();if(action==352)savePanel();if(action==353)clearPanel();return;
   }
   if(action==220){Tab5UsbStorage::mount();return;}
   if(action==221){Tab5UsbStorage::eject();return;}
   if(action==222||action==223){if(hooks.browse)hooks.browse(action==222?"/USB":"/");return;}
   if(action==225){writing::usbHighSpeed(!writing::usbHighSpeed());return;}
   if(action==224){Tab5UsbStorage::automatic(!Tab5UsbStorage::automatic());return;}
   if(action==226||action==227){if(hooks.sdAction)note(hooks.sdAction(action==227));return;}
   if(action==228){storageRepairRequested=true;Tab5SystemResources::start(hooks.sdMounted&&hooks.sdMounted());serviceStorage(true);return;}
   if(action==229){Tab5SystemResources::cancel();serviceStorage(true);return;}
   if(action==230){hideKeyboard();if(!Tab5UsbFormatUi::open({hooks.translate,hooks.font,hooks.color}))note("Stockage USB indisponible");return;}

  if(action==0){if(tool==Tab5Tool::Python)act(13);else close();return;}
  if(action==90){inputKeyboard(tool==Tab5Tool::Sdr?Tab5SdrUi::input():activeField?activeField:input?input:path,true);return;}
  if(action==91){hideKeyboard();return;}
  if(action>=10&&action<90)hideKeyboard();
  if(securityAction(action))return;
  if(tool==Tab5Tool::Calculator){
    if(action==1){calculate();return;}if(action==2){calculatorEntry.recall(input,"");return;}if(action==3){calculatorEntry.result=false;writing::KeyEvent e;e.key=writing::Key::Backspace;Tab5TextInput::key(input,e);return;}
    if(action==4){memory=answer;note("Valeur mémorisée");return;}if(action==5){calculatorInsert("mem");return;}
   if(action==6){memory+=answer;note("Mémoire augmentée");return;}if(action==7){memory-=answer;note("Mémoire diminuée");return;}if(action==8){memory=0;note("Mémoire effacée");return;}
    if(action==9){calculatorConvert();return;}
    if(const char* text=Tab5CalculatorUi::insertion(action))calculatorInsert(text);return;
  }
  if(tool==Tab5Tool::Firmware){
   if((action==51||action==53)&&Tab5BatteryGuard::status().countdown){note("Branchez la tablette avant l’installation ou le redémarrage");return;}
   if(action==50||action==51||action==53){if(Tab5Print::busy()||Tab5Vault::busy()||Tab5Python::running()||Tab5MapPrint::busy()||tab5ExecutableBusy()){firmwareUpdates=false;note("Terminer les opérations en cours avant l’installation");return;}}
   if(action==50)firmwareUpdates=Tab5Firmware::inspect(lv_textarea_get_text(path));
   if(action==51)firmwareUpdates=Tab5Firmware::install(lv_textarea_get_text(path));if(action==52)Tab5Firmware::cancel();if(action==53)Tab5Firmware::restart();return;
  }
  if(tool==Tab5Tool::Theme){if(action==202){if(themeReadHex())themeRefresh();}if(action==203)themeSave();if(action==204)themeDelete();return;}
  if(tool==Tab5Tool::Terminal){
   if(action==210){if(Tab5Terminal::submit(lv_textarea_get_text(input))){lv_textarea_set_text(input,"");note("Commande en cours");}else note("Terminal occupé ou mémoire insuffisante");}
   if(action==211)Tab5Terminal::stop();
   if(action==215)Tab5TextInput::copySelection(output);
   visible(nativeStop,Tab5Terminal::busy());
   return;
  }
  if(tool==Tab5Tool::OpenWith){
   if(action==370){
    if(!hooks.openNativeFile||!nativeMode)return;
    const unsigned selected=lv_dropdown_get_selected(nativeMode);
    if(selected>=unsigned(Tab5NativeFileApp::Count))return;
    String document=openWithDocument;auto launch=hooks.openNativeFile;
    if(!tab5FS.exists(document)){note("Fichier inaccessible");return;}
    close();launch(Tab5NativeFileApp(selected),document.c_str());return;
   }
   if(action==371||action==372)chooseOpenWith(action==372);
   return;
  }
  if(tool==Tab5Tool::Executable){
   if(action==63){pick(path,true);return;}if(action==64){pick(input);return;}if(action==65){pick(input,false,true);return;}
   if(action==66){note(nativeHistory(true)?"Favori enregistré":"Enregistrement impossible");return;}if(action==67){if(lv_dropdown_get_option_count(nativeRecent)>1){nativeRecall(1);act(60);}return;}
   if(action==68){Tab5LaunchProfile profile;if(readNativeProfile(profile))note(tab5LaunchProfileSave(lv_textarea_get_text(path),profile)?"Lancement enregistré":"Enregistrement impossible");return;}
   if(action==60){
    if(Tab5Firmware::busy()){note("Terminer l’installation du firmware avant de lancer une application");return;}
    if(tab5ExecutableBusy()){note("Une application est déjà en cours");return;}
    char executable[261],argument[261]{};
    if(!tab5LaunchPath(lv_textarea_get_text(path),executable,sizeof(executable))){note("Chemin invalide");return;}
    const auto mode=Tab5LaunchMode(lv_dropdown_get_selected(nativeMode));const char* requested=input?lv_textarea_get_text(input):"";
    if(mode!=Tab5LaunchMode::None&&(*requested||mode==Tab5LaunchMode::File)){
     if(!tab5LaunchPath(requested,argument,sizeof(argument))){note("Choisir le fichier à ouvrir");return;}
     if(!tab5FS.exists(argument)){note("Fichier inaccessible");return;}
    }
    hideKeyboard();
    if(hooks.runExecutable){nativeHistory();if(hooks.runExecutable(executable,*argument?argument:nullptr)){close();return;}}
    else {note("Lancement des applications indisponible");return;}
    note(tab5ExecutableStatus());consoleRevision=UINT32_MAX;
   }
   if(action==61){tab5ExecutableStop();note(tab5ExecutableStatus());}
   if(action==62){Tab5ContextMenu::clearOutput(output);}
   return;
  }
  if(tool==Tab5Tool::Python){
   if(action==13){Tab5PythonReader::close();Tab5PythonUI::close();Tab5Python::stop();close(false);}
   return;
  }
  if(tool==Tab5Tool::Print){
   if(action==27||action==28){if(sectionScanning||sectionDirty||Tab5Print::busy())return;if(action==27){if(!sectionPage)return;--sectionPage;}else {if((sectionPage+1)*SectionPageSize>=sectionTotal)return;++sectionPage;}sectionDirty=true;changedPrint();return;}
   if(action==23){if(Tab5Printers::search()){scanning=true;printUpdates=false;note(Tab5Printers::status());}return;}
   if(action==24){unsigned n=lv_dropdown_get_selected(printers);if(n<detected.size()){lv_textarea_set_text(uri,detected[n].uri.c_str());printerUri=detected[n].uri;lv_dropdown_set_text(printers,nullptr);if(Tab5Printers::remember(printerUri))note("Imprimante sélectionnée");else note(Tab5Printers::status());}return;}
   if(action==25){if(Tab5Print::trustCertificate()){note(Tab5Print::status());lv_obj_add_flag(trustButton,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(trustHint,LV_OBJ_FLAG_HIDDEN);}else note(Tab5Print::status());return;}
   if(action==26){tab5print::Endpoint e;if(tab5print::endpoint(lv_textarea_get_text(uri),e)&&!e.tls){String secure=String("ipps://")+e.host.c_str()+":"+String(e.port)+e.path.c_str();lv_textarea_set_text(uri,secure.c_str());act(20);}return;}
   if(action==20||action==21){
    if(action==21&&(sectionDirty||sectionScanning)){note("Lecture des titres en cours…");return;}
    printUpdates=false;rememberPrinter();auto o=printOptions();printerUri=o.uri;
    if(o.scalePercent<25||o.scalePercent>200){note("Échelle : 25 à 200 %");return;}
    if(Tab5Print::busy()){note("Opération en cours : patienter ou annuler");return;}
    estimateDirty=false;estimateWaiting=false;
    if(mapMode&&action==21){if(mapPreparing){note("Impression en cours");return;}if(hooks.beginMapPrint&&hooks.beginMapPrint(o.paper,o.dpi,o.landscape,o.scalePercent)){mapPreparing=true;mapOptions=o;}else note(Tab5MapPrint::status());return;}
    printUpdates=true;if(!Tab5Print::start(o,action==20))note(Tab5Print::status());return;
   }
   if(action==22){if(mapPreparing){Tab5MapPrint::cancel();mapPreparing=false;}Tab5Print::cancel();printUpdates=false;estimateDirty=false;estimateWaiting=false;note("Envoi interrompu : vérifier la file de l’imprimante");return;}
  }
  if(tool==Tab5Tool::Vault){
   if(action==33){open(Tab5Tool::Keys,nullptr);return;}
   if(action==34){close();if(hooks.browse)hooks.browse("/Documents");return;}
   if(action==32){Tab5Vault::cancel();return;}
   if(action==30){vaultUpdates=false;
    if(!path||!strlen(lv_textarea_get_text(path))){note("Choisir un fichier dans l’explorateur");return;}
    String source=lv_textarea_get_text(path),dest=uniqueCopy(source,lockedFile);if(!dest.length()){note("Aucun nom de fichier disponible");return;}
    bool started=false;
    if(legacyFile){const char* pass=lv_textarea_get_text(password);if(!*pass){note("Saisir le mot de passe du fichier");return;}if(strlen(pass)>256){note("Valeur trop longue");return;}started=Tab5Vault::start(source,dest,pass,true);}
    else started=Tab5Vault::startKey(source,dest,selectedKey(),lockedFile);
    if(!started){note(Tab5Vault::status());return;}
    lv_label_set_text(destination,dest.c_str());vaultUpdates=true;if(password)erasePassword(password);return;
   }
  }

 }
 #include "Tab5CredentialUi.inc"
 void open(Tab5Tool which,const char* selected,bool launch=false,bool changePass=false,bool recover=false){close(true);if(root)return;mapMode=false;mapFallbacks=0;scanning=false;estimateDirty=estimateWaiting=false;printUpdates=vaultUpdates=false;lastOutput="";lockedFile=legacyFile=false;tool=which;vaultGate=false;recoveryPage=recover;vaultChanging=changePass;vaultUnlocking=false;vaultSelection=selected?selected:"";consoleRevision=UINT32_MAX;scriptLauncher=launch;firmwareUpdates=false;root=lv_obj_create(parent);lv_obj_set_pos(root,0,0);lv_obj_set_size(root,lv_pct(100),lv_obj_get_content_height(parent));lv_obj_set_style_bg_color(root,lv_color_hex(color(0)),0);lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);lv_obj_set_style_text_color(root,lv_color_hex(color(2)),0);lv_obj_set_style_pad_all(root,16,0);lv_obj_set_style_border_width(root,0,0);lv_obj_set_style_radius(root,0,0);lv_obj_set_scroll_dir(root,LV_DIR_VER);lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLL_CHAIN);lv_obj_scroll_to(root,0,0,LV_ANIM_OFF);if(hooks.font)lv_obj_set_style_text_font(root,hooks.font(),0);
  layoutAnchorCount=0;layoutWidth=0;toolHeading=toolClose=toolKeyboard=nullptr;
  const char* titles[]={"Calculatrice","MicroPython","Impression","Coffre chiffré","Clés de chiffrement","Firmware","Application","Créer un thème","Terminal","Stockage","RTL-SDR","Application","Coffre de mots de passe","Ouvrir avec…"};toolHeading=label(titles[unsigned(which)],0,0,990);lv_label_set_long_mode(toolHeading,LV_LABEL_LONG_DOT);toolClose=button(LV_SYMBOL_CLOSE,1094,0,48,0);if(which!=Tab5Tool::Firmware&&which!=Tab5Tool::OpenWith)toolKeyboard=button(LV_SYMBOL_KEYBOARD,1018,0,60,90);
  status=label("Prêt",0,568,1120);lv_obj_set_style_text_color(status,lv_color_hex(color(3)),0);
  bool secured=securityTool();
  if(secured&&(!Tab5KeyStore::unlocked()||changePass||recover||Tab5KeyStore::resetPending()||(Tab5KeyStore::usingDevice()&&!Tab5KeyStore::pinConfigured()))){buildSecurityGate();return;}
  if(secured)tab5_env_import_secrets();
  if(which==Tab5Tool::OpenWith){
   openWithDocument=selected?selected:"";
   label("Fichier",0,64,1128);auto* document=label("",0,102,1128);
   lv_label_set_text(document,openWithDocument.c_str());lv_label_set_long_mode(document,LV_LABEL_LONG_DOT);
   label("Application native",0,170,1128);String choices;
   for(unsigned i=0;i<unsigned(Tab5NativeFileApp::Count);++i){if(i)choices+='\n';choices+=tr(Tab5OpenWith::nativeName(Tab5NativeFileApp(i)));}
   nativeMode=dropdown(choices.c_str(),0,210,850);lv_dropdown_set_selected(nativeMode,unsigned(Tab5OpenWith::preferred(selected)));
   auto* launch=button("Ouvrir",866,210,262,370);if(!hooks.openNativeFile)lv_obj_add_state(launch,LV_STATE_DISABLED);
   auto* elf=button("Choisir une application ELF…",0,296,552,371);auto* python=button("Choisir un script Python…",576,296,552,372);
   for(auto* choice:{elf,python}){auto* caption=lv_obj_get_child(choice,0);Tab5Focus.describe(choice,lv_label_get_text(caption));lv_obj_set_width(caption,lv_pct(100));lv_label_set_long_mode(caption,LV_LABEL_LONG_DOT);lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(caption);}
   lv_obj_set_pos(status,0,456);note("Choisir une application");
  }else if(which==Tab5Tool::Sdr){
   visible(status,false);
   if(!Tab5SdrUi::open(root,hooks)){visible(status,true);note("Mémoire insuffisante");}
  }else if(which==Tab5Tool::Storage){
    buildStorage();
   }else if(which==Tab5Tool::Theme){buildTheme();}
  else if(which==Tab5Tool::Terminal){
   Tab5Terminal::open();output=field("",0,60,1128,444,false);Tab5TextInput::setReadOnly(output);
   input=field("",0,524,802,52,true);lv_textarea_set_max_length(input,4096);
   lv_obj_add_event_cb(input,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->act(210);},LV_EVENT_READY,this);
   button("Exécuter",818,524,148,210);nativeStop=button("Arrêter",982,524,146,211);visible(nativeStop,Tab5Terminal::busy());
   editToolbar(0,584);bindConsole(1076,584);lv_obj_set_pos(status,420,592);lv_obj_set_width(status,632);activeField=input;editState();
  }else if(which==Tab5Tool::AppPanel){
    const auto* spec=Tab5AppPanel::specification();if(!spec){close();return;}
    lv_label_set_text(toolHeading,tr(spec->title));
    label(spec->targetLabel,0,64,690);input=field(spec->initialTarget,0,100,742);lv_textarea_set_max_length(input,2048);
    panelMode=dropdown(spec->modes,762,100,366);
    panelValueLabel=label(spec->valueLabel,0,160,940);path=field(spec->value,0,198,742);lv_textarea_set_max_length(path,512);
    nativeRun=button(spec->runLabel,762,198,176,350);nativeStop=button("Arrêter",952,198,176,351);visible(nativeStop,Tab5AppPanel::busy());
    output=field("",0,266,1128,282,false);Tab5TextInput::setReadOnly(output);
    Tab5ContextMenu::bindOutput(output,[](void* user){static_cast<Impl*>(user)->clearPanel();},this);
    editToolbar(0,566);auto* clear=button(LV_SYMBOL_TRASH,968,566,72,353);Tab5Focus.describe(clear,tr("Effacer la sortie"));
    panelSave=button(LV_SYMBOL_SAVE,1056,566,72,352);Tab5Focus.describe(panelSave,tr("Enregistrer le rapport · Ctrl+S"));
    lv_obj_set_pos(status,420,572);lv_obj_set_width(status,532);activeField=input;editState();panelModeChanged();
    lv_obj_add_event_cb(panelMode,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->panelModeChanged();},LV_EVENT_VALUE_CHANGED,this);
    Tab5Focus.describe(nativeRun,tr("Analyser · Ctrl+Entrée"));Tab5Focus.describe(nativeStop,tr("Arrêter · Ctrl+."));
   }else if(which==Tab5Tool::Calculator){
    calculator={};calculatorEntry.result=false;calculator.input=input=field("",0,64,800,52);lv_textarea_set_max_length(input,2048);Tab5TextInput::track(input);calculator.angle=angle=dropdown("Degrés\nRadians",824,64,304);calculator.status=status;
    lv_obj_add_event_cb(input,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->calculatorEntry.result=false;},LV_EVENT_CLICKED,this);
    calculator.history=calculatorHistoryView.create(root,calculatorHistory,[](const char* text,void* context){auto* self=static_cast<Impl*>(context);self->calculatorEntry.recall(self->input,text);self->inputKeyboard(self->input);},[](lv_obj_t* object,bool result,void* context){auto* self=static_cast<Impl*>(context);Tab5UiStyle::control(object);lv_obj_set_style_bg_opa(object,LV_OPA_TRANSP,0);lv_obj_set_style_text_color(object,lv_color_hex(self->color(result?2:3)),0);},this);
    lv_obj_set_style_bg_color(calculator.history,lv_color_hex(color(1)),0);lv_obj_set_style_border_color(calculator.history,lv_color_hex(color(4)),0);calculator.conversion=button("Conversions",0,0,208,9);
    const lv_font_t* font=lv_obj_get_style_text_font(root,LV_PART_MAIN);
    for(auto* icon:{toolClose,toolKeyboard})if(icon)lv_obj_set_style_text_font(lv_obj_get_child(icon,0),&lv_font_montserrat_24,0);
    for(size_t i=0;i<Tab5CalculatorUi::KeyCount;++i){const auto& key=Tab5CalculatorUi::Keys[i];auto* b=calculator.keys[i]=button(Tab5CalculatorUi::caption(key,key.action==3?&lv_font_montserrat_24:font),0,0,80,key.action);
     if(key.action==3){lv_obj_set_style_text_font(lv_obj_get_child(b,0),&lv_font_montserrat_24,0);Tab5Focus.describe(b,tr("Effacer le caractère précédent"));}
     else if(key.action==2)Tab5Focus.describe(b,tr("Réinitialiser"));
     else if(key.action==1)Tab5Focus.describe(b,tr("Calculer · Entrée"));
    }
  }else if(which==Tab5Tool::Python){
   String title=selected?selected:"";title=title.substring(title.lastIndexOf('/')+1);label(title.c_str(),0,60,1120);button("Quitter",0,122,240,13);
   output=label("",0,204,1128);lv_obj_set_height(output,330);lv_label_set_long_mode(output,LV_LABEL_LONG_WRAP);Tab5TextInput::selectable(output);bindConsole(258,122);
  }else if(which==Tab5Tool::Executable){
   label("Application",0,58,1128);
   const char* associated=tab5ApplicationForFile(selected);const bool gameRom=associated!=nullptr;
   const char* executable=associated?associated:selected?selected:tab5ExecutableSourcePath();
   char shown[266]{};if(!tab5LaunchDisplayPath(executable,shown,sizeof(shown)))snprintf(shown,sizeof(shown),"%s",executable);
   path=field(shown,0,94,906);lv_textarea_set_max_length(path,265);button("Choisir",922,94,206,63);
   nativeMode=dropdown("Demander au lancement\nSans fichier\nToujours ce fichier",0,160,714);
   button("Enregistrer",730,160,398,68);
   input=field("",0,224,676);lv_textarea_set_max_length(input,265);lv_textarea_set_placeholder_text(input,tr("sd:/Dossier/fichier ou usb:/Dossier/fichier"));
   nativeFile=button("Fichier",692,224,210,64);nativeFolder=button("Dossier",918,224,210,65);
   loadNativeProfile();if(gameRom){lv_dropdown_set_selected(nativeMode,0);char document[266];if(tab5LaunchDisplayPath(selected,document,sizeof(document)))lv_textarea_set_text(input,document);nativeModeChanged();}
   lv_obj_add_event_cb(nativeMode,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->nativeModeChanged();},LV_EVENT_VALUE_CHANGED,this);
   nativeRun=button("Exécuter",0,288,230,60);Tab5Focus.describe(nativeRun,tr("Exécuter l’application · Ctrl+Entrée"));
   nativeStop=button("Arrêter",250,288,230,61);Tab5Focus.describe(nativeStop,tr("Demander l’arrêt · Ctrl+."));lv_obj_add_flag(nativeStop,LV_OBJ_FLAG_HIDDEN);
   button("Favori",826,288,302,66);
   nativeRecent=dropdown("Récents et favoris",0,346,884);nativeChoices();button("Reprendre",900,346,228,67);
   lv_obj_add_event_cb(nativeRecent,[](lv_event_t* e){auto* a=static_cast<Impl*>(lv_event_get_user_data(e));a->nativeRecall(lv_dropdown_get_selected(a->nativeRecent));},LV_EVENT_VALUE_CHANGED,this);
   nativeConsole=lv_obj_create(root);lv_obj_set_pos(nativeConsole,0,404);lv_obj_set_size(nativeConsole,1128,144);
   lv_obj_set_style_bg_color(nativeConsole,lv_color_hex(color(1)),0);lv_obj_set_style_border_width(nativeConsole,0,0);lv_obj_set_style_pad_all(nativeConsole,10,0);lv_obj_set_scroll_dir(nativeConsole,LV_DIR_VER);
   output=label("",0,0,1090);lv_obj_set_parent(output,nativeConsole);lv_label_set_long_mode(output,LV_LABEL_LONG_WRAP);Tab5TextInput::selectable(output);bindConsole(500,288);
   note(tab5ExecutableStatus());nativeLast=0;
  }else if(which==Tab5Tool::Print){
   printerUri=Tab5Printers::selected();scanning=Tab5Printers::busy();
   button("Chercher",0,54,190,23);printers=dropdown("Aucune imprimante détectée",206,54,924);
   lv_obj_add_event_cb(printers,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->act(24);},LV_EVENT_VALUE_CHANGED,this);
   label("Adresse IPP / IPPS / USB",0,110,920);uri=field(printerUri.c_str(),0,146,1128);
   refreshPrinters();
   lv_obj_add_event_cb(uri,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->printerSelection();},LV_EVENT_VALUE_CHANGED,this);
   label("Fichier",0,210,1000);path=field(selected?selected:"",0,246,1128);
   label("À imprimer",0,306,210);printScopeHint=label("Document entier",218,306,910);
   printSection=dropdown("Document entier",0,342,970);printPrevious=button(LV_SYMBOL_LEFT,986,342,60,27);printNext=button(LV_SYMBOL_RIGHT,1064,342,60,28);
   label("Papier",0,402,150);label("Orientation",170,402,230);label("Résolution",428,402,230);label("Taille (%)",686,402,180);
   paper=dropdown("A4\nLetter\nA5",0,438,154);landscape=dropdown("Portrait\nPaysage",170,438,240);dpi=dropdown("150 dpi\n300 dpi\n600 dpi",428,438,240);lv_dropdown_set_selected(dpi,1);scale=field("100",686,438,140);lv_textarea_set_accepted_chars(scale,"0123456789");lv_textarea_set_max_length(scale,3);
   pageLabel=label("Pages estimées : …",848,430,280);
   for(auto* control:{paper,landscape,dpi,scale,printSection})lv_obj_add_event_cb(control,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->changedPrint();},LV_EVENT_VALUE_CHANGED,this);
   lv_obj_add_event_cb(path,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->changedPrintPath();},LV_EVENT_VALUE_CHANGED,this);
   button("Vérifier",0,506,190,20);button("Imprimer",206,506,190,21);button("Annuler",412,506,190,22);tlsButton=button("Essayer IPPS",618,506,250,26);
   output=label("",0,610,1128);lv_obj_set_height(output,92);lv_label_set_long_mode(output,LV_LABEL_LONG_WRAP);lv_obj_set_pos(status,0,562);
   trustButton=button("Faire confiance",0,730,280,25);trustHint=label("Vérifier cette empreinte sur l’imprimante avant de confirmer.",296,730,816);
   lv_obj_add_flag(trustButton,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(trustHint,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(tlsButton,LV_OBJ_FLAG_HIDDEN);
   changedPrintPath();
  }else if(which==Tab5Tool::Vault){
   label("Fichier sélectionné",0,58,1100);path=field(selected?selected:"",0,98,1128);lv_obj_add_state(path,LV_STATE_DISABLED);
   lockedFile=selected&&String(selected).endsWith(".t5v");int found=lockedFile?Tab5Vault::keyForFile(selected):-2;legacyFile=lockedFile&&found==-2;
   label("Clé",0,168,540);keyList=dropdown("",0,206,770);refreshKeys();button("Gérer les clés",790,206,338,33);
   for(unsigned i=0;i<keys.size();++i)if(int(keys[i].slot)==found)lv_dropdown_set_selected(keyList,i);
   if(lockedFile&&!legacyFile){lv_obj_add_state(keyList,LV_STATE_DISABLED);if(found<0)note("Clé absente : restaurer sa sauvegarde dans Réglages");}
   if(legacyFile){label("Mot de passe du fichier ancien",0,272,1100);password=credentialField(0,310,700,256);}
   auto* run=button(lockedFile?"Déchiffrer":"Chiffrer",0,382,300,30);button("Annuler",320,382,210,32);button("Explorateur",550,382,250,34);button("Fermer le coffre",820,382,308,45);
   if(!selected||!*selected||(!legacyFile&&keys.empty())||(lockedFile&&!legacyFile&&found<0))lv_obj_add_state(run,LV_STATE_DISABLED);
   label("Copie créée — le fichier original est conservé.",0,446,1128);destination=label("",0,488,1128);
   if(!selected||!*selected)note("Choisir un fichier dans l’explorateur");
   else if(!lockedFile&&keys.empty())note("Aucune clé — créer dans Réglages");
  }else if(which==Tab5Tool::Firmware){
   Tab5Firmware::clearReview();
   label("Ce fichier remplacera le programme du Tab5 après redémarrage.",0,62,1128);
   path=field(selected?selected:"",0,118,1128);lv_obj_add_state(path,LV_STATE_DISABLED);
   label("Vos documents et vos réglages sont conservés.",0,184,1128);
   output=label("",0,244,1128);lv_obj_set_height(output,184);
   firmwareVerify=button("Vérifier",0,446,220,50);firmwareInstall=button("Installer",240,446,220,51);firmwareCancel=button("Annuler",480,446,220,52);firmwareRestart=button("Redémarrer",720,446,360,53);
   const bool firmwareAvailable=!Tab5Firmware::busy()&&!Tab5Print::busy()&&!Tab5Vault::busy()&&!Tab5Python::running()&&!Tab5MapPrint::busy()&&!tab5ExecutableBusy();
   enabled(firmwareVerify,firmwareAvailable&&selected&&*selected);
   enabled(firmwareInstall,firmwareAvailable&&selected&&*selected&&!Tab5BatteryGuard::status().countdown);
   enabled(firmwareRestart,false);enabled(firmwareCancel,false);
   lv_obj_add_flag(firmwareRestart,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(firmwareCancel,LV_OBJ_FLAG_HIDDEN);
   firmwareProgress=lv_bar_create(root);lv_obj_set_pos(firmwareProgress,0,516);lv_obj_set_size(firmwareProgress,1128,14);lv_obj_set_style_bg_color(firmwareProgress,lv_color_hex(color(1)),0);lv_obj_set_style_border_width(firmwareProgress,0,0);lv_obj_set_style_bg_color(firmwareProgress,lv_color_hex(color(4)),LV_PART_INDICATOR);
   label("Garder une alimentation stable pendant l’installation.",0,600,1128);
  }else if(which==Tab5Tool::Keys){buildKeyPage(selected);}
  else if(which==Tab5Tool::Secrets){buildSecretPage();}

  Tab5UiStyle::tree(root);captureLayout();
  if(which==Tab5Tool::AppPanel)Tab5Focus.focusObject(input);
 }
};
Tab5ToolsApp Tab5Tools;
void Tab5ToolsApp::clearSecrets(){Tab5TextInput::clearSecrets();if(impl_)impl_->wipeSecurityFields();}
void Tab5ToolsApp::begin(lv_obj_t* parent,Tab5ToolsHooks hooks){if(!impl_)impl_=new Impl;impl_->parent=parent;impl_->hooks=hooks;}
void Tab5ToolsApp::open(Tab5Tool tool,const char* path){if(!impl_)return;if(tool==Tab5Tool::Python&&impl_->hooks.editSource){impl_->hooks.editSource(path);return;}impl_->open(tool,path);}
bool Tab5ToolsApp::launchExecutable(const char* selected,bool configure){
 if(!impl_||!selected)return false;if(Tab5Firmware::busy()){impl_->note("Terminer l’installation du firmware avant de lancer une application");return false;}String stable=selected;
 if(configure||tab5ApplicationForFile(stable.c_str())){impl_->open(Tab5Tool::Executable,stable.c_str());return impl_->root!=nullptr;}
 char executable[261];if(!tab5LaunchPath(stable.c_str(),executable,sizeof(executable))){impl_->open(Tab5Tool::Executable,stable.c_str());impl_->note("Chemin invalide");return false;}
 Tab5LaunchProfile profile;tab5LaunchProfileLoad(executable,profile);
 if(profile.mode==Tab5LaunchMode::Ask){impl_->open(Tab5Tool::Executable,executable);return impl_->root!=nullptr;}
 if(profile.mode==Tab5LaunchMode::File&&!tab5FS.exists(profile.argument)){impl_->open(Tab5Tool::Executable,executable);impl_->note("Fichier inaccessible");return false;}
 if(impl_->hooks.runExecutable&&impl_->hooks.runExecutable(executable,profile.mode==Tab5LaunchMode::File?profile.argument:nullptr))return true;
 impl_->open(Tab5Tool::Executable,executable);impl_->note(tab5ExecutableStatus());return false;
}
void Tab5ToolsApp::openWith(const char* document){if(!impl_||!document)return;char canonical[261];if(!tab5LaunchPath(document,canonical,sizeof(canonical)))return;String copy=canonical;impl_->open(Tab5Tool::OpenWith,copy.c_str());}
void Tab5ToolsApp::mountStorage(lv_obj_t* parent){
 if(!impl_||!parent)return;if(!storage_)storage_=new Impl;auto* panel=storage_;
 if(panel->root&&lv_obj_is_valid(panel->root))lv_obj_delete(panel->root);
 panel->hooks=impl_->hooks;panel->parent=parent;panel->tool=Tab5Tool::Storage;panel->storageEmbedded=true;panel->status=nullptr;
 panel->root=lv_obj_create(parent);lv_obj_set_pos(panel->root,0,0);lv_obj_set_width(panel->root,lv_pct(100));lv_obj_set_height(panel->root,LV_SIZE_CONTENT);
 lv_obj_set_style_bg_opa(panel->root,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(panel->root,0,0);lv_obj_set_style_pad_all(panel->root,16,0);
 lv_obj_set_style_text_color(panel->root,lv_color_hex(panel->color(2)),0);if(panel->hooks.font)lv_obj_set_style_text_font(panel->root,panel->hooks.font(),0);
 lv_obj_add_event_cb(panel->root,[](lv_event_t* e){auto* panel=static_cast<Impl*>(lv_event_get_user_data(e));if(lv_event_get_target_obj(e)==panel->root)panel->root=nullptr;},LV_EVENT_DELETE,panel);
 panel->buildStorage();
}
void Tab5ToolsApp::openMapPrint(){if(!impl_)return;impl_->open(Tab5Tool::Print,impl_->tr("Carte affichée"));impl_->mapMode=true;impl_->sectionDirty=false;lv_obj_add_state(impl_->printPrevious,LV_STATE_DISABLED);lv_obj_add_state(impl_->printNext,LV_STATE_DISABLED);lv_obj_add_state(impl_->path,LV_STATE_DISABLED);}
void Tab5ToolsApp::close(){Tab5AppPanel::close();if(impl_)impl_->close();}
void Tab5ToolsApp::suspend(){Tab5AppPanel::close();if(impl_)impl_->close(true);}
bool Tab5ToolsApp::resumePythonApp(){
 if(!impl_||!Tab5Python::running())return false;
 if(!impl_->root||impl_->tool!=Tab5Tool::Python){String path=Tab5Python::runningPath();impl_->open(Tab5Tool::Python,path.c_str(),true);}
 if(!impl_->root||impl_->tool!=Tab5Tool::Python)return false;
 impl_->pythonWasRunning=true;
 impl_->scriptUI();Tab5PythonReader::resume();return true;
}
bool Tab5ToolsApp::pythonForeground()const{return impl_&&impl_->root&&impl_->tool==Tab5Tool::Python;}
bool Tab5ToolsApp::openPythonApp(const char* path,const char* argument){
 if((!argument||!*argument)&&path&&(!strcmp(path,"/MicroPython/themes.py")||!strcmp(path,"/MicroPython/create_theme.py")||!strcmp(path,"/MicroPython/creer_theme.py"))){open(Tab5Tool::Theme);return scope()!=nullptr;}
 if(!impl_||!path)return false;
 if(Tab5Python::running()){
  bool same=strcmp(Tab5Python::runningPath(),path)==0&&strcmp(Tab5Python::runningArgument(),argument?argument:"")==0;
  if(!resumePythonApp())return false;
  if(!same){Tab5PythonReader::suspend();impl_->askSwitch(path,argument);}
  return true;
 }
 impl_->open(Tab5Tool::Python,path,true);
 if(!impl_->root||impl_->tool!=Tab5Tool::Python)return false;
 return impl_->launchScript(path,argument);
}
void Tab5ToolsApp::resize(int height){if(impl_)impl_->resize(height);}
lv_obj_t* Tab5ToolsApp::scope()const{return impl_?impl_->root:nullptr;}
lv_obj_t* Tab5ToolsApp::modalScope()const{return Tab5UsbFormatUi::scope()?Tab5UsbFormatUi::scope():impl_?(impl_->calculatorConversion.scope()?impl_->calculatorConversion.scope():impl_->switchPrompt):nullptr;}
void Tab5ToolsApp::service(){Tab5UsbFormatUi::service();Tab5TextInput::serviceSecrets();if(!Tab5KeyStore::unlocked())Tab5TextInput::clearSecrets();if(storage_)storage_->serviceStorage();Tab5Terminal::service();Tab5Printers::service();if(Tab5AppPanel::takeOpenRequest()&&impl_&&Tab5AppPanel::active())open(Tab5Tool::AppPanel);if(impl_&&impl_->tool==Tab5Tool::AppPanel&&impl_->root&&!Tab5AppPanel::active())impl_->close();if(nativeThemeRequested.exchange(false)){Tab5Python::stop();open(Tab5Tool::Theme);}if(!scope()||millis()-impl_->last<150)return;impl_->last=millis();auto* a=impl_;
 if(a->switchConfirmed&&!Tab5Python::running()){
  String next=a->pendingPythonPath,argument=a->pendingPythonArgument;a->cancelSwitch();a->open(Tab5Tool::Python,next.c_str(),true);
  if(a->root&&a->tool==Tab5Tool::Python)a->launchScript(next.c_str(),argument.c_str());return;
 }
 if(a->pythonResume){if(Tab5Python::running())lv_obj_remove_state(a->pythonResume,LV_STATE_DISABLED);else lv_obj_add_state(a->pythonResume,LV_STATE_DISABLED);}
 if(a->consoleClear&&a->output){const char* text=lv_obj_check_type(a->output,&lv_textarea_class)?lv_textarea_get_text(a->output):lv_label_get_text(a->output);a->visible(a->consoleClear,text&&*text);}
 if(a->tool==Tab5Tool::Print)a->serviceOutline();
 if(a->tool==Tab5Tool::Print&&a->usbPrinterRevision!=Tab5UsbPrinter::revision())a->refreshPrinters();
 if(a->tool==Tab5Tool::Firmware){
  if(a->firmwareUpdates)a->note(Tab5Firmware::status());lv_bar_set_value(a->firmwareProgress,Tab5Firmware::progress(),LV_ANIM_OFF);
  auto show=[](lv_obj_t* o,bool visible){if(visible)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);};
  const bool firmwareIdle=!Tab5Firmware::busy();
  const bool otherBusy=Tab5Print::busy()||Tab5Vault::busy()||Tab5Python::running()||Tab5MapPrint::busy()||tab5ExecutableBusy();
  const bool havePath=a->path&&*lv_textarea_get_text(a->path);
  const bool powerReady=!Tab5BatteryGuard::status().countdown;
  show(a->firmwareInstall,firmwareIdle&&!Tab5Firmware::installed());
  a->enabled(a->firmwareInstall,firmwareIdle&&havePath&&!otherBusy&&powerReady);
  show(a->firmwareRestart,Tab5Firmware::installed());
  a->enabled(a->firmwareRestart,firmwareIdle&&!otherBusy&&powerReady);
  show(a->firmwareCancel,!firmwareIdle);
  a->enabled(a->firmwareCancel,!firmwareIdle);
  show(a->firmwareVerify,firmwareIdle&&!Tab5Firmware::ready()&&!Tab5Firmware::installed());
  a->enabled(a->firmwareVerify,firmwareIdle&&havePath&&!otherBusy);
  if(!Tab5Firmware::busy()){String text=Tab5Firmware::description();if(text!=a->lastOutput){a->lastOutput=text;lv_label_set_text(a->output,text.c_str());}}
 }
 if(a->tool==Tab5Tool::Theme&&a->pendingThemeRole>=0){unsigned role=a->pendingThemeRole;a->pendingThemeRole=-1;a->themeSelect(role);}
 if(a->tool==Tab5Tool::Sdr)Tab5SdrUi::service();
 if(a->tool==Tab5Tool::Terminal){const bool running=Tab5Terminal::busy();a->visible(a->nativeStop,running);uint32_t rev=Tab5Terminal::revision();
  // Updating a full console relays out all glyphs: cap running output at 2 Hz.
  // Keep the final result immediate and never replace an active text selection.
  if(rev!=a->consoleRevision&&(!running||millis()-a->terminalPaint>=500)&&!Tab5TextInput::hasSelection(a->output)){
   bool bottom=lv_obj_get_scroll_bottom(a->output)<8;const int scroll=lv_obj_get_scroll_y(a->output);
   const char* text=Tab5Terminal::snapshot(a->consoleRevision);lv_textarea_set_text(a->output,text);a->terminalPaint=millis();
   lv_obj_scroll_to_y(a->output,bottom?LV_COORD_MAX:scroll,LV_ANIM_OFF);
  }String prompt=Tab5Terminal::directory()+" $ "+a->tr(running?"Commande en cours":"Prêt");if(strcmp(lv_label_get_text(a->status),prompt.c_str()))a->note(prompt.c_str());}
 if(a->tool==Tab5Tool::Terminal||a->tool==Tab5Tool::AppPanel)a->editState();
  if(a->tool==Tab5Tool::AppPanel){
   bool busy=Tab5AppPanel::busy();a->visible(a->nativeStop,busy);a->enabled(a->nativeRun,!busy);a->enabled(a->input,!busy);a->enabled(a->path,!busy);a->enabled(a->panelMode,!busy);
   uint32_t revision=Tab5AppPanel::revision();if(revision!=a->consoleRevision&&(!busy||millis()-a->panelPaint>=500)&&!Tab5TextInput::hasSelection(a->output)){
    bool bottom=lv_obj_get_scroll_bottom(a->output)<8;const int scroll=lv_obj_get_scroll_y(a->output);
    const char* report=Tab5AppPanel::snapshot(a->consoleRevision);lv_textarea_set_text(a->output,report);a->panelPaint=millis();
    lv_obj_scroll_to_y(a->output,bottom?LV_COORD_MAX:scroll,LV_ANIM_OFF);
   }a->note(Tab5AppPanel::status());
   a->enabled(a->panelSave,!busy&&*lv_textarea_get_text(a->output));
  }
  if(a->tool==Tab5Tool::Storage)a->serviceStorage();
  if(a->tool==Tab5Tool::Executable&&millis()-a->nativeLast>=250){
  a->nativeLast=millis();const bool busy=tab5ExecutableBusy();
  if(busy)lv_obj_add_state(a->nativeRun,LV_STATE_DISABLED);else lv_obj_remove_state(a->nativeRun,LV_STATE_DISABLED);
  if(busy)lv_obj_remove_flag(a->nativeStop,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(a->nativeStop,LV_OBJ_FLAG_HIDDEN);
  a->note(tab5ExecutableStatus());const uint32_t revision=tab5ExecutableRevision();
  if(revision!=a->consoleRevision&&!Tab5TextInput::hasSelection(a->output)){a->consoleRevision=revision;const char* out=tab5ExecutableConsoleSnapshot();if(out&&strcmp(lv_label_get_text(a->output),out)){lv_label_set_text(a->output,out);lv_obj_scroll_to_y(a->nativeConsole,LV_COORD_MAX,LV_ANIM_OFF);}}
 }
 if(a->tool==Tab5Tool::Python){if(a->pythonWasRunning&&!Tab5Python::running()&&!Tab5Python::failed()){a->close(false);return;}a->scriptUI();uint32_t revision=Tab5Python::outputRevision();if(revision!=a->consoleRevision&&!Tab5TextInput::hasSelection(a->output)){String out=Tab5Python::output();a->consoleRevision=revision;const char* tail=out.c_str();size_t n=strlen(tail);unsigned lines=0;while(n){--n;if(tail[n]=='\n'&&++lines==4){tail+=n+1;break;}}lv_label_set_text(a->output,tail);}bool running=Tab5Python::running();if(running||a->pythonWasRunning)a->note(running?"Script en cours…":Tab5Python::failed()?"Script terminé avec une erreur":"Prêt");a->pythonWasRunning=running;if(a->switchPrompt)lv_obj_move_foreground(a->switchPrompt);}
 if(a->tool==Tab5Tool::Print&&a->scanning){
  a->note(Tab5Printers::status());if(!Tab5Printers::busy()){a->scanning=false;a->refreshPrinters(true);
  }
 }
 if(a->tool==Tab5Tool::Print&&a->estimateDirty&&!a->sectionDirty&&!a->sectionScanning&&millis()-a->estimateAt>=700&&!Tab5Print::busy()&&!a->mapPreparing&&!a->scanning){
  auto o=a->printOptions();a->estimateDirty=false;
  if(o.scalePercent<25||o.scalePercent>200){lv_label_set_text(a->pageLabel,a->tr("Échelle : 25 à 200 %"));}
  else if(a->mapMode){double w=o.paper==1?215.9:o.paper==2?148:210,h=o.paper==1?279.4:o.paper==2?210:297;if(o.landscape)std::swap(w,h);auto count=tab5print::imagePlacement(tab5print::page(w,h,o.dpi),1172,614,o.scalePercent).pages();String text=String(a->tr("Pages estimées : "))+String(count);lv_label_set_text(a->pageLabel,text.c_str());}
  else if(o.path.length()){if(Tab5Print::estimate(o)){a->estimateWaiting=true;a->printUpdates=false;}else {lv_label_set_text(a->pageLabel,a->tr("Estimation indisponible"));a->printUpdates=true;a->note(Tab5Print::status());}}
  else lv_label_set_text(a->pageLabel,a->tr("Choisir un fichier"));
 }
 if(a->tool==Tab5Tool::Print&&a->estimateWaiting&&!Tab5Print::busy()){
  a->estimateWaiting=false;if(!a->estimateDirty){unsigned count=Tab5Print::pages();String text=count?String(a->tr("Pages estimées : "))+String(count):String(a->tr("Estimation indisponible"));lv_label_set_text(a->pageLabel,text.c_str());if(!count){a->printUpdates=true;a->note(Tab5Print::status());}}
 }
 if(a->tool==Tab5Tool::Print&&a->mapPreparing){a->note(Tab5MapPrint::status());String out=Tab5MapPrint::detail();if(out!=a->lastOutput){a->lastOutput=out;lv_label_set_text(a->output,out.c_str());}if(!Tab5MapPrint::busy()){a->mapPreparing=false;String path=Tab5MapPrint::takePath();a->mapFallbacks=Tab5MapPrint::fallbackTileCount();if(path.length()){a->mapOptions.path=path;a->mapOptions.temporarySource=true;a->printUpdates=true;if(!Tab5Print::start(a->mapOptions)){tab5FS.remove(path);a->note(Tab5Print::status());}}}return;}
 if(a->tool==Tab5Tool::Print&&!a->scanning&&(a->printUpdates||(Tab5Print::busy()&&!Tab5Print::estimating()))){a->note(Tab5Print::status());String out=Tab5Print::detail();if(a->mapMode&&a->mapFallbacks)out+="\n"+String(a->tr("Certaines tuiles sont moins détaillées."));if(out!=a->lastOutput){a->lastOutput=out;lv_label_set_text(a->output,out.c_str());}}
 if(a->tool==Tab5Tool::Print){bool pending=a->printUpdates&&Tab5Print::certificatePending();
  if(pending){bool first=lv_obj_has_flag(a->trustButton,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(a->trustButton,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(a->trustHint,LV_OBJ_FLAG_HIDDEN);if(first){lv_obj_update_layout(a->root);lv_obj_scroll_to_view(a->trustButton,LV_ANIM_OFF);}}
  else {lv_obj_add_flag(a->trustButton,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(a->trustHint,LV_OBJ_FLAG_HIDDEN);}
  if(a->printUpdates&&strstr(Tab5Print::status(),"426"))lv_obj_remove_flag(a->tlsButton,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(a->tlsButton,LV_OBJ_FLAG_HIDDEN);
 }
 a->securityService();
}
bool Tab5ToolsApp::key(const writing::KeyEvent& e){if(Tab5UsbFormatUi::key(e))return true;if(!scope())return false;auto* a=impl_;using K=writing::Key;
 if(a->securityKeyboard(e))return true;
 if(a->tool==Tab5Tool::Sdr){if(Tab5SdrUi::key(e))return true;if(e.key==K::Escape){a->close();return true;}return false;}
 if((e.ctrl||e.meta)&&!e.shift&&!e.alt&&e.key==K::Text&&(e.text[0]=='l'||e.text[0]=='L')&&a->consoleClear){a->act(212);return true;}
 if(a->calculatorConversion.scope())return a->calculatorConversion.key(e);
 if(a->tool==Tab5Tool::Calculator&&a->calculatorHistoryView.key(e))return true;
 auto* focused=Tab5Focus.focused();
 if(a->tool==Tab5Tool::Calculator&&focused==a->input&&(e.key==K::Left||e.key==K::Right||e.key==K::Home||e.key==K::End||e.key==K::Delete))a->calculatorEntry.result=false;
 const bool back=e.key==K::Backspace&&!e.ctrl&&!e.meta&&!e.alt&&!e.shift&&!a->editing&&!Tab5Focus.controlling()&&
  !(focused&&lv_obj_is_valid(focused)&&lv_obj_check_type(focused,&lv_textarea_class));
 if(a->tool==Tab5Tool::Terminal){auto* focused=Tab5Focus.focused();bool command=e.ctrl||e.meta;
  if(command&&e.key==K::Text&&(e.text[0]=='c'||e.text[0]=='C')){if(!Tab5TextInput::copySelection(focused))Tab5Terminal::stop();return true;}
  if((e.key==K::Enter&&focused==a->input)||(command&&e.key==K::Enter)){a->act(210);return true;}
  if(command&&e.key==K::Text&&e.text[0]=='.'){a->act(211);return true;}
  if(focused==a->input&&!command&&!e.alt){if(e.key==K::Up||e.key==K::Down){String h=Tab5Terminal::history(e.key==K::Up?-1:1,lv_textarea_get_text(a->input));lv_textarea_set_text(a->input,h.c_str());lv_textarea_set_cursor_pos(a->input,LV_TEXTAREA_CURSOR_LAST);return true;}if(e.key==K::Tab&&!e.shift){String h=Tab5Terminal::complete(lv_textarea_get_text(a->input));lv_textarea_set_text(a->input,h.c_str());lv_textarea_set_cursor_pos(a->input,LV_TEXTAREA_CURSOR_LAST);return true;}}
 }
 if(a->tool==Tab5Tool::AppPanel&&(e.ctrl||e.meta)){
   if(e.key==K::Enter){a->act(350);return true;}if(e.key==K::Text&&e.text[0]=='.'){a->act(351);return true;}if(e.key==K::Text&&(e.text[0]=='s'||e.text[0]=='S')){a->act(352);return true;}if(e.key==K::Text&&(e.text[0]=='l'||e.text[0]=='L')){a->act(353);return true;}
  }
  if((e.ctrl||e.meta)&&e.key==K::Text&&strchr("aAcCxXvVzZyY",e.text[0])&&focused&&lv_obj_check_type(focused,&lv_textarea_class)){if(a->tool==Tab5Tool::Calculator&&strchr("xXvVzZyY",e.text[0]))a->calculatorEntry.result=false;return Tab5TextInput::key(focused,e);}
   if(a->tool==Tab5Tool::Calculator&&(e.ctrl||e.meta)){
    if(e.key==K::Enter){a->calculate();return true;}
    if(e.key==K::Text&&strchr("aAcCxXvVzZyY",e.text[0])){if(strchr("xXvVzZyY",e.text[0]))a->calculatorEntry.result=false;return Tab5TextInput::key(a->input,e);}
   }
  if(a->switchPrompt){
  if((e.key==K::Escape||back)&&!a->switchConfirmed)a->cancelSwitch();
  else Tab5Focus.key(e,a->switchPrompt);
  return true;
 }
 if((e.key!=K::Backspace||back)&&a->pythonShortcut(e))return true;
 if(a->tool==Tab5Tool::Calculator&&!e.ctrl&&!e.meta&&!e.alt){
   const char* text=e.key==K::Text?Tab5CalculatorUi::keyboardToken(e.text):nullptr;
   bool erase=e.key==K::Backspace&&(!focused||!lv_obj_check_type(focused,&lv_dropdown_class));
   if((e.key==K::Enter&&(!Tab5Focus.focused()||Tab5Focus.focused()==a->input))||text||erase){
   if(e.key==K::Enter||!strcmp(e.text,"=")){a->calculate();return true;}
    if(erase)a->act(3);else a->calculatorInsert(text);a->activeField=a->input;a->editing=true;Tab5Focus.editObject(a->input);return true;
  }
 }
 if((e.ctrl||e.meta)&&a->tool==Tab5Tool::Executable){
  if(e.key==K::Enter){a->act(60);return true;}
  if(e.key==K::Text&&e.text[0]=='.'){a->act(61);return true;}
 }
 if((e.ctrl||e.meta)&&a->tool==Tab5Tool::Python&&e.key==K::Text&&e.text[0]=='.'){a->act(13);return true;}
 if(a->scriptPanel&&a->editing&&e.key==K::Enter&&!e.ctrl&&!e.meta&&!e.alt){auto* field=Tab5Focus.focused();if(field&&lv_obj_is_valid(field)&&lv_obj_check_type(field,&lv_textarea_class)&&lv_obj_get_parent(field)==a->scriptPanel&&lv_obj_has_flag(field,LV_OBJ_FLAG_USER_4)){lv_obj_send_event(field,LV_EVENT_READY,nullptr);return true;}}
 if((e.key==K::Escape||back)&&a->scriptPanel&&!a->editing){a->act(13);return true;}
 if((e.key==K::Escape||back)&&!a->editing){if(a->tool==Tab5Tool::Python)a->act(13);else a->close();return true;}
 if(e.key==K::Escape){a->hideKeyboard();if(a->activeField&&lv_obj_is_valid(a->activeField))Tab5Focus.focusObject(a->activeField);return true;}
 if(a->tool==Tab5Tool::Calculator&&e.key==K::Enter&&!Tab5Focus.focused()){a->calculate();return true;}
 return false;
}

bool Tab5ToolsApp::isInput(lv_obj_t* field)const{return impl_&&impl_->root&&((impl_->tool==Tab5Tool::Sdr&&Tab5SdrUi::isInput(field))||(impl_->tool==Tab5Tool::Calculator&&(field==impl_->input||field==impl_->calculatorConversion.input())));}
void Tab5ToolsApp::insert(const char* text){if(!impl_||!text)return;auto* a=impl_;if(a->tool==Tab5Tool::Sdr){Tab5SdrUi::insert(text);return;}if(a->calculatorConversion.scope()){Tab5TextInput::insert(a->calculatorConversion.input(),text);return;}if(a->tool!=Tab5Tool::Calculator||!a->input)return;if(!strcmp(text,"=")){a->calculate();return;}a->calculatorInsert(Tab5CalculatorUi::keyboardToken(text)?Tab5CalculatorUi::keyboardToken(text):text);}
void Tab5ToolsApp::touchKey(writing::Key key){if(impl_&&impl_->tool==Tab5Tool::Sdr){Tab5SdrUi::touchKey(key);return;}if(!impl_||impl_->tool!=Tab5Tool::Calculator)return;auto* a=impl_;if(key==writing::Key::Enter){if(a->calculatorConversion.scope())a->calculatorConversion.calculate();else a->calculate();return;}a->calculatorEntry.result=false;writing::KeyEvent event;event.key=key;Tab5TextInput::key(a->calculatorConversion.scope()?a->calculatorConversion.input():a->input,event);}
