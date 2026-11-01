#pragma once
#include <stdint.h>
namespace Tab5UsbFormatPolicy {
enum class Format : uint8_t { Fat, Fat32 };
// Keep FAT12/16 volumes interoperable with common removable-media readers.
inline bool geometry(Format format,uint32_t sectors,uint32_t sectorBytes){
 if(sectorBytes<512||sectorBytes>4096||(sectorBytes&(sectorBytes-1))||!sectors)return false;
 const uint64_t bytes=uint64_t(sectors)*sectorBytes;
 if(format==Format::Fat)return bytes>=1024*1024&&bytes<=uint64_t(2)*1024*1024*1024;
 if(format==Format::Fat32)return bytes>=uint64_t(64)*1024*1024&&bytes<=uint64_t(2)*1024*1024*1024*1024;
 return false;
}
}
