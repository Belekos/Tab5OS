#include "Tab5RadioCodec.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static unsigned checks;
#define CHECK(expression) do { ++checks; if (!(expression)) { \
  std::fprintf(stderr, "Codec check failed at line %d: %s\n", __LINE__, #expression); \
  std::exit(1); } } while (false)
using Codec = Tab5RadioCodec;

int main() {
  for (const char* value : {"MP3", "mp3", "audio/mpeg", "Audio/MP3; charset=binary",
                            " audio/MPEG \t; anything=yes", "audio/x-mpeg-3"})
    CHECK(tab5RadioCodecHint(value) == Codec::Mp3);
  for (const char* value : {"AAC", "AAC+", "HE-AAC", "he-aacv2", "audio/aac",
                            "audio/aacp", "audio/x-aac; codecs=mp4a.40.5"})
    CHECK(tab5RadioCodecHint(value) == Codec::Aac);
  for (const char* value : {"text/html", "application/json", "audio/ogg", "audio/opus",
                            "audio/flac", "audio/wav", "audio/L16", "audio/mp4",
                            "application/vnd.apple.mpegurl", "audio/x-scpls"})
    CHECK(tab5RadioCodecHint(value) == Codec::Unsupported);
  for (const char* value : {"", "application/octet-stream", "text/plain", "aacWhatever"})
    CHECK(tab5RadioCodecHint(value) == Codec::Unknown);
  CHECK(tab5RadioCodecHint(nullptr) == Codec::Unknown);
  CHECK(tab5RadioCodecFromUrl(nullptr) == Codec::Unknown);
  CHECK(tab5RadioCodecFromUrl("https://radio.test/live.AAC?token=value#play") == Codec::Aac);
  CHECK(tab5RadioCodecFromUrl("http://radio.test/file.mp3?format=.aac") == Codec::Mp3);
  CHECK(tab5RadioCodecFromUrl("https://radio.test/opaque?filename=stream.aac") == Codec::Unknown);
  CHECK(tab5RadioCodecFromUrl("https://radio.test/old.aac/live") == Codec::Unknown);
  CHECK(tab5RadioCodecFromUrl("https://radio.aac") == Codec::Unknown);
  CHECK(tab5RadioCodecFromUrl("relative/STREAM.MP3") == Codec::Mp3);
  CHECK(tab5RadioCodecFromUrl("https://radio.test/live.aacp") == Codec::Unknown);

  const uint8_t adts[] = {0xff, 0xf1, 0x50, 0x80, 0x03, 0x7f, 0xfc};
  const uint8_t mp3[] = {0xff, 0xfb, 0x90, 0x64};
  const uint8_t html[] = {'<','h','t','m','l','>'};
  CHECK(tab5RadioProbeCodec(adts, sizeof(adts)) == Codec::Aac);
  CHECK(tab5RadioProbeCodec(mp3, sizeof(mp3)) == Codec::Mp3);
  CHECK(tab5RadioProbeCodec(html, sizeof(html)) == Codec::Unknown);
  for (const char* prefix : {"OggS", "fLaC", "RIFFxxxxWAVE", "xxxxftypisom",
                             "#EXTM3U\n", "[playlist]"}) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(prefix);
    CHECK(tab5RadioProbeCodec(bytes, std::strlen(prefix)) == Codec::Unsupported);
    CHECK(tab5RadioSelectCodec(bytes, std::strlen(prefix), Codec::Unknown, Codec::Unknown,
                              "http://radio.test/opaque") == Codec::Unsupported);
  }
  CHECK(tab5RadioProbeCodec(nullptr, 20) == Codec::Unknown);
  for (size_t length = 0; length < sizeof(adts); ++length) {
    const std::vector<uint8_t> fragment(adts, adts + length);
    CHECK(tab5RadioProbeCodec(fragment.data(), fragment.size()) == Codec::Unknown);
  }
  for (size_t length = 0; length < sizeof(mp3); ++length) {
    const std::vector<uint8_t> fragment(mp3, mp3 + length);
    CHECK(tab5RadioProbeCodec(fragment.data(), fragment.size()) == Codec::Unknown);
  }
  uint8_t malformed[sizeof(adts)];
  std::memcpy(malformed, adts, sizeof(adts)); malformed[2] = 0x74; // reserved frequency
  CHECK(tab5RadioProbeCodec(malformed, sizeof(malformed)) == Codec::Unknown);
  std::memcpy(malformed, adts, sizeof(adts)); malformed[3] = 0; // no channel configuration
  CHECK(tab5RadioProbeCodec(malformed, sizeof(malformed)) == Codec::Unknown);
  std::memcpy(malformed, adts, sizeof(adts)); malformed[4] = 0; malformed[5] = 0;
  CHECK(tab5RadioProbeCodec(malformed, sizeof(malformed)) == Codec::Unknown);
  uint8_t invalidMp3[sizeof(mp3)];
  std::memcpy(invalidMp3, mp3, sizeof(mp3)); invalidMp3[1] = 0xeb; // reserved MPEG version
  CHECK(tab5RadioProbeCodec(invalidMp3, sizeof(invalidMp3)) == Codec::Unknown);
  std::memcpy(invalidMp3, mp3, sizeof(mp3)); invalidMp3[2] = 0xfc;
  CHECK(tab5RadioProbeCodec(invalidMp3, sizeof(invalidMp3)) == Codec::Unknown);

  // A complete leading ID3 tag can precede either format. A large incomplete
  // tag must not force MP3 over the server/catalogue's AAC hint.
  std::vector<uint8_t> tagged = {'I','D','3',4,0,0,0,0,0,0};
  tagged.insert(tagged.end(), adts, adts + sizeof(adts));
  CHECK(tab5RadioProbeCodec(tagged.data(), tagged.size()) == Codec::Aac);
  tagged[9] = 127;
  CHECK(tab5RadioProbeCodec(tagged.data(), tagged.size()) == Codec::Unknown);
  CHECK(tab5RadioSelectCodec(tagged.data(), tagged.size(), Codec::Aac, Codec::Unknown,
                            "http://radio.test/live") == Codec::Aac);
  tagged[6] = 0x80;
  CHECK(tab5RadioProbeCodec(tagged.data(), tagged.size()) == Codec::Unknown);
  CHECK(tab5RadioSelectCodec(adts, sizeof(adts), Codec::Unknown, Codec::Mp3,
                            "http://radio.test/live.mp3") == Codec::Aac);
  CHECK(tab5RadioSelectCodec(mp3, sizeof(mp3), Codec::Aac, Codec::Aac,
                            "http://radio.test/live.aac") == Codec::Mp3);
  CHECK(tab5RadioSelectCodec(nullptr, 0, Codec::Unknown, Codec::Aac,
                            "http://radio.test/opaque") == Codec::Aac);
  CHECK(tab5RadioSelectCodec(nullptr, 0, Codec::Mp3, Codec::Aac,
                            "http://radio.test/opaque") == Codec::Mp3);
  CHECK(tab5RadioSelectCodec(nullptr, 0, Codec::Unknown, Codec::Unknown,
                            "http://radio.test/live.aac") == Codec::Aac);
  CHECK(tab5RadioSelectCodec(nullptr, 0, Codec::Unknown, Codec::Unknown,
                            "http://radio.test/opaque") == Codec::Mp3);
  CHECK(tab5RadioSelectCodec(mp3, sizeof(mp3), Codec::Unsupported, Codec::Mp3,
                            "http://radio.test/error") == Codec::Unsupported);
  CHECK(tab5RadioSelectCodec(html, sizeof(html), Codec::Unsupported, Codec::Unknown,
                            "http://radio.test/live.mp3") == Codec::Unsupported);
  std::printf("RADIO CODEC PASS checks=%u\n", checks);
}
