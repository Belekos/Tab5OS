#include "Tab5Alarm.h"
#include "Tab5UiStyle.h"
#include "Tab5AlarmPolicy.h"
#include "Tab5AlarmTone.h"
#include "Tab5Audio.h"
#include "Tab5AudioDriver.h"
#include "Tab5Task.h"
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5Storage.h"
#include <Preferences.h>
#include <M5Unified.h>
#include <esp_sleep.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <sys/time.h>
namespace Tab5Alarm {
namespace {
Hooks hooks;Tab5AlarmPolicy::State state;
lv_obj_t *overlay=nullptr,*panel=nullptr,*status=nullptr,*clockLabel=nullptr,*hours=nullptr,*minutes=nullptr,*repeat=nullptr,*enabled=nullptr,*volume=nullptr,*volumeText=nullptr,*tone=nullptr,*previewButton=nullptr;
std::atomic<bool> soundWanted{false},soundRunning{false},soundFailed{false};
std::atomic<uint8_t> soundVolume{200},soundTone{0};
const char* notice=nullptr;bool alert=false,preview=false,ringPending=false,missed=false,persistenceOk=true;uint32_t ringAt=0,lastPoll=0;
const char* tr(const char* s){return hooks.translate?hooks.translate(s):s;}
uint32_t color(unsigned n){return hooks.color?hooks.color(n):(n==2?0xffffff:0x202428);}
bool save(){Preferences p;if(!p.begin("tab5_alarm",false))return persistenceOk=false;bool ok=p.putBytes("state",&state,sizeof(state))==sizeof(state);p.end();return persistenceOk=ok;}
void note(const char* s){notice=s;if(status)lv_label_set_text(status,tr(s));}
void stopPreview(){if(preview){soundWanted=false;preview=false;}}
void close(){stopPreview();if(overlay){Tab5Focus.clear();lv_obj_delete(overlay);overlay=panel=status=clockLabel=hours=minutes=repeat=enabled=volume=volumeText=tone=previewButton=nullptr;}}
lv_obj_t* label(const char* text){auto* o=lv_label_create(panel);lv_obj_set_width(o,LV_PCT(100));lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);lv_label_set_text(o,tr(text));Tab5TextInput::selectable(o);return o;}
lv_obj_t* row(){auto* o=lv_obj_create(panel);lv_obj_remove_style_all(o);lv_obj_set_size(o,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(o,LV_FLEX_FLOW_ROW_WRAP);lv_obj_set_style_pad_column(o,16,0);lv_obj_set_style_pad_row(o,12,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;}
lv_obj_t* button(lv_obj_t* parent,const char* text,void(*action)()){
 auto* o=lv_button_create(parent);lv_obj_set_size(o,LV_PCT(47),64);lv_obj_set_style_bg_color(o,lv_color_hex(color(1)),0);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,lv_color_hex(color(4)),0);lv_obj_set_style_text_color(o,lv_color_hex(color(2)),0);
 Tab5UiStyle::selection(o);
 auto* t=lv_label_create(o);lv_obj_set_width(t,LV_PCT(100));lv_label_set_long_mode(t,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);lv_label_set_text(t,tr(text));lv_obj_center(t);
 lv_obj_add_event_cb(o,[](lv_event_t* e){auto f=reinterpret_cast<void(*)()>(lv_event_get_user_data(e));f();},LV_EVENT_CLICKED,reinterpret_cast<void*>(action));return o;
}
void makePanel(){
 close();notice=nullptr;overlay=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(overlay);lv_obj_set_size(overlay,LV_PCT(100),LV_PCT(100));lv_obj_set_style_bg_color(overlay,lv_color_black(),0);lv_obj_set_style_bg_opa(overlay,LV_OPA_70,0);lv_obj_remove_flag(overlay,LV_OBJ_FLAG_SCROLLABLE);
 panel=lv_obj_create(overlay);lv_obj_set_size(panel,std::min<int>(1000,lv_display_get_horizontal_resolution(nullptr)-32),std::min<int>(670,lv_display_get_vertical_resolution(nullptr)-32));lv_obj_center(panel);lv_obj_set_style_bg_color(panel,lv_color_hex(color(0)),0);lv_obj_set_style_text_color(panel,lv_color_hex(color(2)),0);if(hooks.font)lv_obj_set_style_text_font(panel,hooks.font(),0);lv_obj_set_flex_flow(panel,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(panel,20,0);lv_obj_set_style_pad_row(panel,16,0);lv_obj_set_scroll_dir(panel,LV_DIR_VER);lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLL_ELASTIC);
}
void soundTask(void*){
 // Borrow playback cooperatively; it resumes when this lease is released.
 uint32_t token=0;while(soundWanted.load()&&!token){token=Tab5Audio.acquireMemoryPriority(1000);if(!token){soundFailed=true;vTaskDelay(pdMS_TO_TICKS(50));}}
 bool claimed=false;const uint8_t oldVolume=Tab5AudioDevice.volume();
 while(soundWanted.load()&&token&&!claimed){claimed=Tab5AudioDevice.claim(Tab5AudioClient::Alarm,250);if(!claimed){soundFailed=true;vTaskDelay(pdMS_TO_TICKS(20));}}
 if(claimed){
  Tab5PcmSpec spec;spec.sampleRate=24000;spec.channels=1;spec.format=Tab5PcmFormat::S16;
  soundFailed=false;bool ok=Tab5AudioDevice.configure(Tab5AudioClient::Alarm,spec);
  int16_t pcm[256];uint32_t sample=0;
  while(ok&&soundWanted.load()){
   Tab5AudioDevice.setVolume(soundVolume.load());
   Tab5AlarmTone::render(soundTone.load(),sample,pcm,256);
   ok=Tab5AudioDevice.writePcm(Tab5AudioClient::Alarm,pcm,256,spec);vTaskDelay(1);
  }
  if(!ok)soundFailed=true;
  Tab5AudioDevice.stop(Tab5AudioClient::Alarm);Tab5AudioDevice.setVolume(oldVolume);Tab5AudioDevice.release(Tab5AudioClient::Alarm);
 }
 if(token)Tab5Audio.releaseMemoryPriority(token);
 tab5TaskCheckpoint("alarm");soundRunning=false;vTaskDelete(nullptr);
}
void dismiss(){soundWanted=false;alert=false;close();}
void snooze(){auto before=state;state.snooze=time(nullptr)+300;if(!save()){state=before;note("Enregistrement du réveil impossible");return;}dismiss();}
bool startSound(uint8_t selectedTone,uint8_t selectedVolume){
 soundTone=Tab5AlarmTone::normalize(selectedTone);soundVolume=selectedVolume;soundWanted=true;soundRunning=true;soundFailed=false;
 if(tab5CreateTask(soundTask,"tab5_alarm",6144,nullptr,3,nullptr,0)==pdPASS)return true;
 soundWanted=false;soundRunning=false;soundFailed=true;return false;
}
void previewLabel(){if(previewButton)lv_label_set_text(lv_obj_get_child(previewButton,0),tr(preview?"Arrêter":"Essayer la sonnerie"));}
void togglePreview(){
 if(preview){stopPreview();previewLabel();return;}
 if(soundRunning.load()){note("Attendez la fin de la sonnerie");return;}
 notice=nullptr;ringAt=millis();preview=true;
 if(hooks.wake)hooks.wake();if(hooks.quiet)hooks.quiet();
 if(!startSound(uint8_t(lv_dropdown_get_selected(tone)),uint8_t(lv_slider_get_value(volume)))){preview=false;note("Sonnerie indisponible : vérifiez le son");}
 previewLabel();
}
void start(){
 if(alert)return;
 if(soundRunning.load()){ringPending=true;stopPreview();return;}
 ringPending=false;alert=true;missed=false;ringAt=millis();
 if(hooks.wake)hooks.wake();if(hooks.quiet)hooks.quiet();
 startSound(state.reserved[0],state.volume);
 makePanel();label("Réveil");clockLabel=label("");status=label("La sonnerie s’arrête après cinq minutes.");auto* actions=row();button(actions,"Arrêter",dismiss);button(actions,"Reporter de 5 minutes",snooze);Tab5UiStyle::tree(panel);Tab5Focus.focusFirst(panel);
}
void update(){
 if(!clockLabel)return;time_t now=time(nullptr);tm local{};localtime_r(&now,&local);char text[160];strftime(text,sizeof(text),"%Y-%m-%d  %H:%M",&local);if(strcmp(lv_label_get_text(clockLabel),text))lv_label_set_text(clockLabel,text);
 if(alert){if(soundFailed.load()||(millis()-ringAt>15000&&!soundRunning.load()))note("Sonnerie indisponible : vérifiez le son");return;}
 if(preview&&soundFailed.load()){note("Sonnerie indisponible : vérifiez le son");return;}
 if(!persistenceOk){note("Enregistrement du réveil impossible");return;}
 if(notice)return;
 time_t due=Tab5AlarmPolicy::deadline(state);
 if(due){tm next{};localtime_r(&due,&next);char stamp[64];strftime(stamp,sizeof(stamp),"%Y-%m-%d  %H:%M",&next);snprintf(text,sizeof(text),"%s %s",tr("Prochain réveil :"),stamp);if(strcmp(lv_label_get_text(status),text))lv_label_set_text(status,text);}
 else if(status)lv_label_set_text(status,tr(missed?"Réveil manqué":"Réveil désactivé"));
}
void commit(){
 time_t now=time(nullptr);if(!Tab5AlarmPolicy::validTime(now)){note("Réglez la date et l’heure avant d’activer le réveil");return;}
 auto before=state;state.hour=lv_dropdown_get_selected(hours);state.minute=lv_dropdown_get_selected(minutes);state.daily=lv_dropdown_get_selected(repeat);state.enabled=lv_obj_has_state(enabled,LV_STATE_CHECKED);state.volume=lv_slider_get_value(volume);state.reserved[0]=Tab5AlarmTone::normalize(uint8_t(lv_dropdown_get_selected(tone)));Tab5AlarmPolicy::schedule(state,now);
 if(!save()){state=before;note("Enregistrement du réveil impossible");return;}
 missed=false;notice=nullptr;update();
}
void sleepUntilAlarm(){
 if(soundRunning.load()){note("Attendez la fin de la sonnerie");return;}
 time_t now=time(nullptr),due=Tab5AlarmPolicy::deadline(state);
 if(!due){note("Activez et enregistrez d’abord un réveil");return;}
 if(due-now<180){note("Réveil proche : la tablette reste allumée");return;}
 if(!M5.Rtc.isEnabled()){note("Horloge de réveil indisponible");return;}
 if(hooks.prepareSleep){const char* problem=hooks.prepareSleep();if(problem){note(problem);return;}}
 Tab5StorageRecovery exclusive;if(!exclusive){note("Stockage occupé : réessayez après les opérations en cours");return;}
 now=time(nullptr);if(due-now<180){note("Réveil proche : la tablette reste allumée");return;}
 // RTC clocks coarser than a second may round up by a minute. Wake two
 // minutes early for this rounding and OS startup, then ring at the deadline.
 uint64_t delayMs=uint64_t(due-now-120)*1000;
 if(delayMs>UINT32_MAX){note("Réveil trop éloigné pour cette veille");return;}
 m5::rtc_datetime_t rtc{};
 if(!M5.Rtc.getDateTime(&rtc)){note("Horloge de réveil indisponible");return;}
 state.sleepUtc=time(nullptr);state.sleepRtc=Tab5AlarmPolicy::civilSeconds(rtc.date.year,rtc.date.month,rtc.date.date,rtc.time.hours,rtc.time.minutes,rtc.time.seconds);
 if(!state.sleepRtc){state.sleepUtc=0;note("Horloge de réveil indisponible");return;}
 M5.Rtc.disableIRQ();M5.Rtc.clearIRQ();uint32_t actual=M5.Rtc.setTimerIRQ(uint32_t(delayMs));
 if(!actual||actual>delayMs+60000){M5.Rtc.disableIRQ();state.sleepUtc=state.sleepRtc=0;note("Programmation du réveil impossible");return;}
 if(!save()){M5.Rtc.disableIRQ();state.sleepUtc=state.sleepRtc=0;note("Enregistrement du réveil impossible");return;}
 // Power.powerOff() preserves the verified RTC timer on Tab5. The ESP timer
 // is a fallback if the power controller cannot cut the main supply.
 esp_sleep_enable_timer_wakeup(uint64_t(actual)*1000);
 M5.getIOExpander(1).digitalWrite(0,false);
 M5.Power.powerOff();
}
}
void begin(Hooks value){
 hooks=value;Preferences p;if(p.begin("tab5_alarm",true)){Tab5AlarmPolicy::State saved;if(p.getBytes("state",&saved,sizeof(saved))==sizeof(saved)&&Tab5AlarmPolicy::valid(saved))state=saved;p.end();}state.reserved[0]=Tab5AlarmTone::normalize(state.reserved[0]);
 if(M5.Rtc.isEnabled()){
  M5.Rtc.disableIRQ();M5.Rtc.clearIRQ();m5::rtc_datetime_t rtc{};
  if(state.sleepUtc&&M5.Rtc.getDateTime(&rtc)){
   time_t restored=Tab5AlarmPolicy::restoredClock(state,Tab5AlarmPolicy::civilSeconds(rtc.date.year,rtc.date.month,rtc.date.date,rtc.time.hours,rtc.time.minutes,rtc.time.seconds));
   if(restored){timeval tv{restored,0};settimeofday(&tv,nullptr);tm local{};localtime_r(&restored,&local);rtc.date.year=local.tm_year+1900;rtc.date.month=local.tm_mon+1;rtc.date.date=local.tm_mday;rtc.date.weekDay=local.tm_wday;rtc.time.hours=local.tm_hour;rtc.time.minutes=local.tm_min;rtc.time.seconds=local.tm_sec;M5.Rtc.setDateTime(rtc);}
  }
 }
 if(state.sleepUtc){state.sleepUtc=state.sleepRtc=0;save();}
}
void open(){
 if(alert)return;makePanel();label("Réveil");clockLabel=label("");label("Vérifiez la date, l’heure et le fuseau horaire dans les réglages.");
 auto* timeRow=row();auto* enableLabel=lv_label_create(timeRow);lv_obj_set_width(enableLabel,200);lv_label_set_long_mode(enableLabel,LV_LABEL_LONG_WRAP);lv_label_set_text(enableLabel,tr("Activer le réveil"));enabled=lv_switch_create(timeRow);if(state.enabled)lv_obj_add_state(enabled,LV_STATE_CHECKED);Tab5Focus.describe(enabled,tr("Activer le réveil"));
 auto choices=[](unsigned count){String s;for(unsigned n=0;n<count;++n){char text[12];snprintf(text,sizeof(text),"%02u",n);if(n)s+='\n';s+=text;}return s;};
 hours=lv_dropdown_create(timeRow);lv_obj_set_width(hours,120);lv_dropdown_set_options(hours,choices(24).c_str());lv_dropdown_set_selected(hours,state.hour);Tab5Focus.describe(hours,tr("Heures"));
 minutes=lv_dropdown_create(timeRow);lv_obj_set_width(minutes,120);lv_dropdown_set_options(minutes,choices(60).c_str());lv_dropdown_set_selected(minutes,state.minute);Tab5Focus.describe(minutes,tr("Minutes"));
 repeat=lv_dropdown_create(timeRow);lv_obj_set_width(repeat,260);lv_dropdown_set_options(repeat,tr("Une fois\nTous les jours"));lv_dropdown_set_selected(repeat,state.daily);
 label("Sonnerie du réveil");tone=lv_dropdown_create(panel);lv_obj_set_width(tone,LV_PCT(100));lv_dropdown_set_options(tone,tr("Classique\nCarillon\nPulsations\nDoux"));lv_dropdown_set_selected(tone,state.reserved[0]);Tab5Focus.describe(tone,tr("Sonnerie du réveil"));
 volumeText=label("");lv_label_set_text_fmt(volumeText,"%s %u %%",tr("Volume du réveil"),unsigned((state.volume*100+127)/255));
 volume=lv_slider_create(panel);lv_obj_set_width(volume,LV_PCT(95));lv_obj_set_height(volume,24);lv_obj_set_ext_click_area(volume,16);lv_slider_set_range(volume,1,255);lv_slider_set_value(volume,state.volume,LV_ANIM_OFF);lv_obj_add_event_cb(volume,[](lv_event_t* e){lv_label_set_text_fmt(volumeText,"%s %u %%",tr("Volume du réveil"),unsigned((lv_slider_get_value(lv_event_get_target_obj(e))*100+127)/255));},LV_EVENT_VALUE_CHANGED,nullptr);
 auto* actions=row();button(actions,"Enregistrer",commit);previewButton=button(actions,"Essayer la sonnerie",togglePreview);
 status=label("");auto* sleep=row();button(sleep,"Veille jusqu’au réveil",sleepUntilAlarm);button(sleep,"Fermer",close);
 label("La veille prolongée redémarre la tablette avant la sonnerie. Gardez une batterie chargée ou une alimentation branchée. La charge de la batterie est interrompue pendant cette veille.");
 update();Tab5UiStyle::tree(panel);Tab5Focus.focusFirst(panel);
}
void service(){
 if(preview&&millis()-ringAt>10000U){stopPreview();previewLabel();}
 if(alert&&millis()-ringAt>300000U){dismiss();}
 if(ringPending&&!soundRunning.load())start();
 if(uint32_t(millis()-lastPoll)<250)return;lastPoll=millis();
 if(!alert){auto event=Tab5AlarmPolicy::poll(state,time(nullptr));if(event!=Tab5AlarmPolicy::Event::None){save();if(event==Tab5AlarmPolicy::Event::Ring)start();else missed=true;}}
 if(alert&&hooks.wake)hooks.wake();update();
}
bool visible(){return overlay!=nullptr;}
bool ringing(){return alert;}
void key(const writing::KeyEvent& e){const auto* focus=Tab5Focus.focused();bool input=focus&&lv_obj_check_type(focus,&lv_textarea_class);if(e.key==writing::Key::Escape||(e.key==writing::Key::Backspace&&!input&&!Tab5Focus.controlling())){if(alert)dismiss();else close();return;}if(alert&&e.key==writing::Key::Text&&!e.ctrl&&e.text[0]==' '){snooze();return;}if(panel)Tab5Focus.key(e,panel);}
}
