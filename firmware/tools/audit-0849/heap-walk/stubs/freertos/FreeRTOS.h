#pragma once
/* Only the platform lock boundary is substituted. Allocator code is upstream. */
typedef int portMUX_TYPE;
void audit_heap_lock(void *lock);
void audit_heap_unlock(void *lock);
#define portENTER_CRITICAL_SAFE(lock) audit_heap_lock(lock)
#define portEXIT_CRITICAL_SAFE(lock) audit_heap_unlock(lock)
#define portMUX_INITIALIZE(lock) (*(lock) = 0)
#define portMUX_INITIALIZER_UNLOCKED 0
