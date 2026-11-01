#include "image_core.h"
#include <stdlib.h>
#include <stdio.h>
static void*a(void*c,size_t n){(void)c;return malloc(n);}static void f(void*c,void*p){(void)c;free(p);}
int main(void){IEDoc d;IEBrush b={0xff0000ff,19,50,100,0,0,0};int i;if(!ie_init(&d,a,f,0,40,40)||!ie_new(&d,40,40,0)||!ie_begin(&d))return 1;for(i=0;i<12;i++)ie_brush(&d,20,20,20,20,&b);printf("{\"opacity\":50,\"alpha\":%u}\n",ie_composite(&d,20,20)>>24);ie_destroy(&d);return 0;}
