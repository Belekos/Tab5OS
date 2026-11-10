#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

// Exercise the production ABI callbacks against a deterministic scheduler and
// an audio device whose terminal failure clears only after release + claim.
struct Context {
 uint32_t lastCooperation = 0;
 bool audio = false;
};
static Context state;
static Context* context = &state;
static std::atomic<bool> cancelled{false};
static bool isWorker = true, closing = false;
static uint32_t clockMs = 0;
static unsigned delays = 0, cancelAfter = 0, closeAfter = 0, checks = 0;
#define CHECK(expression) do { ++checks; assert(expression); } while (false)

bool worker() { return context && isWorker; }
bool panelClosing() { return closing; }
uint32_t millis() { return clockMs; }
void vTaskDelay(unsigned ticks) {
 assert(ticks == 1);
 ++clockMs;
 ++delays;
 if (cancelAfter && delays >= cancelAfter) cancelled = true;
 if (closeAfter && delays >= closeAfter) closing = true;
}

enum class Tab5AudioClient { Native };
enum class Tab5PcmFormat { S16 };
struct Tab5PcmSpec { uint32_t sampleRate = 0; unsigned channels = 0; Tab5PcmFormat format = Tab5PcmFormat::S16; };
struct Device {
 bool owned = false, busy = false, failed = false, failNextWrite = false, failRelease = false;
 unsigned claims = 0, writes = 0, releases = 0;
 Tab5PcmSpec lastSpec;
 size_t lastFrames = 0;
 bool claim(Tab5AudioClient client, unsigned timeout) {
  assert(client == Tab5AudioClient::Native && timeout == 0);
  ++claims;
  if (busy) return false;
  assert(!owned); // Never reclaim while failed release retains the lease.
  owned = true;
  failed = false;
  return true;
 }
 bool writePcm(Tab5AudioClient client, const int16_t* samples, size_t frames, const Tab5PcmSpec& spec) {
  assert(client == Tab5AudioClient::Native && owned && samples);
  ++writes;
  lastSpec = spec;
  lastFrames = frames;
  if (failNextWrite) { failed = true; failNextWrite = false; }
  return !failed;
 }
 bool release(Tab5AudioClient client) {
  assert(client == Tab5AudioClient::Native && owned);
  ++releases;
  if (failRelease) return false;
  owned = false;
  return true;
 }
};
static Device Tab5AudioDevice;

#include "native_runtime.inc"

static void reset(uint32_t now = 0, uint32_t last = 0) {
 state = Context{};
 context = &state;
 state.lastCooperation = last;
 isWorker = true;
 closing = false;
 cancelled = false;
 clockMs = now;
 delays = cancelAfter = closeAfter = 0;
 Tab5AudioDevice = Device{};
}

int main() {
 reset(20);
 CHECK(appSleep(0) == 0 && delays == 1 && clockMs == 21);
 CHECK(context->lastCooperation == 21);
 CHECK(cooperate() && delays == 1); // Explicit sleep already let idle tasks run.

 reset(100);
 CHECK(appSleep(5) == 0 && delays == 5 && clockMs == 105);
 CHECK(context->lastCooperation == 105);
 clockMs += 7;
 CHECK(appMillis() == 112 && delays == 5);
 ++clockMs;
 CHECK(appMillis() == 114 && delays == 6 && context->lastCooperation == 114);
 reset();
 for (unsigned i = 0; i < 64; ++i) {
  ++clockMs;
  const uint32_t observed = appMillis();
  CHECK(observed == clockMs);
  CHECK(uint32_t(clockMs - context->lastCooperation) < 8U);
 }
 CHECK(delays == 8); // Polling the clock still provides mandatory cooperation.

 const uint32_t wrapStart = std::numeric_limits<uint32_t>::max() - 2U;
 reset(wrapStart, wrapStart);
 CHECK(appSleep(5) == 0 && delays == 5 && clockMs == 2);
 CHECK(context->lastCooperation == 2);
 clockMs += 8;
 CHECK(cooperate() && delays == 6 && clockMs == 11);

 reset(); cancelled = true;
 CHECK(appSleep(100) == 1 && delays == 0);
 reset(); closing = true;
 CHECK(appSleep(100) == 1 && delays == 0);
 reset(); cancelAfter = 2;
 CHECK(appSleep(100) == 1 && delays == 2 && context->lastCooperation == 2);
 reset(); closeAfter = 2;
 CHECK(appSleep(100) == 1 && delays == 2 && context->lastCooperation == 2);
 reset(); isWorker = false;
 CHECK(appSleep(100) == 1 && appMillis() == 0 && !cooperate() && delays == 0);
 context = nullptr;
 CHECK(appSleep(100) == 1 && appMillis() == 0 && !cooperate() && delays == 0);

 int16_t samples[4096]{};
 reset(); Tab5AudioDevice.busy = true;
 CHECK(appAudio(samples, 1024, 22050) == -1 && !context->audio);
 CHECK(Tab5AudioDevice.claims == 1 && Tab5AudioDevice.writes == 0 && Tab5AudioDevice.releases == 0);
 Tab5AudioDevice.busy = false;
 CHECK(appAudio(samples, 1024, 22050) == 0 && context->audio);
 CHECK(Tab5AudioDevice.lastFrames == 1024 && Tab5AudioDevice.lastSpec.sampleRate == 22050);
 CHECK(Tab5AudioDevice.lastSpec.channels == 2 && Tab5AudioDevice.lastSpec.format == Tab5PcmFormat::S16);
 CHECK(appAudio(samples, 128, 22050) == 0 && Tab5AudioDevice.claims == 2);

 Tab5AudioDevice.failNextWrite = true;
 CHECK(appAudio(samples, 1024, 22050) == -1 && !context->audio && !Tab5AudioDevice.owned);
 CHECK(Tab5AudioDevice.releases == 1 && Tab5AudioDevice.failed);
 CHECK(appAudio(samples, 1024, 22050) == 0 && context->audio && !Tab5AudioDevice.failed);
 CHECK(Tab5AudioDevice.claims == 3 && Tab5AudioDevice.releases == 1);

 reset(); Tab5AudioDevice.failNextWrite = true; Tab5AudioDevice.failRelease = true;
 CHECK(appAudio(samples, 1024, 22050) == -1 && context->audio && Tab5AudioDevice.owned);
 CHECK(Tab5AudioDevice.claims == 1 && Tab5AudioDevice.releases == 1);
 Tab5AudioDevice.failRelease = false;
 CHECK(appAudio(samples, 1024, 22050) == -1 && !context->audio && !Tab5AudioDevice.owned);
 CHECK(Tab5AudioDevice.claims == 1 && Tab5AudioDevice.releases == 2);
 CHECK(appAudio(samples, 1024, 22050) == 0 && Tab5AudioDevice.claims == 2);

 reset();
 CHECK(appAudio(nullptr, 1024, 22050) == -1);
 CHECK(appAudio(samples, 2049, 22050) == -1);
 CHECK(appAudio(samples, 1024, 7999) == -1);
 CHECK(appAudio(samples, 1024, 48001) == -1);
 cancelled = true;
 CHECK(appAudio(samples, 1024, 22050) == -1);
 cancelled = false; isWorker = false;
 CHECK(appAudio(samples, 1024, 22050) == -1);
 context = nullptr;
 CHECK(appAudio(samples, 1024, 22050) == -1);
 CHECK(Tab5AudioDevice.claims == 0 && Tab5AudioDevice.writes == 0 && Tab5AudioDevice.releases == 0);
 std::printf("Production native runtime callbacks: %u checks passed (sleep fairness, wrap, cancellation, audio release and retry).\n", checks);
}
