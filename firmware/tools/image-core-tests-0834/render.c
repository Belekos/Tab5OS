#define main legacy_main
#include "test_core.c"
#undef main
#include <math.h>
static int sample_ref(int x,int32_t origin,int zoom){long double value=((long double)origin*zoom+(long double)x*6553600)/((long double)zoom*65536);return(int)floorl(value);}
static uint16_t reference(const IEDoc*d,int x,int y,int32_t ox,int32_t oy,int zoom){int sx=sample_ref(x,ox,zoom),sy=sample_ref(y,oy,zoom);uint32_t c,bg;unsigned a,r,g,b;if(sx<0||sy<0||sx>=d->current.width||sy>=d->current.height)return 0x28c7;c=ie_composite(d,sx,sy);a=c>>24;bg=((sx/8+sy/8)&1)?0x8a7968:0xe4d3c2;r=((c&255)*a+((bg>>16)&255)*(255-a))/255;g=(((c>>8)&255)*a+((bg>>8)&255)*(255-a))/255;b=(((c>>16)&255)*a+(bg&255)*(255-a))/255;return(uint16_t)(((r&248)<<8)|((g&252)<<3)|(b>>3));}
int main(void){IEDoc d;Memory m;uint16_t out[131*83],saved[131*83];int zooms[]={5,10,17,25,33,50,67,93,100,125,200,400,1600,6400};int32_t origins[]={-40*65536,-1,0,1,57825,19*65536+31337,96*65536};int z,o,x,y,round;size_t calls;uint64_t hash;setup(&d,&m,96,80);CHECK(ie_layer_duplicate(&d));for(y=0;y<80;y++)for(x=0;x<96;x++){d.current.layers[0].pixels[y*96+x]=IE_RGBA(x*17%256,y*9%256,81,(x+y)%256);d.current.layers[1].pixels[y*96+x]=IE_RGBA(y*13%256,x*11%256,190,(x*y)%256);}CHECK(ie_layer_style(&d,111,1));hash=digest(&d);calls=m.calls;
 for(z=0;z<14;z++)for(o=0;o<7;o++){int32_t ox=origins[o],oy=origins[6-o];memset(out,0x55,sizeof(out));CHECK(ie_render_view_rgb565(&d,out,131,127,83,ox,oy,zooms[z],0,0,127,83,0xe4d3c2,0x8a7968,0x2c183c)==127u*83);
 for(y=0;y<83;y++)for(x=0;x<131;x++)CHECK(out[y*131+x]==(x>=127?0x5555:reference(&d,x,y,ox,oy,zooms[z])));
 memcpy(saved,out,sizeof(out));memset(out,0x55,sizeof(out));for(round=0;round<3;round++)CHECK(ie_render_view_rgb565(&d,out,131,127,83,ox,oy,zooms[z],round*43,0,43,83,0xe4d3c2,0x8a7968,0x2c183c)>0);CHECK(!memcmp(saved,out,sizeof(out)));CHECK(m.calls==calls);CHECK(digest(&d)==hash);
 }
 for(z=0;z<14;z++)for(o=0;o<7;o++)for(x=-140;x<140;x++)CHECK(ie_view_sample(x,origins[o],zooms[z])==sample_ref(x,origins[o],zooms[z]));
 CHECK(ie_view_sample(-1,57825,17)==-6);CHECK(!ie_render_view_rgb565(&d,out,126,127,83,0,0,100,0,0,127,83,0,0,0));CHECK(!ie_render_view_rgb565(&d,out,SIZE_MAX,127,83,0,0,100,0,0,127,83,0,0,0));CHECK(!ie_render_view_rgb565(&d,out,131,127,83,0,0,0,0,0,127,83,0,0,0));CHECK(!ie_render_view_rgb565(&d,out,131,127,83,0,0,100,INT_MAX,INT_MAX,INT_MAX,INT_MAX,0,0,0));cleanup(&d,&m);printf("{\"checks\":%u,\"viewportCases\":196,\"remainingBytes\":0,\"status\":\"passed\"}\n",checks);return 0;}
