#pragma once
#include <cstddef>

// A single TLS connection, owned exclusively by the MicroPython worker.
namespace Tab5PythonIrc {
int call(int op, int a, int b, const void* input, size_t length,
         void* output, size_t capacity);
void reset();
}
