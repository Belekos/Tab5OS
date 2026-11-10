#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

struct SpeakerConfig {
  uint32_t sample_rate = 48000;
  size_t dma_buf_len = 256, dma_buf_count = 8;
  uint8_t task_priority = 0, task_pinned_core = 0;
};

// The real Speaker retains a current and a queued raw PCM pointer. A third
// submission waits until the oldest is retired. Keep both snapshots here to
// detect any write to live storage before the equivalent retirement barrier.
struct MockSpeaker {
  struct Block { const int16_t* pointer; size_t length; uint32_t rate; std::vector<int16_t> samples; };
  SpeakerConfig settings;
  std::vector<Block> retained, submitted;
  bool running = false, failNext = false, memoryChanged = false;
  unsigned waitCount = 0, stopCount = 0, endCount = 0, calls = 0;
  bool isRunning() const { return running; }
  SpeakerConfig config() const { return settings; }
  void config(const SpeakerConfig& value) { settings = value; }
  bool begin() { running = true; return true; }
  void verify() {
    for (const auto& block : retained)
      if (!std::equal(block.samples.begin(), block.samples.end(), block.pointer)) memoryChanged = true;
  }
  void end() { verify(); retained.clear(); running = false; ++endCount; }
  void setVolume(uint8_t) {}
  bool stopAndWait(uint8_t) { verify(); retained.clear(); ++stopCount; return true; }
  void stop(uint8_t channel) { stopAndWait(channel); }
  bool playRaw(const int16_t* pcm, size_t count, uint32_t rate, bool stereo, unsigned repeat, uint8_t channel) {
    verify(); ++calls;
    if (failNext) { failNext = false; return false; }
    if (!running || !stereo || repeat != 1 || channel != 0) return false;
    if (retained.size() == 2) { retained.erase(retained.begin()); ++waitCount; }
    Block block{pcm, count, rate, {pcm, pcm + count}};
    retained.push_back(block); submitted.push_back(block);
    return true;
  }
};

struct MockIOExpander {
  bool readRegister(uint8_t, uint8_t* value, size_t) { *value = 0; return true; }
  bool digitalWrite(uint8_t, bool) { return true; }
};
struct MockM5 {
  MockSpeaker Speaker;
  MockIOExpander io;
  MockIOExpander& getIOExpander(unsigned) { return io; }
};
extern MockM5 M5;
