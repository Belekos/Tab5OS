#include "Tab5SdrUi.h"
#include "Tab5Sdr.h"
#include "Tab5UiStyle.h"
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5AudioDriver.h"
#include "Tab5AtomicFile.h"
#include <esp_heap_caps.h>
#include <new>
#include <cmath>
namespace Tab5SdrUi {namespace {
constexpr char SavedFile[]="/System/SDR/detections.csv";
constexpr size_t FileCapacity=Tab5Sdr::MaxDetections*48+16;
constexpr unsigned PageSize=24;
enum Action { Tune=1, Stop, Down, Up, Mode, Unit, Preset, Scan, Pause, Focus,
 SaveCurrent, SaveList, Delete, Clear, Listen, Axes, Volume, Previous, Next, Select=100 };
struct Ui {
 Tab5ToolsHooks hooks;lv_obj_t *parent=nullptr,*pane=nullptr,*tabs=nullptr,*frequency=nullptr,*unit=nullptr,*step=nullptr,*mode=nullptr,*direct=nullptr,*chart=nullptr,*status=nullptr,*active=nullptr;
 lv_obj_t *xmin=nullptr,*xmid=nullptr,*xmax=nullptr,*yrange=nullptr,*span=nullptr,*offset=nullptr,*ymin=nullptr,*ymax=nullptr,*band=nullptr,*first=nullptr,*last=nullptr,*dwell=nullptr,*threshold=nullptr,*hold=nullptr,*pause=nullptr,*detections=nullptr,*count=nullptr,*volume=nullptr,*receive=nullptr,*stopButton=nullptr,*scanButton=nullptr,*listenButton=nullptr,*deleteButton=nullptr,*previous=nullptr,*next=nullptr,*pageNumber=nullptr,*yhigh=nullptr,*ymid=nullptr,*ylow=nullptr,*fixSpectrum=nullptr,*fixScan=nullptr;
 lv_chart_series_t* series=nullptr;lv_obj_t *ppm=nullptr,*deemphasis=nullptr;
 Tab5Sdr::DetectionList found;Tab5Sdr::Frame frame;
 bool haveFrame=false,dirty=false,unitsMHz=true,updating=false,pendingSettings=false;unsigned selected=0,page=0;uint32_t lastHz=0,noticeUntil=0;
 int width=0,height=0,low=-100,high=0;unsigned spanHz=240000;int offsetHz=0;
 const char* tr(const char* s)const{return hooks.translate?hooks.translate(s):s;}
 uint32_t color(unsigned n)const{return hooks.color?hooks.color(n):n==2?0xffffff:0x202428;}
 void note(const char* text,bool transient=true){lv_label_set_text(status,tr(text));noticeUntil=transient?millis()+4000:0;}
 void style(lv_obj_t* o){Tab5UiStyle::control(o);}
 lv_obj_t* box(lv_obj_t* p,bool horizontal=false){auto* o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_set_width(o,LV_PCT(100));lv_obj_set_height(o,LV_SIZE_CONTENT);lv_obj_set_flex_flow(o,horizontal?LV_FLEX_FLOW_ROW_WRAP:LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(o,6,0);lv_obj_set_style_pad_column(o,8,0);lv_obj_set_flex_align(o,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_START);return o;}
 lv_obj_t* label(lv_obj_t* p,const char* text,int w=LV_SIZE_CONTENT){auto* o=lv_label_create(p);lv_label_set_text(o,tr(text));lv_obj_set_width(o,w);lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);return o;}
 static void event(lv_event_t* e){auto* self=static_cast<Ui*>(lv_event_get_user_data(e));auto* o=lv_event_get_target_obj(e);self->action(int(reinterpret_cast<intptr_t>(lv_obj_get_user_data(o))));}
 void bind(lv_obj_t* o,int action,lv_event_code_t code){lv_obj_set_user_data(o,reinterpret_cast<void*>(intptr_t(action)));lv_obj_add_event_cb(o,event,code,this);}
 void buttonStyle(lv_obj_t* o){
  lv_obj_set_style_bg_color(o,lv_color_hex(color(1)),0);lv_obj_set_style_text_color(o,lv_color_hex(color(2)),0);
  for(auto state:{LV_STATE_PRESSED,LV_STATE_CHECKED,static_cast<lv_state_t>(LV_STATE_CHECKED|LV_STATE_PRESSED)}){lv_obj_set_style_bg_color(o,lv_color_hex(Tab5UiStyle::color(8)),state);lv_obj_set_style_text_color(o,lv_color_hex(color(2)),state);lv_obj_set_style_border_color(o,lv_color_hex(color(4)),state);}
  lv_obj_set_style_bg_color(o,lv_color_hex(color(1)),LV_STATE_DISABLED);lv_obj_set_style_text_color(o,lv_color_hex(color(3)),LV_STATE_DISABLED);lv_obj_set_style_bg_opa(o,LV_OPA_50,LV_STATE_DISABLED);
  lv_obj_set_style_shadow_width(o,0,0);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,lv_color_hex(color(3)),0);style(o);
 }
 lv_obj_t* button(lv_obj_t* p,const char* text,int action,int w=156){lv_point_t measured{};lv_text_get_size(&measured,tr(text),hooks.font?hooks.font():LV_FONT_DEFAULT,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);w=std::max(w,std::min(320,int(measured.x)+16));auto* o=lv_button_create(p);lv_obj_set_size(o,w,44);lv_obj_set_style_pad_all(o,4,0);buttonStyle(o);auto* l=label(o,text,w-8);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);lv_obj_set_height(l,LV_SIZE_CONTENT);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(l);bind(o,action,LV_EVENT_CLICKED);Tab5Focus.describe(o,tr(text));return o;}
 lv_obj_t* field(lv_obj_t* p,const char* caption,const char* value,int w=160,bool negative=false){auto* holder=box(p);lv_obj_set_width(holder,w);label(holder,caption,w);auto* o=lv_textarea_create(holder);lv_obj_set_size(o,w,44);lv_textarea_set_one_line(o,true);lv_textarea_set_max_length(o,16);lv_textarea_set_accepted_chars(o,negative?"-0123456789.":"0123456789.");lv_textarea_set_text(o,value);style(o);
  lv_obj_add_event_cb(o,[](lv_event_t* e){auto* self=static_cast<Ui*>(lv_event_get_user_data(e));self->active=lv_event_get_target_obj(e);},LV_EVENT_FOCUSED,this);
  lv_obj_add_event_cb(o,[](lv_event_t* e){auto* self=static_cast<Ui*>(lv_event_get_user_data(e));auto* target=lv_event_get_target_obj(e);if(!self->updating&&(target==self->frequency||target==self->ppm))self->pendingSettings=true;},LV_EVENT_VALUE_CHANGED,this);
  lv_obj_add_event_cb(o,[](lv_event_t* e){auto* self=static_cast<Ui*>(lv_event_get_user_data(e));self->active=lv_event_get_target_obj(e);Tab5Focus.editObject(self->active);if(self->hooks.keyboard)self->hooks.keyboard(true,self->active);},LV_EVENT_CLICKED,this);
  lv_obj_add_event_cb(o,[](lv_event_t* e){auto* self=static_cast<Ui*>(lv_event_get_user_data(e));if(self->hooks.keyboard)self->hooks.keyboard(false,nullptr);if(lv_event_get_target_obj(e)==self->frequency)self->action(Tune);},LV_EVENT_READY,this);return o;
 }
 lv_obj_t* dropdown(lv_obj_t* p,const char* caption,const char* choices,int w,int action){auto* holder=box(p);lv_obj_set_width(holder,w);if(caption&&*caption)label(holder,caption,w);auto* o=lv_dropdown_create(holder);lv_obj_set_size(o,w,44);lv_dropdown_set_options(o,choices);if(action)bind(o,action,LV_EVENT_VALUE_CHANGED);style(o);return o;}
 bool integer(lv_obj_t* o,int& result,int minimum,int maximum){const char* text=lv_textarea_get_text(o);char* end=nullptr;long value=strtol(text,&end,10);if(!text[0]||!end||*end||value<minimum||value>maximum)return false;result=int(value);return true;}
 bool settings(Tab5Sdr::Settings& s){s.mode=lv_dropdown_get_selected(mode);s.direct=lv_dropdown_get_selected(direct);if(!Tab5Sdr::frequency(lv_textarea_get_text(frequency),unitsMHz?1000000:1000,s.hz)){note("Fréquence invalide");return false;}if(!integer(ppm,s.ppm,-200,200)){note("Réglages de réception invalides");return false;}static const unsigned times[]={50,75,0};s.deemphasis=times[lv_dropdown_get_selected(deemphasis)];return true;}
 void displayFrequency(uint32_t hz){char text[32];snprintf(text,sizeof(text),unitsMHz?"%.6f":"%.3f",double(hz)/(unitsMHz?1000000.:1000.));bool before=updating;updating=true;lv_textarea_set_text(frequency,text);updating=before;lastHz=hz;}
 void displayNumber(lv_obj_t* field,double value,unsigned decimals=3){char text[32];snprintf(text,sizeof(text),"%.*f",int(decimals),value);lv_textarea_set_text(field,text);}
 void enabled(lv_obj_t* object,bool on){if(on)lv_obj_remove_state(object,LV_STATE_DISABLED);else lv_obj_add_state(object,LV_STATE_DISABLED);}
 void select(unsigned index){
  if(index>=found.count)return;selected=index;
  for(unsigned row=0;row<lv_obj_get_child_count(detections);++row){auto* item=lv_obj_get_child(detections,row);if(page*PageSize+row==selected){lv_obj_add_state(item,LV_STATE_CHECKED);lv_obj_scroll_to_view(item,LV_ANIM_OFF);}else lv_obj_remove_state(item,LV_STATE_CHECKED);}
 }
 void showList(){
  selected=found.count?std::min(selected,found.count-1):0;page=std::min(page,found.count?(found.count-1)/PageSize:0u);
  // A bounded page creates at most 24 rows, independent of the PSRAM list size.
  int scroll=lv_obj_get_scroll_y(detections);lv_obj_clean(detections);unsigned begin=page*PageSize,end=std::min(begin+PageSize,found.count);
  if(found.count&&(selected<begin||selected>=end))selected=begin;
  if(!found.count)label(detections,"Aucune détection",LV_PCT(100));
  for(unsigned i=begin;i<end;++i){const auto& d=found.values[i];char line[96];
   if(d.hz<1000000)snprintf(line,sizeof(line),"%u.   %.3f kHz     %s     %d dBFS",i+1,double(d.hz)/1000.,d.mode==1?"WFM":d.mode==2?"AM":d.mode==3?"NFM":"FFT",d.db);
   else snprintf(line,sizeof(line),"%u.   %.6f MHz     %s     %d dBFS",i+1,double(d.hz)/1000000.,d.mode==1?"WFM":d.mode==2?"AM":d.mode==3?"NFM":"FFT",d.db);
   auto* row=button(detections,line,Select+int(i),0);lv_obj_set_width(row,LV_PCT(100));lv_obj_add_event_cb(row,event,LV_EVENT_FOCUSED,this);auto* text=lv_obj_get_child(row,0);lv_obj_set_width(text,LV_PCT(100));lv_obj_set_style_text_align(text,LV_TEXT_ALIGN_LEFT,0);
  }
  if(found.count)select(selected);lv_obj_scroll_to_y(detections,scroll,LV_ANIM_OFF);
  char number[64];snprintf(number,sizeof(number),"%u / %u%s",found.count,Tab5Sdr::MaxDetections,dirty?" *":"");String info=String(tr("Détections"))+" : "+number;lv_label_set_text(count,info.c_str());
  snprintf(number,sizeof(number),"%u-%u",found.count?begin+1:0,end);lv_label_set_text(pageNumber,number);
  enabled(previous,page>0);enabled(next,end<found.count);enabled(listenButton,found.count>0);enabled(deleteButton,found.count>0);
 }
 void controls(){
  bool running=Tab5Sdr::busy(),stopping=Tab5Sdr::stopping();
  const char* caption=tr(running?"Appliquer":"Recevoir");auto* text=lv_obj_get_child(receive,0);if(strcmp(lv_label_get_text(text),caption))lv_label_set_text(text,caption);
  enabled(receive,!stopping&&(!running||pendingSettings));enabled(stopButton,running&&!stopping);
  enabled(fixSpectrum,running&&!stopping&&lv_dropdown_get_selected(mode)!=Tab5Sdr::Spectrum);enabled(fixScan,running&&!stopping&&lv_dropdown_get_selected(mode)!=Tab5Sdr::Spectrum);enabled(listenButton,found.count&&!stopping);
  enabled(scanButton,!stopping&&!Tab5Sdr::scanning());enabled(pause,!stopping&&Tab5Sdr::scanning());
  caption=tr(Tab5Sdr::scanPaused()?"Reprendre":"Pause");text=lv_obj_get_child(pause,0);if(strcmp(lv_label_get_text(text),caption))lv_label_set_text(text,caption);
 }
 void load(){
  Tab5StorageUse lease(true);if(!lease)return;if(!tab5AtomicRecover(SavedFile))return;File f=tab5FS.open(SavedFile);if(!f)return;
  size_t size=f.size();if(!size||size>=FileCapacity){note("Fichier de détections invalide");return;}
  char* data=static_cast<char*>(heap_caps_malloc(size+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));auto* list=static_cast<Tab5Sdr::DetectionList*>(heap_caps_malloc(sizeof(Tab5Sdr::DetectionList),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!data||!list){heap_caps_free(data);heap_caps_free(list);note("Mémoire insuffisante");return;}new(list)Tab5Sdr::DetectionList;
  bool ok=f.read(reinterpret_cast<uint8_t*>(data),size)==size;if(ok){data[size]=0;ok=list->parse(data,size);}f.close();if(ok)found=*list;else note("Fichier de détections invalide");list->~DetectionList();heap_caps_free(list);heap_caps_free(data);
 }
 void save(){
  Tab5StorageUse lease(true);if(!lease){note("Stockage indisponible");return;}
  if((!tab5FS.exists("/System")&&!tab5FS.mkdir("/System"))||(!tab5FS.exists("/System/SDR")&&!tab5FS.mkdir("/System/SDR"))){note("Enregistrement impossible");return;}
  // Each canonical record is at most 20 bytes; size scratch to this list,
  // retaining the larger import bound for existing externally edited files.
  size_t capacity=16+size_t(found.count)*24;char* data=static_cast<char*>(heap_caps_malloc(capacity,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!data){note("Mémoire insuffisante");return;}
  size_t used=found.serialize(data,capacity);bool ok=used&&tab5AtomicWrite(SavedFile,data,used);heap_caps_free(data);
  if(ok){dirty=false;showList();note("Détections enregistrées dans /System/SDR/detections.csv");}else note("Enregistrement impossible");
 }
 void paint(){
  if(!haveFrame)return;const int half=int(frame.sampleRate/2),limit=std::max(0,half-int(spanHz/2));offsetHz=std::clamp(offsetHz,-limit,limit);
  auto* points=lv_chart_get_y_array(chart,series);
  for(unsigned i=0;i<Tab5Sdr::BinCount;++i){int offset=offsetHz-int(spanHz/2)+int(uint64_t(i)*spanHz/Tab5Sdr::BinCount);unsigned bin=unsigned(std::clamp(int64_t(offset+half)*Tab5Sdr::BinCount/frame.sampleRate,int64_t(0),int64_t(Tab5Sdr::BinCount-1)));points[i]=frame.levels[bin];}
  lv_chart_set_range(chart,LV_CHART_AXIS_PRIMARY_Y,low,high);lv_chart_refresh(chart);
  char text[40];double centre=double(frame.hz)+offsetHz;
  snprintf(text,sizeof(text),"%.6f MHz",(centre-spanHz/2.)/1000000.);lv_label_set_text(xmin,text);
  snprintf(text,sizeof(text),"%.6f MHz",centre/1000000.);lv_label_set_text(xmid,text);
  snprintf(text,sizeof(text),"%.6f MHz",(centre+spanHz/2.)/1000000.);lv_label_set_text(xmax,text);
  snprintf(text,sizeof(text),"%d",high);lv_label_set_text(yhigh,text);snprintf(text,sizeof(text),"%d",(high+low)/2);lv_label_set_text(ymid,text);snprintf(text,sizeof(text),"%d",low);lv_label_set_text(ylow,text);
 }
 void action(int action){
  if(updating)return;
  if(action>=Select){select(unsigned(action-Select));return;}
  if(action==Previous||action==Next){if(action==Previous&&page)--page;else if(action==Next&&(page+1)*PageSize<found.count)++page;showList();lv_obj_scroll_to_y(detections,0,LV_ANIM_OFF);return;}
  if(action==Tune||action==Scan||action==Listen||action==Focus){if(hooks.keyboard)hooks.keyboard(false,nullptr);Tab5Focus.finishEditing();active=nullptr;}
  if(action==Stop){Tab5Sdr::stop();controls();return;}
  if(action==Unit){Tab5Sdr::Settings s;if(!settings(s)){updating=true;lv_dropdown_set_selected(unit,unitsMHz?1:0);updating=false;return;}unitsMHz=lv_dropdown_get_selected(unit)==1;displayFrequency(s.hz);return;}
  if(action==Tune||action==Mode){Tab5Sdr::Settings s;if(settings(s)&&(action==Tune||Tab5Sdr::busy())){if(!Tab5Sdr::configure(s))note(Tab5Sdr::status());else {noticeUntil=0;pendingSettings=false;controls();}}return;}
  if(action==Down||action==Up){Tab5Sdr::Settings s;uint32_t delta;if(!settings(s))return;if(!Tab5Sdr::frequency(lv_textarea_get_text(step),1000,delta)){note("Pas de fréquence invalide");return;}int64_t target=int64_t(s.hz)+(action==Up?int64_t(delta):-int64_t(delta));if(target<1||target>UINT32_MAX){note("Fréquence invalide");return;}s.hz=uint32_t(target);displayFrequency(s.hz);pendingSettings=true;if(Tab5Sdr::busy()){if(!Tab5Sdr::configure(s))note(Tab5Sdr::status());else pendingSettings=false;}return;}
  if(action==Preset){unsigned i=lv_dropdown_get_selected(band);if(!i)return;const auto& p=Tab5Sdr::Presets[i];displayNumber(first,p.first/1000.);displayNumber(last,p.last/1000.);displayNumber(step,p.step/1000.);displayFrequency(p.first);lv_dropdown_set_selected(mode,p.mode);if(Tab5Sdr::busy())this->action(Tune);return;}
  if(action==Scan){Tab5Sdr::Settings s;Tab5Sdr::Scan sweep;int duration,level;
   if(!settings(s))return;if(!Tab5Sdr::frequency(lv_textarea_get_text(first),1000,sweep.first)||!Tab5Sdr::frequency(lv_textarea_get_text(last),1000,sweep.last)||!Tab5Sdr::frequency(lv_textarea_get_text(step),1000,sweep.step)||!integer(dwell,duration,150,10000)||!integer(threshold,level,-120,0)){note("Balayage invalide ou trop étendu");return;}
   sweep.dwell=duration;sweep.threshold=level;sweep.hold=lv_obj_has_state(hold,LV_STATE_CHECKED);if(!Tab5Sdr::scan(sweep,s))note(Tab5Sdr::status());else {noticeUntil=0;pendingSettings=false;controls();}return;
  }
  if(action==Pause){Tab5Sdr::pauseScan(!Tab5Sdr::scanPaused());controls();return;}
  if(action==Focus){Tab5Sdr::Settings s=Tab5Sdr::current();s.mode=lv_dropdown_get_selected(mode);if(s.mode==Tab5Sdr::Spectrum){note("Choisissez AM, WFM ou NFM pour écouter");return;}displayFrequency(s.hz);if(!Tab5Sdr::configure(s))note(Tab5Sdr::status());else {pendingSettings=false;controls();}return;}
  if(action==Axes){int bottom,top,pan;static const unsigned spans[]={240000,120000,60000,30000};if(!integer(ymin,bottom,-120,-1)||!integer(ymax,top,-119,0)||bottom>=top||!integer(offset,pan,-120,120)){note("Axes invalides : minimum inférieur au maximum");return;}low=bottom;high=top;spanHz=spans[lv_dropdown_get_selected(span)];offsetHz=pan*1000;paint();displayNumber(offset,offsetHz/1000.,0);return;}
  if(action==SaveCurrent){Tab5Sdr::Settings s=Tab5Sdr::current();if(!Tab5Sdr::busy()){if(!settings(s))return;}int db=-120;if(haveFrame)for(auto level:frame.levels)db=std::max(db,int(level));if(found.add({s.hz,uint8_t(s.mode),uint8_t(s.direct),int16_t(db)})){dirty=true;showList();}else note("Liste pleine : supprimez des détections");return;}
  if(action==SaveList){save();return;}
  if(action==Delete){if(found.count){found.erase(selected);dirty=true;showList();}return;}
  if(action==Clear){found.count=0;found.full=false;dirty=true;showList();return;}
  if(action==Listen){unsigned i=selected;if(i>=found.count)return;const auto& d=found.values[i];displayFrequency(d.hz);lv_dropdown_set_selected(mode,d.mode);lv_dropdown_set_selected(direct,d.direct);Tab5Sdr::Settings s;if(settings(s)){if(!Tab5Sdr::configure(s))note(Tab5Sdr::status());else {pendingSettings=false;controls();}}return;}
  if(action==Volume){Tab5Sdr::volume(lv_slider_get_value(volume));return;}
 }
 void build(){
  pane=box(parent);lv_obj_set_pos(pane,0,56);lv_obj_set_height(pane,600);lv_obj_set_style_text_color(pane,lv_color_hex(color(2)),0);if(hooks.font)lv_obj_set_style_text_font(pane,hooks.font(),0);lv_obj_remove_flag(pane,LV_OBJ_FLAG_SCROLLABLE);
  auto* tuning=box(pane,true);lv_obj_set_flex_align(tuning,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_START);
  frequency=field(tuning,"Fréquence","100.000000",204);unit=dropdown(tuning,"Unité","kHz\nMHz",96,Unit);lv_dropdown_set_selected(unit,1);step=field(tuning,"Pas (kHz)","100",124);
  auto* arrows=box(tuning,true);lv_obj_set_flex_flow(arrows,LV_FLEX_FLOW_ROW);lv_obj_set_size(arrows,104,44);button(arrows,"-",Down,48);button(arrows,"+",Up,48);
  mode=dropdown(tuning,"Mode",String(String(tr("Spectre"))+"\nWFM\nAM\nNFM").c_str(),136,Mode);lv_dropdown_set_selected(mode,1);
  receive=button(tuning,"Recevoir",Tune,134);stopButton=button(tuning,"Arrêter",Stop,118);
  tabs=lv_tabview_create(pane);lv_obj_set_width(tabs,LV_PCT(100));lv_obj_set_height(tabs,0);lv_obj_set_flex_grow(tabs,1);lv_tabview_set_tab_bar_size(tabs,44);lv_obj_set_style_bg_color(tabs,lv_color_hex(color(0)),0);lv_obj_set_style_text_color(tabs,lv_color_hex(color(2)),0);lv_obj_set_style_radius(tabs,0,0);if(hooks.font){lv_obj_set_style_text_font(tabs,hooks.font(),0);lv_obj_set_style_text_font(lv_tabview_get_tab_bar(tabs),hooks.font(),0);}
  auto* spectrum=lv_tabview_add_tab(tabs,tr("Spectre"));auto* scanning=lv_tabview_add_tab(tabs,tr("Balayage"));auto* memories=lv_tabview_add_tab(tabs,tr("Détections"));auto* settingsPage=lv_tabview_add_tab(tabs,tr("Réglages"));
  auto* tabBar=lv_tabview_get_tab_bar(tabs);lv_obj_set_style_bg_color(tabBar,lv_color_hex(color(0)),0);for(unsigned i=0;i<lv_obj_get_child_count(tabBar);++i)buttonStyle(lv_obj_get_child(tabBar,i));
  for(auto* page:{spectrum,scanning,memories,settingsPage}){lv_obj_set_style_pad_all(page,6,0);lv_obj_set_style_pad_row(page,8,0);lv_obj_set_flex_flow(page,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_bg_color(page,lv_color_hex(color(0)),0);lv_obj_set_style_text_color(page,lv_color_hex(color(2)),0);if(hooks.font)lv_obj_set_style_text_font(page,hooks.font(),0);}
  auto* plot=box(spectrum,true);lv_obj_set_flex_grow(plot,1);lv_obj_set_style_min_height(plot,100,0);lv_obj_set_height(plot,0);lv_obj_set_flex_flow(plot,LV_FLEX_FLOW_ROW);
  yrange=box(plot);lv_obj_set_size(yrange,64,LV_PCT(100));lv_obj_set_flex_align(yrange,LV_FLEX_ALIGN_SPACE_BETWEEN,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);yhigh=label(yrange,"0");ymid=label(yrange,"-50");ylow=label(yrange,"-100");chart=lv_chart_create(plot);lv_obj_set_flex_grow(chart,1);lv_obj_set_height(chart,LV_PCT(100));lv_obj_set_style_min_height(chart,100,0);
  lv_obj_set_style_radius(chart,0,LV_PART_MAIN);lv_obj_set_style_pad_all(chart,0,0);lv_obj_set_style_bg_color(chart,lv_color_hex(color(1)),0);lv_obj_set_style_border_width(chart,0,0);lv_obj_set_style_line_color(chart,lv_color_hex(color(3)),LV_PART_MAIN);lv_chart_set_div_line_count(chart,5,9);
  lv_chart_set_type(chart,LV_CHART_TYPE_LINE);lv_chart_set_point_count(chart,Tab5Sdr::BinCount);lv_chart_set_range(chart,LV_CHART_AXIS_PRIMARY_Y,low,high);lv_obj_set_style_size(chart,0,0,LV_PART_INDICATOR);series=lv_chart_add_series(chart,lv_color_hex(color(4)),LV_CHART_AXIS_PRIMARY_Y);
  auto* xaxis=box(spectrum,true);label(xaxis,"dBFS",64);xmin=label(xaxis,"99.880000 MHz");xmid=label(xaxis,"100.000000 MHz");xmax=label(xaxis,"100.120000 MHz");for(auto* l:{xmin,xmid,xmax}){lv_obj_set_flex_grow(l,1);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);}
  auto* listen=box(spectrum,true);label(listen,"Volume");volume=lv_slider_create(listen);lv_obj_set_size(volume,220,18);lv_obj_set_style_margin_left(volume,12,0);lv_slider_set_range(volume,0,100);lv_slider_set_value(volume,unsigned(Tab5AudioDevice.volume())*100/255,LV_ANIM_OFF);bind(volume,Volume,LV_EVENT_VALUE_CHANGED);style(volume);button(listen,"Mémoriser",SaveCurrent,150);fixSpectrum=button(listen,"Fixer",Focus,110);
  auto* reception=box(settingsPage,true);ppm=field(reception,"Correction (ppm)","0",156,true);deemphasis=dropdown(reception,"Désaccentuation FM",String(String("50 µs\n75 µs\n")+tr("Désactivé")).c_str(),232,Mode);direct=dropdown(reception,"Entrée",String(String(tr("Tuner"))+"\n"+tr("Direct I")+"\n"+tr("Direct Q")).c_str(),164,Mode);
  auto* axes=box(settingsPage,true);span=dropdown(axes,"Largeur X","240 kHz\n120 kHz\n60 kHz\n30 kHz",140,Axes);offset=field(axes,"Décalage X (kHz)","0",156,true);ymin=field(axes,"Y min (dBFS)","-100",130,true);ymax=field(axes,"Y max (dBFS)","0",130,true);button(axes,"Ajuster",Axes,130);
  auto* presets=box(scanning,true);String presetsText;for(const auto& p:Tab5Sdr::Presets){if(presetsText.length())presetsText+='\n';presetsText+=tr(p.name);}band=dropdown(presets,"Plage",presetsText.c_str(),280,Preset);
  auto* limits=box(scanning,true);first=field(limits,"Début (kHz)","87500",184);last=field(limits,"Fin (kHz)","108000",184);dwell=field(limits,"Durée (ms)","400",132);threshold=field(limits,"Seuil (dBFS)","-45",142,true);
  hold=lv_checkbox_create(scanning);lv_checkbox_set_text(hold,tr("Pause sur signal"));style(hold);
  auto* scanActions=box(scanning,true);scanButton=button(scanActions,"Balayer",Scan,140);pause=button(scanActions,"Pause",Pause,144);fixScan=button(scanActions,"Fixer",Focus,110);
  auto* summary=box(memories,true);count=label(summary,"Détections");lv_obj_set_flex_grow(count,1);previous=button(summary,LV_SYMBOL_LEFT,Previous,48);pageNumber=label(summary,"0-0",176);lv_obj_set_style_text_align(pageNumber,LV_TEXT_ALIGN_CENTER,0);next=button(summary,LV_SYMBOL_RIGHT,Next,48);Tab5Focus.describe(previous,tr("Page précédente"));Tab5Focus.describe(next,tr("Page suivante"));
  detections=box(memories);lv_obj_set_height(detections,0);lv_obj_set_flex_grow(detections,1);lv_obj_set_style_min_height(detections,64,0);lv_obj_set_scroll_dir(detections,LV_DIR_VER);lv_obj_set_style_pad_right(detections,8,0);lv_obj_set_scrollbar_mode(detections,LV_SCROLLBAR_MODE_AUTO);
  auto* memoryActions=box(memories,true);listenButton=button(memoryActions,"Écouter",Listen,140);button(memoryActions,"Mémoriser",SaveCurrent,150);deleteButton=button(memoryActions,"Supprimer",Delete,150);button(memoryActions,"Vider",Clear,110);button(memoryActions,"Enregistrer",SaveList,170);
  for(auto* value:{frequency,step,first,last})lv_textarea_set_max_length(value,12);
  for(auto* value:{ppm,offset,ymin,ymax,threshold}){lv_textarea_set_accepted_chars(value,"-0123456789");lv_textarea_set_max_length(value,4);}
  lv_textarea_set_accepted_chars(dwell,"0123456789");lv_textarea_set_max_length(dwell,5);
  status=label(pane,"Prêt",LV_PCT(100));lv_label_set_long_mode(status,LV_LABEL_LONG_DOT);lv_obj_set_height(status,LV_SIZE_CONTENT);load();showList();controls();Tab5UiStyle::tree(pane);lv_obj_set_style_radius(chart,0,0);
 }
 void service(){
  controls();
  lv_obj_update_layout(parent);
  int w=lv_obj_get_content_width(parent),h=std::max<int32_t>(160,lv_obj_get_content_height(parent)-56);if(w!=width||h!=height){width=w;height=h;lv_obj_set_size(pane,w,h);}
  if(Tab5Sdr::spectrum(frame)){haveFrame=true;paint();if(Tab5Sdr::scanning()&&(!active||active!=frequency))displayFrequency(frame.hz);}
  Tab5Sdr::Detection d;bool changed=false;while(Tab5Sdr::takeDetection(d)){if(found.add(d)){dirty=true;changed=true;}else {Tab5Sdr::pauseScan(true);note("Liste pleine : supprimez des détections");}}
  if(changed)showList();if(Tab5Sdr::detectionOverflow())note("Détections en attente : balayage en pause");
  if(!noticeUntil||int32_t(millis()-noticeUntil)>=0){const char* text=tr(Tab5Sdr::status());if(strcmp(lv_label_get_text(status),text))lv_label_set_text(status,text);noticeUntil=0;}
 }
};
Ui* ui=nullptr;
}
bool open(lv_obj_t* parent,const Tab5ToolsHooks& hooks){close();void* mem=heap_caps_malloc(sizeof(Ui),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!mem)return false;ui=new(mem)Ui;ui->parent=parent;ui->hooks=hooks;ui->build();ui->service();return true;}
void close(){Tab5Sdr::stop();if(!ui)return;if(ui->hooks.keyboard)ui->hooks.keyboard(false,nullptr);if(ui->pane&&lv_obj_is_valid(ui->pane))lv_obj_delete(ui->pane);ui->~Ui();heap_caps_free(ui);ui=nullptr;}
void service(){if(ui)ui->service();}
lv_obj_t* input(){return ui?(ui->active?ui->active:ui->frequency):nullptr;}
bool isInput(lv_obj_t* object){return ui&&object&&lv_obj_check_type(object,&lv_textarea_class)&&Tab5Focus.contains(ui->pane,object);}
void insert(const char* text){if(ui){if(!ui->active&&isInput(Tab5Focus.focused()))ui->active=Tab5Focus.focused();if(ui->active&&isInput(ui->active))Tab5TextInput::insert(ui->active,text);}}
void touchKey(writing::Key key){
 // The on-screen keyboard may own UI focus; it must not turn cursor arrows
 // into navigation, nor switch the field being edited to another textarea.
 if(!ui||!isInput(ui->active))return;writing::KeyEvent event;event.key=key;
 if(key==writing::Key::Enter){if(ui->active==ui->frequency)ui->action(Tune);if(ui->hooks.keyboard)ui->hooks.keyboard(false,nullptr);Tab5Focus.finishEditing();ui->active=nullptr;return;}
 Tab5Focus.edit(ui->active,event);
}
bool key(const writing::KeyEvent& event){
 if(!ui)return false;auto* focused=Tab5Focus.focused();if(isInput(focused))ui->active=focused;
 if(event.key==writing::Key::Escape){if(ui->active){if(ui->hooks.keyboard)ui->hooks.keyboard(false,nullptr);ui->active=nullptr;Tab5Focus.finishEditing();return true;}return false;}
 if(ui->active&&isInput(ui->active)&&Tab5Focus.controlling()){
  if(event.key==writing::Key::Enter){if(ui->active==ui->frequency)ui->action(Tune);if(ui->hooks.keyboard)ui->hooks.keyboard(false,nullptr);Tab5Focus.finishEditing();ui->active=nullptr;return true;}
  return Tab5Focus.edit(ui->active,event);
 }
 return Tab5Focus.key(event,ui->pane);
}
}
