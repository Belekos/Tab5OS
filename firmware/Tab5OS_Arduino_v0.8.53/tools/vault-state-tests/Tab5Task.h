#pragma once
constexpr int pdPASS=1;
inline int tab5CreateTask(void(*f)(void*),const char*,int,void* p,int,void*,int){f(p);return pdPASS;}
inline void tab5TaskCheckpoint(const char*){}
