#include "Tab5RadioBodyReader.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); std::exit(1); } } while (0)

struct Client {
  std::string data;
  size_t position = 0, visible = 0, maximumRead = 65536;
  unsigned reads = 0, availableCalls = 0;
  bool transient = false;
  int available() { ++availableCalls; return static_cast<int>(visible - position); }
  int read(uint8_t* output, size_t length) {
    ++reads;
    if (transient) { transient = false; return -1; }
    length = std::min(length, std::min(maximumRead, visible - position));
    if (!length) return -1;
    std::memcpy(output, data.data() + position, length);
    position += length;
    return static_cast<int>(length);
  }
  int read() { uint8_t byte = 0; return read(&byte, 1) == 1 ? byte : -1; }
};

static std::string decode(const std::string& wire, size_t fragmentation, size_t callerSize,
                          size_t maximumRead = 65536, bool chunked = true) {
  Client client{wire}; client.maximumRead = maximumRead;
  Tab5RadioBodyReader reader; reader.reset(chunked);
  std::string decoded;
  uint8_t output[64];
  for (size_t visible = 0;; visible = std::min(wire.size(), visible + fragmentation)) {
    client.visible = visible;
    for (;;) {
      std::memset(output, 0xA5, sizeof(output));
      const uint32_t n = reader.read(client, output, static_cast<uint32_t>(callerSize));
      CHECK(n <= callerSize && output[callerSize] == 0xA5);
      decoded.append(reinterpret_cast<char*>(output), n);
      CHECK(!reader.failed());
      if (!n) break;
    }
    const auto calls = client.reads;
    CHECK(reader.read(client, output, static_cast<uint32_t>(callerSize)) == 0);
    CHECK(client.reads == calls);
    if (visible == wire.size()) break;
  }
  reader.endInput();
  CHECK(reader.finished() && !reader.failed());
  CHECK(reader.read(client, output, sizeof(output)) == 0);
  return decoded;
}

static void invalid(const std::string& wire) {
  for (size_t split = 1; split <= 5; ++split) {
    Client client{wire}; Tab5RadioBodyReader reader; reader.reset(true);
    uint8_t output[7];
    for (size_t visible = 0; visible < wire.size() && !reader.failed();) {
      visible = std::min(wire.size(), visible + split); client.visible = visible;
      while (reader.read(client, output, sizeof(output))) {}
    }
    reader.endInput();
    CHECK(reader.failed() && !reader.finished());
    const size_t consumed = client.position;
    CHECK(reader.read(client, output, sizeof(output)) == 0 && client.position == consumed);
  }
}

int main() {
  static_assert(sizeof(Tab5RadioBodyReader) <= 32, "Framing state must remain small and allocation-free");
  const std::string bytes("\x00\xFF\x03\r\n\x80", 6);
  const std::string wire = "4;foo=bar\r\nWiki\r\n5 ; baz=\"x y\"\r\npedia\r\n6\r\n" + bytes +
      "\r\n0\r\nX-Checksum: abc\r\nOther: def\r\n\r\nUNRELATED";
  for (size_t fragment = 1; fragment <= 31; ++fragment)
    for (size_t caller = 1; caller <= 31; caller += 5)
      for (size_t shortRead : {size_t(1), size_t(3), size_t(65536)})
        CHECK(decode(wire, fragment, caller, shortRead) == "Wikipedia" + bytes);
  for (size_t fragment = 1; fragment <= 20; ++fragment)
    CHECK(decode(bytes, fragment, 9, 2, false) == bytes);
  CHECK(decode("0\r\n\r\n", 1, 1).empty());
  CHECK(decode("000A\r\n0123456789\r\n0;end=yes\r\n\r\n", 3, 5) == "0123456789");
  for (const std::string bad : {"G\r\nx\r\n0\r\n\r\n", "\r\n", " 1\r\nx\r\n0\r\n\r\n",
       "1x\r\nx\r\n0\r\n\r\n", "1 2\r\nx\r\n0\r\n\r\n", "100000000\r\n",
       "FFFFFFFFF\r\n", "1\nx\r\n0\r\n\r\n", "1\r\nxX\n0\r\n\r\n",
       "1\r\nx\rX0\r\n\r\n", "0\r\nX: y\n\r\n", "0\r\n\rX", "1;bad\n\r\n",
       "1\r\n", "1\r\nx", "1\r\nx\r", "1\r\nx\r\n", "0\r\n", "0\r\nX:y\r\n"}) invalid(bad);
  invalid("1;" + std::string(513, 'a') + "\r\nx\r\n0\r\n\r\n");
  invalid("0\r\nX:" + std::string(513, 'a') + "\r\n\r\n");
  std::string trailers = "0\r\n";
  for (unsigned i = 0; i < 500; ++i) trailers += "Long-Trailer: abcdef\r\n";
  invalid(trailers + "\r\n");
  Client transient{"1\r\nx\r\n0\r\n\r\n"}; transient.visible = transient.data.size(); transient.transient = true;
  Tab5RadioBodyReader reader; reader.reset(true); uint8_t output[8] = {};
  CHECK(reader.read(transient, output, sizeof(output)) == 0 && !reader.failed());
  CHECK(reader.read(transient, output, sizeof(output)) == 1 && output[0] == 'x' && reader.finished());
  reader.reset(true); Client noData{""};
  CHECK(reader.read(noData, output, sizeof(output)) == 0 && noData.availableCalls == 1 && noData.reads == 0);
  CHECK(reader.read(noData, nullptr, sizeof(output)) == 0);
  CHECK(reader.read(noData, output, 0) == 0);
  reader.endInput(); CHECK(reader.failed());
  reader.reset(false); CHECK(!reader.failed()); reader.endInput(); CHECK(reader.finished());
  std::printf("RADIO HTTP BODY PASS checks=%u\n", checks);
}
