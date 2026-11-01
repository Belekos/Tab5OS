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
static std::vector<uint8_t> emitted;static std::deque<uint8_t> incoming;static bool plugged=true,usbInstalled=false,rsInstalled=false,failSetup=false,failSave=false,failAllocation=false;static unsigned saves=0,installs=0,uninstalls=0;static int pinTx=-1,pinRx=-1,pinDe=-1;static size_t maxWrite=17;static void(*taskFn)(void*)=nullptr;static bool pumping=false;
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* value){*value=false;return value;}
int xSemaphoreTake(SemaphoreHandle_t value,unsigned timeout){if(*value){CHECK(timeout==0);return 0;}*value=true;return 1;}
void xSemaphoreGive(SemaphoreHandle_t value){CHECK(*value);*value=false;}
struct Yield{};
static void pump(){CHECK(taskFn&&!pumping);pumping=true;try{taskFn(nullptr);}catch(const Yield&){}pumping=false;}
static void vTaskDelay(unsigned){if(pumping)throw Yield{};pump();}
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
int main(){using namespace Tab5Serial;
 CHECK(begin());CHECK(stats().ready&&stats().connected);CHECK(installs==1&&uninstalls==0);
 CHECK(logVfs.write&&logVfs.open&&logVfs.close&&logVfs.fstat);CHECK(logVfs.write(0,"stdio",5)==5);pump();CHECK(std::string(emitted.begin(),emitted.end())=="stdio");emitted.clear();
 incoming.push_back('m');pump();CHECK(debugAvailable()==1&&debugRead()=='m');CHECK(debugRead()==-1);
 Config c=config();CHECK(c.mode==Mode::Debug);CHECK(!write("x",1));
 plugged=false;std::string large(20000,'d');debugWrite(large.data(),large.size());CHECK(stats().pending==BufferSize&&stats().dropped==large.size()-BufferSize);
 c.mode=Mode::Data;CHECK(configure(c));CHECK(stats().pending==0&&uninstalls==1&&installs==2);CHECK(!stats().connected);CHECK(saves==1);CHECK(configure(c));CHECK(saves==1);
 debugWrite("SECRET LOG",10);CHECK(stats().pending==0);
 CHECK(logVfs.write(0,"SDK LOG",7)==7);CHECK(stats().pending==0);
 incoming.push_back('x');pump();CHECK(debugAvailable()==0&&debugRead()==-1&&stats().available==1);char untouched;CHECK(read(&untouched,1)==1&&untouched=='x');
 CHECK(write("abcdef",6)==6);CHECK(!configure(Config{}));for(int i=0;i<10;++i)pump();CHECK(emitted.empty());plugged=true;pump();CHECK(std::string(emitted.begin(),emitted.end())=="abcdef");CHECK(stats().pending==0);
 clear();emitted.clear();std::string text(512*1024,'T');text[0]='a';text.back()='z';CHECK(sendText(text.c_str()));CHECK(!sendText("busy"));CHECK(!write("bad",3));CHECK(!configure(Config{}));for(size_t i=0;i<text.size()/17+5;++i)pump();CHECK(stats().pending==0&&emitted.size()==text.size()&&!memcmp(emitted.data(),text.data(),text.size()));
 text+='!';CHECK(!sendText(text.c_str()));CHECK(!sendText(""));failAllocation=true;CHECK(!sendText("oom"));failAllocation=false;
 incoming.clear();for(unsigned i=0;i<12000;++i)incoming.push_back(uint8_t(i));uint32_t before=stats().dropped;for(int i=0;i<30;++i)pump();CHECK(stats().available==8192&&stats().dropped-before==12000-8192);uint8_t received[8192];CHECK(read(received,sizeof(received))==sizeof(received));for(unsigned i=0;i<8192;++i)CHECK(received[i]==uint8_t(i));
 c.port=Port::Rs485;c.baud=9600;c.frame=Frame::E81;CHECK(configure(c));CHECK(pinTx==20&&pinRx==21&&pinDe==34);CHECK(stats().config.baud==9600);CHECK(write("RS485",5)==5);pump();CHECK(stats().pending==0);
 Config invalid=c;invalid.baud=0;CHECK(!configure(invalid));CHECK(config()==c);invalid=c;invalid.frame=Frame(99);CHECK(!configure(invalid));
 failSave=true;Config next=c;next.baud=115200;CHECK(!configure(next));CHECK(config()==c);failSave=false;
 clear();CHECK(stats().available==0&&stats().pending==0);
 std::string output;auto out=[&](const char* p,size_t n){output.append(p,n);};auto tr=[](const char* p){return std::string(p);};auto cancel=[](){return false;};
 CHECK(command({"other"},out,cancel,tr)==-1);CHECK(command({"serial"},out,cancel,tr)==0);CHECK(output.find("RS485")!=std::string::npos);
 for(const auto& bad:std::vector<std::vector<std::string>>{{"serial","port","invalid"},{"serial","baud","0"},{"serial","baud","1200x"},{"serial","frame","9N1"},{"serial","ending","wrong"},{"serial","read","-1"},{"serial","listen","3600001"}})CHECK(command(bad,out,cancel,tr)==2);
 CHECK(command({"serial","ending","crlf"},out,cancel,tr)==0);emitted.clear();CHECK(command({"serial","send","Bonjour","été"},out,cancel,tr)==0);while(stats().pending)pump();CHECK(std::string(emitted.begin(),emitted.end())=="Bonjour été\r\n");
 std::string binary("a\0b\xff",4);size_t inputAt=0;auto input=[&](char* p,size_t n){n=std::min(n,binary.size()-inputAt);memcpy(p,binary.data()+inputAt,n);inputAt+=n;return n;};emitted.clear();CHECK(command({"serial","write"},out,cancel,tr,input)==0);while(stats().pending)pump();CHECK(emitted.size()==4&&!memcmp(emitted.data(),binary.data(),4));
 for(unsigned i=0;i<2048;++i)incoming.push_back(uint8_t(i));for(int i=0;i<4;++i)pump();output.clear();CHECK(command({"serial","read","2048","0"},out,cancel,tr)==0);CHECK(output.size()==2048);for(unsigned i=0;i<2048;++i)CHECK(uint8_t(output[i])==uint8_t(i));
 CHECK(command({"serial","listen"},out,[]{return true;},tr)==130);

 // Explicit ownership: a terminal reader cannot steal an editor's packet,
 // cancellation never erases RX, and a large send is one immutable snapshot.
 clear();emitted.clear();
 CHECK(claimReceive(Client::Editor));CHECK(!claimReceive(Client::Terminal));CHECK(!claimReceive(Client::Editor));
 incoming.push_back('R');incoming.push_back('X');pump();
 char packet[8]{};CHECK(read(packet,8)==0);CHECK(read(packet,8,Client::Terminal)==0);
 CHECK(command({"serial","read","2","0"},out,cancel,tr)==1);
 next=config();next.baud=19200;CHECK(!requestConfigure(next));CHECK(!configure(next));
 CHECK(sendSnapshot(2,[](char* dst,size_t n){CHECK(n==2);memcpy(dst,"AB",n);},Client::Editor));
 CHECK(stats().sender==Client::Editor&&stats().pending==2);
 cancelSend(Client::Terminal);CHECK(stats().pending==2);cancelSend(Client::Editor);CHECK(stats().pending==0);
 CHECK(read(packet,8,Client::Editor)==2&&!memcmp(packet,"RX",2));
 releaseReceive(Client::Terminal);CHECK(stats().receiver==Client::Editor);releaseReceive(Client::Editor);CHECK(stats().receiver==Client::None);
 CHECK(command({"serial","listen"},out,[]{return true;},tr)==130);CHECK(stats().receiver==Client::None);
 CHECK(requestConfigure(next));CHECK(stats().configuring);CHECK(config().baud!=19200);pump();CHECK(!stats().configuring&&config().baud==19200);
 CHECK(!sendSnapshot(MaxSnapshot+1,[](char*,size_t){CHECK(false);},Client::Editor));
 failAllocation=true;CHECK(!sendSnapshot(4,[](char*,size_t){CHECK(false);},Client::Editor));failAllocation=false;
 unsigned snapshots=0;CHECK(sendSnapshot(MaxSnapshot,[&](char* dst,size_t n){++snapshots;memset(dst,'Q',n);dst[0]='A';dst[n-1]='Z';},Client::Editor));
 CHECK(snapshots==1&&stats().pending==MaxSnapshot);size_t oldWrite=maxWrite;maxWrite=512;
 while(stats().pending)pump();CHECK(snapshots==1&&emitted.size()==MaxSnapshot&&emitted.front()=='A'&&emitted.back()=='Z');maxWrite=oldWrite;
 CHECK(stats().sender==Client::None);
 for(unsigned cycle=0;cycle<500;++cycle){CHECK(claimReceive(Client::Editor));incoming.push_back('a');pump();CHECK(sendSnapshot(5,[](char* dst,size_t n){memset(dst,'s',n);},Client::Editor));cancelSend(Client::Editor);CHECK(read(packet,8,Client::Editor)==1&&packet[0]=='a');releaseReceive(Client::Editor);CHECK(stats().pending==0&&stats().receiver==Client::None);}
 // Random wraparound/partial-consumption against an independent FIFO oracle.
 Ring<31> ring;std::deque<uint8_t> oracle;unsigned seed=123;
 for(unsigned round=0;round<10000;++round){seed=seed*1664525+1013904223;uint8_t data[53];for(unsigned i=0;i<53;++i)data[i]=uint8_t(seed+i);size_t n=seed%53;if(seed&1){size_t expected=std::min(n,size_t(31-oracle.size()));CHECK(ring.push(data,n)==expected);for(size_t i=0;i<expected;++i)oracle.push_back(data[i]);}else {size_t expected=std::min(n,oracle.size());CHECK(ring.pop(data,n)==expected);for(size_t i=0;i<expected;++i){CHECK(data[i]==oracle.front());oracle.pop_front();}}CHECK(ring.count==oracle.size());}
 printf("PASS serial backend %u checks\n",checks);return 0;
}
