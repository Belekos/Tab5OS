#pragma once
extern int testStorageUsers;
struct Tab5StorageUse { Tab5StorageUse() { ++testStorageUsers; } ~Tab5StorageUse() { --testStorageUsers; } explicit operator bool() const { return true; } };
inline void tab5StorageFault(const char*,const char*,int) {}
