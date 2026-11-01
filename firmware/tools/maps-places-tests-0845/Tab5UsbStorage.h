#pragma once
#include <cstdint>
#include <cstring>
namespace Tab5UsbStorage {
inline unsigned references=0;
inline bool path(const char* p){return !strncmp(p,"/USB",4);}
inline bool retain(uint32_t& gen){++references;gen=1;return true;}
inline void release(){--references;}
inline bool valid(uint32_t gen){return gen==1;}
}
