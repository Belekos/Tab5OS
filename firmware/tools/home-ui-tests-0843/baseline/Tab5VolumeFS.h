#pragma once
#include <FS.h>
// Logical SD paths remain unchanged. /USB maps to the mounted removable disk.
extern fs::FS tab5FS;
// Called by the UI while it owns Tab5StorageRecovery, before unmounting and
// after mounting. SD files remain counted until their last File copy closes.
unsigned tab5SdOpenFiles();
void tab5SdSetAvailable(bool available);
bool tab5SdAvailable();
// Cheap change notification: incremented after a successful mutation or a
// writable file is closed, never by reads. UI consumers debounce this counter.
uint32_t tab5FsRevision();
uint32_t tab5SdGeneration();
void tab5FsChanged(); // For users of the underlying POSIX filesystem.
