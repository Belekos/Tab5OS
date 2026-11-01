#pragma once
#include <stddef.h>
namespace Tab5Hardware {
enum class Owner { Python=1, Application=2 };
int call(Owner owner,int op,int a,int b,int c,const void* input,size_t length,void* output,size_t capacity);
void release(Owner owner);
}
