#pragma once
#include <cstdint>
namespace tab5mapprint {
struct Plan {unsigned width=0,height=0,zoom=0;double left=0,top=0,scale=0;int64_t firstX=0,lastX=0,firstY=0,lastY=0;unsigned tiles=0;};
Plan jpegPlan(double x,double y,unsigned viewZoom,unsigned maxZoom);
Plan plan(double centerX,double centerY,unsigned viewZoom,unsigned maxZoom,double paperWidthMm,double paperHeightMm,unsigned dpi);
}
