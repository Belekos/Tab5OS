#pragma once
#include <cstdlib>
#include <limits>
#include <new>
#include <string>
#include <vector>

#include "WriterDocument.h"

namespace writing {
// Index, rich text and view geometry must not consume the internal heap
// reserved for TLS/DMA, including their many allocations smaller than 4 KiB.
// Standard container allocation semantics, with the same PSRAM owner as text.
template <class T>
struct ExternalAllocator {
  using value_type = T;
  ExternalAllocator() = default;
  template <class U>
  ExternalAllocator(const ExternalAllocator<U>&) {}
  T* allocate(size_t n) {
    void* p = n <= std::numeric_limits<size_t>::max() / sizeof(T) ? writing::allocate(n * sizeof(T))
                                                                  : nullptr;
    if (!p) {
#if defined(__cpp_exceptions)
      throw std::bad_alloc();
#else
      std::abort();
#endif
    }
    return static_cast<T*>(p);
  }
  void deallocate(T* p, size_t) { writing::release(p); }
  template <class U>
  bool operator==(const ExternalAllocator<U>&) const {
    return true;
  }
  template <class U>
  bool operator!=(const ExternalAllocator<U>&) const {
    return false;
  }
};
template <class T>
using ExternalVector = std::vector<T, ExternalAllocator<T>>;
using IndexString = std::basic_string<char, std::char_traits<char>, ExternalAllocator<char>>;
}  // namespace writing
