#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include <new>
#include <sys/stat.h>
#include "Tab5Serial.h"
static unsigned checks=0;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#v);exit(1);}}while(0)
using TickType_t=unsigned;using StaticSemaphore_t=bool;using SemaphoreHandle_t=bool*;using TaskHandle_t=void*;using uart_port_t=int;using vprintf_like_t=int(*)(const char*,va_list);using ssize_t=ptrdiff_t;
#ifndef S_IFCHR
#define S_IFCHR _S_IFCHR
#endif
constexpr int UART_NUM_1=1,ESP_OK=0,pdTRUE=1,pdPASS=1,portMAX_DELAY=0xffffffff,MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,UART_DATA_8_BITS=8,UART_PARITY_EVEN=2,UART_PARITY_ODD=1,UART_PARITY_DISABLE=0,UART_STOP_BITS_1=1,UART_STOP_BITS_2=2,UART_HW_FLOWCTRL_DISABLE=0,UART_SCLK_DEFAULT=0,UART_PIN_NO_CHANGE=-1,UART_MODE_RS485_HALF_DUPLEX=1;
#define pdMS_TO_TICKS(n) (n)
static std::vector<uint8_t> emitted;static std::deque<uint8_t> incoming;static bool plugged=true,usbInstalled=false,rsInstalled=false,failSetup=false,failSave=false,failAllocation=false;static unsigned saves=0,installs=0,uninstalls=0;static int pinTx=-1,pinRx=-1,pinDe=-1;static size_t maxWrite=17;static void(*taskFn)(void*)=nullptr;static bool pumping=false;static bool holdWorker=false;static unsigned delayCalls=0,delayMs=0;
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* value){*value=false;return value;}
int xSemaphoreTake(SemaphoreHandle_t value,unsigned timeout){if(*value){CHECK(timeout==0);return 0;}*value=true;return 1;}
void xSemaphoreGive(SemaphoreHandle_t value){CHECK(*value);*value=false;}
struct Yield{};
static void pump(){CHECK(taskFn&&!pumping);pumping=true;try{taskFn(nullptr);}catch(const Yield&){}pumping=false;}
static void vTaskDelay(unsigned ticks){if(pumping)throw Yield{};++delayCalls;delayMs+=ticks;if(!holdWorker)pump();}
static int tab5CreateTask(void(*fn)(void*),const char*,unsigned stack,void*,unsigned,TaskHandle_t* result,int core){CHECK(stack==4096&&core==0);taskFn=fn;*result=reinterpret_cast<void*>(1);return 1;}
static void* heap_caps_malloc(size_t n,int){return failAllocation?nullptr:malloc(n);}static void heap_caps_free(void* p){free(p);}
static vprintf_like_t esp_log_set_vprintf(vprintf_like_t){return nullptr;}
static FILE* serial_freopen(const char* path,const char*,FILE* file){CHECK(!strcmp(path,"/dev/tab5debug"));return file;}
#define freopen serial_freopen
struct FakeSerial {size_t write(const uint8_t*,size_t n){return n;}} Serial0;
constexpr unsigned ESP_VFS_FLAG_DEFAULT=0;
struct esp_vfs_t {unsigned flags=0;ssize_t(*write)(int,const void*,size_t)=nullptr;int(*open)(const char*,int,int)=nullptr;int(*close)(int)=nullptr;int(*fstat)(int,struct stat*)=nullptr;};
static esp_vfs_t logVfs;static int esp_vfs_register(const char* path,esp_vfs_t* vfs,void*){CHECK(!strcmp(path,"/dev/tab5debug"));logVfs=*vfs;return 0;}
class Preferences {public:bool begin(const char*,bool=false){return !failSave;}size_t getBytesLength(const char*){return 0;}size_t getBytes(const char*,void*,size_t){return 0;}size_t putBytes(const char*,const void*,size_t n){++saves;return failSave?0:n;}void end(){}};
struct usb_serial_jtag_driver_config_t {size_t tx_buffer_size,rx_buffer_size;};
static int usb_serial_jtag_driver_install(usb_serial_jtag_driver_config_t* c){CHECK(pumping);CHECK(c->tx_buffer_size==2048&&c->rx_buffer_size==4096);if(failSetup)return -1;usbInstalled=true;++installs;return 0;}
static int usb_serial_jtag_driver_uninstall(){CHECK(pumping);usbInstalled=false;++uninstalls;return 0;}
static bool usb_serial_jtag_is_connected(){return plugged;}
static int receive(void* p,size_t n){n=std::min(n,incoming.size());for(size_t i=0;i<n;++i){static_cast<uint8_t*>(p)[i]=incoming.front();incoming.pop_front();}return int(n);}
static int transmit(const void* p,size_t n){n=std::min(n,maxWrite);auto* bytes=static_cast<const uint8_t*>(p);emitted.insert(emitted.end(),bytes,bytes+n);return int(n);}
static int usb_serial_jtag_read_bytes(void* p,size_t n,unsigned){return receive(p,n);}
static int usb_serial_jtag_write_bytes(const void* p,size_t n,unsigned ticks){CHECK(ticks==0);return transmit(p,n);}
struct uart_config_t {uint32_t baud_rate;int data_bits,parity,stop_bits,flow_ctrl,source_clk;};
static bool uart_is_driver_installed(int){return rsInstalled;}
static int uart_driver_install(int,int,int,int,void*,int){if(failSetup)return -1;rsInstalled=true;return 0;}
static int uart_param_config(int,uart_config_t* c){CHECK(c->baud_rate>=1200);return failSetup?-1:0;}
static int uart_set_pin(int,int tx,int rx,int de,int){pinTx=tx;pinRx=rx;pinDe=de;return 0;}
static int uart_set_mode(int,int){return 0;}
static int uart_read_bytes(int,void* p,size_t n,unsigned){return receive(p,n);}
static int uart_get_tx_buffer_free_size(int,size_t* n){*n=17;return 0;}
static int uart_write_bytes(int,const void* p,size_t n){return transmit(p,n);}
static int uart_flush_input(int){incoming.clear();return 0;}
// The runner inserts the exact production implementation, excluding only its
// SDK include block and Print wrapper instance. No service logic is replaced.
#include "serial-production.inc"

