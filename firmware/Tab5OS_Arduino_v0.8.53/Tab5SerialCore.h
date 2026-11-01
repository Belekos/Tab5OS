#pragma once
#include <stddef.h>
#include <stdint.h>
#include <cstring>
#include <algorithm>
namespace Tab5Serial {
enum class Port:uint8_t { Usb, Rs485 };
enum class Mode:uint8_t { Debug, Data };
enum class Frame:uint8_t { N81, E81, O81, N82 };
enum class Ending:uint8_t { None, LF, CRLF };
enum class Client:uint8_t { None, Editor, Terminal };
struct Config {uint32_t baud=115200;Port port=Port::Usb;Mode mode=Mode::Debug;Frame frame=Frame::N81;Ending ending=Ending::LF;};
inline bool valid(const Config& c){return c.baud>=1200&&c.baud<=921600&&unsigned(c.port)<=1&&unsigned(c.mode)<=1&&unsigned(c.frame)<=3&&unsigned(c.ending)<=2;}
inline bool operator==(const Config& a,const Config& b){return a.baud==b.baud&&a.port==b.port&&a.mode==b.mode&&a.frame==b.frame&&a.ending==b.ending;}
inline bool operator!=(const Config& a,const Config& b){return !(a==b);}
template<size_t N> struct Ring {
 uint8_t data[N]{};size_t head=0,count=0;
 size_t free()const{return N-count;}
 size_t push(const void* input,size_t n){n=std::min(n,free());auto* p=static_cast<const uint8_t*>(input);size_t at=(head+count)%N,first=std::min(n,N-at);memcpy(data+at,p,first);memcpy(data,p+first,n-first);count+=n;return n;}
 size_t peek(void* output,size_t n)const{n=std::min(n,count);size_t first=std::min(n,N-head);memcpy(output,data+head,first);memcpy(static_cast<uint8_t*>(output)+first,data,n-first);return n;}
 void consume(size_t n){n=std::min(n,count);head=(head+n)%N;count-=n;}
 size_t pop(void* output,size_t n){n=peek(output,n);consume(n);return n;}
 void clear(){memset(data,0,N);head=count=0;}
};
constexpr size_t BufferSize=8192,MaxText=512*1024;
constexpr size_t MaxSnapshot=2*1024*1024; // Matches the real writing document limit.
struct Stats {Config config;size_t available=0,pending=0;uint32_t received=0,sent=0,dropped=0;bool ready=false,connected=false,configuring=false,configurationSucceeded=false;Client receiver=Client::None,sender=Client::None;};
}
