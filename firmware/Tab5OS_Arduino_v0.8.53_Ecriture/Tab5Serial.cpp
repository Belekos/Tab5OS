#include "Tab5Serial.h"
#include "Tab5Task.h"
#include <Preferences.h>
#include <driver/uart.h>
#include <driver/usb_serial_jtag.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_vfs.h>
#include <sys/stat.h>
#include <freertos/semphr.h>
#include <atomic>
#include <new>
#include <cstdarg>
Tab5SerialDebug Tab5DebugSerial;
namespace Tab5Serial { namespace {
// P4 USB Serial/JTAG uses the FS USB-C PHY. WriterUsb owns HS OTG controller 0
// on USB-A. No TinyUSB device/host instance or host PHY is changed here.
// Official M5Stack Tab5 HAL: UART1, TX20, RX21, DE34 for onboard RS485.
constexpr uart_port_t RsUart=UART_NUM_1;
StaticSemaphore_t lockStorage;SemaphoreHandle_t mutex(){static auto m=xSemaphoreCreateMutexStatic(&lockStorage);return m;}
struct Lock {bool held;explicit Lock(TickType_t ticks=portMAX_DELAY):held(xSemaphoreTake(mutex(),ticks)==pdTRUE){}~Lock(){if(held)xSemaphoreGive(mutex());}};
struct State {Ring<BufferSize> rx,tx;Config current,requested;uint32_t received=0,sent=0,dropped=0;char* text=nullptr;size_t textSize=0,textAt=0;Client receiver=Client::None,sender=Client::None;bool usb=false,rs=false,ready=false,request=false,persist=false,result=false;};
State* state=nullptr;TaskHandle_t task=nullptr;std::atomic<bool> initialized{false};
void freeText(){if(state->text){memset(state->text,0,state->textSize);heap_caps_free(state->text);state->text=nullptr;}state->textSize=state->textAt=0;state->sender=Client::None;}
void resetQueues(){state->rx.clear();state->tx.clear();freeText();}
bool transport(const Config& c){
 if(c.port==Port::Usb){if(!state->usb){usb_serial_jtag_driver_config_t options{};options.tx_buffer_size=2048;options.rx_buffer_size=4096;if(usb_serial_jtag_driver_install(&options)!=ESP_OK)return false;state->usb=true;}return true;}
 if(!state->rs){if(uart_is_driver_installed(RsUart))return false;if(uart_driver_install(RsUart,4096,2048,0,nullptr,0)!=ESP_OK)return false;state->rs=true;}
 uart_config_t options{};options.baud_rate=c.baud;options.data_bits=UART_DATA_8_BITS;options.parity=c.frame==Frame::E81?UART_PARITY_EVEN:c.frame==Frame::O81?UART_PARITY_ODD:UART_PARITY_DISABLE;
 options.stop_bits=c.frame==Frame::N82?UART_STOP_BITS_2:UART_STOP_BITS_1;options.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;options.source_clk=UART_SCLK_DEFAULT;
 return uart_param_config(RsUart,&options)==ESP_OK&&uart_set_pin(RsUart,20,21,34,UART_PIN_NO_CHANGE)==ESP_OK&&uart_set_mode(RsUart,UART_MODE_RS485_HALF_DUPLEX)==ESP_OK;
}
void worker(void*){
 uint8_t bytes[512];
 for(;;){{Lock guard;if(state&&state->request){
  const Config next=state->requested,before=state->current;bool ok=true;
  // Install and uninstall run on this worker/core, as required by the USJ ISR.
  if(state->usb&&(before.port!=next.port||before.mode!=next.mode)){usb_serial_jtag_driver_uninstall();state->usb=false;}
  ok=transport(next);
  if(ok&&state->persist){Preferences preferences;ok=preferences.begin("tab5_serial",false);if(ok){ok=preferences.putBytes("config",&next,sizeof(next))==sizeof(next);preferences.end();}}
  if(ok){state->current=next;state->ready=true;resetQueues();}else state->ready=transport(before);
  state->result=ok;state->request=false;
 }if(state&&state->ready){
  int got=state->current.port==Port::Usb?usb_serial_jtag_read_bytes(bytes,sizeof(bytes),0):uart_read_bytes(RsUart,bytes,sizeof(bytes),0);
  if(got>0){state->received+=got;state->dropped+=size_t(got)-state->rx.push(bytes,size_t(got));}
  const uint8_t* source=bytes;size_t n=state->tx.peek(bytes,sizeof(bytes));bool bulk=false;
  if(!n&&state->text){bulk=true;n=std::min(sizeof(bytes),state->textSize-state->textAt);source=reinterpret_cast<uint8_t*>(state->text+state->textAt);}
  int sent=0;if(n){if(state->current.port==Port::Usb){if(usb_serial_jtag_is_connected())sent=usb_serial_jtag_write_bytes(source,n,0);}
   else {size_t free=0;if(uart_get_tx_buffer_free_size(RsUart,&free)==ESP_OK&&free)sent=uart_write_bytes(RsUart,source,std::min(n,free));}}
  if(sent>0){state->sent+=sent;if(bulk){state->textAt+=sent;if(state->textAt==state->textSize)freeText();}else state->tx.consume(sent);}
 }}vTaskDelay(pdMS_TO_TICKS(5));}
}
ssize_t logWrite(int,const void* bytes,size_t n){size_t written=Serial0.write(static_cast<const uint8_t*>(bytes),n);debugWrite(static_cast<const char*>(bytes),n);return written;}
int logOpen(const char*,int,int){return 0;}int logClose(int){return 0;}
int logStat(int,struct stat* info){memset(info,0,sizeof(*info));info->st_mode=S_IFCHR;return 0;}
bool routeStdio(){esp_vfs_t vfs{};vfs.flags=ESP_VFS_FLAG_DEFAULT;vfs.write=logWrite;vfs.open=logOpen;vfs.close=logClose;vfs.fstat=logStat;
 if(esp_vfs_register("/dev/tab5debug",&vfs,nullptr)!=ESP_OK)return false;
 if(!freopen("/dev/tab5debug","w",stdout)||!freopen("/dev/tab5debug","w",stderr))return false;
 setvbuf(stdout,nullptr,_IONBF,0);setvbuf(stderr,nullptr,_IONBF,0);return true;
}
}
bool begin(){
 {Lock guard;if(initialized.load())return state&&state->ready;initialized=true;
 void* memory=heap_caps_malloc(sizeof(State),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!memory)return false;state=new(memory)State;
 Preferences preferences;if(preferences.begin("tab5_serial",true)){Config saved;if(preferences.getBytesLength("config")==sizeof(saved)&&preferences.getBytes("config",&saved,sizeof(saved))==sizeof(saved)&&valid(saved))state->current=saved;preferences.end();}
 // The SDK duplicates stdout/stderr to USB Serial/JTAG by default. Keep their
 // original UART destination and our bounded mirror through a dedicated VFS.
 // ESP_LOG keeps its normal vprintf path: no recursive global logging hook.
 // This occurs at startup before application workers inherit the FILEs.
 if(!routeStdio())return false;
 if(tab5CreateTask(worker,"tab5_serial",4096,nullptr,2,&task,0)!=pdPASS){state->ready=false;return false;}
 }
 return configure(config(),false);
}
Config config(){Lock guard;return state?state->current:Config{};}
namespace {
bool queueConfiguration(const Config& next,bool persist){
 if(!state||!task||state->request)return false;
 if(next==state->current&&state->ready){state->result=true;return true;}
 if(state->receiver!=Client::None||(state->current.mode==Mode::Data&&(state->tx.count||state->text)))return false;
 state->requested=next;state->persist=persist;state->result=false;state->request=true;return true;
}
}
bool requestConfigure(const Config& next,bool persist){
 if(!valid(next))return false;if(!initialized.load()&&!begin())return false;Lock guard;
 return queueConfiguration(next,persist);
}
bool tryRequestConfigure(const Config& next,bool persist){
 if(!valid(next)||!initialized.load())return false;
 Lock guard(0);
 return guard.held&&queueConfiguration(next,persist);
}
bool configure(const Config& next,bool persist){
 if(!requestConfigure(next,persist))return false;
 for(unsigned i=0;i<400;++i){{Lock guard;if(!state->request)return state->result&&state->ready&&state->current==next;}vTaskDelay(pdMS_TO_TICKS(5));}return false;
}
namespace {
Stats snapshotLocked(){Stats result;if(!state)return result;result.config=state->current;result.configuring=state->request;result.configurationSucceeded=state->result;result.receiver=state->receiver;result.sender=state->sender;result.available=state->rx.count;result.pending=state->tx.count+(state->textSize-state->textAt);result.received=state->received;result.sent=state->sent;result.dropped=state->dropped;result.ready=state->ready;result.connected=state->ready&&(state->current.port==Port::Rs485||usb_serial_jtag_is_connected());return result;}
}
Stats stats(){Lock guard;return snapshotLocked();}
bool tryStats(Stats& result){Lock guard(0);if(!guard.held)return false;result=snapshotLocked();return true;}
size_t write(const void* input,size_t n){if(!input||!n)return 0;Lock guard;if(!state||!state->ready||state->current.mode!=Mode::Data||state->text)return 0;return state->tx.push(input,n);}
bool claimReceive(Client owner){if(owner==Client::None)return false;Lock guard;if(!state||!state->ready||state->request||state->current.mode!=Mode::Data||state->receiver!=Client::None)return false;state->receiver=owner;return true;}
void releaseReceive(Client owner){Lock guard;if(state&&owner!=Client::None&&state->receiver==owner)state->receiver=Client::None;}
size_t read(void* output,size_t n,Client owner){if(!output||!n)return 0;Lock guard;return state&&state->receiver==owner?state->rx.pop(output,n):0;}
bool sendSnapshot(size_t n,const std::function<void(char*,size_t)>& fill,Client owner){
 if(!n||n>MaxSnapshot||!fill)return false;Lock guard;
 if(!state||!state->ready||state->request||state->current.mode!=Mode::Data||state->tx.count||state->text)return false;
 char* copy=static_cast<char*>(heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!copy)return false;
 fill(copy,n);state->text=copy;state->textSize=n;state->textAt=0;state->sender=owner;return true;
}
bool sendText(const char* text,size_t n){if(!text)return false;if(n==SIZE_MAX)n=strnlen(text,MaxText+1);if(n>MaxText)return false;return sendSnapshot(n,[text](char* destination,size_t count){memcpy(destination,text,count);},Client::None);}
void cancelSend(Client owner){Lock guard;if(!state||(owner!=Client::None&&state->sender!=owner))return;state->tx.clear();freeText();}
namespace {
void clearLocked(){if(!state)return;resetQueues();uint8_t discard[128];if(state->current.port==Port::Usb){for(unsigned i=0;i<32&&usb_serial_jtag_read_bytes(discard,sizeof(discard),0)>0;++i){}}else if(state->rs)uart_flush_input(RsUart);}
}
void clear(){Lock guard;clearLocked();}
bool tryClear(){Lock guard(0);if(!guard.held)return false;clearLocked();return true;}
void debugWrite(const char* p,size_t n){if(!initialized.load()||!p||!n)return;Lock guard(0);if(!guard.held||!state||!state->ready||state->current.mode!=Mode::Debug)return;state->dropped+=n-state->tx.push(p,n);}
int debugAvailable(){Lock guard(0);return guard.held&&state&&state->current.mode==Mode::Debug?int(state->rx.count):0;}
int debugRead(){Lock guard(0);uint8_t byte=0;return guard.held&&state&&state->current.mode==Mode::Debug&&state->rx.pop(&byte,1)?int(byte):-1;}
}
