#pragma once
#include <stdio.h>
#include <stdarg.h>
static inline int esp_rom_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}
