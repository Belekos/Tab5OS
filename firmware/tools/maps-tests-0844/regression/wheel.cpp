#include "Tab5MapWheel.h"
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <limits>
using namespace Tab5MapWheel;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);abort();}}while(0)
static void anchor(const View& v,int x,int y,double& longitude,double& worldLatitude){const double world=std::ldexp(256.0,v.zoom);longitude=(v.longitude+180)/360.0+(x-566)/world;worldLatitude=worldY(v.latitude,world)/world+(y-300)/world;}
int main(){View out,from{48.8,2.3,12};CHECK(zoomAt(from,1,566,300,1132,600,0,22,out));CHECK(out.zoom==13&&fabs(out.latitude-from.latitude)<1e-9&&fabs(out.longitude-from.longitude)<1e-9);
 for(int lat=-65;lat<=65;lat+=5)for(int lon=-180;lon<180;lon+=15)for(unsigned z=4;z<=20;++z)for(int steps:{-2,-1,1,2})for(int corner=0;corner<4;++corner){View v{double(lat),double(lon),uint8_t(z)};int x=corner&1?1050:80,y=corner&2?540:60;CHECK(zoomAt(v,steps,x,y,1132,600,0,22,out));CHECK(valid(out)&&out.zoom==int(z)+steps);double ax,ay,bx,by;anchor(v,x,y,ax,ay);anchor(out,x,y,bx,by);CHECK(fabs(std::remainder(ax-bx,1.0))<1e-10&&fabs(ay-by)<1e-10);}
 for(double lat:{-90.,90.})for(double lon:{-180.,180.}){View v{lat,lon,22};CHECK(zoomAt(v,-22,0,0,1132,600,0,22,out));CHECK(valid(out)&&out.zoom==0);}
 CHECK(!zoomAt(from,1,-1,20,1132,600,0,22,out));CHECK(!zoomAt(from,1,1132,20,1132,600,0,22,out));CHECK(!zoomAt(from,1,10,20,0,600,0,22,out));CHECK(!zoomAt(from,1,10,20,1132,600,23,22,out));CHECK(!zoomAt(from,1,10,20,1132,600,0,25,out));CHECK(!zoomAt(from,0,10,20,1132,600,0,22,out));CHECK(zoomAt(from,INT_MAX,566,300,1132,600,0,22,out)&&out.zoom==22);CHECK(!zoomAt(out,1,566,300,1132,600,0,22,from));from={48.8,2.3,12};CHECK(zoomAt(from,INT_MIN,566,300,1132,600,0,22,out)&&out.zoom==0);
 View bad=from;bad.latitude=std::numeric_limits<double>::quiet_NaN();CHECK(!zoomAt(bad,1,20,20,1132,600,0,22,out));bad=from;bad.longitude=181;CHECK(!zoomAt(bad,1,20,20,1132,600,0,22,out));
 Pending p;CHECK(sizeof(p)<=96);CHECK(p.push(1,566,300,1132,600,from,100));CHECK(!p.take(159,from,1132,600,0,22,true,out));CHECK(p.take(160,from,1132,600,0,22,true,out)&&out.zoom==13);CHECK(!p.take(300,from,1132,600,0,22,true,out));
 CHECK(p.push(1,566,300,1132,600,from,400));CHECK(p.push(-1,566,300,1132,600,from,410));CHECK(!p.pending());CHECK(!p.take(900,from,1132,600,0,22,true,out));
 CHECK(p.push(1,566,300,1132,600,from,1000));CHECK(!p.take(1100,from,1132,600,0,22,false,out)&&!p.pending());CHECK(p.push(1,566,300,1132,600,from,1200));View changed=from;changed.longitude+=1;CHECK(!p.take(1300,changed,1132,600,0,22,true,out)&&!p.pending());CHECK(p.push(1,566,300,1132,600,from,1400));CHECK(!p.take(1500,from,1200,600,0,22,true,out)&&!p.pending());
 CHECK(p.push(1,566,300,1132,600,from,UINT32_MAX-20));CHECK(!p.take(20,from,1132,600,0,22,true,out));CHECK(p.take(40,from,1132,600,0,22,true,out));
 unsigned generations=0;uint32_t last=0;View current{0,0,10};for(uint32_t now=2000;now<7000;now+=5){int steps=(now/150)%2?1:-1;CHECK(p.push(steps,566,300,1132,600,current,now));if(p.take(now,current,1132,600,0,22,true,out)){CHECK(!last||now-last>=120);last=now;current=out;++generations;}}CHECK(generations<=42);p.cancel();CHECK(!p.pending());
 printf("checks=%u PASS; burst generations=%u; pending bytes=%zu\n",checks,generations,sizeof(p));
}
