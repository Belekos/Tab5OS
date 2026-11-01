/* Atelier Image: cooperative, bounded, native Tab5OS ABI6 application. */
#include "Tab5AppApi.h"
#include "image_core.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define W 800
#define H 480
#define CX 8
#define CY 54
#define CW 580
#define CH 346
#define ARENA_BYTES (16u*1024u*1024u)
TAB5_APP_STACK(48u*1024u);
static const struct tab5_app_api6 *api;
static const struct tab5_app_api *base;
static const struct tab5_app_api2 *video;
static uint16_t *pixels;
static IEDoc doc;
struct Block {size_t bytes;struct Block* next;int used;uint32_t align;};
struct Chunk {size_t bytes;struct Chunk* next;uint32_t align[2];};
static struct Chunk* chunks;
static size_t memory_bytes;
static unsigned chunk_count;
/* Fallible PSRAM arena in independent chunks. Empty chunks return immediately
   to the OS, including after history eviction and document replacement. */
static void* memory_alloc(void* context,size_t n){
 (void)context;if(!n)n=1;if(n>ARENA_BYTES-64)return NULL;n=(n+15)&~(size_t)15;
 for(struct Chunk* c=chunks;c;c=c->next)for(struct Block* b=(void*)(c+1);b;b=b->next){if(b->used||b->bytes<n||(n<65536&&c->bytes>262144))continue;
  if(b->bytes>=n+sizeof(*b)+32){struct Block* next=(void*)((char*)(b+1)+n);*next=(struct Block){b->bytes-n-sizeof(*b),b->next,0,0};b->bytes=n;b->next=next;}
  b->used=1;return b+1;
 }
 size_t need=n+sizeof(struct Chunk)+sizeof(struct Block),size=need<262144?262144:(need+4095u)&~(size_t)4095u;
 if(chunk_count>=30||need>ARENA_BYTES-memory_bytes)return NULL;
 if(size>ARENA_BYTES-memory_bytes)size=ARENA_BYTES-memory_bytes;
 struct Chunk* c=base->allocate(size);if(!c)return NULL;*c=(struct Chunk){size,chunks,{0,0}};chunks=c;memory_bytes+=size;chunk_count++;
 struct Block* b=(void*)(c+1);*b=(struct Block){size-sizeof(*c)-sizeof(*b),NULL,0,0};
 return memory_alloc(context,n);
}
static void memory_free(void* context,void* p){
 (void)context;if(!p)return;struct Chunk** at=&chunks;
 while(*at){struct Chunk* c=*at;if((uintptr_t)p>(uintptr_t)c&&(uintptr_t)p<(uintptr_t)c+c->bytes){
  ((struct Block*)p-1)->used=0;struct Block* first=(void*)(c+1);
  for(struct Block* b=first;b&&b->next;){if(!b->used&&!b->next->used){b->bytes+=sizeof(*b)+b->next->bytes;b->next=b->next->next;}else b=b->next;}
  if(!first->used&&!first->next){*at=c->next;memory_bytes-=c->bytes;chunk_count--;base->release(c);}return;
 }at=&c->next;}
}
static int fr,tab,tool,fill=1,outline=0,thickness=2,text_scale=3,red=22,green=90,blue=180;
static int brightness,contrast,saturation,gain_r=100,gain_g=100,gain_b=100,all_layers;
static int scale_x=100,scale_y=100,shear,turns,new_w=640,new_h=360;
static int active_slider=-1,dragging,gesture,gesture_dirty,sx,sy,lx,ly,exit_requested,redraw=1;
static IEBrush brush={0,16,100,85,0,0,0};
static char status[160],project_path[256],text_value[160]="Texte";
static int dx,dy,dw,dh;
enum {TOOL_BRUSH,TOOL_ERASE,TOOL_SELECT,TOOL_MOVE,TOOL_RECT,TOOL_ELLIPSE,TOOL_LINE,TOOL_TEXT,TOOL_PICK};
enum {A_TAB=1,A_TOOL,A_NEW,A_OPEN,A_SAVE,A_BMP,A_UNDO,A_REDO,A_HELP,A_QUIT,A_FILL,A_OUTLINE,A_ROUND,A_PALETTE,A_LAYER,A_VIS,A_ADD,A_DUP,A_DEL,A_UP,A_DOWN,A_MERGE,A_CLEAR,A_APPLY,A_ALL,A_TRANSFORM,A_ROTATE,A_RESET_TRANSFORM,A_DESELECT,A_WIDTH,A_HEIGHT,A_RESIZE,A_CROP,A_TEXT,A_RENAME,A_CONFIRM,A_CANCEL,A_INPUT,A_CHAR,A_BACKSPACE,A_SHIFT,A_LAYER_OPACITY,A_SLIDER};
enum {SL_SIZE,SL_OPACITY,SL_HARDNESS,SL_DIFFUSION,SL_RED,SL_GREEN,SL_BLUE,SL_BRIGHT,SL_CONTRAST,SL_SATURATION,SL_GAIN_R,SL_GAIN_G,SL_GAIN_B,SL_SCALE_X,SL_SCALE_Y,SL_SHEAR,SL_THICKNESS,SL_TEXT_SCALE,SL_LAYER_OPACITY};
struct Hit {int x,y,w,h,action,value;};
static struct Hit hits[128];static int hit_count;
static int modal,modal_action,confirm_action,shift;
static char input[256],prompt[80],pending_path[256];
static const char* tr(const char* en,const char* french){return fr?french:en;}
static int clamp(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
static void copy(char* out,size_t cap,const char* in){size_t n=strlen(in);if(n>=cap)n=cap-1;memcpy(out,in,n);out[n]=0;}
static uint16_t rgb(int r,int g,int b){return (uint16_t)(((r&248)<<8)|((g&252)<<3)|(b>>3));}
static uint16_t bg,card,edge,ink,muted,accent;
static void rect(int x,int y,int w,int h,uint16_t c){int x1=clamp(x+w,0,W),y1=clamp(y+h,0,H);x=clamp(x,0,W);y=clamp(y,0,H);for(int yy=y;yy<y1;++yy)for(int xx=x;xx<x1;++xx)pixels[yy*W+xx]=c;}
static void frame(int x,int y,int w,int h,uint16_t c){rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c);}
static unsigned next_char(const unsigned char** p){unsigned c=*(*p)++;if(c<128)return c;if(c>=0xc2&&c<=0xdf&&(**p&0xc0)==0x80){unsigned b=*(*p)++;return((c&31)<<6)|(b&63);}if(c>=0xe0&&c<=0xef&&(**p&0xc0)==0x80&&((*p)[1]&0xc0)==0x80){unsigned a=*(*p)++,b=*(*p)++;return((c&15)<<12)|((a&63)<<6)|(b&63);}return '?';}
static void text(int x,int y,const char* s,int scale,uint16_t c){
 const unsigned char* p=(const unsigned char*)s;
 while(*p&&x<W){unsigned ch=next_char(&p);for(int row=0;row<7;++row){uint8_t bits=ie_glyph_row(ch,row);for(int col=0;col<5;++col)if(bits&(1u<<(4-col)))rect(x+col*scale,y+row*scale,scale,scale,c);}x+=6*scale;}
}
static void hit(int x,int y,int w,int h,int action,int value){if(hit_count<128)hits[hit_count++]=(struct Hit){x,y,w,h,action,value};}
static void button(int x,int y,int w,int h,const char* label,int action,int value,int selected){rect(x,y,w,h,selected?accent:card);frame(x,y,w,h,selected?accent:edge);int len=(int)strlen(label);int scale=len*12<=w-8?2:1;text(x+(w-len*6*scale)/2,y+(h-7*scale)/2,label,scale,ink);hit(x,y,w,h,action,value);}
static int slider_value(int id){switch(id){case SL_SIZE:return brush.size;case SL_OPACITY:return brush.opacity;case SL_HARDNESS:return brush.hardness;case SL_DIFFUSION:return brush.diffusion;case SL_RED:return red;case SL_GREEN:return green;case SL_BLUE:return blue;case SL_BRIGHT:return brightness;case SL_CONTRAST:return contrast;case SL_SATURATION:return saturation;case SL_GAIN_R:return gain_r;case SL_GAIN_G:return gain_g;case SL_GAIN_B:return gain_b;case SL_SCALE_X:return scale_x;case SL_SCALE_Y:return scale_y;case SL_SHEAR:return shear;case SL_THICKNESS:return thickness;case SL_TEXT_SCALE:return text_scale;case SL_LAYER_OPACITY:return doc.current.layers[doc.current.active].opacity;}return 0;}
static void slider_range(int id,int* lo,int* hi){*lo=0;*hi=100;if(id==SL_SIZE){*lo=1;*hi=128;}if(id==SL_RED||id==SL_GREEN||id==SL_BLUE||id==SL_LAYER_OPACITY)*hi=255;if(id==SL_BRIGHT||id==SL_CONTRAST||id==SL_SATURATION)*lo=-100;if(id>=SL_GAIN_R&&id<=SL_GAIN_B)*hi=200;if(id==SL_SCALE_X||id==SL_SCALE_Y){*lo=25;*hi=400;}if(id==SL_SHEAR){*lo=-100;*hi=100;}if(id==SL_THICKNESS){*lo=1;*hi=48;}if(id==SL_TEXT_SCALE){*lo=1;*hi=16;}}
static void slider(int y,const char* label,int id){int value=slider_value(id),lo,hi;slider_range(id,&lo,&hi);char out[64];snprintf(out,sizeof(out),"%s %d",label,value);text(608,y,out,strlen(out)<=14?2:1,ink);rect(608,y+18,176,4,edge);int x=608+(value-lo)*176/(hi-lo);rect(x-4,y+11,8,18,accent);hit(602,y+7,188,28,A_SLIDER,id);}
static void message(const char* s){copy(status,sizeof(status),s);redraw=1;}
static int result(int ok){if(!ok)message(ie_error(&doc));else {message(tr("Done - Undo available","Fait - Annuler disponible"));}return ok;}
static void cooperate(void* unused){(void)unused;(void)base->millis();}
static uint32_t paint_color(void){return IE_RGBA(red,green,blue,255);}
static void layout_canvas(void){dw=CW;dh=doc.current.height*dw/doc.current.width;if(dh>CH){dh=CH;dw=doc.current.width*dh/doc.current.height;}if(dw<1)dw=1;if(dh<1)dh=1;dx=CX+(CW-dw)/2;dy=CY+(CH-dh)/2;}
static int image_x(int x){return clamp((x-dx)*doc.current.width/dw,0,doc.current.width-1);}
static int image_y(int y){return clamp((y-dy)*doc.current.height/dh,0,doc.current.height-1);}
static void selection_frame(int x,int y,int w,int h,uint16_t c){int xx=dx+x*dw/doc.current.width,yy=dy+y*dh/doc.current.height;int ww=w*dw/doc.current.width,hh=h*dh/doc.current.height;frame(xx,yy,ww>0?ww:1,hh>0?hh:1,c);}
static void render_canvas(void){rect(CX,CY,CW,CH,rgb(14,20,28));layout_canvas();for(int y=0;y<dh;++y){if(!(y&15))(void)base->millis();for(int x=0;x<dw;++x){uint32_t c=ie_composite(&doc,x*doc.current.width/dw,y*doc.current.height/dh);unsigned a=c>>24,check=((x/10+y/10)&1)?210:245;unsigned r=((c&255)*a+check*(255-a))/255,g=(((c>>8)&255)*a+check*(255-a))/255,b=(((c>>16)&255)*a+check*(255-a))/255;pixels[(dy+y)*W+dx+x]=rgb(r,g,b);}}frame(dx-1,dy-1,dw+2,dh+2,edge);
 if(doc.current.selection.enabled){IERect s=doc.current.selection;selection_frame(s.x,s.y,s.w,s.h,rgb(255,205,40));}
 if(dragging&&tool>=TOOL_SELECT&&tool<=TOOL_LINE&&tool!=TOOL_TEXT){int x=sx<lx?sx:lx,y=sy<ly?sy:ly;selection_frame(x,y,(sx>lx?sx-lx:lx-sx)+1,(sy>ly?sy-ly:ly-sy)+1,rgb(80,220,255));}
}
static void open_input(int action,const char* title,const char* initial){modal=1;modal_action=action;copy(prompt,sizeof(prompt),title);copy(input,sizeof(input),initial);shift=0;active_slider=-1;redraw=1;}
static void ask(int action,const char* title){modal=2;confirm_action=action;copy(prompt,sizeof(prompt),title);redraw=1;}
static void request_action(int action){if(doc.dirty&&(action==A_NEW||action==A_OPEN||action==A_QUIT)){ask(action,tr("Discard unsaved changes?","Abandonner les modifications ?"));return;}if(action==A_QUIT){exit_requested=1;return;}if(action==A_NEW){if(result(ie_new(&doc,new_w,new_h,IE_RGBA(255,255,255,255)))){project_path[0]=0;doc.dirty=0;}return;}open_input(A_OPEN,tr("Open BMP or T5I: SD path","Ouvrir BMP ou T5I : chemin SD"),"/Picture/");}
static size_t read_bytes(void* context,void* data,size_t n){int handle=*(int*)context;size_t done=0;while(done<n&&!base->should_stop()){size_t part=n-done;if(part>4096)part=4096;int r=base->read(handle,(uint8_t*)data+done,part);if(r<=0)break;done+=(size_t)r;(void)base->millis();}return done;}
static size_t write_bytes(void* context,const void* data,size_t n){int handle=*(int*)context;size_t done=0;while(done<n&&!base->should_stop()){size_t part=n-done;if(part>4096)part=4096;int r=api->write_chunk(handle,(const uint8_t*)data+done,part);if(r<=0)break;done+=(size_t)r;(void)base->millis();}return done;}
static int suffix(const char* path,const char* ext){size_t n=strlen(path),m=strlen(ext);if(n<m)return 0;for(size_t i=0;i<m;i++){char a=path[n-m+i],b=ext[i];if(a>='A'&&a<='Z')a+=32;if(a!=b)return 0;}return 1;}
static void open_file(const char* path){if(strlen(path)>=sizeof(project_path)){message(tr("Path too long","Chemin trop long"));return;}int f=base->open_read(path);if(f<=0){message(tr("Cannot open file","Ouverture impossible"));return;}int ok=suffix(path,".t5i")?ie_load_project(&doc,read_bytes,&f):suffix(path,".bmp")?ie_load_bmp(&doc,read_bytes,&f):0;base->close(f);if(!ok){message(suffix(path,".bmp")||suffix(path,".t5i")?ie_error(&doc):tr("Use BMP or T5I format","Format BMP ou T5I requis"));return;}new_w=doc.current.width;new_h=doc.current.height;project_path[0]=0;if(suffix(path,".t5i"))copy(project_path,sizeof(project_path),path);doc.dirty=0;message(tr("Image opened","Image ouverte"));}
static void save_file(const char* path,int format){int f=api->write_begin(path);if(f<=0){message(tr("Cannot write /Picture file","Ecriture /Picture impossible"));return;}int ok=format==A_BMP?ie_save_bmp(&doc,write_bytes,&f):ie_save_project(&doc,write_bytes,&f);int final=api->write_finish(f,ok?1:0);if(!ok||final<0){message(tr("Save failed; previous file preserved","Echec : ancien fichier conserve"));return;}if(format==A_SAVE){copy(project_path,sizeof(project_path),path);doc.dirty=0;}message(path);}
static void input_submit(void){char value[256];copy(value,sizeof(value),input);int action=modal_action;modal=0;
 if(action==A_OPEN){open_file(value);return;}
 if(action==A_SAVE||action==A_BMP){char path[256];const char* ext=action==A_SAVE?".t5i":".bmp";if(value[0]=='/')copy(path,sizeof(path),value);else {int n=snprintf(path,sizeof(path),"/Picture/%s",value);if(n<0||(size_t)n>=sizeof(path)){message(tr("Name too long","Nom trop long"));return;}}if(!suffix(path,ext)){size_t n=strlen(path);if(n+strlen(ext)>=sizeof(path)){message(tr("Name too long","Nom trop long"));return;}copy(path+n,sizeof(path)-n,ext);}size_t prefix=!strncmp(path,"/Picture/",9)?9:!strncmp(path,"/Images/",8)?8:0;if(!prefix||strchr(path+prefix,'/')||strchr(path,'\\')){message(tr("Save in /Picture, no subfolder","Enregistrer dans /Picture sans sous-dossier"));return;}copy(pending_path,sizeof(pending_path),path);int f=base->open_read(path);if(f>0){base->close(f);ask(action,tr("Replace existing file?","Remplacer le fichier existant ?"));return;}save_file(path,action);return;}
 if(action==A_TEXT){copy(text_value,sizeof(text_value),value);tool=TOOL_TEXT;message(tr("Tap canvas to insert text","Touchez l'image pour inserer le texte"));return;}
 if(action==A_RENAME){if(!value[0])return;if(ie_begin(&doc)){copy(doc.current.layers[doc.current.active].name,sizeof(doc.current.layers[0].name),value);doc.dirty=1;}else message(ie_error(&doc));return;}
 int n=0;for(const char* p=value;*p;p++){if(*p<'0'||*p>'9'||n>1024){n=-1;break;}n=n*10+*p-'0';}if(n<1||n>1024){message(tr("Resolution: 1 to 1024 pixels","Resolution : 1 a 1024 pixels"));return;}if(action==A_WIDTH)new_w=n;if(action==A_HEIGHT)new_h=n;
}
static void set_slider(int id,int x){int lo,hi;slider_range(id,&lo,&hi);int v=lo+clamp(x-608,0,176)*(hi-lo)/176;switch(id){case SL_SIZE:brush.size=v;break;case SL_OPACITY:brush.opacity=v;break;case SL_HARDNESS:brush.hardness=v;break;case SL_DIFFUSION:brush.diffusion=v;break;case SL_RED:red=v;break;case SL_GREEN:green=v;break;case SL_BLUE:blue=v;break;case SL_BRIGHT:brightness=v;break;case SL_CONTRAST:contrast=v;break;case SL_SATURATION:saturation=v;break;case SL_GAIN_R:gain_r=v;break;case SL_GAIN_G:gain_g=v;break;case SL_GAIN_B:gain_b=v;break;case SL_SCALE_X:scale_x=v;break;case SL_SCALE_Y:scale_y=v;break;case SL_SHEAR:shear=v;break;case SL_THICKNESS:thickness=v;break;case SL_TEXT_SCALE:text_scale=v;break;case SL_LAYER_OPACITY:doc.current.layers[doc.current.active].opacity=(uint8_t)v;doc.dirty=1;break;}redraw=1;}
static const uint32_t palette[]={0xff000000,0xffffffff,0xff3030e8,0xff30a0ff,0xff40e8f8,0xff60c020,0xffc8c020,0xffdb8620,0xffd04090,0xffa020d0,0xff808080,0xff4060a0};
static void activate(int action,int value){char out[32];redraw=1;
 switch(action){case A_TAB:tab=value;break;case A_TOOL:tool=value;break;case A_NEW:case A_OPEN:case A_QUIT:request_action(action);break;
 case A_SAVE:open_input(A_SAVE,tr("Project name (.t5i)","Nom du projet (.t5i)"),project_path[0]?project_path:"/Picture/Image.t5i");break;
 case A_BMP:open_input(A_BMP,tr("Flattened BMP on white","BMP aplati sur fond blanc"),"/Picture/Image.bmp");break;
 case A_UNDO:result(ie_undo(&doc));break;case A_REDO:result(ie_redo(&doc));break;case A_HELP:modal=3;break;
 case A_FILL:fill=!fill;if(!fill&&!outline)outline=1;break;case A_OUTLINE:outline=!outline;if(!fill&&!outline)fill=1;break;case A_ROUND:brush.square=value;break;
 case A_PALETTE:red=palette[value]&255;green=(palette[value]>>8)&255;blue=(palette[value]>>16)&255;break;
 case A_LAYER:doc.current.active=value;break;case A_VIS:if(ie_begin(&doc))doc.current.layers[value].visible=!doc.current.layers[value].visible;else message(ie_error(&doc));break;
 case A_ADD:snprintf(out,sizeof(out),"%s %d",tr("Layer","Calque"),doc.current.count+1);result(ie_layer_add(&doc,out));break;case A_DUP:result(ie_layer_duplicate(&doc));break;case A_DEL:result(ie_layer_delete(&doc));break;case A_UP:result(ie_layer_move(&doc,1));break;case A_DOWN:result(ie_layer_move(&doc,-1));break;case A_MERGE:result(ie_layer_merge_down(&doc));break;case A_CLEAR:result(ie_clear(&doc));break;
 case A_APPLY:result(ie_adjust(&doc,brightness,contrast,saturation,gain_r,gain_g,gain_b,all_layers));break;case A_ALL:all_layers=!all_layers;break;
 case A_TRANSFORM:result(ie_transform(&doc,scale_x,scale_y,shear,turns));break;case A_ROTATE:turns=(turns+1)%4;break;case A_RESET_TRANSFORM:scale_x=scale_y=100;shear=turns=0;break;case A_DESELECT:ie_deselect(&doc);break;
 case A_WIDTH:case A_HEIGHT:snprintf(out,sizeof(out),"%d",action==A_WIDTH?new_w:new_h);open_input(action,tr("Pixels, 1..1024","Pixels, 1..1024"),out);break;
 case A_RESIZE:result(ie_resize(&doc,new_w,new_h));break;case A_CROP:if(doc.current.selection.enabled){IERect s=doc.current.selection;result(ie_crop(&doc,s.x,s.y,s.w,s.h));new_w=doc.current.width;new_h=doc.current.height;}else message(tr("Drag a selection first","Tracez d'abord une selection"));break;
 case A_TEXT:open_input(A_TEXT,tr("Text to insert","Texte a inserer"),text_value);break;case A_RENAME:open_input(A_RENAME,tr("Layer name","Nom du calque"),doc.current.layers[doc.current.active].name);break;
 case A_CONFIRM:{int a=confirm_action;modal=0;if(a==A_SAVE||a==A_BMP)save_file(pending_path,a);else if(a==A_QUIT)exit_requested=1;else if(a==A_NEW){if(result(ie_new(&doc,new_w,new_h,IE_RGBA(255,255,255,255)))){project_path[0]=0;doc.dirty=0;}}else if(a==A_OPEN)open_input(A_OPEN,tr("Open BMP or T5I: SD path","Ouvrir BMP ou T5I : chemin SD"),"/Picture/");break;}
 case A_CANCEL:modal=0;break;case A_INPUT:input_submit();break;
 case A_BACKSPACE:{size_t n=strlen(input);if(n){do{--n;}while(n&&((unsigned char)input[n]&0xc0)==0x80);input[n]=0;}break;}
 case A_CHAR:{char c=(char)value;if(shift&&c>='a'&&c<='z')c-=32;size_t n=strlen(input);if(n+1<sizeof(input)){input[n]=c;input[n+1]=0;}break;}case A_SHIFT:shift=!shift;break;
 }
}
static void render_panel(void){char out[64];rect(598,54,194,380,card);
 if(tab==0){
  const char* en[]={"Brush","Eraser","Select","Move","Rectangle","Ellipse","Line","Text","Pick color"};const char* french[]={"Pinceau","Gomme","Selection","Deplacer","Rectangle","Ellipse","Ligne","Texte","Pipette"};
  for(int i=0;i<9;i++)button(604+(i%2)*92,62+(i/2)*42,88,36,fr?french[i]:en[i],A_TOOL,i,tool==i);
  button(604,278,88,32,tr("Fill","Plein"),A_FILL,0,fill);button(696,278,88,32,tr("Outline","Contour"),A_OUTLINE,0,outline);slider(324,tr("Line px","Trait px"),SL_THICKNESS);
  button(604,370,180,34,tr("Deselect","Deselectionner"),A_DESELECT,0,0);
 }else if(tab==1){button(604,62,88,34,tr("Round","Rond"),A_ROUND,0,!brush.square);button(696,62,88,34,tr("Square","Carre"),A_ROUND,1,brush.square);slider(112,tr("Size px","Taille px"),SL_SIZE);slider(167,tr("Opacity %","Opacite %"),SL_OPACITY);slider(222,tr("Edge contrast","Contraste"),SL_HARDNESS);slider(277,tr("Diffusion %","Diffusion %"),SL_DIFFUSION);text(608,334,tr("Soft <-> hard edge","Bord doux <-> dur"),1,muted);text(608,354,tr("Diffusion = spray","Diffusion = spray"),1,muted);rect(608,378,176,26,rgb(red,green,blue));
 }else if(tab==2){for(int i=0;i<12;++i){uint32_t c=palette[i];int x=608+(i%4)*44,y=64+(i/4)*40;rect(x,y,38,32,rgb(c&255,(c>>8)&255,(c>>16)&255));frame(x,y,38,32,edge);hit(x,y,38,32,A_PALETTE,i);}slider(200,tr("Red","Rouge"),SL_RED);slider(258,tr("Green","Vert"),SL_GREEN);slider(316,tr("Blue","Bleu"),SL_BLUE);button(608,378,176,32,tr("Pick in image","Pipette image"),A_TOOL,TOOL_PICK,tool==TOOL_PICK);
 }else if(tab==3){for(int i=0;i<doc.current.count;i++){int index=doc.current.count-1-i,y=62+i*26;IELayer* l=&doc.current.layers[index];button(604,y,26,23,l->visible?"O":"-",A_VIS,index,0);char name[26];snprintf(name,sizeof(name),"%d %s",index+1,l->name);button(634,y,150,23,name,A_LAYER,index,index==doc.current.active);}int y=276;button(604,y,58,28,"+",A_ADD,0,0);button(666,y,58,28,tr("Copy","Copie"),A_DUP,0,0);button(728,y,56,28,"-",A_DEL,0,0);button(604,310,58,28,tr("Up","Haut"),A_UP,0,0);button(666,310,58,28,tr("Down","Bas"),A_DOWN,0,0);button(728,310,56,28,tr("Merge","Fusion"),A_MERGE,0,0);slider(350,tr("Opacity","Opacite"),SL_LAYER_OPACITY);button(604,395,88,28,tr("Rename","Nom"),A_RENAME,0,0);button(696,395,88,28,tr("Clear","Effacer"),A_CLEAR,0,0);
 }else if(tab==4){slider(62,tr("Brightness","Lumiere"),SL_BRIGHT);slider(107,tr("Contrast","Contraste"),SL_CONTRAST);slider(152,tr("Saturation","Saturation"),SL_SATURATION);slider(197,tr("Red gain %","Gain rouge %"),SL_GAIN_R);slider(242,tr("Green gain %","Gain vert %"),SL_GAIN_G);slider(287,tr("Blue gain %","Gain bleu %"),SL_GAIN_B);button(604,338,180,30,all_layers?tr("All visible layers","Calques visibles"):tr("Active layer","Calque actif"),A_ALL,0,all_layers);button(604,382,180,36,tr("Apply","Appliquer"),A_APPLY,0,1);
 }else if(tab==5){slider(64,tr("Width %","Largeur %"),SL_SCALE_X);slider(119,tr("Height %","Hauteur %"),SL_SCALE_Y);slider(174,tr("Shear %","Inclinaison %"),SL_SHEAR);snprintf(out,sizeof(out),"%s %d",tr("Rotate","Rotation"),turns*90);button(604,232,180,32,out,A_ROTATE,0,0);button(604,275,180,32,tr("Reset values","Reinitialiser"),A_RESET_TRANSFORM,0,0);button(604,318,180,34,tr("Move selection","Deplacer selection"),A_TOOL,TOOL_MOVE,tool==TOOL_MOVE);button(604,374,180,40,tr("Apply","Appliquer"),A_TRANSFORM,0,1);
 }else if(tab==6){text(608,64,tr("Image resolution","Resolution image"),1,ink);snprintf(out,sizeof(out),"%s %d px",tr("Width","Largeur"),new_w);button(604,88,180,40,out,A_WIDTH,0,0);snprintf(out,sizeof(out),"%s %d px",tr("Height","Hauteur"),new_h);button(604,140,180,40,out,A_HEIGHT,0,0);button(604,196,180,40,tr("Resize","Redimensionner"),A_RESIZE,0,1);button(604,248,180,40,tr("Crop selection","Recadrer selection"),A_CROP,0,0);text(608,312,tr("1..1024 px / side","1..1024 px / cote"),1,muted);text(608,334,tr("T5I: layers + alpha","T5I: calques + alpha"),1,muted);text(608,356,tr("BMP: flat RGB24","BMP: RGB24 aplati"),1,muted);text(608,378,tr("8 MiB document limit","Document : 8 Mio max"),1,muted);
 }else{button(604,64,180,44,tr("Edit text","Saisir texte"),A_TEXT,0,1);char preview[28];copy(preview,sizeof(preview),text_value);text(608,126,preview,1,ink);slider(164,tr("Text scale","Echelle texte"),SL_TEXT_SCALE);button(604,224,180,40,tr("Place text","Placer texte"),A_TOOL,TOOL_TEXT,tool==TOOL_TEXT);text(608,292,tr("Tap to place text","Toucher pour placer"),1,muted);text(608,316,tr("Bitmap 5x7 font","Police bitmap 5x7"),1,muted);text(608,340,tr("Uses current color","Couleur courante"),1,muted);text(608,364,tr("Ctrl+Z: Undo","Ctrl+Z : Annuler"),1,muted);}
}
static void render_modal(void){if(!modal)return;rect(20,48,760,390,rgb(24,33,46));frame(20,48,760,390,accent);hit_count=0;
 if(modal==2){text(48,105,prompt,2,ink);text(48,150,tr("This action replaces the current content.","Cette action remplace le contenu concerne."),1,muted);button(110,260,250,60,tr("Cancel - Esc","Annuler - Echap"),A_CANCEL,0,0);button(420,260,250,60,tr("Confirm - Enter","Valider - Entree"),A_CONFIRM,0,1);return;}
 if(modal==3){const char* en[]={"ATELIER IMAGE - TOUCH + KEYBOARD","Paint: drag. Shapes/selection: drag and release.","Text: enter text, then touch the canvas.","Selection limits paint, color and transforms.","Layers: top of the list is top of the image.","Ctrl+Z/Y: undo/redo. Ctrl+S: project. Ctrl+O: open.","Escape cancels a gesture/dialog; otherwise asks to quit.","T5I keeps layers. BMP export flattens onto white.","BMP24/32 input, max1024x1024; up to8 layers /8MiB.","History: up to16 states /8MiB, limited by free memory.","Changes stay in RAM until you explicitly save."};const char* french[]={"ATELIER IMAGE - TACTILE + CLAVIER","Peindre : glisser. Formes/selection : glisser-relacher.","Texte : saisir, puis toucher l'image.","La selection limite dessin, reglages et deformation.","Calques : le haut de la liste est au premier plan.","Ctrl+Z/Y : annuler/retablir. Ctrl+S : projet. Ctrl+O : ouvrir.","Echap annule le geste/dialogue, sinon propose de quitter.","T5I conserve les calques. BMP aplatit sur fond blanc.","Entree BMP24/32, max1024x1024 ; 8 calques /8Mio max.","Historique :16 etats /8Mio max, selon memoire disponible.","Enregistrez explicitement pour conserver votre travail."};for(int i=0;i<11;i++)text(40,70+i*27,fr?french[i]:en[i],i==0?2:1,i==0?ink:muted);button(275,382,250,38,tr("Close - Esc / Enter","Fermer - Echap / Entree"),A_CANCEL,0,1);return;}
 text(40,65,prompt,2,ink);rect(40,100,720,42,bg);frame(40,100,720,42,edge);const char* visible=input;size_t n=strlen(input);if(n>57)visible=input+n-57;text(50,114,visible,2,ink);int cursor=50+(int)strlen(visible)*12;if(cursor>744)cursor=744;rect(cursor,110,2,23,accent);
 const char* rows[]={"1234567890-_/.","qwertyuiop","asdfghjkl","zxcvbnm"};for(int row=0;row<4;row++){int count=(int)strlen(rows[row]),kw=row==0?51:58,total=count*kw,x=(W-total)/2;for(int i=0;i<count;i++){char s[2]={rows[row][i],0};if(shift&&s[0]>='a'&&s[0]<='z')s[0]-=32;button(x+i*kw,157+row*49,kw-5,42,s,A_CHAR,rows[row][i],0);}}
 button(40,356,95,32,"Shift",A_SHIFT,0,shift);button(145,356,350,32,tr("Space","Espace"),A_CHAR,' ',0);button(505,356,110,32,tr("Backspace","Effacer"),A_BACKSPACE,0,0);button(40,397,340,30,tr("Cancel - Esc","Annuler - Echap"),A_CANCEL,0,0);button(400,397,360,30,tr("OK - Enter","OK - Entree"),A_INPUT,0,1);
}
static void render(void){bg=rgb(16,23,33);card=rgb(31,42,56);edge=rgb(65,82,102);ink=rgb(239,245,250);muted=rgb(161,179,198);accent=rgb(20,113,150);hit_count=0;rect(0,0,W,H,bg);
 const char* en[]={"New","Open","Project","BMP","Undo","Redo","Help","Quit"};const char* french[]={"Nouveau","Ouvrir","Projet","BMP","Annuler","Refaire","Aide","Quitter"};const int actions[]={A_NEW,A_OPEN,A_SAVE,A_BMP,A_UNDO,A_REDO,A_HELP,A_QUIT};for(int i=0;i<8;i++)button(8+i*99,8,92,36,fr?french[i]:en[i],actions[i],0,i==2);
 render_canvas();render_panel();char out[96];snprintf(out,sizeof(out),"%d x %d  |  %s %d/%d  |  %s %u/%u %s",doc.current.width,doc.current.height,tr("Layer","Calque"),doc.current.active+1,doc.current.count,tr("Undo","Annuler"),doc.undo_count,doc.redo_count,doc.dirty?"*":"");text(10,408,out,1,ink);text(10,426,status,1,muted);
 const char* tabs_en[]={"Tools","Brush","Color","Layers","Adjust","Transform","Size","Text"};const char* tabs_fr[]={"Outils","Pinceau","Couleur","Calques","Reglage","Deform.","Format","Texte"};for(int i=0;i<8;i++)button(8+i*99,446,92,28,fr?tabs_fr[i]:tabs_en[i],A_TAB,i,tab==i);render_modal();video->video_present(pixels,W*H);redraw=0;}
