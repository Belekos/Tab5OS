#pragma once
#include <lvgl.h>
#include "Tab5Map.h"
namespace Tab5MapOverlay {
bool attach(lv_obj_t* image);
void viewport(double latitude,double longitude,unsigned zoom,int width,int height);
void refresh();
bool hit(int x,int y,Tab5MapMarker& marker);
}
