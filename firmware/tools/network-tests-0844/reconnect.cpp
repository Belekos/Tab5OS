#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
static uint32_t tick = 0;
uint32_t millis() { return tick++; }
void vTaskDelay(unsigned n) { tick += n; }
struct String {
  std::string value;
  String(const char* v=""):value(v){}
  const char* c_str()const{return value.c_str();}
};
struct Stream {
  unsigned long _timeout=1000,_startMillis=0;
  virtual ~Stream()=default;
  virtual int read()=0;
  void setTimeout(unsigned long t){_timeout=t;}
  unsigned long getTimeout(){return _timeout;}
  int timedRead();
};
struct Transport:Stream {
  virtual int read(uint8_t* b,size_t n){if(!n)return 0;int x=Transport::read();if(x<0)return -1;*b=x;return 1;}
  int read()override{return tick>=deliverAt?'H':-1;}
  virtual int available(){return 0;}
  virtual uint8_t connected(){return live;}
  void stop(){live=false;}
  int connect(const char*,uint16_t,int32_t timeout){connectTimeout=timeout;live=true;return 1;}
  uint32_t deliverAt=UINT32_MAX;int32_t connectTimeout=0;bool live=true;
};
#include "Tab5CooperativeClient.h"
struct HTTPClient {
  Transport* _client;
  String _host="example.org";
  uint16_t _port=443,_tcpTimeout=8000;
  int32_t _connectTimeout=8000;
  bool _reuse=true;
  bool connected();
  void setTimeout(uint16_t);
  bool connect();
};
#define log_d(...) ((void)0)
#define log_e(...) ((void)0)
// SDK_FUNCTIONS
static bool stopped(void* p){return *static_cast<bool*>(p);}
int main(){
  Tab5CooperativeClient<Transport> client;
  HTTPClient http{&client};
  bool cancel=true;
  client.setCancellation(stopped,&cancel);
  assert(client.read()==-1&&client.interrupted());
  assert(client.getTimeout()==0&&!client.live);
  client.clearCancellation();
  cancel=false;
  client.setCancellation(stopped,&cancel);
  http.setTimeout(8000); // Exact SDK does not set Stream timeout when disconnected.
  assert(client.getTimeout()==0);
  assert(http.connect()); // Exact SDK must restore it after reconnect.
  assert(client.getTimeout()==8000&&client.connectTimeout==8000);
  client.deliverAt=tick+100;
  assert(client.timedRead()=='H'); // Fragmented header succeeds after waiting.
  assert(!client.interrupted());
  client.clearCancellation();
  std::puts("PASS: actual SDK HTTP connect/setTimeout/connected + Stream timedRead restore timeout after cooperative cancel and reconnect; fragmented response header succeeds");
}
