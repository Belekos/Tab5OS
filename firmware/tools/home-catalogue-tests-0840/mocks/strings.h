#pragma once
#if defined(_WIN32)
#include <cstring>
#define strcasecmp _stricmp
#else
#include_next <strings.h>
#endif
