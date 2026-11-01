#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// UI-owned, no allocations. Coalesce fast USB reports before starting another
// tile generation. One notch is one zoom level; keep the place under the cursor.
namespace Tab5MapWheel {
struct View {double latitude=0,longitude=0;uint8_t zoom=0;};
inline bool valid(const View& v){return std::isfinite(v.latitude)&&std::isfinite(v.longitude)&&v.latitude>=-90&&v.latitude<=90&&v.longitude>=-180&&v.longitude<=180&&v.zoom<=24;}
inline bool same(const View& a,const View& b){return a.latitude==b.latitude&&a.longitude==b.longitude&&a.zoom==b.zoom;}
inline double worldY(double latitude,double world){
 const double s=std::sin(std::clamp(latitude,-85.05112878,85.05112878)*0.017453292519943295);
 return (0.5-std::log((1+s)/(1-s))/(4*3.141592653589793))*world;
}
inline bool zoomAt(const View& from,int steps,int x,int y,int width,int height,unsigned minimum,unsigned maximum,View& to){
 if(!valid(from)||width<=0||height<=0||x<0||x>=width||y<0||y>=height||minimum>maximum||maximum>24||!steps)return false;
 const int z=std::clamp(int(from.zoom)+std::clamp(steps,-24,24),int(minimum),int(maximum));
 if(z==from.zoom)return false;
 const double oldWorld=std::ldexp(256.0,int(from.zoom)),world=std::ldexp(256.0,z),scale=std::ldexp(1.0,z-int(from.zoom));
 const double dx=x-width*0.5,dy=y-height*0.5;
 const double centerX=((from.longitude+180.0)/360.0*oldWorld+dx)*scale-dx;
 const double centerY=(worldY(from.latitude,oldWorld)+dy)*scale-dy;
 double wrapped=std::fmod(centerX,world);if(wrapped<0)wrapped+=world;
 const double normalizedY=std::clamp(centerY/world,0.0,1.0);
 to.longitude=wrapped/world*360.0-180.0;
 to.latitude=std::atan(std::sinh(3.141592653589793*(1.0-2.0*normalizedY)))*57.29577951308232;
 to.zoom=uint8_t(z);return true;
}
class Pending {
 public:
  static constexpr uint32_t QuietMs=60,MaximumDelayMs=120;
  void cancel(){steps_=0;pending_=false;}
  bool push(int steps,int x,int y,int width,int height,const View& view,uint32_t now){
   if(!steps||!valid(view)||width<=0||height<=0||x<0||x>=width||y<0||y>=height)return false;
   if(!pending_||!same(view,base_)||width!=width_||height!=height_){cancel();base_=view;first_=now;width_=width;height_=height;}
   const int64_t sum=int64_t(steps_)+steps;steps_=int(std::clamp<int64_t>(sum,-24,24));
   x_=x;y_=y;last_=now;pending_=steps_!=0;return true;
  }
  bool take(uint32_t now,const View& current,int width,int height,unsigned minimum,unsigned maximum,bool enabled,View& to){
   if(!enabled||!same(current,base_)||width!=width_||height!=height_){cancel();return false;}
   if(!pending_||(uint32_t(now-last_)<QuietMs&&uint32_t(now-first_)<MaximumDelayMs))return false;
   const int steps=steps_;cancel();return zoomAt(current,steps,x_,y_,width,height,minimum,maximum,to);
  }
  bool pending()const{return pending_;}
 private:
  View base_{};uint32_t first_=0,last_=0;int steps_=0,x_=0,y_=0,width_=0,height_=0;bool pending_=false;
};
}
