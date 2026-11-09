#pragma once
#include <stdint.h>
typedef intptr_t jmp_buf[5];
#define setjmp(buffer) __builtin_setjmp(buffer)
#define longjmp(buffer,value) __builtin_longjmp(buffer,1)
