#pragma once
#include "FS.h"
inline bool tab5AtomicRecover(const char*){return true;}
inline bool tab5AtomicWrite(const char* name,const char* text,size_t n){if(failWrite)return false;disk[name]=std::string(text,n);return true;}
