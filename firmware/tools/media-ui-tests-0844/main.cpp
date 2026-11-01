#include <lvgl.h>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
static unsigned checks=0,invalidations=0,flushes=0,snapshotReads=0,videoReads=0,indicatorForces=0,indicatorCalls=0;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);abort();}}while(0)
struct String {
 std::string value;
 String()=default;String(const char* s):value(s?s:""){}String(const std::string& s):value(s){}
 const char* c_str()const{return value.c_str();}size_t length()const{return value.size();}
 bool operator==(const char* s)const{return value==s;}bool operator!=(const String& s)const{return value!=s.value;}
 String& operator+=(const String& s){value+=s.value;return *this;}
};
#include "production-structs.inc"
static bool snapshotBusy=false,audioDirty=false,videoDirty=false;
static Tab5AudioSnapshot storedAudio;
static Tab5VideoSnapshot storedVideo;
struct Audio {
 bool takeUiDirty(){bool result=audioDirty;audioDirty=false;return result;}
 bool snapshot(Tab5AudioSnapshot& out){++snapshotReads;if(snapshotBusy)return false;out=storedAudio;return true;}
 bool mediaSnapshot(Tab5AudioSnapshot& out,uint32_t& seen,bool force){if(!force&&!audioDirty&&seen)return false;++snapshotReads;if(snapshotBusy){out=storedAudio;out.nowPlaying="partial-copy";return false;}out=storedAudio;audioDirty=false;++seen;return true;}
}Tab5Audio;
struct Video {
 bool snapshot(Tab5VideoSnapshot& out){++videoReads;if(snapshotBusy)return false;out=storedVideo;return true;}
 bool takeUiDirty(){bool result=videoDirty;videoDirty=false;return result;}
}Tab5Video;
struct {bool hasTranslations(){return false;}}Tab5Locale;
namespace Tab5MediaSession {struct State{char selected[4][192]{};char title[4][128]{};}saved;const State& state(){return saved;}}
static uint32_t clockMs=1000;uint32_t millis(){return clockMs;}
enum class AudioView:uint8_t{Local,Radio,Recordings,Videos};static AudioView audioView=AudioView::Local;
static uint32_t lastAudioUiUpdateMs=0;
static bool audioProgressDragging=false,videoViewerActive=false;
static lv_obj_t *audioPage,*audioNowPlaying,*audioStatus,*audioProgressSlider,*audioTimeLabel,*audioPlayBtn,*audioStopBtn,*audioRecordBtn,*audioRecordGraphic,*audioRecordControls,*audioVolumeIcon,*audioVolumeValue,*audioVolumeSlider,*audioLevelBar;
static const char* T(const char* s){return s;}static String translateRuntimeText(const String& s){return s;}
static void audioRememberPlayback(const Tab5AudioSnapshot&){}
static bool audioPathIsSeekable(const String& path){return path.length()!=0;}
static void updateTopAudioIndicator(bool force){++indicatorCalls;indicatorForces+=force;}
#include "production-ui.inc"
static void pump(){clockMs+=20;lv_timer_handler();}
static lv_obj_t* label(lv_obj_t* parent,int y){auto* obj=lv_label_create(parent);lv_obj_set_width(obj,380);lv_obj_set_pos(obj,2,y);return obj;}
static void setup(){
 auto* parent=lv_screen_active();audioPage=lv_obj_create(parent);lv_obj_set_size(audioPage,480,710);lv_obj_set_pos(audioPage,770,0);lv_obj_remove_flag(audioPage,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_pad_all(audioPage,18,0);
 audioNowPlaying=label(audioPage,32);audioStatus=label(audioPage,140);audioTimeLabel=label(audioPage,230);
 audioProgressSlider=lv_slider_create(audioPage);lv_obj_set_size(audioProgressSlider,380,28);lv_obj_set_pos(audioProgressSlider,2,198);lv_slider_set_range(audioProgressSlider,0,1000);
 audioPlayBtn=lv_button_create(audioPage);lv_obj_set_pos(audioPlayBtn,100,278);lv_obj_set_size(audioPlayBtn,64,60);lv_label_create(audioPlayBtn);
 audioStopBtn=lv_button_create(audioPage);lv_obj_set_pos(audioStopBtn,170,278);lv_obj_set_size(audioStopBtn,58,56);lv_label_create(audioStopBtn);
 audioRecordControls=lv_obj_create(audioPage);lv_obj_set_size(audioRecordControls,380,80);lv_obj_set_pos(audioRecordControls,0,370);lv_obj_remove_flag(audioRecordControls,LV_OBJ_FLAG_SCROLLABLE);
 audioRecordBtn=lv_button_create(audioRecordControls);lv_obj_set_pos(audioRecordBtn,2,4);lv_obj_set_size(audioRecordBtn,82,64);lv_label_create(audioRecordBtn);audioRecordGraphic=lv_obj_create(audioRecordBtn);lv_obj_set_size(audioRecordGraphic,10,10);
 audioLevelBar=lv_bar_create(audioRecordControls);lv_obj_set_size(audioLevelBar,250,22);lv_obj_set_pos(audioLevelBar,100,25);lv_bar_set_range(audioLevelBar,0,32767);
 audioVolumeIcon=label(audioPage,490);audioVolumeValue=label(audioPage,490);lv_obj_set_width(audioVolumeValue,72);lv_obj_align(audioVolumeValue,LV_ALIGN_TOP_RIGHT,-2,490);
 audioVolumeSlider=lv_slider_create(audioPage);lv_obj_set_size(audioVolumeSlider,380,26);lv_obj_set_pos(audioVolumeSlider,2,538);lv_slider_set_range(audioVolumeSlider,0,255);
}
int main(){
 lv_init();lv_tick_set_cb([](){return clockMs;});auto* display=lv_display_create(1280,720);static uint8_t buf[1280*32*2];lv_display_set_buffers(display,buf,nullptr,sizeof(buf),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,[](lv_display_t* d,const lv_area_t*,uint8_t*){++flushes;lv_display_flush_ready(d);});
 lv_display_add_event_cb(display,[](lv_event_t*){++invalidations;},LV_EVENT_INVALIDATE_AREA,nullptr);
 setup();updateAudioUi(true);for(unsigned i=0;i<10;++i)pump();invalidations=flushes=snapshotReads=indicatorForces=indicatorCalls=0;
 for(unsigned t=0;t<60000;t+=8){clockMs+=8;updateAudioUi(false);lv_timer_handler();}
 const unsigned idleSnapshots=snapshotReads,idleInvalidations=invalidations,idleFlushes=flushes,idleForces=indicatorForces;
 CHECK(lv_slider_get_value(audioVolumeSlider)==150);CHECK(idleInvalidations==0);CHECK(idleFlushes==0);
 // A change notification updates status, progress, controls without altering user selection.
 storedAudio.mode=Tab5AudioMode::Local;storedAudio.playing=true;storedAudio.currentPath="/Audio/a.wav";storedAudio.nowPlaying="A";storedAudio.status="playing";storedAudio.playbackEstimatedSeconds=100;storedAudio.playbackProgressPermille=250;storedAudio.playbackElapsedSeconds=25;audioDirty=true;
 clockMs+=256;updateAudioUi(false);pump();CHECK(!strcmp(lv_label_get_text(audioNowPlaying),"A"));CHECK(lv_slider_get_value(audioProgressSlider)==250);CHECK(!lv_obj_has_state(audioProgressSlider,LV_STATE_DISABLED));CHECK(!lv_obj_has_flag(audioStopBtn,LV_OBJ_FLAG_HIDDEN));CHECK(!strcmp(lv_label_get_text(audioTimeLabel),"00:25  /  01:40"));
 // Meter/time changes remain visible, including while dragging the seek bar.
 storedAudio.playbackProgressPermille=500;storedAudio.playbackElapsedSeconds=50;audioDirty=true;audioProgressDragging=true;clockMs+=256;updateAudioUi(false);CHECK(lv_slider_get_value(audioProgressSlider)==250);audioProgressDragging=false;audioDirty=true;clockMs+=256;updateAudioUi(false);CHECK(lv_slider_get_value(audioProgressSlider)==500);
 storedAudio.mode=Tab5AudioMode::Recording;storedAudio.recording=true;storedAudio.recordingSeconds=3;storedAudio.recordingLevel=17000;audioDirty=true;clockMs+=256;updateAudioUi(false);pump();CHECK(!lv_obj_has_flag(audioRecordControls,LV_OBJ_FLAG_HIDDEN));CHECK(lv_bar_get_value(audioLevelBar)==17000);CHECK(!strcmp(lv_label_get_text(audioTimeLabel),"REC  •  00:03"));
 #if NEW
 CHECK(idleSnapshots==0);CHECK(idleForces==0);
 // Consumed dirty/force signals survive a failed snapshot (OOM/busy boundary).
 snapshotBusy=true;storedAudio.nowPlaying="B";audioDirty=true;clockMs+=256;updateAudioUi(false);CHECK(!strcmp(lv_label_get_text(audioNowPlaying),"A"));snapshotBusy=false;updateAudioUi(false);CHECK(!strcmp(lv_label_get_text(audioNowPlaying),"B"));
 snapshotBusy=true;storedAudio.volume=100;updateAudioUi(true);snapshotBusy=false;updateAudioUi(false);CHECK(lv_slider_get_value(audioVolumeSlider)==100);
 // Never steal video notifications from the full-screen viewer.
 audioView=AudioView::Videos;videoViewerActive=true;videoDirty=true;updateAudioUi(false);CHECK(videoDirty);videoViewerActive=false;storedVideo.state=Tab5VideoState::Playing;storedVideo.name="movie";storedVideo.status="video";updateAudioUi(false);CHECK(!videoDirty);CHECK(!strcmp(lv_label_get_text(audioNowPlaying),"movie"));
 // Failed audio copy must never publish partially copied strings, even when a video event is pending.
 storedVideo.name="movie2";videoDirty=true;snapshotBusy=true;updateAudioUi(false);CHECK(!strcmp(lv_label_get_text(audioNowPlaying),"movie"));snapshotBusy=false;updateAudioUi(false);CHECK(!strcmp(lv_label_get_text(audioNowPlaying),"movie2"));
 #else
 CHECK(idleSnapshots>=200);CHECK(idleForces>=200);
 #endif
 // Producer progress at approximately20Hz must not turn the player into a20Hz renderer.
 audioView=AudioView::Local;storedAudio.mode=Tab5AudioMode::Local;storedAudio.recording=false;storedAudio.recordingLevel=0;updateAudioUi(true);pump();snapshotReads=0;
 uint32_t producerAt=clockMs;
 for(unsigned t=0;t<60000;t+=8){clockMs+=8;if(clockMs-producerAt>=50){producerAt=clockMs;storedAudio.playbackProgressPermille=t/64;storedAudio.playbackElapsedSeconds=t/1000;audioDirty=NEW;}updateAudioUi(false);lv_timer_handler();}
 const unsigned progressSnapshots=snapshotReads;
 #if NEW
 CHECK(progressSnapshots<=240&&progressSnapshots>200);
 #endif
 printf("{\"passed\":true,\"checks\":%u,\"idleSnapshots60s\":%u,\"idleInvalidations60s\":%u,\"idleFlushes60s\":%u,\"idleIndicatorForces60s\":%u,\"progressSnapshots60s\":%u,\"physicalDeviceTested\":false}\n",checks,idleSnapshots,idleInvalidations,idleFlushes,idleForces,progressSnapshots);
 lv_display_delete(display);lv_deinit();
}
