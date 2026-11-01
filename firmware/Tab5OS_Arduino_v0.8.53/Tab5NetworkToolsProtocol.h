#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace Tab5NetworkToolsProtocol {
inline bool hostname(const std::string& name) {
  if (name.empty() || name.size() > 253) return false;
  size_t length = 0;
  for (size_t i = 0; i < name.size(); ++i) {
    const unsigned char c = name[i];
    if (c == '.') {
      if (!length || name[i - 1] == '-') return false;
      length = 0;
    } else {
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || (c == '-' && length))) return false;
      if (++length > 63) return false;
    }
  }
  return name.back() != '-' && (length != 0 || name.back() == '.');
}
inline bool port(const std::string& value, uint16_t& result) {
  if (value.empty() || value.size() > 5) return false;
  unsigned number = 0;
  for (unsigned char c : value) {
    if (c < '0' || c > '9') return false;
    number = number * 10 + c - '0';
  }
  if (!number || number > 65535) return false;
  result = static_cast<uint16_t>(number);
  return true;
}
inline uint16_t read16(const uint8_t* data) {
  return (uint16_t(data[0]) << 8) | data[1];
}
inline void write16(uint8_t* data, uint16_t value) {
  data[0] = uint8_t(value >> 8); data[1] = uint8_t(value);
}
inline uint16_t checksum(const uint8_t* data, size_t length) {
  uint32_t sum = 0;
  while (length >= 2) { sum += read16(data); data += 2; length -= 2; }
  if (length) sum += uint16_t(*data) << 8;
  while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
  return uint16_t(~sum);
}
inline void echoRequest(uint8_t (&packet)[16], uint16_t id, uint16_t sequence,
                        const uint8_t (&nonce)[8]) {
  memset(packet, 0, sizeof(packet)); packet[0] = 8;
  write16(packet + 4, id); write16(packet + 6, sequence);
  memcpy(packet + 8, nonce, sizeof(nonce));
  write16(packet + 2, checksum(packet, sizeof(packet)));
}
// lwIP raw IPv4 sockets return the IP header followed by the ICMP message.
// Reject malformed, truncated, fragmented, unrelated and corrupt packets.
inline bool echoReply(const uint8_t* packet, size_t size, uint16_t id,
                      uint16_t sequence, const uint8_t (&nonce)[8], uint8_t& ttl) {
  if (size < 36 || (packet[0] >> 4) != 4 || packet[9] != 1) return false;
  const size_t header = (packet[0] & 15) * 4;
  const size_t total = read16(packet + 2);
  if (header < 20 || total != header + 16 || total > size ||
      (read16(packet + 6) & 0xbfff)) return false;
  const uint8_t* icmp = packet + header;
  if (icmp[0] || icmp[1] || read16(icmp + 4) != id ||
      read16(icmp + 6) != sequence || memcmp(icmp + 8, nonce, 8) ||
      checksum(icmp, 16)) return false;
  ttl = packet[8];
  return true;
}
}
