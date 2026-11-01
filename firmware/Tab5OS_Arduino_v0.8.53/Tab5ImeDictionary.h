#pragma once
#include "Tab5Ime.h"
#include <Arduino.h>

// Dictionaries remain on SD. One lookup reads logarithmic index records and
// one candidate page; no complete lexicon or multi-megabyte index enters RAM.
class Tab5ImeDictionary {
 public:
  static constexpr size_t PAGE_SIZE=6;
  struct Page { String words[PAGE_SIZE];size_t count=0;uint32_t total=0;bool available=false; };
  Page lookup(tab5ime::Mode mode,const char* key,uint32_t first=0)const;
};
extern Tab5ImeDictionary Tab5ImeWords;
