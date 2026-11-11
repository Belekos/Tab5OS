/* Exact nearest-neighbour presentation without a division for every pixel.
 * Repeated output rows reuse the decoded row, including 320x200 VGA enlarged
 * to the 4:3 display shape of its original monitor (non-square guest pixels). */
#pragma once
#include <stdint.h>
#include <string.h>
struct dos_viewport {int x,y,w,h;};
static struct dos_viewport dos_display_viewport(int width,int height,int text_mode,int keyboard)
{
    struct dos_viewport view={0,0,0,0};
    if(width<=0||width>1024||height<=0||height>768)return view;
    const int area_width=keyboard||text_mode?800:624;
    const int area_height=keyboard?252:text_mode?400:468;
    if(text_mode){
        /* Keep all glyph columns at the DOS prompt whenever they fit. */
        view.w=width;view.h=height;
        if(view.w>area_width){view.h=view.h*area_width/view.w;view.w=area_width;}
        if(view.h>area_height){view.w=view.w*area_height/view.h;view.h=area_height;}
    }else{
        view.h=area_height;view.w=view.h*4/3;
        if(view.w>area_width){view.w=area_width;view.h=view.w*3/4;}
    }
    view.x=(area_width-view.w)/2;view.y=(area_height-view.h)/2;
    return view;
}
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
    int source_y=0,error_y=0,last_source_y=-1;
    for(int y=0;y<drawn_height;y++){
        uint16_t *out=destination+y*stride;
        if(source_y==last_source_y)memcpy(out,out-stride,(size_t)drawn_width*sizeof(*out));
        else if(width==drawn_width)render_line(source_y,out);
        else {
            render_line(source_y,scratch);
            for(int x=0;x<drawn_width;x++)out[x]=scratch[columns[x]];
        }
        last_source_y=source_y;
        error_y+=height;
        while(error_y>=drawn_height){error_y-=drawn_height;source_y++;}
    }
}
