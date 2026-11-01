#pragma once
#include <lvgl.h>
#include "tools/native-apps/image-editor/image_core.h"

namespace Tab5ImageText {
// Temporary A8 source and one output row, charged to the editor's allocator.
struct Raster {
 uint8_t* pixels=nullptr; uint8_t* row=nullptr;
 int x=0,y=0,width=0,height=0,stride=0,sourceX=0,sourceY=0,sourceW=0,sourceH=0;
 int originX=0,originY=0,scale=100,textW=0,textH=0;
};
bool prepare(Raster& out,IEDoc* doc,lv_obj_t* layout,const char* text,int x,int y,int scale);
void stamp(const Raster& image,IEDoc* doc,uint32_t color,int opacity);
void release(Raster& image,IEDoc* doc);
lv_point_t position(lv_obj_t* layout,uint32_t index,int scale);
int lineHeight(lv_obj_t* layout,int scale);
}