static void cancel_gesture(void){if(gesture){ie_cancel(&doc);doc.dirty=gesture_dirty;}dragging=gesture=0;active_slider=-1;redraw=1;}
static void pointer_down(int x,int y){if(dragging||gesture||active_slider>=0)cancel_gesture();for(int i=hit_count-1;i>=0;i--){struct Hit h=hits[i];if(x<h.x||y<h.y||x>=h.x+h.w||y>=h.y+h.h)continue;if(h.action==A_SLIDER){active_slider=h.value;if(h.value==SL_LAYER_OPACITY)gesture_dirty=doc.dirty;if(h.value==SL_LAYER_OPACITY&&!ie_gesture_begin(&doc)){active_slider=-1;message(ie_error(&doc));return;}if(h.value==SL_LAYER_OPACITY)gesture=1;set_slider(h.value,x);}else activate(h.action,h.value);return;}if(modal||x<dx||y<dy||x>=dx+dw||y>=dy+dh)return;
 sx=lx=image_x(x);sy=ly=image_y(y);if(tool==TOOL_PICK){uint32_t c=ie_composite(&doc,sx,sy);red=c&255;green=(c>>8)&255;blue=(c>>16)&255;tab=2;redraw=1;return;}
 if(tool==TOOL_TEXT){if(ie_begin(&doc))ie_text(&doc,sx,sy,text_value,text_scale,paint_color(),brush.opacity);else message(ie_error(&doc));redraw=1;return;}
 if((tool==TOOL_BRUSH||tool==TOOL_ERASE)&&!doc.current.layers[doc.current.active].visible){message(tr("Show the active layer before painting","Afficher le calque actif pour peindre"));return;}
 dragging=1;if(tool==TOOL_BRUSH||tool==TOOL_ERASE){gesture_dirty=doc.dirty;if(!ie_gesture_begin(&doc)){dragging=0;message(ie_error(&doc));return;}gesture=1;brush.color=paint_color();brush.erase=tool==TOOL_ERASE;ie_brush(&doc,sx,sy,sx,sy,&brush);}redraw=1;
}
static void pointer_move(int x,int y){if(active_slider>=0){set_slider(active_slider,x);return;}if(!dragging)return;int nx=image_x(x),ny=image_y(y);if(tool==TOOL_BRUSH||tool==TOOL_ERASE)ie_brush(&doc,lx,ly,nx,ny,&brush);lx=nx;ly=ny;redraw=1;}
static void pointer_up(int x,int y){if(active_slider>=0){set_slider(active_slider,x);if(gesture)ie_commit(&doc);active_slider=-1;gesture=0;return;}if(!dragging)return;pointer_move(x,y);if(tool==TOOL_SELECT)ie_select(&doc,sx,sy,lx,ly);else if(tool==TOOL_MOVE)result(ie_translate_selection(&doc,lx-sx,ly-sy));else if(tool>=TOOL_RECT&&tool<=TOOL_LINE){if(ie_begin(&doc))ie_shape(&doc,tool==TOOL_RECT?IE_SHAPE_RECT:tool==TOOL_ELLIPSE?IE_SHAPE_ELLIPSE:IE_SHAPE_LINE,sx,sy,lx,ly,paint_color(),brush.opacity,thickness,fill,outline);else message(ie_error(&doc));}if(gesture)ie_commit(&doc);dragging=gesture=0;redraw=1;}
static void key_event(const struct tab5_app_input_event* e){if(e->key==TAB5_KEY_ESCAPE){if(dragging||gesture||active_slider>=0)cancel_gesture();else if(modal)modal=0;else request_action(A_QUIT);redraw=1;return;}if(modal){if(e->key==TAB5_KEY_ENTER){if(modal==1)input_submit();else if(modal==2)activate(A_CONFIRM,0);else modal=0;}else if(modal==1&&e->key==TAB5_KEY_BACKSPACE)activate(A_BACKSPACE,0);else if(modal==1&&e->key==TAB5_KEY_TEXT&&!(e->modifiers&(TAB5_MOD_CTRL|TAB5_MOD_META))){size_t n=strlen(input),m=strlen(e->text);if(n+m<sizeof(input)){memcpy(input+n,e->text,m+1);}}redraw=1;return;}
 if(e->key!=TAB5_KEY_TEXT)return;
 if(dragging||gesture||active_slider>=0)cancel_gesture();
 char c=e->text[0];if(c>='A'&&c<='Z')c+=32;if(e->modifiers&(TAB5_MOD_CTRL|TAB5_MOD_META)){if(c=='z')activate((e->modifiers&TAB5_MOD_SHIFT)?A_REDO:A_UNDO,0);if(c=='y')activate(A_REDO,0);if(c=='s')activate(A_SAVE,0);if(c=='o')activate(A_OPEN,0);if(c=='n')activate(A_NEW,0);return;}if(c=='b')tool=TOOL_BRUSH;if(c=='e')tool=TOOL_ERASE;if(c=='s')tool=TOOL_SELECT;if(c=='v')tool=TOOL_MOVE;if(c=='t'){tool=TOOL_TEXT;tab=7;}redraw=1;}
