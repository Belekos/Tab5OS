/* Compile the shared, tested FAT16/MBR constructors only. Unused legacy
 * machine functions are removed by section GC; their disk array is private. */
#define disks dos386_unused_legacy_disks
#include "../dos/core/ibmpc/disk.c"