#ifdef TEST_CURRENT
#include "ui-production.inc"
#endif
int main(){
 using namespace Tab5Serial;
 CHECK(begin());CHECK(stats().ready);
 Config before=config(),next=before;next.baud=9600;next.port=Port::Rs485;
 delayCalls=delayMs=0;holdWorker=true;
#ifdef TEST_CURRENT
 CHECK(tryRequestConfigure(next));serialSettingsRequested=next;serialSettingsPending=true;
 CHECK(delayCalls==0);serialSettingsService();CHECK(serialSettingsPending&&notices.empty());
 // A worker configuring the physical driver/NVS holds this lock. The UI must
 // not enter the driver, wait, replace the caller's snapshot or claim success.
 Stats untouched;untouched.sent=123;*mutex()=true;
 CHECK(!tryStats(untouched)&&untouched.sent==123);CHECK(!tryRequestConfigure(before));CHECK(!tryClear());
 serialSettingsService();CHECK(serialSettingsPending&&notices.empty()&&delayCalls==0);
 *mutex()=false;
 holdWorker=false;pump();serialSettingsService();
 CHECK(!serialSettingsPending&&config()==next&&notices.back()=="Réglages série enregistrés.");
 CHECK(delayCalls==0&&saves==1);serialSettingsService();CHECK(notices.size()==1);
 // No-op accepted without another NVS write; still acknowledged from state.
 CHECK(tryRequestConfigure(next));serialSettingsRequested=next;serialSettingsPending=true;serialSettingsService();CHECK(saves==1&&notices.size()==2);
 // Save failure leaves the previous config and never reports success.
 before=next;next.baud=19200;failSave=true;
 CHECK(tryRequestConfigure(next));serialSettingsRequested=next;serialSettingsPending=true;
 serialSettingsService();CHECK(serialSettingsPending);pump();serialSettingsService();
 CHECK(!serialSettingsPending&&config()==before&&notices.back()=="Série : réglage impossible ou envoi en cours.");failSave=false;
 CHECK(claimReceive(Client::Editor)==false); // Debug mode has no data receiver.
 before.mode=Mode::Data;CHECK(configure(before));CHECK(claimReceive(Client::Editor));
 CHECK(!tryRequestConfigure(next));releaseReceive(Client::Editor);
 // Pending send remains protected, just like the synchronous API.
 CHECK(write("abc",3)==3);CHECK(!tryRequestConfigure(next));pump();
 CHECK(tryRequestConfigure(next));pump();CHECK(config()==next);
 // Previously a failed NVS write followed by successful fallback with the
 // same configuration was indistinguishable from a successful write.
 state->ready=false;failSave=true;
 CHECK(tryRequestConfigure(next));serialSettingsRequested=next;serialSettingsPending=true;
 pump();CHECK(stats().ready&&stats().config==next&&!stats().configurationSucceeded);
 serialSettingsService();CHECK(!serialSettingsPending&&notices.back()=="Série : réglage impossible ou envoi en cours.");
 failSave=false;CHECK(tryRequestConfigure(next));CHECK(stats().configurationSucceeded);
 CHECK(tryClear());
 Config invalid=next;invalid.baud=0;CHECK(!tryRequestConfigure(invalid));
 printf("PASS serial current checks=%u ui_wait_ms=0 pending_ack=1 failure_preserved=1\n",checks);
#else
 CHECK(!configure(next));CHECK(delayCalls==400&&delayMs==2000);CHECK(stats().configuring&&config()==before);
 holdWorker=false;pump();CHECK(config()==next&&saves==1);
 const unsigned callbackWaitMs=delayMs;state->ready=false;failSave=true;CHECK(configure(next)); // baseline falsely succeeds
 printf("PASS serial baseline checks=%u ui_wait_ms=%u delayed_success_after_reported_failure=1 nvs_failure_falsely_succeeds=1\n",checks,callbackWaitMs);
#endif
 return 0;
}
