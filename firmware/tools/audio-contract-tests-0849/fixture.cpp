#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <alloca.h>
#define CONFIG_IDF_TARGET_ESP32P4 1
#define M5UNIFIED_I2S_HW_V2 1
#define M5UNIFIED_HAS_CLOCKED_ENABLE 1
#define TAB5_AUDIO_DRIVER_TRACE 0
#define SAMPLERATE_MUL 256
#define portNUM_PROCESSORS 2
#define pdMS_TO_TICKS(x) (x)
using i2s_port_t=int;using esp_err_t=int;using BaseType_t=int;using TickType_t=uint32_t;
constexpr int I2S_NUM_0=0,I2S_PIN_NO_CHANGE=-1,ESP_OK=0,pdFALSE=0,pdTRUE=1,pdPASS=1;
constexpr uint32_t portMAX_DELAY=0xffffffff;
std::atomic<unsigned> checks{0};
#define CHECK(x) do {++checks; if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";std::abort();}}while(0)
struct Semaphore {std::mutex mutex;std::condition_variable cv;bool ready;explicit Semaphore(bool r):ready(r){}};
using SemaphoreHandle_t=Semaphore*;std::vector<Semaphore*> semaphores;
SemaphoreHandle_t xSemaphoreCreateBinary(){auto s=new Semaphore(false);semaphores.push_back(s);return s;}
SemaphoreHandle_t xSemaphoreCreateMutex(){auto s=new Semaphore(true);semaphores.push_back(s);return s;}
int xSemaphoreTake(SemaphoreHandle_t s,uint32_t ticks){std::unique_lock<std::mutex> l(s->mutex);if(ticks==portMAX_DELAY)s->cv.wait(l,[&]{return s->ready;});else if(!s->cv.wait_for(l,std::chrono::milliseconds(ticks),[&]{return s->ready;}))return pdFALSE;s->ready=false;return pdTRUE;}
void semaphoreHook(SemaphoreHandle_t);
void xSemaphoreGive(SemaphoreHandle_t s){{std::lock_guard<std::mutex> l(s->mutex);s->ready=true;s->cv.notify_all();}semaphoreHook(s);}
struct Task{std::thread thread;std::mutex mutex;std::condition_variable cv;unsigned notices=0;};using TaskHandle_t=Task*;
thread_local TaskHandle_t currentTask=nullptr;std::vector<TaskHandle_t> tasks;
TaskHandle_t xTaskGetCurrentTaskHandle(){return currentTask;}
void xTaskNotifyGive(TaskHandle_t t){CHECK(t);std::lock_guard<std::mutex> l(t->mutex);++t->notices;t->cv.notify_all();}
uint32_t ulTaskNotifyTake(int clear,uint32_t ticks){auto t=currentTask;CHECK(t);std::unique_lock<std::mutex> l(t->mutex);if(ticks==portMAX_DELAY)t->cv.wait(l,[&]{return t->notices;});else t->cv.wait_for(l,std::chrono::milliseconds(ticks),[&]{return t->notices;});auto n=t->notices;if(clear)t->notices=0;else if(n)--t->notices;return n;}
int xTaskCreate(void(*fn)(void*),const char*,size_t,void*arg,int,TaskHandle_t*out){auto t=new Task;tasks.push_back(t);*out=t;t->thread=std::thread([=]{currentTask=t;fn(arg);});return pdPASS;}
int xTaskCreatePinnedToCore(void(*f)(void*),const char*n,size_t s,void*a,int p,TaskHandle_t*h,int){return xTaskCreate(f,n,s,a,p,h);}
void vTaskDelay(unsigned){std::this_thread::yield();}
void vTaskDelete(void*){}
uint32_t millis(){static auto begin=std::chrono::steady_clock::now();return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();}
// SPEAKER_CONFIG
// SPEAKER_CLASS
Speaker_Class* liveSpeaker=nullptr;
std::mutex retireMutex;std::condition_variable retireCv;bool watchRetirement=false,beforeClear=false,allowClear=false,afterClear=false,allowMarker=false;
void semaphoreHook(SemaphoreHandle_t s){
 if(!currentTask||!liveSpeaker||s!=liveSpeaker->_task_semaphore)return;
 auto&ch=liveSpeaker->_ch_info[0];std::unique_lock<std::mutex> l(retireMutex);
 if(!watchRetirement||!ch.current.data||ch.current.repeat)return;
 if(!beforeClear && (liveSpeaker->_play_channel_bits.load()&1)){
  beforeClear=true;retireCv.notify_all();retireCv.wait(l,[]{return allowClear;});
 }else if(beforeClear&&!afterClear&&!(liveSpeaker->_play_channel_bits.load()&1)){
  afterClear=true;retireCv.notify_all();retireCv.wait(l,[]{return allowMarker;});watchRetirement=false;
 }
}
std::atomic<unsigned> installs{0},uninstalls{0},enables{0},disables{0},writes{0},clockStops{0};
std::atomic<bool> failSetup{false};
std::mutex ioMutex;std::condition_variable ioCv;bool blockWorker=false,workerAtGate=false;std::vector<int16_t> captured;
int _spk_i2s_handle[1]={0};struct i2s_chan_info_t{size_t total_dma_buf_size;};
int i2s_channel_get_info(int,i2s_chan_info_t*info){info->total_dma_buf_size=liveSpeaker->_cfg.dma_buf_len*liveSpeaker->_cfg.dma_buf_count*4;return ESP_OK;}
int Speaker_Class::_setup_i2s(){if(failSetup)return -1;++installs;return ESP_OK;}
int _spk_i2s_start(i2s_port_t){return ESP_OK;}int _spk_i2s_stop(i2s_port_t){++clockStops;return ESP_OK;}
int _spk_i2s_driver_uninstall(i2s_port_t){++uninstalls;return ESP_OK;}
int _spk_i2s_write(i2s_port_t,void*data,size_t bytes,size_t*done,uint32_t){
 std::unique_lock<std::mutex> l(ioMutex);
 if(currentTask){workerAtGate=true;ioCv.notify_all();ioCv.wait(l,[]{return !blockWorker;});auto p=static_cast<int16_t*>(data);captured.insert(captured.end(),p,p+bytes/2);}
 ++writes;*done=bytes;return ESP_OK;
}
// SPEAKER_FUNCTIONS
struct Expander{bool readRegister(uint8_t,uint8_t*p,size_t){*p=0;return true;}bool digitalWrite(uint8_t,bool){return true;}};
struct Device{Speaker_Class Speaker;Expander exp;Expander&getIOExpander(int){return exp;}} M5;
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
std::atomic<unsigned> pcmBuffers{0};std::atomic<bool> failPcm{false};
void* heap_caps_calloc(size_t n,size_t z,int){if(failPcm)return nullptr;auto p=calloc(n,z);if(p)++pcmBuffers;return p;}
void heap_caps_free(void*p){if(p){CHECK(!M5.Speaker.isRunning());--pcmBuffers;free(p);}}
class AudioOutput{public:virtual~AudioOutput()=default;virtual bool begin(){return true;}virtual bool ConsumeSample(int16_t[2]){return true;}virtual bool SetRate(int){return true;}virtual bool SetChannels(int){return true;}virtual void flush(){}virtual bool stop(){return true;}int hertz=0,channels=0,gainF2P6=0;};
// DRIVER_HEADER
// DRIVER_SOURCE

