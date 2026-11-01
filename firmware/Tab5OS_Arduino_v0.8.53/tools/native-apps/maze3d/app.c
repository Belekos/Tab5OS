#include "../common/runtime.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define W 320
#define H 200
#define SIDE 17
struct Game {uint32_t magic,level,seed;float x,y,angle;uint8_t cells[SIDE*SIDE];uint8_t keys;uint32_t crc;};
static struct Game game;static uint16_t* screen;static float depth[W];static const struct tab5_app_api2* api;
static float sine(float x){while(x>3.14159265f)x-=6.2831853f;while(x<-3.14159265f)x+=6.2831853f;if(x>1.57079633f)x=3.14159265f-x;if(x<-1.57079633f)x=-3.14159265f-x;float q=x*x;return x*(1+q*(-1.f/6+q*(1.f/120+q*(-1.f/5040+q/362880))));}
static float cosine(float x){return sine(x+1.57079633f);}
static const char* savePath="/Saves/Maze3D/last.state";
static uint32_t random_value(void){uint32_t x=game.seed;x^=x<<13;x^=x>>17;x^=x<<5;return game.seed=x?x:1;}
static uint16_t color(int r,int g,int b){if(r<0)r=0;if(g<0)g=0;if(b<0)b=0;if(r>255)r=255;if(g>255)g=255;if(b>255)b=255;return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
static int wall(int x,int y){return x<0||y<0||x>=SIDE||y>=SIDE||game.cells[y*SIDE+x]==1;}
static void new_game(void){
 uint32_t level=game.level+1,seed=game.seed?random_value():api->base.millis()+0x7392f9u;memset(&game,0,sizeof(game));game.magic=0x33475a4d;game.level=level;game.seed=seed;memset(game.cells,1,sizeof(game.cells));
 int stack[64],count=1;stack[0]=SIDE+1;game.cells[stack[0]]=0;
 while(count){int p=stack[count-1],x=p%SIDE,y=p/SIDE,candidates[4],n=0;const int dx[]={2,-2,0,0},dy[]={0,0,2,-2};for(int d=0;d<4;++d){int nx=x+dx[d],ny=y+dy[d];if(nx>0&&ny>0&&nx<SIDE-1&&ny<SIDE-1&&game.cells[ny*SIDE+nx])candidates[n++]=d;}
  if(!n){--count;continue;}int d=candidates[random_value()%n],next=(y+dy[d])*SIDE+x+dx[d];game.cells[next]=0;game.cells[(y+dy[d]/2)*SIDE+x+dx[d]/2]=0;stack[count++]=next;
 }
 game.cells[3*SIDE+3]=2;game.cells[11*SIDE+7]=2;game.cells[7*SIDE+13]=2;game.cells[15*SIDE+15]=3;game.keys=3;game.x=1.5f;game.y=1.5f;game.angle=0;
}
static void save_game(void){game.crc=tab5_crc32(0,&game,offsetof(struct Game,crc));int r=api->save(savePath,&game,sizeof(game));tab5_message(r?"Save failed.":"Game saved.",r?"Échec de la sauvegarde.":"Partie sauvegardée.");}
static void load_game(void){int f=api->base.open_read(savePath);int ok=f>0&&api->file_size(f)==sizeof(game)&&tab5_load(f,&game,sizeof(game));if(f>0)api->base.close(f);ok=ok&&game.magic==0x33475a4d&&game.crc==tab5_crc32(0,&game,offsetof(struct Game,crc))&&isfinite(game.x)&&isfinite(game.y)&&isfinite(game.angle)&&game.x>=1&&game.x<16&&game.y>=1&&game.y<16&&game.keys<=3;
 for(int i=0;ok&&i<SIDE*SIDE;++i)if(game.cells[i]>3||((i%SIDE==0||i%SIDE==16||i/SIDE==0||i/SIDE==16)&&game.cells[i]!=1))ok=0;
 if(!ok){memset(&game,0,sizeof(game));new_game();}
}
static void rect(int x,int y,int w,int h,uint16_t c){for(int yy=y;yy<y+h&&yy<H;++yy)if(yy>=0)for(int xx=x;xx<x+w&&xx<W;++xx)if(xx>=0)screen[yy*W+xx]=c;}
static void beep(void){int16_t samples[440*2];for(int i=0;i<440;++i)samples[2*i]=samples[2*i+1]=(i%28<14?2000:-2000)*(440-i)/440;api->audio_pcm(samples,440,22050);}
static void draw(int map){
 float dirx=cosine(game.angle),diry=sine(game.angle),planex=-diry*.66f,planey=dirx*.66f;
 for(int y=0;y<H;++y){uint16_t c=y<H/2?color(12+y/6,20+y/5,33+y/4):color(24+(y-H/2)/2,28+(y-H/2)/2,28+(y-H/2)/3);for(int x=0;x<W;++x)screen[y*W+x]=c;}
 for(int x=0;x<W;++x){float camera=2.f*x/W-1.f,rx=dirx+planex*camera,ry=diry+planey*camera;int mx=game.x,my=game.y,sx=rx<0?-1:1,sy=ry<0?-1:1;float dx=fabsf(rx)<1e-8f?1e8f:fabsf(1/rx),dy=fabsf(ry)<1e-8f?1e8f:fabsf(1/ry);float tx=(rx<0?game.x-mx:mx+1-game.x)*dx,ty=(ry<0?game.y-my:my+1-game.y)*dy;int side=0;
  for(int k=0;k<64;++k){if(tx<ty){tx+=dx;mx+=sx;side=0;}else{ty+=dy;my+=sy;side=1;}if(wall(mx,my))break;}
  float dist=side?ty-dy:tx-dx;if(dist<.05f)dist=.05f;depth[x]=dist;int height=H/dist,y0=(H-height)/2,y1=(H+height)/2;float along=side?game.x+dist*rx:game.y+dist*ry;int texture=(int)(along*16)&15;
  for(int y=y0<0?0:y0;y<y1&&y<H;++y){int row=(int)((y-y0)*16.f/height)&15;int brick=(row==0||texture==0)?45:170;float light=(side?.75f:1.f)/(1+dist*.1f);screen[y*W+x]=color(brick*light*.60f,brick*light*.76f,brick*light);}
 }
 for(int y=1;y<16;++y)for(int x=1;x<16;++x){int cell=game.cells[y*SIDE+x];if(cell<2)continue;float dx=x+.5f-game.x,dy=y+.5f-game.y,z=dx*dirx+dy*diry,side=-dx*diry+dy*dirx;if(z<=.2f)continue;int center=W/2+(side/(z*.66f))*W/2,size=(cell==2?32:80)/z;int y0=H/2-size/2;
  for(int xx=center-size/2;xx<center+size/2;++xx){if(xx<0||xx>=W||z>=depth[xx])continue;for(int yy=y0;yy<y0+size&&yy<H;++yy){if(yy<0)continue;if(cell==2&&abs(xx-center)+abs(yy-H/2)>size/2)continue;screen[yy*W+xx]=cell==2?color(245,200,50):color(20,game.keys?70:230,90);}}
 }
 for(unsigned i=0;i<3;++i)rect(10+i*16,10,10,10,i<game.keys?color(250,200,40):color(25,70,70));
 if(map){for(int y=0;y<SIDE;++y)for(int x=0;x<SIDE;++x){int c=game.cells[y*SIDE+x];rect(W-90+x*5,8+y*5,4,4,c==1?color(100,130,155):c==2?color(250,200,40):c==3?color(0,220,90):color(12,24,36));}rect(W-90+game.x*5,8+game.y*5,3,3,0xffff);}
}
__attribute__((visibility("default"))) int tab5_app_main(int argc,char** argv){(void)argc;(void)argv;api=tab5_app_api_v2();if(!api||api->base.size<sizeof(*api))return 1;tab5_runtime_init(api);if(__builtin_setjmp(tab5_escape))return 2;screen=api->base.allocate(W*H*2);if(!screen)return 3;load_game();if(api->video_begin(W,H))return 4;
 uint32_t last=api->base.millis(),previous=0,lastSaved=last;int map=0;
 while(!api->base.should_stop()){
  uint32_t now=api->base.millis(),keys=api->buttons();float dt=(now-last)*.001f;if(dt>.06f)dt=.06f;last=now;if(keys&256)break;if(keys&512)save_game();if((keys&1024)||((keys&128)&&!(previous&128)))new_game();if((keys&64)&&!(previous&64))map=!map;previous=keys;
  if(keys&1)game.angle+=dt*1.8f;if(keys&2)game.angle-=dt*1.8f;if(game.angle>6.2831853f)game.angle-=6.2831853f;if(game.angle<0)game.angle+=6.2831853f;
  float speed=dt*((keys&32)?3.f:1.8f),move=(keys&4?1.f:0.f)-(keys&8?1.f:0.f);float nx=game.x+cosine(game.angle)*speed*move,ny=game.y+sine(game.angle)*speed*move;
  if(!wall(nx+.16f,game.y)&&!wall(nx-.16f,game.y))game.x=nx;if(!wall(game.x,ny+.16f)&&!wall(game.x,ny-.16f))game.y=ny;
  int position=(int)game.y*SIDE+(int)game.x;if(game.cells[position]==2){game.cells[position]=0;if(game.keys)--game.keys;beep();}if(game.cells[position]==3&&!game.keys){beep();new_game();save_game();}
  draw(map);api->video_present(screen,W*H);if(now-lastSaved>30000){save_game();lastSaved=now;}int wait=33-(int)(api->base.millis()-now);api->base.sleep_ms(wait>0?wait:0);
 }save_game();api->base.release(screen);return 0;
}
