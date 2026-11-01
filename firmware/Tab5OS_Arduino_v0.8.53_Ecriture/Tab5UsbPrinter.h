#pragma once
#include <Arduino.h>
#include <stdint.h>
namespace Tab5UsbPrinter {
class Response {
 uint8_t* bytes_=nullptr;size_t length_=0;
 public:
 ~Response();
 Response()=default;Response(const Response&)=delete;Response& operator=(const Response&)=delete;
 bool prepare();void clear(){length_=0;}void reserve(size_t){}
 void push_back(uint8_t b){bytes_[length_++]=b;}
 const uint8_t* data()const{return bytes_;}size_t size()const{return length_;}
};
bool begin(); // after the existing USB host installation, no second host stack
bool ready();
const char* status();
uint32_t revision();
uint32_t connectionToken();
// One exact URI routes to IPP over USB; no wireless/network lease is acquired.
bool uri(const char* value);
// Complete one HTTP transaction. An incomplete transaction quarantines this
// channel until physical reconnection; it is never automatically retransmitted.
bool post(Stream& payload,size_t bytes,Response& response,int& httpStatus,
          uint32_t expectedToken,uint32_t timeoutMs,bool(*cancel)(void*),void* context);
}
