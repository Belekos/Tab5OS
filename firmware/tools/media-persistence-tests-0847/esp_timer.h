#pragma once
#include "boundary.h"
inline int64_t esp_timer_get_time(){return Boundary::clockUs.load();}
