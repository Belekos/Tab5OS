#pragma once
#include "platform.h"
using intr_handler_t=void(*)(void*);
using intr_handle_t=void*;
extern "C" int esp_intr_alloc(int,int,intr_handler_t,void*,intr_handle_t*);
