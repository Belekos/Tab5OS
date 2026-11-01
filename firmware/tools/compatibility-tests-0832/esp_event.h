#pragma once
#include <stdint.h>
using esp_event_base_t = const char*;
using esp_event_handler_instance_t = void*;
using esp_event_handler_t = void(*)(void*, esp_event_base_t, int32_t, void*);
constexpr int ESP_OK = 0;
constexpr int ESP_ERR_INVALID_STATE = 259;
constexpr int ESP_EVENT_ANY_ID = -1;
int esp_event_loop_create_default();
int esp_event_handler_instance_register(esp_event_base_t base, int32_t id,
    esp_event_handler_t callback, void* arg, esp_event_handler_instance_t* result);
