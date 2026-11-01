#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <functional>
#define fd_set FakeFdSet
#define timeval FakeTimeval
#define sockaddr FakeSockaddr
#define sockaddr_in FakeSockaddrIn
#define select fakeSelect
#define socket fakeSocket
#define connect fakeConnect
#define close fakeClose
#define fcntl fakeFcntl
#define getsockopt fakeGetSockOpt
#define sendto fakeSendTo
#define recvfrom fakeRecvFrom
#define inet_pton fakeInetPton
#define inet_ntop fakeInetNtop
#define htons fakeSwap16
#define ntohl fakeSwap32
#define AF_INET 2
#define SOCK_STREAM 1
#define SOCK_RAW 3
#define IPPROTO_TCP 6
#define IPPROTO_ICMP 1
#define F_SETFL 4
#define O_NONBLOCK 2048
#define SOL_SOCKET 1
#define SO_ERROR 4
#define INET_ADDRSTRLEN 16
#define WL_CONNECTED 3
#define LWIP_DNS_ADDRTYPE_IPV4 0
#define ERR_OK 0
#define ERR_INPROGRESS -5
#define IP_IS_V4(a) true
#define ip_addr_isany(a) ((a)->addr == 0)
#define ip_2_ip4(a) (a)
#define FD_ZERO(a) ((void)0)
#define FD_SET(a,b) ((void)0)
using socklen_t = unsigned;
using err_t = int;
struct FakeFdSet {};
struct FakeTimeval { long seconds, microseconds; };
struct FakeSockaddr { unsigned short family; };
struct FakeAddress { uint32_t s_addr = 0; };
struct FakeSockaddrIn { unsigned short sin_family = 0, sin_port = 0; FakeAddress sin_addr; };
struct ip_addr_t { uint32_t addr = 0; };
struct Text {
  std::string value;
  const char* c_str() const { return value.c_str(); }
};
struct Address { Text toString() const { return {"192.0.2.1"}; } };
struct Wifi {
  int status() const;
  Text SSID() const { return {"test"}; }
  Address localIP() const { return {}; }
  Address gatewayIP() const { return {}; }
  Address subnetMask() const { return {}; }
  Address dnsIP() const { return {}; }
};
inline Wifi WiFi;
struct Audio { int wifiRssi() const { return -50; } };
inline Audio Tab5Audio;
namespace Fake {
inline uint32_t tick = 0;
inline bool enabled = true, wifi = true, gate = true;
inline int open = 0, opened = 0, sent = 0, tcpError = 0;
inline bool tcpTimeout = false, echo = false, enqueueFails = false;
inline int dnsDelay = 10, dnsResult = ERR_INPROGRESS;
inline uint32_t cancelAt = 0;
inline std::vector<uint8_t> echoPacket;
inline std::function<void()> pendingDns;
inline uint32_t dnsAt = 0;
inline void advance(uint32_t ms) {
  tick += ms;
  if (pendingDns && tick >= dnsAt) { auto call = pendingDns; pendingDns = {}; call(); }
}
inline void reset() {
  if (pendingDns) { auto call = pendingDns; pendingDns = {}; call(); }
  tick = 0; enabled = wifi = gate = true; opened = sent = tcpError = 0;
  tcpTimeout = echo = enqueueFails = false; dnsDelay = 10; dnsResult = ERR_INPROGRESS;
  cancelAt = 0; echoPacket.clear();
}
}
inline int Wifi::status() const { return Fake::wifi ? WL_CONNECTED : 0; }
inline uint32_t millis() { return Fake::tick; }
inline void delay(uint32_t ms) { Fake::advance(ms); }
inline bool tab5NetworkEnabled() { return Fake::enabled; }
using Tab5NetworkCancel = bool (*)(void*);
struct Tab5NetworkLease {
  Tab5NetworkLease(const char*, uint32_t, Tab5NetworkCancel, void*) {}
  explicit operator bool() const { return Fake::gate; }
};
inline void esp_fill_random(void* data, size_t size) { memset(data, 0x5a, size); }
inline uint32_t esp_random() { return 0x12345678; }
inline uint16_t fakeSwap16(uint16_t n) { return uint16_t(n >> 8 | n << 8); }
inline uint32_t fakeSwap32(uint32_t n) { return ((n & 255) << 24) | ((n & 65280) << 8) | ((n >> 8) & 65280) | (n >> 24); }
inline int fakeInetPton(int, const char* text, void* output) {
  unsigned a,b,c,d; char extra;
  if (sscanf_s(text, "%u.%u.%u.%u%c", &a,&b,&c,&d,&extra,1U) != 4 || a>255 || b>255 || c>255 || d>255) return 0;
  *static_cast<uint32_t*>(output) = fakeSwap32((a << 24) | (b << 16) | (c << 8) | d);
  return 1;
}
inline const char* fakeInetNtop(int, const void* input, char* output, size_t size) {
  auto n = fakeSwap32(*static_cast<const uint32_t*>(input));
  snprintf(output,size,"%u.%u.%u.%u",n>>24,(n>>16)&255,(n>>8)&255,n&255); return output;
}
inline err_t tcpip_try_callback(void (*callback)(void*), void* arg) {
  if (Fake::enqueueFails) return -1;
  callback(arg); return ERR_OK;
}
inline err_t dns_gethostbyname_addrtype(const char*, ip_addr_t* output,
       void (*callback)(const char*, const ip_addr_t*, void*), void* arg, int) {
  output->addr = fakeSwap32(0xc0000201);
  if (Fake::dnsResult == ERR_INPROGRESS) {
    Fake::dnsAt = Fake::tick + Fake::dnsDelay;
    Fake::pendingDns = [=] { ip_addr_t a{fakeSwap32(0xc0000201)}; callback(nullptr, &a, arg); };
  }
  return Fake::dnsResult;
}
inline int fakeSocket(int, int, int) { ++Fake::open; ++Fake::opened; return 3; }
inline int fakeClose(int) { --Fake::open; return 0; }
inline int fakeFcntl(int, int, int) { return 0; }
inline int fakeConnect(int, const FakeSockaddr*, size_t) { errno=EINPROGRESS; return -1; }
inline int fakeGetSockOpt(int, int, int, void* out, socklen_t*) { *static_cast<int*>(out)=Fake::tcpError; return 0; }
inline int fakeSelect(int, FakeFdSet* read, FakeFdSet*, FakeFdSet*, FakeTimeval*) {
  Fake::advance(20);
  return read ? int(Fake::echo && !Fake::echoPacket.empty()) : int(!Fake::tcpTimeout);
}
inline int fakeSendTo(int, const void* data, size_t size, int, const FakeSockaddr*, size_t) {
  ++Fake::sent; const auto* bytes=static_cast<const uint8_t*>(data);
  Fake::echoPacket.assign(bytes,bytes+size); return int(size);
}
inline int fakeRecvFrom(int, void* output, size_t, int, FakeSockaddr* sender, socklen_t*) {
  auto* bytes=static_cast<uint8_t*>(output); memset(bytes,0,36);
  bytes[0]=0x45; bytes[3]=36; bytes[8]=64; bytes[9]=1;
  memcpy(bytes+20,Fake::echoPacket.data(),16); bytes[20]=0; bytes[22]=bytes[23]=0;
  uint32_t sum=0; for(unsigned i=20;i<36;i+=2)sum+=(bytes[i]<<8)|bytes[i+1];
  while(sum>>16)sum=(sum&65535)+(sum>>16); sum=uint16_t(~sum);
  bytes[22]=uint8_t(sum>>8); bytes[23]=uint8_t(sum);
  reinterpret_cast<FakeSockaddrIn*>(sender)->sin_addr.s_addr=fakeSwap32(0xc0000201);
  Fake::echoPacket.clear(); return 36;
}
