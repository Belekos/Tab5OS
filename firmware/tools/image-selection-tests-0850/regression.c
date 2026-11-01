#include "image_core.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static void*a(void*c,size_t n){(void)c;return malloc(n);}static void f(void*c,void*p){(void)c;free(p);}
int main(void){IEDoc d;uint32_t saved[20*20];int restored,alpha;
 if(!ie_init(&d,a,f,0,20,20))return 2;
 for(int i=0;i<400;i++)d.current.layers[0].pixels[i]=IE_RGBA(i*13%256,i*19%256,i*23%256,255);
 memcpy(saved,d.current.layers[0].pixels,sizeof(saved));ie_select(&d,1,1,4,4);
 if(!ie_translate_selection(&d,5,0)||!ie_translate_selection(&d,5,0))return 2;
 restored=1;for(int y=1;y<5;y++)for(int x=6;x<10;x++)if(d.current.layers[0].pixels[y*20+x]!=saved[y*20+x])restored=0;
 if(!ie_new(&d,20,20,IE_RGBA(21,43,67,128)))return 2;
 memcpy(saved,d.current.layers[0].pixels,sizeof(saved));ie_select(&d,1,1,4,4);d.current.selection_mask=a(0,400);memset(d.current.selection_mask,0,400);for(int y=1;y<5;y++)for(int x=1;x<5;x++)d.current.selection_mask[y*20+x]=128;
 if(!ie_translate_selection(&d,5,0)||!ie_translate_selection(&d,-5,0))return 2;
 alpha=memcmp(saved,d.current.layers[0].pixels,sizeof(saved))==0;
 printf("{\"destinationRestored\":%s,\"fractionalRoundTripExact\":%s}\n",restored?"true":"false",alpha?"true":"false");ie_destroy(&d);return 0;
}
