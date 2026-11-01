#pragma once
#include <NetworkClientSecure.h>
#include <time.h>
#include <cerrno>

// Public Internet clients share the IDF certificate bundle. Local printers
// have a separate explicit certificate/pinning workflow.
class Tab5SecureClient : public NetworkClientSecure {
 public:
  Tab5SecureClient() {
    _use_ca_bundle = true;
    attach_ssl_certificate_bundle(sslclient.get(), true);
  }
  static bool clockReady() { return time(nullptr) >= 1700000000; }
  int connect(const char* host, uint16_t port) override {
    if(!clockReady()){errno=EINVAL;return 0;}
    return NetworkClientSecure::connect(host,port);
  }
  int connect(const char* host, uint16_t port, int32_t timeout) override {
    if(!clockReady()){errno=EINVAL;return 0;}
    return NetworkClientSecure::connect(host,port,timeout);
  }
  int connect(IPAddress ip,uint16_t port) override {
    if(!clockReady()){errno=EINVAL;return 0;}
    return NetworkClientSecure::connect(ip,port);
  }
  int connect(IPAddress ip,uint16_t port,int32_t timeout) override {
    if(!clockReady()){errno=EINVAL;return 0;}
    return NetworkClientSecure::connect(ip,port,timeout);
  }
};
