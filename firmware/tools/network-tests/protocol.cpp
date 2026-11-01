#include "Tab5NetworkToolsProtocol.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace Tab5NetworkToolsProtocol;
int main() {
  for (auto text : {"example.org", "host", "my-host.local", "xn--bcher-kva.example", "example.org."})
    assert(hostname(text));
  for (auto text : {"", ".", "a..b", "-host", "host-", "host-.org", "https://example.org", "a b", "a\nb", "::1", "*.org"})
    assert(!hostname(text));
  assert(hostname(std::string(63, 'a') + ".org"));
  assert(!hostname(std::string(64, 'a') + ".org"));
  assert(!hostname(std::string(254, 'a')));
  uint16_t p = 0;
  assert(port("1", p) && p == 1); assert(port("65535", p) && p == 65535);
  for (auto text : {"", "0", "65536", "-1", " 80", "80-90", "80x", "999999999"}) assert(!port(text, p));
  uint8_t nonce[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  uint8_t request[16]; echoRequest(request, 0x1234, 4, nonce);
  assert(request[0] == 8 && checksum(request, 16) == 0);
  assert(read16(request + 4) == 0x1234 && read16(request + 6) == 4);
  std::vector<uint8_t> reply(36);
  reply[0] = 0x45; write16(reply.data() + 2, 36); reply[8] = 64; reply[9] = 1;
  memcpy(reply.data() + 20, request, 16);
  reply[20] = 0; reply[22] = reply[23] = 0;
  write16(reply.data() + 22, checksum(reply.data() + 20, 16));
  uint8_t ttl = 0;
  assert(echoReply(reply.data(), reply.size(), 0x1234, 4, nonce, ttl) && ttl == 64);
  assert(!echoReply(reply.data(), reply.size(), 0x1234, 5, nonce, ttl));
  assert(!echoReply(reply.data(), reply.size(), 0x4321, 4, nonce, ttl));
  for (size_t i = 0; i < 36; ++i) assert(!echoReply(reply.data(), i, 0x1234, 4, nonce, ttl));
  for (size_t i : {size_t(0), size_t(2), size_t(6), size_t(9), size_t(20), size_t(21), size_t(22), size_t(24), size_t(28)}) {
    auto corrupt = reply; corrupt[i] ^= 0x80;
    assert(!echoReply(corrupt.data(), corrupt.size(), 0x1234, 4, nonce, ttl));
  }
  auto fragmented = reply; fragmented[6] = 0x20;
  assert(!echoReply(fragmented.data(), fragmented.size(), 0x1234, 4, nonce, ttl));
  // IPv4 options are accepted when the payload offset and total length agree.
  reply.insert(reply.begin() + 20, 4, 0); reply[0] = 0x46; write16(reply.data() + 2, 40);
  assert(echoReply(reply.data(), reply.size(), 0x1234, 4, nonce, ttl));
  puts("PASS: hostname/port validation; ICMP checksum, nonce, identity, truncation, fragmentation, IPv4 options.");
}
