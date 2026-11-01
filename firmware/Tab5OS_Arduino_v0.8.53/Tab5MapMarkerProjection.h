#pragma once
#include <cmath>
#include <algorithm>
namespace Tab5MapMarkerProjection {
inline bool project(double latitude,double longitude,double centerLatitude,double centerLongitude,unsigned zoom,int width,int height,int& x,int& y){
 if(!std::isfinite(latitude)||!std::isfinite(longitude)||!std::isfinite(centerLatitude)||!std::isfinite(centerLongitude)||latitude < -90||latitude>90||longitude < -180||longitude>180||zoom>24||width<=0||height<=0)return false;
 const double world=std::ldexp(256.0,int(zoom));
 auto worldY=[&](double lat){const double s=std::sin(std::max(-85.05112878,std::min(85.05112878,lat))*0.017453292519943295);return (0.5-std::log((1+s)/(1-s))/(4*3.141592653589793))*world;};
 double dx=(longitude-centerLongitude)*world/360.0;dx=std::remainder(dx,world);
 const double px=width*0.5+dx,py=height*0.5+worldY(latitude)-worldY(centerLatitude);
 if(px < -24||px > width+24||py < -24||py > height+24)return false;
 x=int(std::lround(px));y=int(std::lround(py));return true;
}
}
