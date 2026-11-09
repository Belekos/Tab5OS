/* Exact nearest-neighbour presentation without a division for every pixel.
 * Normal VGA modes fit the canvas and decode directly into the destination. */
#pragma once
#include <stdint.h>
static void dos_present_screen(uint16_t *destination,int stride,int width,int height,
                               int drawn_width,int drawn_height,uint16_t *scratch,
                               void (*render_line)(int,uint16_t *))
{
    if(width==drawn_width&&height==drawn_height){
        for(int y=0;y<height;y++)render_line(y,destination+y*stride);
        return;
    }
    uint16_t columns[800];
    if(width!=drawn_width){
        int source=0,error=0;
        for(int x=0;x<drawn_width;x++){
            columns[x]=(uint16_t)source;error+=width;
            while(error>=drawn_width){error-=drawn_width;source++;}
        }
    }
    int source_y=0,error_y=0;
    for(int y=0;y<drawn_height;y++){
        uint16_t *out=destination+y*stride;
        if(width==drawn_width)render_line(source_y,out);
        else {
            render_line(source_y,scratch);
            for(int x=0;x<drawn_width;x++)out[x]=scratch[columns[x]];
        }
        error_y+=height;
        while(error_y>=drawn_height){error_y-=drawn_height;source_y++;}
    }
}
