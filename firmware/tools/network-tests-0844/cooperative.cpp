#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>

static uint32_t tick = 0;
static unsigned yields = 0;
uint32_t millis() { return tick; }
void vTaskDelay(unsigned ticks) { assert(ticks == 1); ++tick; ++yields; }
#include "Tab5CooperativeClient.h"

struct Transport {
  virtual ~Transport() = default;
  virtual int read(uint8_t* data, size_t size) { ++reads; if (size) *data = 42; return size ? 1 : 0; }
  virtual int read() { ++reads; return 42; }
  virtual int available() { ++polls; return 0; }
  virtual uint8_t connected() { ++polls; return live; }
  void setTimeout(uint32_t value) { timeout = value; }
  void stop() { ++stops; live = false; }
  unsigned reads = 0, polls = 0, stops = 0;
  uint32_t timeout = 8000;
  bool live = true;
};
struct Context { bool cancel = false; unsigned calls = 0; };
static bool cancelled(void* pointer) {
  auto& context = *static_cast<Context*>(pointer);
  ++context.calls;
  return context.cancel;
}

// MAP_CANCELLATION_GUARD

int main() {
  Tab5CooperativeClient<Transport> client;
  assert(client.read() == 42);
  uint8_t value = 0;
  assert(client.read(&value, 1) == 1 && value == 42);
  assert(!client.interrupted());
  for (unsigned iteration = 0; iteration < 1000; ++iteration) {
    auto context = std::make_unique<Context>();
    client.live = true;
    client.setTimeout(8000);
    client.setCancellation(cancelled, context.get());
    unsigned oldReads = client.reads, oldStops = client.stops;
    // Fragmented header/body: transport has no byte yet, owning task polls.
    for (unsigned i = 0; i < 16; ++i) { ++tick; assert(client.available() == 0); }
    assert(client.connected() && context->calls == 17);
    context->cancel = true;
    assert(client.read(&value, 1) == -1);
    assert(client.interrupted() && client.stops == oldStops + 1);
    assert(client.timeout == 0 && client.reads == oldReads);
    assert(!client.connected() && !client.available());
    assert(client.stops == oldStops + 1); // stop is idempotent after cancel
    client.clearCancellation();
    context.reset(); // Any retained callback would now be a real ASan UAF.
    assert(!client.connected());
    assert(client.read() == -1);
    // New request can safely reuse the stopped transport object.
    Context next;
    client.setCancellation(cancelled, &next);
    client.setTimeout(8000);
    client.live = true;
    assert(client.read() == 42 && client.connected());
    client.clearCancellation();
  }
  // Non-cancelled context also must be cleared before it dies.
  { auto context = std::make_unique<Context>(); client.setCancellation(cancelled, context.get()); assert(client.connected()); client.clearCancellation(); }
  assert(client.connected());
  Tab5MapConnection map;
  for (unsigned iteration = 0; iteration < 1000; ++iteration) {
    auto context = std::make_unique<Context>();
    {
      Tab5MapRequestCancellation binding(map, cancelled, context.get());
      context->cancel = true;
      assert(map.plain.read() == -1 && map.secure.read() == -1);
    }
    context.reset();
    assert(!map.plain.connected() && !map.secure.connected());
    context = std::make_unique<Context>();
    {
      Tab5MapRequestCancellation binding(map, cancelled, context.get());
      map.plain.live = true; map.secure.live = true;
      assert(map.plain.read() == 42 && map.secure.read() == 42);
    }
    context.reset();
    assert(map.plain.connected() && map.secure.connected());
  }
  // Unsigned scheduling remains safe across millis rollover.
  tick = UINT32_MAX - 2;
  assert(client.connected());
  tick = 3;
  auto before = yields;
  assert(client.connected());
  assert(yields == before + 1);
  assert(yields >= 4000);
  std::puts("PASS: 1000 real cooperative-client cancellation/rebind cycles, 1000 actual Map binding-scope cycles, fragmented header/body polling, freed callback contexts, rollover and scheduler yield");
}
