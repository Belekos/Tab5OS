#pragma once
#include <stddef.h>
#include <string.h>

namespace Tab5TranslationFormat {
// printf argument types must remain in the same order in an SD translation.
// Width/precision numbers may change. Positional arguments and %n are refused;
// dynamic width/precision still consume integer arguments and are checked.
struct Signature { char value[256] = {}; size_t length = 0; bool valid = true; };
inline bool digit(char c) { return c >= '0' && c <= '9'; }
inline void append(Signature& signature, char c) {
  if (signature.length < sizeof(signature.value)) signature.value[signature.length++] = c;
  else signature.valid = false;
}
inline Signature signature(const char* text) {
  Signature result;
  for (const char* p = text; *p; ++p) {
    if (*p != '%') continue;
    const char* start = p++;
    if (*p == '%') continue;
    while (*p && strchr("-+ #0'", *p)) ++p;
    if (*p == '*') { append(result, '*'); ++p; }
    else while (digit(*p)) ++p;
    if (*p == '$') { result.valid = false; return result; }
    if (*p == '.') {
      ++p;
      if (*p == '*') { append(result, '*'); ++p; }
      else while (digit(*p)) ++p;
    }
    char length[3] = {};
    if (*p && strchr("hljztL", *p)) {
      length[0] = *p++;
      if ((*p == 'h' || *p == 'l') && *p == length[0]) length[1] = *p++;
    }
    if (*p == 'n') { result.valid = false; return result; }
    if (*p && strchr("diouxXfFeEgGaAcsp", *p)) {
      append(result, '%');
      if (length[0]) append(result, length[0]);
      if (length[1]) append(result, length[1]);
      append(result, *p);
    } else {
      // Percent signs in prose ("100%", for example) are ordinary text.
      // Restart just after '%' so an incomplete conversion cannot overrun.
      p = start;
    }
  }
  return result;
}
inline bool compatible(const char* source, const char* translated) {
  const Signature a = signature(source), b = signature(translated);
  return a.valid && b.valid && a.length == b.length &&
         memcmp(a.value, b.value, a.length) == 0;
}
}
