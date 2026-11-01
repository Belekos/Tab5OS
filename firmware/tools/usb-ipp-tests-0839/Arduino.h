#pragma once
#include <stddef.h>
#include <stdint.h>
class Stream {public:virtual ~Stream()=default;virtual size_t readBytes(char*,size_t)=0;};
