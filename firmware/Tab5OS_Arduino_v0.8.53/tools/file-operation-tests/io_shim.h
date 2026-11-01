#pragma once
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#ifndef ETIMEDOUT
#define ETIMEDOUT 110
#endif
#ifndef ECANCELED
#define ECANCELED 125
#endif
#ifndef ESTALE
#define ESTALE 116
#endif
#endif
enum Fault { None, EarlyRead, WriteError, ShortWrite, SyncError, CorruptWrite, Cancelled, ChangeSource, LateCollision, NewChild };
extern Fault testFault;
extern unsigned testReads, testWrites, testSyncs;
extern int testStorageUsers;
int testOpen(const char* path, int flags, ...);
ssize_t testRead(int fd, void* bytes, size_t size);
ssize_t testWrite(int fd, const void* bytes, size_t size);
int testSync(int fd);
int testMkdir(const char* path, unsigned mode);
#define open testOpen
#define read testRead
#define write testWrite
#define fsync testSync
#define mkdir testMkdir
