#include "Tab5PythonIrc.h"
#include "Tab5CooperativeClient.h"
#include "Tab5NetworkGate.h"
#include "Tab5ResourceManager.h"
#include "Tab5SecureClient.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <cerrno>
#include <cstring>
#include <new>
#include <time.h>

extern "C" int tab5_mp_cancelled();
namespace Tab5PythonIrc {
namespace {
bool cancelled(void*) { return tab5_mp_cancelled() || !tab5NetworkEnabled(); }
class VerifiedClient : public Tab5CooperativeClient<Tab5SecureClient> {
 public:
  VerifiedClient() : Tab5CooperativeClient(cancelled) {
  }
  void connectedTimeout() {
    setTimeout(1200);
    sslclient->socket_timeout = 1200;
  }
};
VerifiedClient* client = nullptr;
bool validHost(const char* host, size_t size) {
  if (!size || size > 253 || host[0] == '.' || host[size-1] == '.') return false;
  for (size_t i=0; i<size; ++i) {
    unsigned char c=host[i];
    if (!((c>='a'&&c<='z') || (c>='A'&&c<='Z') ||
          (c>='0'&&c<='9') || c=='.' || c=='-')) return false;
  }
  return true;
}
}
void reset() {
  if (!client) return;
  client->stop();
  client->~VerifiedClient();
  heap_caps_free(client);
  client=nullptr;
}
int call(int op, int a, int b, const void* input, size_t length,
         void* output, size_t capacity) {
  if ((length&&!input) || (capacity&&!output)) return -EINVAL;
  if (op==89) { reset(); return 0; }
  if (cancelled(nullptr)) { reset(); return -ECANCELED; }
  if (op==86) {
    if (client) return -EBUSY;
    if (a<1 || a>65535 || b!=1 || !input || !validHost(static_cast<const char*>(input),length)) return -EINVAL;
    if (WiFi.status()!=WL_CONNECTED) return -5003;
    if (time(nullptr)<1700000000) return -5001;
    Tab5NetworkLease lease("IRC TLS",3000,cancelled);
    if (!lease) return cancelled(nullptr)?-ECANCELED:-5002;
    auto admission=tab5ReserveMemory(Tab5Resources::Owner::Python,Tab5Resources::tlsBudget(),"IRC TLS");
    if (!admission) return -ENOMEM;
    void* memory=heap_caps_malloc(sizeof(VerifiedClient),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (!memory) return -ENOMEM;
    client=new (memory) VerifiedClient();
    client->setHandshakeTimeout(8);
    client->setConnectionTimeout(8000);
    client->setTimeout(8000);
    char host[254];memcpy(host,input,length);host[length]=0;
    if (!client->connect(host,uint16_t(a)) || cancelled(nullptr)) {
      reset();return cancelled(nullptr)?-ECANCELED:-5007;
    }
    client->connectedTimeout();
    // Existing allocations are now reflected in the heap. An idle IRC socket
    // must not retain the exclusive handshake gate and block other OS apps.
    admission.release();
    return 0;
  }
  if (!client) return -ENOTCONN;
  if (WiFi.status()!=WL_CONNECTED) { reset();return -5003; }
  if (op==87) {
    if (length<3 || length>512) return -EMSGSIZE;
    const auto* bytes=static_cast<const uint8_t*>(input);
    if (bytes[length-2]!='\r' || bytes[length-1]!='\n' ||
        memchr(bytes,0,length) || memchr(bytes,'\r',length-2) || memchr(bytes,'\n',length-2)) return -EINVAL;
    if (client->write(bytes,length)!=length) { reset();return -EPIPE; }
    return int(length);
  }
  if (op==88) {
    if (!capacity || capacity>1024) return -EINVAL;
    int available=client->available();
    if (available>0) {
      size_t count=size_t(available);if(count>capacity)count=capacity;
      int got=client->read(static_cast<uint8_t*>(output),count);
      if (got>=0) return got;
      reset();return -EIO;
    }
    if (!client->connected()) { reset();return -ENOTCONN; }
    return 0;
  }
  return -ENOSYS;
}
}
