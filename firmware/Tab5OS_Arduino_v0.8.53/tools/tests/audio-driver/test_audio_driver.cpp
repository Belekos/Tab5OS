#include "Tab5AudioDriver.h"
#include <cstdio>
#include <vector>

MockM5 M5;
static unsigned checks;
#define CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main() {
  auto& driver = Tab5AudioDevice;
  auto& speaker = M5.Speaker;
  const auto native = Tab5AudioClient::Native;
  Tab5PcmSpec spec; spec.sampleRate = 22050; spec.channels = 2; spec.format = Tab5PcmFormat::S16;
  std::vector<int16_t> pcm(4096);
  for (size_t index = 0; index < pcm.size(); ++index) pcm[index] = static_cast<int16_t>(index * 7);
  CHECK(driver.begin(150, 0, 6));
  CHECK(driver.claim(native));

  // An exact full call must reach the Speaker immediately; a half block must
  // remain buffered until complete. No extra following sample is necessary.
  CHECK(driver.writePcm(native, pcm.data(), 1024, spec));
  CHECK(speaker.submitted.size() == 1 && speaker.retained.size() == 1);
  CHECK(speaker.submitted[0].samples == std::vector<int16_t>(pcm.begin(), pcm.begin() + 2048));
  CHECK(driver.writePcm(native, pcm.data() + 2048, 512, spec));
  CHECK(speaker.submitted.size() == 1);
  CHECK(driver.writePcm(native, pcm.data() + 3072, 512, spec));
  CHECK(speaker.submitted.size() == 2 && speaker.retained.size() == 2);
  CHECK(speaker.submitted[1].samples == std::vector<int16_t>(pcm.begin() + 2048, pcm.end()));

  // Keep current + next raw pointers live while repeatedly wrapping the
  // driver's third buffer, including two blocks supplied in one API call.
  for (unsigned round = 0; round < 8; ++round) {
    for (size_t index = 0; index < pcm.size(); ++index)
      pcm[index] = static_cast<int16_t>((round + 1) * 1000 + index * 3);
    CHECK(driver.writePcm(native, pcm.data(), 2048, spec));
    speaker.verify();
    CHECK(!speaker.memoryChanged && speaker.retained.size() == 2);
  }
  CHECK(speaker.waitCount == 16 && driver.pushedBlocks() == 18);
  CHECK(driver.writePcm(native, pcm.data(), 17, spec));
  CHECK(speaker.submitted.size() == 18);
  CHECK(driver.flush(native));
  CHECK(speaker.submitted.back().length == 34 && speaker.submitted.back().rate == 22050);

  // Cancel/stop retires both pointers before reusing any storage and keeps
  // clocks running. Starting the next partial block cannot replay old data.
  const auto beforeStop = speaker.stopCount;
  driver.stop(native);
  CHECK(speaker.stopCount == beforeStop + 1 && speaker.retained.empty() && speaker.running);
  CHECK(!speaker.memoryChanged);
  const auto beforePartial = speaker.submitted.size();
  CHECK(driver.writePcm(native, pcm.data() + 20, 5, spec));
  CHECK(speaker.submitted.size() == beforePartial);
  CHECK(driver.flush(native));
  CHECK(speaker.submitted.back().samples == std::vector<int16_t>(pcm.begin() + 20, pcm.begin() + 30));

  // A rejected submission remains a terminal error until the existing
  // release/reclaim lifecycle. Retry must never duplicate accepted samples.
  speaker.failNext = true;
  CHECK(!driver.writePcm(native, pcm.data(), 1024, spec) && driver.failed());
  const auto failedCalls = speaker.calls;
  CHECK(!driver.writePcm(native, pcm.data(), 1024, spec) && speaker.calls == failedCalls);
  CHECK(driver.release(native));
  CHECK(speaker.retained.empty() && !speaker.running && !speaker.memoryChanged);
  CHECK(driver.claim(native) && !driver.failed());
  CHECK(driver.writePcm(native, pcm.data(), 1024, spec));
  CHECK(speaker.calls == failedCalls + 1 && driver.pushedBlocks() == 1);
  AudioOutputTab5Speaker busyRadio(&driver);
  const auto busyEnds = speaker.endCount;
  CHECK(!busyRadio.begin() && driver.owner() == native);
  CHECK(speaker.endCount == busyEnds && speaker.retained.size() == 1 && !speaker.memoryChanged);
  CHECK(driver.release(native) && !speaker.memoryChanged);

  // Radio decoders use the real sample-at-a-time adapter, not writePcm().
  // A false ConsumeSample means the decoder must retry that same sample.
  AudioOutputTab5Speaker radio(&driver);
  CHECK(radio.SetRate(44100) && radio.SetChannels(2));
  CHECK(driver.owner() == Tab5AudioClient::None);
  CHECK(radio.begin() && driver.owner() == Tab5AudioClient::Audio);
  CHECK(!driver.claim(native, 0) && driver.owner() == Tab5AudioClient::Audio);
  const auto radioStart = speaker.submitted.size();
  const auto radioEnds = speaker.endCount;
  std::vector<int16_t> radioPcm;
  unsigned retries = 0;
  for (unsigned frame = 0; frame < 2065; ++frame) {
    int16_t sample[2] = {static_cast<int16_t>(frame * 5), static_cast<int16_t>(-int(frame * 3))};
    radioPcm.push_back(sample[0]); radioPcm.push_back(sample[1]);
    if (!radio.ConsumeSample(sample)) {
      ++retries;
      CHECK(radio.ConsumeSample(sample));
    }
  }
  radio.flush();
  CHECK(retries == 2 && speaker.submitted.size() == radioStart + 3);
  std::vector<int16_t> decoded;
  for (size_t i = radioStart; i < speaker.submitted.size(); ++i) {
    const auto& block = speaker.submitted[i];
    CHECK(block.rate == 44100);
    decoded.insert(decoded.end(), block.samples.begin(), block.samples.end());
  }
  CHECK(decoded == radioPcm && !speaker.memoryChanged);
  CHECK(driver.outputRate() == 48000 && speaker.endCount == radioEnds);

  // A source-rate change must publish old partial samples at their original
  // rate while preserving the transport's fixed I2S clock and live buffers.
  int16_t pair[2] = {1234, -5678};
  CHECK(radio.ConsumeSample(pair));
  CHECK(radio.SetRate(32000));
  CHECK(speaker.submitted.back().length == 2 && speaker.submitted.back().rate == 44100);
  CHECK(radio.ConsumeSample(pair));
  radio.flush();
  CHECK(speaker.submitted.back().length == 2 && speaker.submitted.back().rate == 32000);
  CHECK(speaker.endCount == radioEnds && driver.outputRate() == 48000 && !speaker.memoryChanged);
  CHECK(radio.stop() && speaker.retained.empty() && speaker.running);
  CHECK(driver.owner() == Tab5AudioClient::Audio);
  CHECK(radio.ConsumeSample(pair)); radio.flush();
  CHECK(speaker.submitted.back().samples == std::vector<int16_t>({1234, -5678}));
  CHECK(driver.release(Tab5AudioClient::Audio) && !speaker.memoryChanged);
  CHECK(driver.claim(native) && driver.writePcm(native, pcm.data(), 1024, spec));
  CHECK(speaker.submitted.back().rate == 22050 && driver.outputRate() == 48000);
  CHECK(driver.release(native) && !speaker.memoryChanged);
  std::printf("AUDIO DRIVER PASS checks=%u: exact full block, partial flush, three-buffer lifetime, stop, failure, reclaim and radio adapter\n", checks);
  return 0;
}
