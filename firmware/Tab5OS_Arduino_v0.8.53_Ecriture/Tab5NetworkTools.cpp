#include "Tab5NetworkTools.h"
#include "Tab5NetworkToolsProtocol.h"
#include "Tab5NetworkArgs.h"
#include "Tab5NetworkHttp.h"
#include "Tab5NetworkGate.h"
#include "Tab5Audio.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_random.h>
#include <lwip/dns.h>
#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <cerrno>
#include <new>

namespace Tab5NetworkTools {
namespace {
constexpr uint32_t ResolveTimeout = 5000, ConnectTimeout = 1500;
constexpr uint32_t EchoTimeout = 1000, EchoInterval = 250;
constexpr unsigned EchoCount = 4;
struct Socket {
  int fd = -1;
  ~Socket() { if (fd >= 0) ::close(fd); }
  bool open(int type, int protocol) {
    fd = ::socket(AF_INET, type, protocol);
    return fd >= 0 && fcntl(fd, F_SETFL, O_NONBLOCK) == 0;
  }
};
struct Session {
  Output output, errors; Cancel cancel; Translate translate;
  bool stopped() const { return (cancel && cancel()) || !tab5NetworkEnabled(); }
  std::string tr(const char* text) const { return translate ? translate(text) : text; }
  void print(const std::string& text) const { output(text.data(), text.size()); }
  void line(const char* text) const { print(tr(text) + "\n"); }
  void diagnostic(const std::string& text)const{errors(text.data(),text.size());}
  int fail(const char* text) const { diagnostic(tr(text)+"\n"); return stopped() ? 130 : 1; }
};
// lwIP can finish a DNS request after the user cancels. Keep its own reference
// until the callback fires; no pointer into the terminal context is retained.
// A pending timed-out lookup occupies the only resolver slot until completion.
struct DnsJob {
  std::atomic<unsigned> references{2};
  std::atomic<bool> done{false}, abandoned{false};
  bool success = false;
  char name[254]{};
  ip_addr_t address{};
};
std::atomic<bool> dnsPending{false};
void release(DnsJob* job) { if (job->references.fetch_sub(1) == 1) delete job; }
void dnsComplete(const char*, const ip_addr_t* address, void* context) {
  auto* job = static_cast<DnsJob*>(context);
  if (address && IP_IS_V4(address) && !ip_addr_isany(address)) {
    job->address = *address; job->success = true;
  }
  job->done.store(true, std::memory_order_release);
  dnsPending.store(false, std::memory_order_release);
  release(job);
}
void dnsBegin(void* context) {
  auto* job = static_cast<DnsJob*>(context);
  if (job->abandoned.load()) { dnsComplete(nullptr, nullptr, job); return; }
  ip_addr_t address{};
  const err_t error = dns_gethostbyname_addrtype(job->name, &address, dnsComplete,
                                                job, LWIP_DNS_ADDRTYPE_IPV4);
  if (error == ERR_OK) dnsComplete(nullptr, &address, job);
  else if (error != ERR_INPROGRESS) dnsComplete(nullptr, nullptr, job);
}
int resolve(const std::string& name, sockaddr_in& address, const Session& session) {
  address = {}; address.sin_family = AF_INET;
  if (inet_pton(AF_INET, name.c_str(), &address.sin_addr) == 1) {
    const uint32_t host = ntohl(address.sin_addr.s_addr);
    if (!host || host == 0xffffffffU || (host >> 28) == 14)
      return session.fail("Utilisez l’adresse d’un seul appareil.");
    return 0;
  }
  if (!Tab5NetworkToolsProtocol::hostname(name))
    return session.fail("Nom d’hôte invalide. Utilisez une adresse IPv4 ou un nom DNS.");
  bool expected = false;
  if (!dnsPending.compare_exchange_strong(expected, true))
    return session.fail("Une résolution DNS se termine. Réessayez dans un instant.");
  auto* job = new (std::nothrow) DnsJob;
  if (!job) { dnsPending = false; return session.fail("Mémoire insuffisante."); }
  memcpy(job->name, name.c_str(), name.size() + 1);
  if (tcpip_try_callback(dnsBegin, job) != ERR_OK) {
    dnsPending = false; delete job;
    return session.fail("Résolution DNS indisponible.");
  }
  const uint32_t started = millis();
  while (!job->done.load(std::memory_order_acquire) && !session.stopped() &&
         millis() - started < ResolveTimeout) delay(10);
  const bool done = job->done.load(std::memory_order_acquire);
  const bool success = done && job->success;
  if (success) address.sin_addr.s_addr = ip_2_ip4(&job->address)->addr;
  job->abandoned = true;
  release(job);
  if (session.stopped()) return 130;
  if (!done) return session.fail("Délai DNS dépassé.");
  if (!success) return session.fail("Nom d’hôte introuvable.");
  const uint32_t host = ntohl(address.sin_addr.s_addr);
  if (!host || host == 0xffffffffU || (host >> 28) == 14)
    return session.fail("Utilisez l’adresse d’un seul appareil.");
  return 0;
}
std::string numeric(const sockaddr_in& address) {
  char text[INET_ADDRSTRLEN]{};
  inet_ntop(AF_INET, &address.sin_addr, text, sizeof(text));
  return text;
}
int ready(int fd, bool write, uint32_t started, uint32_t timeout, const Session& session) {
  while (!session.stopped() && millis() - started < timeout) {
    fd_set selected; FD_ZERO(&selected); FD_SET(fd, &selected);
    timeval wait{0, 20000};
    const int result = select(fd + 1, write ? nullptr : &selected,
                              write ? &selected : nullptr, nullptr, &wait);
    if (result > 0) return 1;
    if (result < 0 && errno != EINTR) return -1;
    delay(1);
  }
  return session.stopped() ? -2 : 0;
}
int tcp(sockaddr_in address, uint16_t port, const Session& session, unsigned timeout) {
  Socket socket;
  if (!socket.open(SOCK_STREAM, IPPROTO_TCP))
    return session.fail("Connexion TCP indisponible.");
  address.sin_port = htons(port);
  const uint32_t started = millis();
  int error = 0;
  if (::connect(socket.fd, reinterpret_cast<sockaddr*>(&address), sizeof(address))) {
    error = errno;
    if (error == EINPROGRESS || error == EWOULDBLOCK || error == EALREADY) {
      const int state = ready(socket.fd, true, started, timeout, session);
      if (state == -2) return 130;
      if (!state) { session.diagnostic(numeric(address)+":"+std::to_string(port)+" - "); return session.fail("Délai TCP dépassé : aucun résultat confirmé."); }
      if (state < 0) return session.fail("Connexion TCP impossible.");
      socklen_t length = sizeof(error);
      if (getsockopt(socket.fd, SOL_SOCKET, SO_ERROR, &error, &length))
        return session.fail("Connexion TCP impossible.");
    }
  }
  if (session.stopped()) return 130;
  const std::string prefix=numeric(address)+":"+std::to_string(port)+" - ";
  if (!error) {
    session.print(prefix);
    session.print(session.tr("Port ouvert") + " (" +
                   std::to_string(millis() - started) + " ms)\n");
    return 0;
  }
  session.diagnostic(prefix);
  return session.fail(error == ECONNREFUSED ? "Connexion refusée." : "Connexion TCP impossible.");
}
int ping(const sockaddr_in& address, const Session& session, unsigned count, unsigned timeout) {
  Socket socket;
  if (!socket.open(SOCK_RAW, IPPROTO_ICMP))
    return session.fail("Test ICMP indisponible.");
  uint8_t nonce[8]; esp_fill_random(nonce, sizeof(nonce));
  const uint16_t id = uint16_t(esp_random());
  unsigned received = 0, sent = 0;
  session.print("PING " + numeric(address) + "\n");
  for (unsigned sequence = 1; sequence <= count; ++sequence) {
    if (session.stopped()) return 130;
    uint8_t packet[16];
    Tab5NetworkToolsProtocol::echoRequest(packet, id, uint16_t(sequence), nonce);
    const uint32_t started = millis();
    if (sendto(socket.fd, packet, sizeof(packet), 0,
                 reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != sizeof(packet))
      return session.fail("Envoi ICMP impossible.");
    ++sent;
    bool reply = false;
    while (!session.stopped() && millis() - started < timeout) {
      const int state = ready(socket.fd, false, started, timeout, session);
      if (state == -2) return 130;
      if (state < 0) return session.fail("Réception ICMP impossible.");
      if (!state) break;
      uint8_t buffer[128]; sockaddr_in sender{}; socklen_t length = sizeof(sender);
      const int bytes = recvfrom(socket.fd, buffer, sizeof(buffer), 0,
                                  reinterpret_cast<sockaddr*>(&sender), &length);
      uint8_t ttl = 0;
      if (bytes > 0 && sender.sin_addr.s_addr == address.sin_addr.s_addr &&
          Tab5NetworkToolsProtocol::echoReply(buffer, bytes, id, uint16_t(sequence), nonce, ttl)) {
        session.print(std::to_string(sequence) + ": " +
                        std::to_string(millis() - started) + " ms, TTL=" +
                        std::to_string(ttl) + "\n");
        ++received; reply = true; break;
      }
      delay(1);
    }
    if (session.stopped()) return 130;
    if (!reply) session.print(std::to_string(sequence) + ": " + session.tr("Sans réponse") + "\n");
    while (sequence < count && millis() - started < EchoInterval && !session.stopped()) delay(10);
  }
  session.print(session.tr("Réponses : ") + std::to_string(received) + "/" + std::to_string(sent) + "\n");
  if (!received) session.line("Un appareil peut ignorer le ping tout en restant accessible.");
  return received ? 0 : 1;
}
bool cancelLease(void* context) { return static_cast<Session*>(context)->stopped(); }
}

int run(const std::vector<std::string>& args, Output output, Cancel cancel, Translate translate, const std::string& cwd, Output errors) {
  Tab5NetworkArgs::Options options;
  if (!Tab5NetworkArgs::parse(args,options)) return -1;
  Session session{output, errors?errors:output, cancel, translate};
  if (!options.error.empty()) {
    session.diagnostic(session.tr(options.error.c_str())+"\n");
    session.diagnostic("help " + args[0] + "\n"); return 2;
  }
  if (cancel && cancel()) return 130;
  if (!tab5NetworkEnabled() || WiFi.status() != WL_CONNECTED)
    return session.fail("Wi-Fi déconnecté.");
  const auto& command=options.command;
  if (command == "netinfo") {
    session.print("SSID: " + std::string(WiFi.SSID().c_str()) + "\nIPv4: " +
      WiFi.localIP().toString().c_str() + "\n" + session.tr("Masque : ") +
      WiFi.subnetMask().toString().c_str() + "\n" + session.tr("Passerelle : ") +
      WiFi.gatewayIP().toString().c_str() + "\nDNS: " + WiFi.dnsIP().toString().c_str() + "\n");
    const int signal = Tab5Audio.wifiRssi();
    if (signal < 0) session.print("RSSI: " + std::to_string(signal) + " dBm\n");
    return 0;
  }
  Tab5NetworkLease lease("network-tools", 500, cancelLease, &session);
  if (session.stopped()) return 130;
  if (!lease) return session.fail("Réseau occupé. Réessayez après le transfert en cours.");
  if(command=="wget"||command=="curl"||command=="headers")return http(options,output,cancel,translate,cwd,errors);
  sockaddr_in address{};
  const int resolved = resolve(options.host, address, session);
  if (resolved) return resolved;
  if (command == "dns") { session.print(options.host + " = " + numeric(address) + "\n"); return 0; }
  if(command=="ping")return ping(address, session,options.count,options.timeoutMs);
  if(command=="tcp")return tcp(address,uint16_t(options.ports[0]),session,options.timeoutMs);
  session.print(options.host+" ("+numeric(address)+") — "+std::to_string(options.ports.size())+" "+session.tr("ports à tester")+"\n");
  unsigned opened=0, tested=0;
  for(unsigned port:options.ports){
    if(session.stopped())return 130;
    int rc=tcp(address,uint16_t(port),session,options.timeoutMs);
    if(rc==130)return rc;if(!rc)++opened;++tested;delay(1);
  }
  session.print(std::to_string(tested)+" "+session.tr("ports testés ; ouverts : ")+std::to_string(opened)+"\n");
  session.line("Un délai dépassé ne distingue pas un filtrage d'un appareil inaccessible.");
  return 0;
}
}