__attribute__((visibility("default"))) int tab5_app_main(int argc,char** argv){api=tab5_app_api_v6();if(!api)return 1;video=&api->base.base.base.base;base=&video->base;if(base->version!=TAB5_APP_ABI_V6||base->size<sizeof(*api))return 2;fr=base->language&&base->language[0]=='f';
 pixels=base->allocate(W*H*2);if(!pixels){const char* m=fr?"Memoire insuffisante pour Atelier Image.\n":"Insufficient memory for Image Studio.\n";base->log(m,strlen(m));return 3;}
 if(!ie_init(&doc,memory_alloc,memory_free,NULL,640,360)){base->release(pixels);return 4;}ie_set_yield(&doc,cooperate,NULL);doc.dirty=0;if(api->canvas_begin(W,H)){ie_destroy(&doc);base->release(pixels);return 5;}message(tr("Ready - BMP and T5I / Ctrl+S: save project","Pret - BMP et T5I / Ctrl+S : enregistrer le projet"));if(argc>1&&argv[1]&&argv[1][0])open_file(argv[1]);render();
 while(!base->should_stop()&&!exit_requested){struct tab5_app_input_event e;int count=0,r;do{memset(&e,0,sizeof(e));e.size=sizeof(e);r=api->input_poll(&e);if(r<0){exit_requested=1;break;}if(r>0){if(e.type==TAB5_INPUT_DOWN)pointer_down(e.x,e.y);else if(e.type==TAB5_INPUT_MOVE)pointer_move(e.x,e.y);else if(e.type==TAB5_INPUT_UP)pointer_up(e.x,e.y);else if(e.type==TAB5_INPUT_KEY)key_event(&e);else if(e.type==TAB5_INPUT_CANCEL)cancel_gesture();else if(e.type==TAB5_INPUT_CLOSE){cancel_gesture();request_action(A_QUIT);}if(redraw&&e.type!=TAB5_INPUT_MOVE)render();}}while(r>0&&++count<32&&!exit_requested);if(redraw)render();if(base->sleep_ms(16))break;}
 if(gesture)ie_cancel(&doc);
 ie_destroy(&doc);base->release(pixels);return 0;
}
