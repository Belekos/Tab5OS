#pragma once
#include <cstddef>
namespace Tab5CryptoRandom {
// Call once before any application ADC user is started.
bool begin();
bool fill(void* destination,size_t size);
}
