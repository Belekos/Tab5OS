#pragma once
#include_next <time.h>
struct tm *localtime_r(const time_t *value,struct tm *out);
