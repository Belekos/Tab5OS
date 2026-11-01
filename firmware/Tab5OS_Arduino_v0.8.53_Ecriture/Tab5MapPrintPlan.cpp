#include "Tab5MapPrintPlan.h"
#include <algorithm>
#include <cmath>
namespace tab5mapprint {
Plan plan(double x,double y,unsigned viewZoom,unsigned maxZoom,double paperW,double paperH,unsigned dpi){
 Plan p;if(viewZoom>22||maxZoom>22||dpi<72||dpi>600||paperW<50||paperH<50||paperW>1000||paperH>1000||!std::isfinite(x)||!std::isfinite(y))return p;
 const double target=std::min((paperW-16)*dpi/25.4,(paperH-16)*dpi/25.4*1172/614);
 int desired=int(viewZoom)+int(std::ceil(std::log2(target/1172)));
 p.zoom=std::min(maxZoom,unsigned(std::max(0,desired)));
 for(;;){double factor=std::ldexp(1.0,int(p.zoom)-int(viewZoom));
  p.left=(x-586)*factor;p.top=(y-288)*factor;
  p.firstX=int64_t(std::floor(p.left/256));p.lastX=int64_t(std::floor((p.left+1172*factor-1)/256));
  p.firstY=std::max<int64_t>(0,int64_t(std::floor(p.top/256)));p.lastY=std::min<int64_t>((1u<<p.zoom)-1,int64_t(std::floor((p.top+576*factor-1)/256)));
  p.tiles=unsigned((p.lastX-p.firstX+1)*std::max<int64_t>(0,p.lastY-p.firstY+1));
  if(p.tiles<=256||!p.zoom){p.width=std::max(1u,unsigned(std::min(target,1172*factor)));p.height=std::max(1u,unsigned(std::lround(p.width*576.0/1172)));p.scale=p.width/(1172*factor);return p;}--p.zoom;
 }
}
}

namespace tab5mapprint {
Plan jpegPlan(double x,double y,unsigned viewZoom,unsigned maxZoom){
 Plan p;if(viewZoom>22||maxZoom>22||!std::isfinite(x)||!std::isfinite(y))return p;
 p.zoom=std::min(viewZoom+1,maxZoom);double factor=std::ldexp(1.0,int(p.zoom)-int(viewZoom));
 p.left=(x-586)*factor;p.top=(y-288)*factor;p.width=std::max(1u,unsigned(std::lround(1172*factor)));p.height=std::max(1u,unsigned(std::lround(576*factor)));p.scale=1;
 p.firstX=int64_t(std::floor(p.left/256));p.lastX=int64_t(std::floor((p.left+p.width-1)/256));
 p.firstY=std::max<int64_t>(0,int64_t(std::floor(p.top/256)));p.lastY=std::min<int64_t>((1u<<p.zoom)-1,int64_t(std::floor((p.top+p.height-1)/256)));
 p.tiles=unsigned((p.lastX-p.firstX+1)*std::max<int64_t>(0,p.lastY-p.firstY+1));return p;
}
}
