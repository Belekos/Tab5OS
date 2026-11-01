#pragma once
#include <algorithm>
#include <cstring>
#include <stddef.h>
#include <stdint.h>
namespace Tab5ConsoleBuffer {
// Holds all 1024 port results with the shipped languages, including Russian.
// Allocation remains fixed and belongs to the generic ELF console PSRAM context.
constexpr size_t Capacity=160*1024;
constexpr char Truncated[]="\n[...]\n";
// Caller owns synchronization and retains the truncation flag across writes.
inline bool append(char* output,size_t capacity,size_t& length,bool& truncated,
                   const char* data,size_t size){
 if(truncated||!size)return false;
 if(capacity<sizeof(Truncated)||length>capacity-sizeof(Truncated))return false;
 const size_t available=capacity-sizeof(Truncated)-length;
 size_t copied=std::min(size,available);
 if(copied<size){
  // If the first omitted byte continues a code point, omit its lead byte too.
  while(copied&&(uint8_t(data[copied])&0xc0)==0x80)--copied;
 }
 if(copied){memcpy(output+length,data,copied);length+=copied;}
 if(copied<size){memcpy(output+length,Truncated,sizeof(Truncated)-1);length+=sizeof(Truncated)-1;truncated=true;}
 output[length]=0;return copied||truncated;
}
}
