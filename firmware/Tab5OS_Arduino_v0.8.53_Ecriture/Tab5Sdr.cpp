#include "Tab5Sdr.h"
#include "Tab5SdrDsp.h"
#include "WriterUsb.h"
#include "Tab5Task.h"
#include "Tab5AudioDriver.h"
#include "Tab5Audio.h"
#include "src/rtl_sdr/rtl-sdr.h"
#include "src/rtl_sdr/libusb.h"
#include <esp_heap_caps.h>
#include <atomic>
#include <new>
namespace Tab5Sdr {namespace {
std::atomic<bool> working{false},cancelled{false},scanActive{false},scanHeld{false},overflow{false};
std::atomic<const char*> message{"Prêt"};
std::atomic<uint32_t> tunedHz{100000000},tunedSequence{0};std::atomic<unsigned> tunedMode{Wfm},tunedDirect{0},tunedDeemphasis{50};std::atomic<int> tunedPpm{0};
std::atomic_flag commandLock=ATOMIC_FLAG_INIT,frameLock=ATOMIC_FLAG_INIT;
struct Command {Settings settings;Scan sweep;bool pending=false,sweeping=false;};Command command;
Frame frame;bool fresh=false;
constexpr unsigned RingSize=16;Detection ring[RingSize]{};std::atomic<unsigned> readIndex{0},writeIndex{0};
struct State {Tab5SdrDsp::Dsp dsp;uint8_t iq[4096];int16_t pcm[512];Settings settings;bool failedRestoration=false;};State* state=nullptr;
int stopRequested(){return cancelled.load();}
bool queue(const Settings& settings,const Scan* sweep){
 if(!valid(settings)||commandLock.test_and_set(std::memory_order_acquire))return false;
 command.settings=settings;command.pending=true;command.sweeping=sweep!=nullptr;if(sweep)command.sweep=*sweep;
 commandLock.clear(std::memory_order_release);return true;
}
void detected(const Settings& s,int level){
 unsigned w=writeIndex.load(std::memory_order_relaxed),next=(w+1)%RingSize;
 if(next==readIndex.load(std::memory_order_acquire)){overflow=true;scanHeld=true;message="Détections en attente : balayage en pause";return;}
 ring[w]={s.hz,uint8_t(s.mode),uint8_t(s.direct),int16_t(level)};writeIndex.store(next,std::memory_order_release);
}
bool applyTuning(rtlsdr_dev_t* radio,const Settings& next){
 if(rtlsdr_get_freq_correction(radio)!=next.ppm&&rtlsdr_set_freq_correction(radio,next.ppm)<0)return false;
 return tab5_rtlsdr_set_input_and_freq(radio,int(next.direct),next.hz)>=0&&rtlsdr_reset_buffer(radio)>=0;
}
bool tune(rtlsdr_dev_t* radio,const Settings& next,const Settings* previous){
 if(!applyTuning(radio,next)){
  // Do not let the displayed frequency lie after a refused request. A failed
  // restoration stops reception rather than playing at an unknown frequency.
  if(previous){if(!applyTuning(radio,*previous)){state->failedRestoration=true;cancelled=true;}}
  message="Fréquence ou entrée refusée par la clé";return false;
 }
 state->settings=next;state->dsp.resetAudio(next.deemphasis);++tunedSequence;tunedHz=next.hz;tunedMode=next.mode;tunedDirect=next.direct;tunedPpm=next.ppm;tunedDeemphasis=next.deemphasis;++tunedSequence;return true;
}
void run(){
 rtlsdr_dev_t* radio=nullptr;tab5_rtl_cancel_callback(stopRequested);tab5_rtl_stall_callback([](){message="USB ne répond plus : débranchez le RTL-SDR";});
 if(rtlsdr_open(&radio,0)<0){message="RTL-SDR absent ou indisponible";return;}
 if(rtlsdr_set_sample_rate(radio,Tab5SdrDsp::SampleRate)<0){message="Débit refusé par la clé";rtlsdr_close(radio);return;}
 if(!tune(radio,state->settings,nullptr)){rtlsdr_close(radio);return;}
 rtlsdr_set_tuner_gain_mode(radio,0);rtlsdr_set_agc_mode(radio,0); // Tuner AGC remains on; RTL digital gain can saturate broadcast FM.
 bool audio=false,outputFailed=false;Tab5PcmSpec spec;spec.channels=1;spec.sampleRate=Tab5SdrDsp::AudioRate;
 Scan sweep;uint32_t entered=millis(),lastSpectrum=0,lastAudioAttempt=0;bool sweeping=false,wasHeld=false;int peak=-120;unsigned peakFrames=0;unsigned discard=3;
 message="Réception en cours";
 while(!cancelled){
  bool pending=false;Command next;
  if(!commandLock.test_and_set(std::memory_order_acquire)){if(command.pending){next=command;command.pending=false;pending=true;}commandLock.clear(std::memory_order_release);}
  if(pending){
   outputFailed=false;lastAudioAttempt=0;
   Settings previous=state->settings;
   if(tune(radio,next.settings,&previous)){sweep=next.sweep;sweeping=next.sweeping;scanActive=sweeping;scanHeld=false;wasHeld=false;entered=millis();peak=-120;peakFrames=0;discard=3;message=sweeping?"Balayage en cours":"Réception en cours";}
   else {sweeping=false;scanActive=false;}
  }
  if(sweeping&&wasHeld&&!scanHeld){uint32_t hz;if(!nextFrequency(sweep,state->settings.hz,hz)){sweeping=false;scanActive=false;message="Balayage terminé";}
   else {Settings nextSettings=state->settings,previous=state->settings;nextSettings.hz=hz;if(!tune(radio,nextSettings,&previous)){sweeping=false;scanActive=false;}else {entered=millis();peak=-120;peakFrames=0;discard=3;}}}
  wasHeld=scanHeld.load();
  bool wantAudio=!outputFailed&&state->settings.mode!=Spectrum&&(!sweeping||scanHeld);
  if(audio&&!wantAudio){Tab5AudioDevice.stop(Tab5AudioClient::Sdr);Tab5AudioDevice.release(Tab5AudioClient::Sdr);audio=Tab5AudioDevice.owner()==Tab5AudioClient::Sdr;}
  if(wantAudio&&!audio&&(!lastAudioAttempt||millis()-lastAudioAttempt>=1000)){lastAudioAttempt=millis();audio=Tab5AudioDevice.claim(Tab5AudioClient::Sdr,0);if(!audio)message="Spectre actif : sortie audio occupée";}
  int bytes=0;if(rtlsdr_read_sync(radio,state->iq,sizeof(state->iq),&bytes)<0){if(!cancelled)message="Lecture USB interrompue";break;}
  if(bytes<=0||bytes>int(sizeof(state->iq))){vTaskDelay(1);continue;}
  if(discard){--discard;continue;}
  if(audio&&wantAudio){
   size_t n=state->dsp.audio(state->iq,size_t(bytes),state->settings.mode,state->pcm);
   if(n&&!Tab5AudioDevice.writePcm(Tab5AudioClient::Sdr,state->pcm,n,spec)){
    Tab5AudioDevice.release(Tab5AudioClient::Sdr);audio=Tab5AudioDevice.owner()==Tab5AudioClient::Sdr;outputFailed=true;
    message="Initialisation du pilote audio impossible";
   }
  }
  uint32_t now=millis();
  if(bytes>=int(2*Tab5SdrDsp::N)&&now-lastSpectrum>=80){
   Frame nextFrame;nextFrame.hz=state->settings.hz;state->dsp.spectrum(state->iq,nextFrame.levels);
   // Evaluate only the tuned channel, excluding the DC bins. This is a signal
   // detector, not a station identifier and not a calibrated power meter.
   unsigned width=state->settings.mode==Wfm?64:state->settings.mode==Nfm?8:6;
   int currentPeak=-120;for(unsigned i=128-width;i<=128+width;++i)if(i<127||i>128)currentPeak=std::max(currentPeak,int(nextFrame.levels[i]));
   if(currentPeak>=sweep.threshold){peak=std::max(peak,currentPeak);++peakFrames;}
   if(!frameLock.test_and_set(std::memory_order_acquire)){frame=nextFrame;fresh=true;frameLock.clear(std::memory_order_release);}lastSpectrum=now;
  }
  if(sweeping&&!scanHeld&&now-entered>=sweep.dwell){
   if(peakFrames>=2){detected(state->settings,peak);if(sweep.hold){scanHeld=true;message="Signal détecté : écoute en pause de balayage";}}
   if(!scanHeld){uint32_t hz;if(!nextFrequency(sweep,state->settings.hz,hz)){sweeping=false;scanActive=false;message="Balayage terminé";}
    else {Settings nextSettings=state->settings,previous=state->settings;nextSettings.hz=hz;
     if(!tune(radio,nextSettings,&previous)){sweeping=false;scanActive=false;}else {entered=now;peak=-120;peakFrames=0;discard=3;}}
   }
  }
  if(scanHeld){entered=now;peak=-120;peakFrames=0;wasHeld=true;}
  vTaskDelay(1);
 }
 if(audio){
  Tab5AudioDevice.stop(Tab5AudioClient::Sdr);
  do{
   Tab5AudioDevice.release(Tab5AudioClient::Sdr);
   if(Tab5AudioDevice.owner()==Tab5AudioClient::Sdr)vTaskDelay(1);
  }while(Tab5AudioDevice.owner()==Tab5AudioClient::Sdr);
 }
 tab5_rtl_cancel_callback(nullptr);rtlsdr_close(radio);
}
void task(void*){
 run();tab5_rtl_stall_callback(nullptr);tab5_rtl_cancel_callback(nullptr);
 if(cancelled&&!state->failedRestoration)message="Réception arrêtée";
 state->~State();heap_caps_free(state);state=nullptr;scanActive=false;scanHeld=false;
 // No shared state below working=false: a new start may now reserve a worker.
 tab5TaskCheckpoint("rtl_sdr");working.store(false,std::memory_order_release);vTaskDelete(nullptr);
}
}
bool start(uint32_t hz,unsigned mode){return start(Settings{hz,mode,0});}
bool start(const Settings& settings){
 if(working)return configure(settings);if(!valid(settings)){message="Fréquence ou mode invalide";return false;}
 if(!writing::usbHighSpeedActive()){message="Activez USB rapide puis redémarrez la tablette";return false;}
 Tab5Resources::Budget budget=Tab5Resources::TaskBudget(12288);budget.internal+=12288;budget.internalBlock=std::max<size_t>(budget.internalBlock,4096);budget.psram=sizeof(State);budget.psramBlock=sizeof(State);
 auto admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,budget,"RTL-SDR");if(!admission){message="Mémoire insuffisante";return false;}
 void* mem=heap_caps_malloc(sizeof(State),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!mem){message="Mémoire insuffisante";return false;}
 state=new(mem)State;state->settings=settings;cancelled=false;scanActive=false;scanHeld=false;overflow=false;working=true;
 // Only this UI producer changes command; no worker exists until task create.
 command.pending=false;readIndex=0;writeIndex=0;fresh=false;message="Recherche du tuner";
 if(tab5CreateAdmittedTask(admission,task,"tab5_sdr",12288,nullptr,1,nullptr,0)!=pdPASS){state->~State();heap_caps_free(state);state=nullptr;working=false;message="Mémoire insuffisante";return false;}return true;
}
bool configure(const Settings& settings){if(!working)return start(settings);if(cancelled){message="Arrêt en cours";return false;}return queue(settings,nullptr);}
bool scan(const Scan& sweep,const Settings& settings){
 if(!valid(sweep)||!valid(settings)){message="Balayage invalide ou trop étendu";return false;}
 Settings first=settings;first.hz=sweep.first;if(!working&&!start(first))return false;if(cancelled)return false;return queue(first,&sweep);
}
void pauseScan(bool pause){if(scanActive){scanHeld=pause;message=pause?"Balayage en pause : écoute fixe":"Balayage en cours";}}
bool scanning(){return scanActive;}bool scanPaused(){return scanHeld;}
Settings current(){for(;;){uint32_t before=tunedSequence.load();if(before&1)continue;Settings result{tunedHz.load(),tunedMode.load(),tunedDirect.load(),tunedPpm.load(),tunedDeemphasis.load()};if(before==tunedSequence.load())return result;}}
void stop(){cancelled=true;}bool stopping(){return working.load(std::memory_order_acquire)&&cancelled.load();}bool busy(){return working.load(std::memory_order_acquire);}const char* status(){return message.load();}
bool spectrum(Frame& out){if(frameLock.test_and_set(std::memory_order_acquire))return false;bool ok=fresh;if(ok){out=frame;fresh=false;}frameLock.clear(std::memory_order_release);return ok;}
bool spectrum(uint8_t* out){Frame f;if(!out||!spectrum(f))return false;for(unsigned i=0;i<BinCount;++i)out[i]=uint8_t(std::max(0,int(f.levels[i])+100));return true;}
bool takeDetection(Detection& out){unsigned r=readIndex.load(std::memory_order_relaxed);if(r==writeIndex.load(std::memory_order_acquire))return false;out=ring[r];readIndex.store((r+1)%RingSize,std::memory_order_release);return true;}
bool detectionOverflow(){return overflow.exchange(false);}
void volume(unsigned percent){Tab5Audio.setVolume(std::min(100u,percent)*255/100);}
}
