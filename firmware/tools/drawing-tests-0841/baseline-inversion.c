#include "image_core.h"
#include <stdio.h>
#include <stdlib.h>
static void* alloc(void*ctx,size_t n){(void)ctx;return malloc(n);}static void release(void*ctx,void*p){(void)ctx;free(p);}
int main(int argc,char**argv){(void)argv;IEDoc d;if(!ie_init(&d,alloc,release,0,16,12))return 2;IESelectionOptions q={0};q.method=IE_SELECT_RECT;q.combine=IE_SELECT_SUBTRACT;q.x0=4;q.y0=3;q.x1=10;q.y1=8;IEJob*j=ie_selection_start(&d,&q);if(!j)return 3;int result=0;while(!result)result=ie_job_step(j,31);unsigned inside=ie_selection_alpha(&d.current,7,5),outside=ie_selection_alpha(&d.current,0,0);printf("firstSubtractInside=%u firstSubtractOutside=%u\n",inside,outside);ie_job_destroy(j);ie_destroy(&d);return argc>1?(inside==255&&outside==0?0:1):(inside==0&&outside==255?0:1);}
