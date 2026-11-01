#pragma once
#include <stdint.h>

namespace Tab5FirmwareFootprint {
struct Snapshot {
  uint32_t imageBytes=0, partitionBytes=0, partitionAddress=0;
  bool valid=false;
  int32_t error=0;
};
// Immutable for this boot: the running partition/image cannot change until
// reboot. Metadata only; this display helper is not a firmware integrity test.
// The first call reads a bounded header and official SDK image metadata. All
// later calls return cached values without flash reads or heap allocations.
const Snapshot& snapshot();
}