std::atomic<unsigned> decoders{0},sources{0},leases{0};
struct AudioFileSource {AudioFileSource(){++sources;}virtual~AudioFileSource(){--sources;}virtual bool isOpen(){return true;}virtual void close(){}};
struct MockGenerator {AudioOutput*output;explicit MockGenerator(AudioOutput*o):output(o){++decoders;}virtual~MockGenerator(){--decoders;}virtual bool stop(){return output->stop();}};
struct AudioGeneratorFLAC {AudioFileSource*file=nullptr;AudioOutput*output=nullptr;bool running=false;virtual~AudioGeneratorFLAC()=default;virtual bool begin(AudioFileSource*s,AudioOutput*t){file=s;output=t;t->begin();running=true;return true;}virtual bool stop(){running=false;return output->stop();}};
struct Tab5SeekableFlacGenerator:AudioGeneratorFLAC {
// FLAC_BEGIN
// FLAC_STOP
};
struct Tab5NetworkLease{Tab5NetworkLease(){++leases;}~Tab5NetworkLease(){--leases;}};
void tab5NetworkSetMediaPending(bool){} void tab5NetDiagnosticsRadioEnd(){}
// PRIORITY_HEADER
using UBaseType_t=unsigned;unsigned uxQueueMessagesWaiting(void*){return 0;}
struct Tab5AudioCommand{};void tab5LogMemory(const char*){}
enum class Tab5AudioMode{Idle,Local,Radio,Recording};
struct Tab5AudioEngine{
 AudioOutput*_output=nullptr;MockGenerator *_mp3=nullptr,*_wav=nullptr,*_aac=nullptr,*_flac=nullptr;
 void*_aacCodecBuffer=nullptr,*_mp3CodecBuffer=nullptr;size_t _mp3CodecBufferSize=0;
 AudioFileSource*_streamSource=nullptr,*_radioSource=nullptr,*_localSource=nullptr;Tab5NetworkLease*_radioLease=nullptr;
 bool _paused=false,_playRadioAfterWifi=false,_radioReconnectPending=false,_playingRecording=false;
 uint32_t _pauseStartedAt=0,_playStartedAt=0,_localEstimatedSeconds=0;Tab5AudioMode _mode=Tab5AudioMode::Local;std::string _currentLocalPath;
 Tab5AudioPriorityControl _memoryPriority;bool _priorityWasRadio=false,_priorityWasPaused=false,_priorityResume=false,_priorityCommandPending=false,_wifiPending=false;
 std::string _priorityPath;uint32_t _priorityPosition=0;void*_commandQueue=nullptr;Tab5AudioCommand _priorityCommand;
 bool decoderRunning(){return _mp3||_wav||_aac||_flac;}uint32_t playbackProgressPermille(){return 0;}
 void setStatus(const char*){}void drainCommands(){}void handleCommand(const Tab5AudioCommand&){}bool resumeSelectedStation(){CHECK(false);return false;}
 bool startLocalDecoder(const std::string&){CHECK(false);return false;}void execSeekPermille(uint32_t){CHECK(false);}void execTogglePause(){CHECK(false);}
 void serviceMemoryPriority();void cleanupDecoder(bool preserveTransport=false);void stopPlayback(bool clearIntent,bool preserveTransport=false);
 void setNowPlaying(const char*){CHECK(Tab5AudioDevice.owner()!=Tab5AudioClient::Audio);}
};
// ENGINE_FUNCTIONS
bool codec(void*,bool enabled){if(enabled)++enables;else ++disables;return true;}
void joinTasks(){for(auto*t:tasks){if(t->thread.joinable())t->thread.join();delete t;}tasks.clear();}
void gate(bool blocked){std::lock_guard<std::mutex> l(ioMutex);blockWorker=blocked;workerAtGate=false;if(!blocked)ioCv.notify_all();}
void awaitGate(){std::unique_lock<std::mutex> l(ioMutex);CHECK(ioCv.wait_for(l,std::chrono::seconds(5),[]{return workerAtGate;}));}
int main(){
 liveSpeaker=&M5.Speaker;M5.Speaker.setCallback(nullptr,codec);CHECK(Tab5AudioDevice.begin(150,0,6));
 const std::vector<int16_t> pcm(16384,12000),silent(4096,0);Tab5PcmSpec spec;spec.sampleRate=44100;
 unsigned cycles=0;
 for(unsigned i=0;i<100;++i){
  CHECK(Tab5AudioDevice.claim(Tab5AudioClient::Audio,0));auto enableBefore=enables.load(),installBefore=installs.load(),clockStopsBefore=clockStops.load();CHECK(pcmBuffers==1);
  CHECK(Tab5AudioDevice.writePcm(Tab5AudioClient::Audio,pcm.data(),pcm.size()/2,spec));CHECK(Tab5AudioDevice.flush(Tab5AudioClient::Audio));
  Tab5AudioDevice.stop(Tab5AudioClient::Audio);CHECK(!M5.Speaker.isPlaying());CHECK(M5.Speaker.isRunning());CHECK(Tab5AudioDevice.owner()==Tab5AudioClient::Audio);CHECK(enables==enableBefore&&installs==installBefore&&clockStops==clockStopsBefore);
  // Repeated pause/seek while already quiescent needs no second marker.
  Tab5AudioDevice.stop(Tab5AudioClient::Audio);spec.sampleRate=(i%2)?44100:48000;
  {std::lock_guard<std::mutex> l(ioMutex);captured.clear();}
  CHECK(Tab5AudioDevice.writePcm(Tab5AudioClient::Audio,silent.data(),silent.size()/2,spec));CHECK(Tab5AudioDevice.flush(Tab5AudioClient::Audio));Tab5AudioDevice.stop(Tab5AudioClient::Audio);
  CHECK(enables==enableBefore&&installs==installBefore&&clockStops==clockStopsBefore);
  {std::lock_guard<std::mutex> l(ioMutex);CHECK(std::all_of(captured.begin(),captured.end(),[](int16_t n){return n==0;}));}
  CHECK(Tab5AudioDevice.release(Tab5AudioClient::Audio));CHECK(!M5.Speaker.isRunning());CHECK(Tab5AudioDevice.owner()==Tab5AudioClient::None&&pcmBuffers==0);joinTasks();++cycles;
 }
 // Worker still reads a queued/current buffer: stop must wait, retain memory,
 // then return without restarting clocks once the worker receives its marker.
 CHECK(Tab5AudioDevice.claim(Tab5AudioClient::Audio,0));gate(true);CHECK(Tab5AudioDevice.writePcm(Tab5AudioClient::Audio,pcm.data(),1024,spec));CHECK(Tab5AudioDevice.flush(Tab5AudioClient::Audio));awaitGate();
 std::atomic<bool> stopped{false};std::thread stop([&]{Tab5AudioDevice.stop(Tab5AudioClient::Audio);stopped=true;});
 while(!M5.Speaker._req_lock.load())std::this_thread::yield();CHECK(!stopped&&pcmBuffers==1);gate(false);stop.join();CHECK(stopped&&!M5.Speaker.isPlaying()&&M5.Speaker.isRunning());
 // Force natural EOF to clear its bit while our stop marker is queued.
 // This transient bit-clear is NOT an acknowledgement of that marker.
 {std::lock_guard<std::mutex> l(retireMutex);watchRetirement=true;}
 CHECK(Tab5AudioDevice.writePcm(Tab5AudioClient::Audio,pcm.data(),1024,spec));CHECK(Tab5AudioDevice.flush(Tab5AudioClient::Audio));
 {std::unique_lock<std::mutex> l(retireMutex);CHECK(retireCv.wait_for(l,std::chrono::seconds(5),[]{return beforeClear;}));}
 stopped=false;std::thread stopAtEof([&]{Tab5AudioDevice.stop(Tab5AudioClient::Audio);stopped=true;});
 auto markerQueued=[] {auto&ch=M5.Speaker._ch_info[0];return (ch.wavinfo[0].state.load()&Speaker_Class::wav_state_stop_marker)||(ch.wavinfo[1].state.load()&Speaker_Class::wav_state_stop_marker);};
 while(!markerQueued())std::this_thread::yield();
 {std::unique_lock<std::mutex> l(retireMutex);allowClear=true;retireCv.notify_all();CHECK(retireCv.wait_for(l,std::chrono::seconds(5),[]{return afterClear;}));}
 // Let the waiting producer run; it must keep waiting despite bit zero.
 for(unsigned n=0;n<10000;++n)std::this_thread::yield();CHECK(!stopped&&markerQueued());
 {std::lock_guard<std::mutex> l(retireMutex);allowMarker=true;retireCv.notify_all();}stopAtEof.join();CHECK(stopped&&!M5.Speaker.isPlaying());
 // Release cannot silently abandon an owner when routing holds the mutex.
 CHECK(xSemaphoreTake(Tab5AudioDevice._ownerMutex,0)==pdTRUE);std::atomic<bool> released{false};std::thread release([&]{CHECK(Tab5AudioDevice.release(Tab5AudioClient::Audio));released=true;});
 std::this_thread::sleep_for(std::chrono::milliseconds(300));CHECK(!released&&pcmBuffers==1&&Tab5AudioDevice.owner()==Tab5AudioClient::Audio);xSemaphoreGive(Tab5AudioDevice._ownerMutex);release.join();CHECK(released&&pcmBuffers==0&&Tab5AudioDevice.owner()==Tab5AudioClient::None);joinTasks();
 for(auto owner:{Tab5AudioClient::Video,Tab5AudioClient::Native,Tab5AudioClient::Alarm,Tab5AudioClient::Sdr}){CHECK(Tab5AudioDevice.claim(owner,0));CHECK(!Tab5AudioDevice.claim(Tab5AudioClient::Audio,0));CHECK(Tab5AudioDevice.writePcm(owner,pcm.data(),1024,spec));CHECK(Tab5AudioDevice.release(owner));CHECK(pcmBuffers==0&&Tab5AudioDevice.owner()==Tab5AudioClient::None);joinTasks();}
 failPcm=true;CHECK(!Tab5AudioDevice.claim(Tab5AudioClient::Audio,0));CHECK(pcmBuffers==0);failPcm=false;
 failSetup=true;CHECK(!Tab5AudioDevice.claim(Tab5AudioClient::Audio,0));CHECK(pcmBuffers==0&&Tab5AudioDevice.owner()==Tab5AudioClient::None);failSetup=false;CHECK(Tab5AudioDevice.claim(Tab5AudioClient::Audio,0));CHECK(Tab5AudioDevice.release(Tab5AudioClient::Audio));joinTasks();
 // Execute actual engine cleanup and replacement contract through the real
 // sink/worker. Decoder and source boundaries are tracked test doubles.
 AudioOutputTab5Speaker sink(&Tab5AudioDevice);Tab5AudioEngine engine;engine._output=&sink;
 CHECK(sink.begin());auto hotEnables=enables.load();engine._mp3=new MockGenerator(&sink);engine._localSource=new AudioFileSource;
 engine.cleanupDecoder(true);CHECK(decoders==0&&sources==0&&Tab5AudioDevice.owner()==Tab5AudioClient::Audio&&M5.Speaker.isRunning());
 engine._flac=new MockGenerator(&sink);engine._streamSource=new AudioFileSource;engine._radioSource=engine._streamSource;engine._radioLease=new Tab5NetworkLease;
 engine.stopPlayback(false,true);CHECK(decoders==0&&sources==0&&leases==0&&enables==hotEnables&&M5.Speaker.isRunning());
 engine._mode=Tab5AudioMode::Radio;engine._paused=true;auto token=engine._memoryPriority.request();CHECK(token);engine.serviceMemoryPriority();CHECK(engine._memoryPriority.state(token)==Tab5AudioPriorityControl::Held&&Tab5AudioDevice.owner()==Tab5AudioClient::None&&pcmBuffers==0);joinTasks();
 CHECK(Tab5AudioDevice.claim(Tab5AudioClient::Video,0));CHECK(Tab5AudioDevice.release(Tab5AudioClient::Video));joinTasks();engine._memoryPriority.release(token);engine.serviceMemoryPriority();CHECK(engine._paused&&engine._memoryPriority.state()==Tab5AudioPriorityControl::Idle);
 engine.stopPlayback(true,true);CHECK(engine._mode==Tab5AudioMode::Idle&&Tab5AudioDevice.owner()==Tab5AudioClient::None&&pcmBuffers==0);joinTasks();
 // Execute the exact FLAC override against its library's ignored sink result.
 {AudioFileSource file;Tab5SeekableFlacGenerator flac;CHECK(Tab5AudioDevice.claim(Tab5AudioClient::Video,0));CHECK(!flac.begin(&file,&sink));CHECK(!flac.running);CHECK(flac.stop());CHECK(Tab5AudioDevice.release(Tab5AudioClient::Video));joinTasks();
  CHECK(flac.begin(&file,&sink));CHECK(flac.running);CHECK(flac.stop());CHECK(Tab5AudioDevice.release(Tab5AudioClient::Audio));joinTasks();}
 CHECK(decoders==0&&sources==0&&leases==0&&pcmBuffers==0);
 for(auto*s:semaphores)delete s;
 std::cout<<"{\"checks\":"<<checks<<",\"cycles\":"<<cycles<<",\"nativeWorkerAndDriver\":true,\"quiescenceWhileWorkerBlocked\":true,\"releaseContention300ms\":true,\"pcmBuffersRemaining\":"<<pcmBuffers<<",\"physicalAudioTested\":false}\n";
}
