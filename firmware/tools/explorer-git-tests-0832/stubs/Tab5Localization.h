#pragma once
#include <cstring>
#include <cstdio>
struct FakeLocale{size_t copyText(const char* s,char* p,size_t n)const{if(p&&n)snprintf(p,n,"%s",s);return strlen(s);}const String effectiveId()const{return "fr-FR";}};
inline FakeLocale Tab5Locale;
