// Select the ESP-IDF-supported EXTERNAL host allocator for this reviewed SDK.
// Generic host pools use PSRAM; *_internal keeps INTERNAL|DMA, and task stacks
// are still allocated by FreeRTOS. No global malloc/calloc interception occurs.
// These six definitions satisfy libbt.a references before its allocator member
// is extracted. tools verification checks this in the final target ELF/map.
#include "Tab5MeshAllocator.h"
#if TAB5_MESH_EXTERNAL_POOLS
#undef CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL
#define CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL 1
#include "Tab5MeshAllocatorEspIdf554.inc"
#endif
