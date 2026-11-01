#pragma once
#include <mutex>
using portMUX_TYPE = std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
inline thread_local int compatibilityCriticalDepth = 0;
#define portENTER_CRITICAL(p) do { (p)->lock(); ++compatibilityCriticalDepth; } while (0)
#define portEXIT_CRITICAL(p) do { --compatibilityCriticalDepth; (p)->unlock(); } while (0)
