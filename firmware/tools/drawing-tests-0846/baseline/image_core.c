#include "image_core.h"
#include <string.h>
#include <limits.h>

void ie_set_yield(IEDoc*d,void(*callback)(void*),void*context){d->yield=callback;d->yield_context=context;}
static void cooperate(const IEDoc*d){if(d->yield)d->yield(d->yield_context);}
static void memory_copy(IEDoc*d,void*dst,const void*src,size_t n){unsigned char*a=dst;const unsigned char*b=src;while(n){size_t m=n>65536?65536:n;memcpy(a,b,m);a+=m;b+=m;n-=m;cooperate(d);}}
static void memory_zero(IEDoc*d,void*dst,size_t n){unsigned char*a=dst;while(n){size_t m=n>65536?65536:n;memset(a,0,m);a+=m;n-=m;cooperate(d);}}
static int clampi(int v,int a,int b){return v<a?a:v>b?b:v;}
static int mini(int a,int b){return a<b?a:b;}
static int maxi(int a,int b){return a>b?a:b;}
static int absi(int a){return a<0?-a:a;}
static size_t pixel_bytes(const IEState *s){return (size_t)s->width*(size_t)s->height*4u;}
static size_t state_bytes(const IEState *s){return pixel_bytes(s)*(size_t)s->count+(s->selection_mask?(size_t)s->width*s->height:0);}
static int valid_size(int w,int h,int count){return w>0&&h>0&&w<=IE_MAX_DIMENSION&&h<=IE_MAX_DIMENSION&&count>0&&count<=IE_MAX_LAYERS&&(size_t)w*(size_t)h*4u*(size_t)count<=IE_DOCUMENT_MAX_BYTES;}
static int fail(IEDoc*d,const char*e){d->error=e;return 0;}
const char *ie_error(const IEDoc*d){return d->error?d->error:"";}
static void name_copy(char dst[24],const char*src){size_t n=src?strlen(src):0;if(n>23)n=23;memset(dst,0,24);if(n)memcpy(dst,src,n);}
static void state_free(IEDoc*d,IEState*s){int i;if(s->selection_mask)d->release(d->memory_context,s->selection_mask);for(i=0;i<s->count;i++)if(s->layers[i].pixels)d->release(d->memory_context,s->layers[i].pixels);memset(s,0,sizeof(*s));}
static void history_free(IEDoc*d,IEState*s){d->history_bytes-=state_bytes(s);state_free(d,s);d->release(d->memory_context,s);}
static void drop_oldest(IEDoc*d,IEState**a,unsigned*n){unsigned i;if(!*n)return;history_free(d,a[0]);for(i=1;i<*n;i++)a[i-1]=a[i];a[--*n]=0;}
static int drop_any(IEDoc*d){if(d->undo_count>1||(d->undo_count&&!d->gesture&&!d->brush_coverage&&!d->warp_scratch)){drop_oldest(d,d->undo,&d->undo_count);return 1;}if(d->redo_count){drop_oldest(d,d->redo,&d->redo_count);return 1;}if(d->saved_redo_count){drop_oldest(d,d->saved_redo,&d->saved_redo_count);return 1;}return 0;}
static void scratch_free(IEDoc*d){if(d->brush_coverage)d->release(d->memory_context,d->brush_coverage);if(d->warp_scratch)d->release(d->memory_context,d->warp_scratch);d->brush_coverage=0;d->warp_scratch=0;d->scratch_side=0;}
void ie_commit(IEDoc*d){scratch_free(d);while(d->saved_redo_count)drop_oldest(d,d->saved_redo,&d->saved_redo_count);d->gesture=0;}
static void clear_history(IEDoc*d){ie_commit(d);while(d->undo_count)drop_oldest(d,d->undo,&d->undo_count);while(d->redo_count)drop_oldest(d,d->redo,&d->redo_count);}
static int state_make(IEDoc*d,IEState*s,int w,int h,int count){int i;memset(s,0,sizeof(*s));if(!valid_size(w,h,count))return fail(d,"Dimensions/calques hors limite (8192, moins de 8 Mio de pixels).");for(;;){s->width=w;s->height=h;s->count=count;for(i=0;i<count;i++){s->layers[i].pixels=(uint32_t*)d->alloc(d->memory_context,pixel_bytes(s));if(!s->layers[i].pixels)break;memory_zero(d,s->layers[i].pixels,pixel_bytes(s));s->layers[i].opacity=255;s->layers[i].visible=1;name_copy(s->layers[i].name,"Calque");}if(i==count)return 1;state_free(d,s);if(!drop_any(d))return fail(d,"Memoire insuffisante; image conservee.");}}

/* Import allocation must never evict undo/redo from the document being replaced. */
static int state_make_import(IEDoc*d,IEState*s,int w,int h,int count){IEDoc temporary;int ok;memset(&temporary,0,sizeof(temporary));temporary.alloc=d->alloc;temporary.release=d->release;temporary.memory_context=d->memory_context;temporary.yield=d->yield;temporary.yield_context=d->yield_context;ok=state_make(&temporary,s,w,h,count);if(!ok)d->error=temporary.error;return ok;}

static IEState*clone_state(IEDoc*d){IEState*s=(IEState*)d->alloc(d->memory_context,sizeof(IEState));int i;if(!s)return 0;memcpy(s,&d->current,sizeof(*s));s->selection_mask=0;for(i=0;i<IE_MAX_LAYERS;i++)s->layers[i].pixels=0;for(i=0;i<s->count;i++){s->layers[i].pixels=(uint32_t*)d->alloc(d->memory_context,pixel_bytes(s));if(!s->layers[i].pixels){state_free(d,s);d->release(d->memory_context,s);return 0;}memory_copy(d,s->layers[i].pixels,d->current.layers[i].pixels,pixel_bytes(s));}if(d->current.selection_mask){size_t n=(size_t)s->width*s->height;s->selection_mask=d->alloc(d->memory_context,n);if(!s->selection_mask){state_free(d,s);d->release(d->memory_context,s);return 0;}memory_copy(d,s->selection_mask,d->current.selection_mask,n);}return s;}
static void push_undo(IEDoc*d,IEState*s){while(d->redo_count)drop_oldest(d,d->redo,&d->redo_count);while(d->undo_count>=IE_MAX_HISTORY)drop_oldest(d,d->undo,&d->undo_count);while(d->history_bytes+state_bytes(s)>IE_HISTORY_MAX_BYTES)if(!drop_any(d))break;d->undo[d->undo_count++]=s;d->history_bytes+=state_bytes(s);d->dirty=1;d->error=0;}
static IEState *prepare_snapshot(IEDoc*d){IEState*s;if(!d||!d->current.count)return 0;if(state_bytes(&d->current)>IE_HISTORY_MAX_BYTES){fail(d,"Mémoire insuffisante pour annuler la sélection ; image conservée");return 0;}while(d->history_bytes+state_bytes(&d->current)>IE_HISTORY_MAX_BYTES)if(!drop_any(d))break;do{s=clone_state(d);if(s)break;}while(drop_any(d));if(!s)fail(d,"Memoire insuffisante pour annuler; image conservee.");return s;}
int ie_begin(IEDoc*d){IEState*s;ie_commit(d);s=prepare_snapshot(d);if(!s)return 0;push_undo(d,s);return 1;}
int ie_gesture_begin(IEDoc*d){IEState*s;unsigned i;ie_commit(d);s=prepare_snapshot(d);if(!s)return 0;d->saved_redo_count=d->redo_count;for(i=0;i<d->redo_count;i++){d->saved_redo[i]=d->redo[i];d->redo[i]=0;}d->redo_count=0;push_undo(d,s);d->gesture=1;return 1;}

static int replace_undo(IEDoc*d,IEState*s){IEState*old;if(state_bytes(&d->current)>IE_HISTORY_MAX_BYTES)return fail(d,"Mémoire insuffisante pour annuler la sélection ; image conservée");ie_commit(d);old=(IEState*)d->alloc(d->memory_context,sizeof(IEState));while(!old&&drop_any(d))old=(IEState*)d->alloc(d->memory_context,sizeof(IEState));if(!old)return fail(d,"Memoire insuffisante; image conservee.");memcpy(old,&d->current,sizeof(*old));memcpy(&d->current,s,sizeof(*s));memset(s,0,sizeof(*s));push_undo(d,old);return 1;}
int ie_init(IEDoc*d,IEAlloc a,IEFree f,void*c,int w,int h){if(!d||!a||!f)return 0;memset(d,0,sizeof(*d));d->alloc=a;d->release=f;d->memory_context=c;d->random=0x43559a15u;return ie_new(d,w,h,0xffffffffu);}
void ie_destroy(IEDoc*d){if(!d||!d->release)return;clear_history(d);state_free(d,&d->current);d->dirty=0;}
int ie_new(IEDoc*d,int w,int h,uint32_t background){IEState s;size_t i,n;if(!state_make(d,&s,w,h,1))return 0;n=(size_t)w*h;for(i=0;i<n;i++){s.layers[0].pixels[i]=background;if((i&16383u)==0)cooperate(d);}name_copy(s.layers[0].name,"Fond");clear_history(d);state_free(d,&d->current);d->current=s;d->dirty=0;d->error=0;return 1;}
static int swap_history(IEDoc*d,IEState**from,unsigned*nf,IEState**to,unsigned*nt){IEState*s,tmp;size_t before;if(!*nf)return fail(d,"Historique vide.");if(state_bytes(&d->current)>IE_HISTORY_MAX_BYTES)return fail(d,"Mémoire insuffisante pour annuler la sélection ; image conservée");s=from[--*nf];from[*nf]=0;before=state_bytes(s);tmp=d->current;d->current=*s;*s=tmp;d->history_bytes=d->history_bytes-before+state_bytes(s);if(*nt==IE_MAX_HISTORY)drop_oldest(d,to,nt);to[(*nt)++]=s;while(d->history_bytes>IE_HISTORY_MAX_BYTES){if(*nf)drop_oldest(d,from,nf);else if(*nt>1)drop_oldest(d,to,nt);else break;}d->dirty=1;d->error=0;return 1;}
int ie_undo(IEDoc*d){ie_commit(d);return swap_history(d,d->undo,&d->undo_count,d->redo,&d->redo_count);}
int ie_redo(IEDoc*d){ie_commit(d);return swap_history(d,d->redo,&d->redo_count,d->undo,&d->undo_count);}
void ie_select(IEDoc*d,int x0,int y0,int x1,int y1){IERect*r=&d->current.selection;int t;if(d->current.selection_mask){d->release(d->memory_context,d->current.selection_mask);d->current.selection_mask=0;}if(x0>x1){t=x0;x0=x1;x1=t;}if(y0>y1){t=y0;y0=y1;y1=t;}r->x=clampi(x0,0,d->current.width-1);r->y=clampi(y0,0,d->current.height-1);r->w=clampi(x1,0,d->current.width-1)-r->x+1;r->h=clampi(y1,0,d->current.height-1)-r->y+1;r->enabled=1;}
void ie_deselect(IEDoc*d){if(d->current.selection_mask)d->release(d->memory_context,d->current.selection_mask);d->current.selection_mask=0;memset(&d->current.selection,0,sizeof(d->current.selection));}
static IERect region(const IEState*s){IERect r=s->selection;if(!r.enabled){r.x=0;r.y=0;r.w=s->width;r.h=s->height;}return r;}
unsigned ie_selection_alpha(const IEState*s,int x,int y){IERect r;if(!s||x<0||y<0||x>=s->width||y>=s->height)return 0;if(!s->selection.enabled)return 255;r=s->selection;if(x<r.x||y<r.y||x>=r.x+r.w||y>=r.y+r.h)return 0;return s->selection_mask?s->selection_mask[(size_t)y*s->width+x]:255;}
int ie_selection_bounds(const IEState*s,IERect*r){if(!s||!r)return 0;*r=region(s);return r->w>0&&r->h>0;}
static int inside(const IEState*s,int x,int y){return ie_selection_alpha(s,x,y)!=0;}
/* Premultiplied interpolation for masked replacement (not source-over). */
static uint32_t mixed(uint32_t a,uint32_t b,unsigned weight){unsigned aa=a>>24,ba=b>>24,oa=(aa*(255-weight)+ba*weight+127)/255,k;uint32_t o=oa<<24;if(!oa)return 0;for(k=0;k<24;k+=8){unsigned v=(((a>>k)&255)*aa*(255-weight)+((b>>k)&255)*ba*weight+127)/255;o|=(uint32_t)mini(255,(v+oa/2)/oa)<<k;}return o;}

static uint32_t over(uint32_t dst,uint32_t src,unsigned opacity){unsigned sa=((src>>24)*opacity+127)/255,da=dst>>24,oa;uint32_t o;unsigned k;/* Common opaque/transparent layers need no channel divisions. */if(sa==255)return src|0xff000000u;if(!sa)return da?dst:0;if(!da)return(src&0xffffffu)|(sa<<24);oa=sa+(da*(255-sa)+127)/255;o=oa<<24;for(k=0;k<24;k+=8){unsigned sc=(src>>k)&255,dc=(dst>>k)&255;unsigned c=(sc*sa+(dc*da*(255-sa)+127)/255+oa/2)/oa;if(c>255)c=255;o|=c<<k;}return o;}
uint32_t ie_composite(const IEDoc*d,int x,int y){uint32_t c=0;int i;if(x<0||y<0||x>=d->current.width||y>=d->current.height)return 0;for(i=0;i<d->current.count;i++){const IELayer*l=&d->current.layers[i];if(l->visible)c=over(c,l->pixels[(size_t)y*d->current.width+x],l->opacity);}return c;}
void ie_composite_row(const IEDoc*d,int y,uint32_t*out){int x;for(x=0;x<d->current.width;x++)out[x]=ie_composite(d,x,y);}
size_t ie_render_rgb565(const IEDoc*d,uint16_t*out,size_t stride,int x,int y,int w,int h,uint32_t light,uint32_t dark){
    if(!d)return 0;
    return ie_render_scaled_rgb565(d,out,stride,d->current.width,d->current.height,x,y,w,h,light,dark);
}
size_t ie_render_scaled_rgb565(const IEDoc*d,uint16_t*out,size_t stride,int ow,int oh,int x,int y,int w,int h,uint32_t light,uint32_t dark){
    const IELayer*visible[IE_MAX_LAYERS];unsigned count=0;int left,top,right,bottom,xx,yy,i,dw,dh;
    int64_t end_x=(int64_t)x+w,end_y=(int64_t)y+h;
    if(!d||!out||w<=0||h<=0||ow<=0||oh<=0||ow>IE_MAX_DIMENSION||oh>IE_MAX_DIMENSION||!valid_size(d->current.width,d->current.height,d->current.count)||stride<(size_t)ow||stride>SIZE_MAX/sizeof(*out)/(size_t)oh)return 0;
    dw=d->current.width;dh=d->current.height;
    left=maxi(x,0);top=maxi(y,0);right=end_x>dw?dw:(end_x<0?0:(int)end_x);bottom=end_y>dh?dh:(end_y<0?0:(int)end_y);
    if(left>=right||top>=bottom)return 0;
    /* ceil(source_edge * output / document) is the first nearest-neighbour
       display sample at or beyond that edge. No off-by-one dirty seam. */
    left=(left*ow+dw-1)/dw;right=(right*ow+dw-1)/dw;
    top=(top*oh+dh-1)/dh;bottom=(bottom*oh+dh-1)/dh;
    for(i=0;i<d->current.count;i++){const IELayer*l=&d->current.layers[i];if(l->visible&&l->opacity&&l->pixels)visible[count++]=l;}
    for(yy=top;yy<bottom;yy++){
        int sy=yy*dh/oh;uint16_t*target=out+(size_t)yy*stride+left;
        for(xx=left;xx<right;xx++,target++){
            int sx=xx*dw/ow;size_t source=(size_t)sy*dw+sx;
            uint32_t c=0,bg;unsigned a,r,g,b,j;
            for(j=0;j<count;j++)c=over(c,visible[j]->pixels[source],visible[j]->opacity);
            a=c>>24;
            if(a==255){r=c&255;g=(c>>8)&255;b=(c>>16)&255;}
            else{bg=((sx/8+sy/8)&1)?dark:light;r=((c&255)*a+((bg>>16)&255)*(255-a))/255;g=(((c>>8)&255)*a+((bg>>8)&255)*(255-a))/255;b=(((c>>16)&255)*a+(bg&255)*(255-a))/255;}
            *target=(uint16_t)(((r&248)<<8)|((g&252)<<3)|(b>>3));
        }
        if(((yy-top)&15)==15)cooperate(d);
    }
    return(size_t)(right-left)*(size_t)(bottom-top);
}

static int64_t floor_div(int64_t n,int64_t denominator){int64_t q=n/denominator;if(n<0&&n%denominator)q--;return q;}
int ie_view_sample(int pixel,int32_t origin16,int zoom){int64_t q;if(zoom<1||zoom>6400)return 0;q=floor_div((int64_t)origin16*zoom+(int64_t)pixel*6553600,(int64_t)zoom*65536);return q<INT_MIN?INT_MIN:q>INT_MAX?INT_MAX:(int)q;}
static uint16_t rgb565(uint32_t rgb){return(uint16_t)(((rgb>>8)&0xf800)|((rgb>>5)&0x07e0)|((rgb>>3)&0x001f));}
size_t ie_render_view_rgb565(const IEDoc*d,uint16_t*out,size_t stride,int ow,int oh,int32_t ox,int32_t oy,int zoom,int x,int y,int w,int h,uint32_t light,uint32_t dark,uint32_t outside){
 int xx,yy,left,top,right,bottom,firstx,firstrem,step,steprem,denominator,i;int64_t ex=(int64_t)x+w,ey=(int64_t)y+h,numerator;uint16_t backdrop=rgb565(outside);const IELayer*visible[IE_MAX_LAYERS];unsigned count=0;
 if(!d||!out||zoom<1||zoom>6400||ow<1||oh<1||ow>2048||oh>2048||w<=0||h<=0||!valid_size(d->current.width,d->current.height,d->current.count)||stride<(size_t)ow||stride>SIZE_MAX/sizeof(*out)/(size_t)oh)return 0;
 left=maxi(0,x);top=maxi(0,y);right=ex>ow?ow:ex<0?0:(int)ex;bottom=ey>oh?oh:ey<0?0:(int)ey;if(left>=right||top>=bottom)return 0;
 denominator=zoom*65536;numerator=(int64_t)ox*zoom+(int64_t)left*6553600;firstx=(int)floor_div(numerator,denominator);firstrem=(int)(numerator-(int64_t)firstx*denominator);step=6553600/denominator;steprem=6553600%denominator;
 for(i=0;i<d->current.count;i++){const IELayer*l=&d->current.layers[i];if(l->visible&&l->opacity&&l->pixels)visible[count++]=l;}
 for(yy=top;yy<bottom;yy++){int sy=ie_view_sample(yy,oy,zoom),sx=firstx,rem=firstrem;uint16_t*row=out+(size_t)yy*stride;
  for(xx=left;xx<right;xx++){
   if(sx<0||sy<0||sx>=d->current.width||sy>=d->current.height)row[xx]=backdrop;
   else{uint32_t c=0,bg;unsigned a,r,g,b,j;size_t source=(size_t)sy*d->current.width+sx;for(j=0;j<count;j++)c=over(c,visible[j]->pixels[source],visible[j]->opacity);a=c>>24;if(a==255){r=c&255;g=(c>>8)&255;b=(c>>16)&255;}else{bg=((sx/8+sy/8)&1)?dark:light;r=((c&255)*a+((bg>>16)&255)*(255-a))/255;g=(((c>>8)&255)*a+((bg>>8)&255)*(255-a))/255;b=(((c>>16)&255)*a+(bg&255)*(255-a))/255;}row[xx]=(uint16_t)(((r&248)<<8)|((g&252)<<3)|(b>>3));}
   sx+=step;rem+=steprem;if(rem>=denominator){sx++;rem-=denominator;}
  }if(((yy-top)&15)==15)cooperate(d);
 }return(size_t)(right-left)*(size_t)(bottom-top);
}

static void paint(IEDoc*d,int x,int y,uint32_t color,int alpha,int erase){uint32_t*p;if(!inside(&d->current,x,y)||alpha<=0)return;alpha=(alpha*(int)ie_selection_alpha(&d->current,x,y)+127)/255;p=&d->current.layers[d->current.active].pixels[(size_t)y*d->current.width+x];if(erase){unsigned a=*p>>24;a=(a*(255-(unsigned)clampi(alpha,0,255))+127)/255;*p=(*p&0xffffffu)|(a<<24);}else *p=over(*p,color,(unsigned)clampi(alpha,0,255));}
static unsigned isqrtu(unsigned n){unsigned x=0,bit=1u<<30;while(bit>n)bit>>=2;while(bit){if(n>=x+bit){n-=x+bit;x=(x>>1)+bit;}else x>>=1;bit>>=2;}return x;}
static uint32_t random_next(IEDoc*d){uint32_t x=d->random;x^=x<<13;x^=x>>17;x^=x<<5;return d->random=x;}
static uint32_t spatial_random(uint32_t seed,int x,int y){uint32_t h=seed^(uint32_t)x*0x9e3779b9u^(uint32_t)y*0x85ebca6bu;h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;return h^(h>>16);}
static int coverage_allocate(IEDoc*d){size_t n=(size_t)d->current.width*d->current.height;uint8_t*p=d->alloc(d->memory_context,n);if(!p)return fail(d,"Memoire insuffisante pour le pinceau; image conservee.");memory_zero(d,p,n);d->brush_coverage=p;d->scratch_layer=d->current.active;d->brush_seed=random_next(d);return 1;}
int ie_brush_begin(IEDoc*d){uint8_t*p;size_t n;if(!d||!d->current.count)return 0;n=(size_t)d->current.width*d->current.height;p=d->alloc(d->memory_context,n);if(!p)return fail(d,"Memoire insuffisante pour le pinceau; image conservee.");memory_zero(d,p,n);if(!ie_gesture_begin(d)){d->release(d->memory_context,p);return 0;}d->brush_coverage=p;d->scratch_layer=d->current.active;d->brush_seed=random_next(d);return 1;}
static void brush_paint(IEDoc*d,int x,int y,const IEBrush*b,int alpha){size_t at;uint32_t base,*out;if(!inside(&d->current,x,y)||alpha<=0)return;alpha=(alpha*(int)ie_selection_alpha(&d->current,x,y)+127)/255;at=(size_t)y*d->current.width+x;if(d->brush_coverage[at]>=(unsigned)alpha)return;d->brush_coverage[at]=(uint8_t)alpha;base=d->undo[d->undo_count-1]->layers[d->scratch_layer].pixels[at];out=&d->current.layers[d->scratch_layer].pixels[at];if(b->erase){unsigned a=((base>>24)*(255u-(unsigned)alpha)+127)/255;*out=(base&0xffffffu)|(a<<24);}else *out=over(base,b->color,(unsigned)alpha);}
static void stamp(IEDoc*d,int cx,int cy,const IEBrush*b){
 int size=clampi(b->size,1,256),radius=size/2,x,y,start=-radius,end=start+size;
 int hard=clampi(b->hardness,0,100),op=clampi(b->opacity,0,100)*255/100,diff=clampi(b->diffusion,0,100),shape=clampi(b->square,0,5),bar=maxi(1,size/5);
 for(y=start;y<end;y++){if(((y-start)&31)==0)cooperate(d);for(x=start;x<end;x++){
  int dx=2*x+(size%2?0:1),dy=2*y+(size%2?0:1),dist,a,ax=cx+x,ay=cy+y;
  if(shape==IE_BRUSH_ROUND){int sq=dx*dx+dy*dy;if(sq>size*size)continue;dist=hard==100?0:(int)isqrtu((unsigned)sq)*100/size;}
  else if(shape==IE_BRUSH_SQUARE)dist=maxi(absi(dx),absi(dy))*100/size;
  else if(shape==IE_BRUSH_HORIZONTAL){if(absi(dy)>bar)continue;dist=maxi(absi(dx)*100/size,absi(dy)*100/(bar+1));}
  else if(shape==IE_BRUSH_VERTICAL){if(absi(dx)>bar)continue;dist=maxi(absi(dy)*100/size,absi(dx)*100/(bar+1));}
  else{int short_axis=shape==IE_BRUSH_DIAGONAL_UP?dx+dy:dx-dy;if(absi(short_axis)>bar*2)continue;dist=maxi(maxi(absi(dx),absi(dy))*100/size,absi(short_axis)*100/(bar*2+1));}
  if(size==1)dist=0;
  a=dist<=hard?255:(100-dist)*255/maxi(1,100-hard);a=a*op/255;
  if(diff&&spatial_random(d->brush_seed,ax,ay)%100u<(unsigned)(diff*4/5))continue;
  brush_paint(d,ax,ay,b,a);
 }}
}
int ie_brush(IEDoc*d,int x0,int y0,int x1,int y1,const IEBrush*b){
 int dx,dy,sx,sy,err,e,steps=0;if(!d||!b||!d->undo_count)return 0;
 if(!d->brush_coverage&&!coverage_allocate(d))return 0;
 if(d->scratch_layer!=d->current.active||d->undo[d->undo_count-1]->width!=d->current.width||d->undo[d->undo_count-1]->height!=d->current.height)return fail(d,"Geste interrompu; recommencer le trait.");
 x0=clampi(x0,-256,d->current.width+256);y0=clampi(y0,-256,d->current.height+256);x1=clampi(x1,-256,d->current.width+256);y1=clampi(y1,-256,d->current.height+256);
 dx=absi(x1-x0);dy=-absi(y1-y0);sx=x0<x1?1:-1;sy=y0<y1?1:-1;err=dx+dy;
 for(;;){if((steps++&7)==0)cooperate(d);stamp(d,x0,y0,b);if(x0==x1&&y0==y1)break;e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}return 1;
}
int ie_warp_begin(IEDoc*d,int size){uint32_t*p;int side;if(!d||!d->current.count)return 0;side=clampi(size,1,256)+132;p=d->alloc(d->memory_context,(size_t)side*side*sizeof(*p));if(!p)return fail(d,"Memoire insuffisante pour la deformation; image conservee.");if(!ie_gesture_begin(d)){d->release(d->memory_context,p);return 0;}d->warp_scratch=p;d->scratch_side=side;d->scratch_layer=d->current.active;return 1;}
/* Bilinear interpolation uses premultiplied channels to avoid dark fringes
   where opaque colour meets transparent pixels. The copied tile includes a
   fixed 66-pixel sampling margin, enough for every bounded displacement. */
static uint32_t warp_sample(const IEDoc*d,int tx,int ty,int tw,int th,int64_t sx16,int64_t sy16){
 int ix=(int)floor_div(sx16,65536),iy=(int)floor_div(sy16,65536),k;unsigned fx=(unsigned)(sx16-(int64_t)ix*65536)>>8,fy=(unsigned)(sy16-(int64_t)iy*65536)>>8;uint64_t alpha=0,channel[3]={0,0,0};int xx,yy;
 for(yy=0;yy<2;yy++)for(xx=0;xx<2;xx++){int x=ix+xx,y=iy+yy;uint32_t c=0;unsigned weight=(xx?fx:256-fx)*(yy?fy:256-fy),a;if(inside(&d->current,x,y)&&x>=tx&&y>=ty&&x<tx+tw&&y<ty+th)c=d->warp_scratch[(size_t)(y-ty)*tw+(x-tx)];a=c>>24;alpha+=(uint64_t)a*weight;for(k=0;k<3;k++)channel[k]+=(uint64_t)((c>>(8*k))&255)*a*weight;}
 if(!alpha)return 0;
 {uint32_t result=(uint32_t)((alpha+32768)/65536)<<24;for(k=0;k<3;k++)result|=(uint32_t)((channel[k]+alpha/2)/alpha)<<(8*k);return result;}
}
static void warp_stamp(IEDoc*d,int cx,int cy,int dx,int dy,int size,int strength,int mode){
 IERect r=region(&d->current);int radius=(size+1)/2,l=maxi(r.x,cx-size/2),t=maxi(r.y,cy-size/2),rr=mini(r.x+r.w-1,cx-size/2+size-1),bb=mini(r.y+r.h-1,cy-size/2+size-1),tx,ty,tw,th,x,y;uint32_t*p=d->current.layers[d->scratch_layer].pixels;int64_t radius2=(int64_t)radius*radius;if(l>rr||t>bb)return;
 tx=maxi(0,l-66);ty=maxi(0,t-66);tw=mini(d->current.width,rr+67)-tx;th=mini(d->current.height,bb+67)-ty;
 if(tw>d->scratch_side||th>d->scratch_side)return;
 for(y=0;y<th;y++){memcpy(d->warp_scratch+(size_t)y*tw,p+(size_t)(ty+y)*d->current.width+tx,(size_t)tw*sizeof(*p));if((y&31)==31)cooperate(d);}
 for(y=t;y<=bb;y++){if((y&15)==0)cooperate(d);for(x=l;x<=rr;x++){
  int ux=x-cx,uy=y-cy;int64_t distance=(int64_t)ux*ux+(int64_t)uy*uy,weight,sx16=(int64_t)x*65536,sy16=(int64_t)y*65536;if(distance>=radius2)continue;weight=(radius2-distance)*65536/radius2;weight=weight*weight/65536;
  if(mode==IE_WARP_PUSH){sx16-=(int64_t)dx*strength*weight/100;sy16-=(int64_t)dy*strength*weight/100;}
  else{int sign=mode==IE_WARP_INFLATE?-1:1;sx16+=sign*(int64_t)ux*strength*weight/200;sy16+=sign*(int64_t)uy*strength*weight/200;}
  p[(size_t)y*d->current.width+x]=mixed(p[(size_t)y*d->current.width+x],warp_sample(d,tx,ty,tw,th,sx16,sy16),ie_selection_alpha(&d->current,x,y));
 }}
}
int ie_warp_brush(IEDoc*d,int x0,int y0,int x1,int y1,int size,int strength,int mode){
 int steps,i,lastx,lasty,dx,dy,spacing;if(!d||!d->warp_scratch||!d->gesture||d->scratch_layer!=d->current.active)return 0;size=clampi(size,1,256);strength=clampi(strength,0,100);if(size+132>d->scratch_side)return fail(d,"Geste interrompu; recommencer le trait.");if(!strength||mode<0||mode>2)return 1;
 x0=clampi(x0,-256,d->current.width+256);y0=clampi(y0,-256,d->current.height+256);x1=clampi(x1,-256,d->current.width+256);y1=clampi(y1,-256,d->current.height+256);dx=x1-x0;dy=y1-y0;if(!dx&&!dy&&mode==IE_WARP_PUSH)return 1;spacing=mini(64,maxi(1,size/4));steps=maxi(absi(dx),absi(dy));steps=maxi(1,(steps+spacing-1)/spacing);lastx=x0;lasty=y0;
 for(i=1;i<=steps;i++){int x=x0+dx*i/steps,y=y0+dy*i/steps;warp_stamp(d,x,y,x-lastx,y-lasty,size,strength,mode);lastx=x;lasty=y;}return 1;
}

void ie_shape(IEDoc*d,int shape,int x0,int y0,int x1,int y1,uint32_t color,int opacity,int thickness,int fill,int outline){int x,y,t,left=mini(x0,x1),right=maxi(x0,x1),top=mini(y0,y1),bottom=maxi(y0,y1);int a=clampi(opacity,0,100)*255/100;thickness=clampi(thickness,1,256);if(shape==IE_SHAPE_LINE){
 int xx,yy;int64_t vx,vy,len; x0=clampi(x0,-IE_MAX_DIMENSION,2*IE_MAX_DIMENSION);y0=clampi(y0,-IE_MAX_DIMENSION,2*IE_MAX_DIMENSION);x1=clampi(x1,-IE_MAX_DIMENSION,2*IE_MAX_DIMENSION);y1=clampi(y1,-IE_MAX_DIMENSION,2*IE_MAX_DIMENSION);vx=x1-x0;vy=y1-y0;len=vx*vx+vy*vy;
 for(yy=maxi(0,mini(y0,y1)-thickness/2);yy<=mini(d->current.height-1,maxi(y0,y1)+thickness/2);yy++){if((yy&15)==0)cooperate(d);for(xx=maxi(0,mini(x0,x1)-thickness/2);xx<=mini(d->current.width-1,maxi(x0,x1)+thickness/2);xx++){
 int64_t px=xx-x0,py=yy-y0,dot=px*vx+py*vy,dist4,limit=(int64_t)thickness*thickness;
 if(dot<=0||!len)dist4=4*(px*px+py*py);else if(dot>=len){px=xx-x1;py=yy-y1;dist4=4*(px*px+py*py);}else{int64_t cross=px*vy-py*vx;if(4*cross*cross<=limit*len)paint(d,xx,yy,color,a,0);continue;}if(dist4<=limit)paint(d,xx,yy,color,a,0);
 }}return;}left=clampi(left,-IE_MAX_DIMENSION,2*IE_MAX_DIMENSION);right=clampi(right,-IE_MAX_DIMENSION,2*IE_MAX_DIMENSION);top=clampi(top,-IE_MAX_DIMENSION,2*IE_MAX_DIMENSION);bottom=clampi(bottom,-IE_MAX_DIMENSION,2*IE_MAX_DIMENSION);for(y=maxi(top,0);y<=mini(bottom,d->current.height-1);y++){if((y&7)==0)cooperate(d);for(x=maxi(left,0);x<=mini(right,d->current.width-1);x++){int outer=1,inner=0;if(shape==IE_SHAPE_RECT){inner=x>=left+thickness&&x<=right-thickness&&y>=top+thickness&&y<=bottom-thickness;}else if(shape==IE_SHAPE_ELLIPSE){int64_t rx=(right-left+1),ry=(bottom-top+1),px=2*(int64_t)x-left-right,py=2*(int64_t)y-top-bottom;outer=px*px*ry*ry+py*py*rx*rx<=rx*rx*ry*ry;rx-=2*thickness;ry-=2*thickness;inner=rx>0&&ry>0&&px*px*ry*ry+py*py*rx*rx<=rx*rx*ry*ry;}else return;t=outer&&(fill||(outline&&!inner));if(t)paint(d,x,y,color,a,0);}}}
int ie_layer_add(IEDoc*d,const char*name){uint32_t*p;int n=d->current.count;if(d->current.selection_mask&&pixel_bytes(&d->current)*(size_t)(n+1)+(size_t)d->current.width*d->current.height>IE_HISTORY_MAX_BYTES)return fail(d,"Mémoire insuffisante pour annuler la sélection ; image conservée");if(!valid_size(d->current.width,d->current.height,n+1))return fail(d,"Limite de calques/memoire atteinte.");do{p=(uint32_t*)d->alloc(d->memory_context,pixel_bytes(&d->current));if(p)break;}while(drop_any(d));if(!p)return fail(d,"Memoire insuffisante; image conservee.");memory_zero(d,p,pixel_bytes(&d->current));if(!ie_begin(d)){d->release(d->memory_context,p);return 0;}d->current.layers[n].pixels=p;d->current.layers[n].opacity=255;d->current.layers[n].visible=1;name_copy(d->current.layers[n].name,name?name:"Calque");d->current.count++;d->current.active=n;return 1;}
int ie_layer_duplicate(IEDoc*d){int source=d->current.active;if(!ie_layer_add(d,d->current.layers[source].name))return 0;memory_copy(d,d->current.layers[d->current.active].pixels,d->current.layers[source].pixels,pixel_bytes(&d->current));d->current.layers[d->current.active].opacity=d->current.layers[source].opacity;d->current.layers[d->current.active].visible=d->current.layers[source].visible;return 1;}
int ie_layer_delete(IEDoc*d){int i;if(d->current.count<=1)return fail(d,"Le dernier calque ne peut pas etre supprime.");if(!ie_begin(d))return 0;d->release(d->memory_context,d->current.layers[d->current.active].pixels);for(i=d->current.active+1;i<d->current.count;i++)d->current.layers[i-1]=d->current.layers[i];memset(&d->current.layers[--d->current.count],0,sizeof(IELayer));if(d->current.active>=d->current.count)d->current.active=d->current.count-1;return 1;}
int ie_layer_move(IEDoc*d,int direction){int to=d->current.active+(direction<0?-1:1);IELayer tmp;if(to<0||to>=d->current.count)return fail(d,"Limite de pile de calques.");if(!ie_begin(d))return 0;tmp=d->current.layers[to];d->current.layers[to]=d->current.layers[d->current.active];d->current.layers[d->current.active]=tmp;d->current.active=to;return 1;}
int ie_layer_style(IEDoc*d,int opacity,int visible){if(!ie_begin(d))return 0;d->current.layers[d->current.active].opacity=(uint8_t)clampi(opacity,0,255);d->current.layers[d->current.active].visible=(uint8_t)!!visible;return 1;}
int ie_layer_merge_down(IEDoc*d){IELayer*lo,*hi;size_t i,n;int j,a=d->current.active;if(a<1)return fail(d,"Aucun calque inferieur.");if(!ie_begin(d))return 0;lo=&d->current.layers[a-1];hi=&d->current.layers[a];n=(size_t)d->current.width*d->current.height;for(i=0;i<n;i++){uint32_t c;if((i&4095u)==0)cooperate(d);c=over(0,lo->pixels[i],lo->visible?lo->opacity:0);lo->pixels[i]=over(c,hi->pixels[i],hi->visible?hi->opacity:0);}lo->opacity=255;lo->visible=1;d->release(d->memory_context,hi->pixels);for(j=a+1;j<d->current.count;j++)d->current.layers[j-1]=d->current.layers[j];memset(&d->current.layers[--d->current.count],0,sizeof(IELayer));d->current.active=a-1;return 1;}
int ie_clear(IEDoc*d){IERect r=region(&d->current);int x,y;if(!r.w||!r.h)return 1;if(!ie_begin(d))return 0;for(y=r.y;y<r.y+r.h;y++){if((y&15)==0)cooperate(d);for(x=r.x;x<r.x+r.w;x++){uint32_t*p=&d->current.layers[d->current.active].pixels[(size_t)y*d->current.width+x];unsigned a=((*p>>24)*(255-ie_selection_alpha(&d->current,x,y))+127)/255;*p=a?(*p&0xffffffu)|(a<<24):0;}}return 1;}
int ie_adjust(IEDoc*d,int brightness,int contrast,int saturation,int red,int green,int blue,int all){IERect r=region(&d->current);int layer,x,y;brightness=clampi(brightness,-100,100)*255/100;contrast=clampi(contrast,-100,100)+100;saturation=clampi(saturation,-100,100)+100;red=clampi(red,0,200);green=clampi(green,0,200);blue=clampi(blue,0,200);if(!ie_begin(d))return 0;for(layer=0;layer<d->current.count;layer++){if((!all&&layer!=d->current.active)||(all&&!d->current.layers[layer].visible))continue;for(y=r.y;y<r.y+r.h;y++){if((y&7)==0)cooperate(d);for(x=r.x;x<r.x+r.w;x++){uint32_t*p=&d->current.layers[layer].pixels[(size_t)y*d->current.width+x],c=*p;int rr=c&255,gg=(c>>8)&255,bb=(c>>16)&255,l=(77*rr+150*gg+29*bb)/256;rr=l+(rr-l)*saturation/100;gg=l+(gg-l)*saturation/100;bb=l+(bb-l)*saturation/100;rr=((rr-128)*contrast/100+128+brightness)*red/100;gg=((gg-128)*contrast/100+128+brightness)*green/100;bb=((bb-128)*contrast/100+128+brightness)*blue/100;*p=mixed(c,IE_RGBA(clampi(rr,0,255),clampi(gg,0,255),clampi(bb,0,255),c>>24),ie_selection_alpha(&d->current,x,y));}}}return 1;}
static int geometry(IEDoc*d,int x,int y,int w,int h,int crop){IEState s;int l,xx,yy;if(!state_make(d,&s,w,h,d->current.count))return 0;s.active=d->current.active;for(l=0;l<s.count;l++){s.layers[l].opacity=d->current.layers[l].opacity;s.layers[l].visible=d->current.layers[l].visible;name_copy(s.layers[l].name,d->current.layers[l].name);for(yy=0;yy<h;yy++){if((yy&7)==0)cooperate(d);for(xx=0;xx<w;xx++){int sx=crop?x+xx:xx*d->current.width/w,sy=crop?y+yy:yy*d->current.height/h;if(sx>=0&&sy>=0&&sx<d->current.width&&sy<d->current.height)s.layers[l].pixels[(size_t)yy*w+xx]=d->current.layers[l].pixels[(size_t)sy*d->current.width+sx];}}}if(!replace_undo(d,&s)){state_free(d,&s);return 0;}return 1;}
int ie_resize(IEDoc*d,int w,int h){return geometry(d,0,0,w,h,0);}
int ie_crop(IEDoc*d,int x,int y,int w,int h){if(x<0||y<0||w<=0||h<=0||x>d->current.width-w||y>d->current.height-h)return fail(d,"Recadrage hors image.");return geometry(d,x,y,w,h,1);}
int ie_transform(IEDoc*d,int scalex,int scaley,int shear,int turns){IERect r=region(&d->current);const uint32_t*original;uint32_t*pixels;int x,y,tw,th,dw,dh,dx,dy,cx,cy;scalex=clampi(scalex,25,400);scaley=clampi(scaley,25,400);shear=clampi(shear,-200,200);turns=((turns%4)+4)%4;if(!r.w||!r.h)return 1;tw=maxi(1,r.w*scalex/100);th=maxi(1,r.h*scaley/100);dw=(turns&1)?th:tw;dh=(turns&1)?tw:th;if(!ie_begin(d))return 0;if(scalex==100&&scaley==100&&!shear&&!turns){ie_deselect(d);return 1;}original=d->undo[d->undo_count-1]->layers[d->current.active].pixels;pixels=d->current.layers[d->current.active].pixels;for(y=0;y<r.h;y++)for(x=0;x<r.w;x++){size_t at=(size_t)(r.y+y)*d->current.width+r.x+x;unsigned a=((pixels[at]>>24)*(255-ie_selection_alpha(&d->current,r.x+x,r.y+y))+127)/255;pixels[at]=a?(pixels[at]&0xffffffu)|(a<<24):0;}cx=r.x+r.w/2;cy=r.y+r.h/2;for(y=0;y<d->current.height;y++){if((y&7)==0)cooperate(d);for(x=0;x<d->current.width;x++){int ux,uy,sx,sy;dy=y-(cy-dh/2);dx=x-(cx-dw/2)-(dy-dh/2)*shear/100;if(dx<0||dy<0||dx>=dw||dy>=dh)continue;if(turns==0){ux=dx;uy=dy;}else if(turns==1){ux=dy;uy=th-1-dx;}else if(turns==2){ux=tw-1-dx;uy=th-1-dy;}else{ux=tw-1-dy;uy=dx;}sx=ux*r.w/tw;sy=uy*r.h/th;if(sx>=0&&sy>=0&&sx<r.w&&sy<r.h)pixels[(size_t)y*d->current.width+x]=over(pixels[(size_t)y*d->current.width+x],original[(size_t)(r.y+sy)*d->current.width+r.x+sx],ie_selection_alpha(&d->current,r.x+sx,r.y+sy));}}ie_deselect(d);return 1;}

int ie_cancel(IEDoc*d){IEState*s;unsigned i;scratch_free(d);if(!d->undo_count)return fail(d,"Historique vide.");s=d->undo[--d->undo_count];d->undo[d->undo_count]=0;d->history_bytes-=state_bytes(s);state_free(d,&d->current);d->current=*s;d->release(d->memory_context,s);if(d->gesture){while(d->redo_count)drop_oldest(d,d->redo,&d->redo_count);d->redo_count=d->saved_redo_count;for(i=0;i<d->saved_redo_count;i++){d->redo[i]=d->saved_redo[i];d->saved_redo[i]=0;}d->saved_redo_count=0;d->gesture=0;}d->dirty=1;d->error=0;return 1;}

int ie_translate_selection(IEDoc*d,int dx,int dy){
 IERect r=region(&d->current);const IEState*before;const uint32_t*original;uint32_t*p;uint8_t*mask=0;int x,y;size_t n=(size_t)d->current.width*d->current.height;
 dx=clampi(dx,-IE_MAX_DIMENSION,IE_MAX_DIMENSION);dy=clampi(dy,-IE_MAX_DIMENSION,IE_MAX_DIMENSION);if((!dx&&!dy)||!r.w||!r.h)return 1;
 if(d->current.selection_mask){mask=d->alloc(d->memory_context,n);if(!mask)return fail(d,"Mémoire insuffisante pour la sélection ; image conservée");memory_zero(d,mask,n);}
 if(!ie_begin(d)){if(mask)d->release(d->memory_context,mask);return 0;}before=d->undo[d->undo_count-1];original=before->layers[d->current.active].pixels;p=d->current.layers[d->current.active].pixels;
 for(y=0;y<r.h;y++)for(x=0;x<r.w;x++){size_t at=(size_t)(r.y+y)*d->current.width+r.x+x;unsigned a=((p[at]>>24)*(255-ie_selection_alpha(before,r.x+x,r.y+y))+127)/255;p[at]=a?(p[at]&0xffffffu)|(a<<24):0;}
 for(y=0;y<r.h;y++){if((y&7)==0)cooperate(d);for(x=0;x<r.w;x++){int xx=r.x+x+dx,yy=r.y+y+dy;if(xx>=0&&yy>=0&&xx<d->current.width&&yy<d->current.height){unsigned a=ie_selection_alpha(before,r.x+x,r.y+y);size_t at=(size_t)yy*d->current.width+xx;p[at]=over(p[at],original[(size_t)(r.y+y)*d->current.width+r.x+x],a);if(mask)mask[at]=(uint8_t)a;}}}
 if(r.enabled){ie_select(d,r.x+dx,r.y+dy,r.x+r.w-1+dx,r.y+r.h-1+dy);if(r.x+dx>=d->current.width||r.y+dy>=d->current.height||r.x+r.w+dx<=0||r.y+r.h+dy<=0)d->current.selection=(IERect){0,0,0,0,1};d->current.selection_mask=mask;}
 return 1;
}


/* Original compact 5x7 bitmap alphabet with French accent/cedilla marks. */
static const unsigned char font[95][7]={
 {0,0,0,0,0,0,0}, {4,4,4,4,4,0,4}, {10,10,10,0,0,0,0}, {10,31,10,10,31,10,0},
 {4,15,20,14,5,30,4}, {17,2,4,8,17,0,0}, {12,18,20,8,21,18,13}, {4,4,8,0,0,0,0},
 {2,4,8,8,8,4,2}, {8,4,2,2,2,4,8}, {0,21,14,31,14,21,0}, {0,4,4,31,4,4,0},
 {0,0,0,0,4,4,8}, {0,0,0,31,0,0,0}, {0,0,0,0,0,4,4}, {1,2,2,4,8,8,16},
 {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
 {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30}, {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
 {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}, {0,4,4,0,4,4,0}, {0,4,4,0,4,4,8},
 {2,4,8,16,8,4,2}, {0,0,31,0,31,0,0}, {8,4,2,1,2,4,8}, {14,17,1,2,4,0,4},
 {14,17,23,21,23,16,14}, {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16}, {14,17,16,23,17,17,15},
 {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17},
 {16,16,16,16,16,16,31}, {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17}, {15,16,16,14,1,1,30},
 {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10},
 {17,17,10,4,10,17,17}, {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}, {14,8,8,8,8,8,14},
 {16,8,8,4,2,2,1}, {14,2,2,2,2,2,14}, {4,10,17,0,0,0,0}, {0,0,0,0,0,0,31},
 {8,4,2,0,0,0,0}, {0,0,14,1,15,17,15}, {16,16,30,17,17,17,30}, {0,0,15,16,16,16,15},
 {1,1,15,17,17,17,15}, {0,0,14,17,31,16,14}, {6,8,8,30,8,8,8}, {0,0,15,17,15,1,14},
 {16,16,30,17,17,17,17}, {4,0,12,4,4,4,14}, {2,0,6,2,2,18,12}, {16,16,18,20,24,20,18},
 {12,4,4,4,4,4,14}, {0,0,26,21,21,21,21}, {0,0,30,17,17,17,17}, {0,0,14,17,17,17,14},
 {0,0,30,17,30,16,16}, {0,0,15,17,15,1,1}, {0,0,22,25,16,16,16}, {0,0,15,16,14,1,30},
 {8,8,30,8,8,9,6}, {0,0,17,17,17,19,13}, {0,0,17,17,17,10,4}, {0,0,17,17,21,21,10},
 {0,0,17,10,4,10,17}, {0,0,17,17,15,1,14}, {0,0,31,2,4,8,31}, {2,4,4,8,4,4,2},
 {4,4,4,4,4,4,4}, {8,4,4,2,4,4,8}, {0,0,8,21,2,0,0}
};
static unsigned base_letter(unsigned c){if(c>=0xc0&&c<=0xc5)return 'A';if(c>=0xe0&&c<=0xe5)return 'a';if(c==0xc7)return 'C';if(c==0xe7)return 'c';if(c>=0xc8&&c<=0xcb)return 'E';if(c>=0xe8&&c<=0xeb)return 'e';if(c>=0xcc&&c<=0xcf)return 'I';if(c>=0xec&&c<=0xef)return 'i';if(c==0xd1)return 'N';if(c==0xf1)return 'n';if(c>=0xd2&&c<=0xd6)return 'O';if(c>=0xf2&&c<=0xf6)return 'o';if(c>=0xd9&&c<=0xdc)return 'U';if(c>=0xf9&&c<=0xfc)return 'u';if(c==0xdd||c==0x178)return 'Y';if(c==0xfd||c==0xff)return 'y';return c;}
uint8_t ie_glyph_row(unsigned c,int row){unsigned base=base_letter(c);int accent=0;if(row<0||row>=7)return 0;if(c==0xe7){static const uint8_t cedilla[7]={0,0,15,16,16,15,4};return cedilla[row];}if(c==0xc7){static const uint8_t cedilla[7]={14,17,16,16,17,14,4};return cedilla[row];}switch(c){case 0xc0:case 0xc8:case 0xcc:case 0xd2:case 0xd9:case 0xe0:case 0xe8:case 0xec:case 0xf2:case 0xf9:accent=1;break;case 0xc1:case 0xc9:case 0xcd:case 0xd3:case 0xda:case 0xdd:case 0xe1:case 0xe9:case 0xed:case 0xf3:case 0xfa:case 0xfd:accent=2;break;case 0xc2:case 0xca:case 0xce:case 0xd4:case 0xdb:case 0xe2:case 0xea:case 0xee:case 0xf4:case 0xfb:accent=3;break;case 0xc4:case 0xcb:case 0xcf:case 0xd6:case 0xdc:case 0xe4:case 0xeb:case 0xef:case 0xf6:case 0xfc:case 0xff:case 0x178:accent=4;break;case 0xc3:case 0xd1:case 0xd5:case 0xe3:case 0xf1:case 0xf5:accent=5;break;default:break;}if(base<32||base>126)base='?';if(accent){if(row==0){static const uint8_t first[6]={0,8,2,4,10,10};return first[accent];}if(row==1){static const uint8_t second[6]={0,4,4,10,0,5};return second[accent];}if(base>='A'&&base<='Z'){static const uint8_t compress[5]={0,1,3,5,6};return font[base-32][compress[row-2]];}}return font[base-32][row];}

static unsigned utf8_next(const unsigned char**p){
 const unsigned char *start=*p,*tail;unsigned lead=*start++,value,n,i;
 *p=start;if(lead<128)return lead;
 if(lead>=0xc2&&lead<=0xdf){n=1;value=lead&31;}
 else if(lead>=0xe0&&lead<=0xef){n=2;value=lead&15;}
 else if(lead>=0xf0&&lead<=0xf4){n=3;value=lead&7;}
 else return '?';
 tail=start;
 for(i=0;i<n;i++){if((*tail&0xc0)!=0x80)return '?';value=(value<<6)|(*tail++&63);}
 if((n==2&&value<0x800)||(n==3&&value<0x10000)||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return '?';
 *p=tail;return value;
}
void ie_text(IEDoc*d,int x,int y,const char*utf8,int scale,uint32_t color,int opacity){const unsigned char*p=(const unsigned char*)utf8;int ox=x,row,col,xx,yy;unsigned count=0;if(!p)return;scale=clampi(scale,1,32);while(*p&&count++<1024){unsigned c=utf8_next(&p);cooperate(d);if(c=='\n'){x=ox;y+=8*scale;continue;}for(row=0;row<7;row++){unsigned bits=ie_glyph_row(c,row);for(col=0;col<5;col++)if(bits&(1u<<(4-col)))for(yy=0;yy<scale;yy++)for(xx=0;xx<scale;xx++)paint(d,x+col*scale+xx,y+row*scale+yy,color,clampi(opacity,0,100)*255/100,0);}x+=6*scale;}}

static unsigned get16(const unsigned char*p){return(unsigned)p[0]|((unsigned)p[1]<<8);}
static uint32_t get32(const unsigned char*p){return(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put16(unsigned char*p,unsigned v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void put32(unsigned char*p,uint32_t v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static int read_all(IERead f,void*c,void*p,size_t n){unsigned char*b=(unsigned char*)p;while(n){size_t want=n>4096?4096:n,got=f(c,b,want);if(!got||got>want)return 0;b+=got;n-=got;}return 1;}
static int write_all(IEWrite f,void*c,const void*p,size_t n){const unsigned char*b=(const unsigned char*)p;while(n){size_t want=n>4096?4096:n,got=f(c,b,want);if(!got||got>want)return 0;b+=got;n-=got;}return 1;}
static uint32_t hash_bytes(uint32_t h,const unsigned char*p,size_t n){while(n--){h^=*p++;h*=16777619u;}return h;}
static void install_loaded(IEDoc*d,IEState*s){clear_history(d);state_free(d,&d->current);d->current=*s;memset(s,0,sizeof(*s));d->dirty=0;d->error=0;}
int ie_load_bmp(IEDoc*d,IERead f,void*c){unsigned char head[54],skip[256],*row;IEState s;uint32_t dib,offset,bits,compression,rowbytes;int32_t width,height;int w,h,topdown,x,y;size_t remaining;if(!f||!read_all(f,c,head,54))return fail(d,"BMP tronque.");dib=get32(head+14);offset=get32(head+10);width=(int32_t)get32(head+18);height=(int32_t)get32(head+22);bits=get16(head+28);compression=get32(head+30);if(head[0]!='B'||head[1]!='M'||dib<40||dib>124||offset<14+dib||offset>65536||get16(head+26)!=1||(bits!=24&&bits!=32)||compression!=0||width<1||width>IE_MAX_DIMENSION||height==0||height>IE_MAX_DIMENSION||height<-IE_MAX_DIMENSION)return fail(d,"BMP non pris en charge (24/32 bits non compresse, <=8192).");w=(int)width;topdown=height<0;h=(int)(topdown?-height:height);rowbytes=((uint32_t)w*(bits/8)+3)&~3u;remaining=offset-54;while(remaining){size_t n=remaining>sizeof(skip)?sizeof(skip):remaining;if(!read_all(f,c,skip,n))return fail(d,"BMP tronque.");remaining-=n;}if(!state_make_import(d,&s,w,h,1))return 0;row=(unsigned char*)d->alloc(d->memory_context,rowbytes);if(!row){state_free(d,&s);return fail(d,"Memoire insuffisante; image conservee.");}for(y=0;y<h;y++){int yy=topdown?y:h-1-y;cooperate(d);if(!read_all(f,c,row,rowbytes)){d->release(d->memory_context,row);state_free(d,&s);return fail(d,"BMP tronque; image conservee.");}for(x=0;x<w;x++){const unsigned char*p=row+(size_t)x*(bits/8);/* BI_RGB's fourth byte is reserved, not an alpha mask. */s.layers[0].pixels[(size_t)yy*w+x]=IE_RGBA(p[2],p[1],p[0],255);}}d->release(d->memory_context,row);name_copy(s.layers[0].name,"Image BMP");install_loaded(d,&s);return 1;}
int ie_save_bmp(const IEDoc*d,IEWrite f,void*c){
 unsigned char head[54],chunk[768],padding[3]={0,0,0};int x,y,w=d->current.width,h=d->current.height;
 uint32_t stride=((uint32_t)w*3+3)&~3u;if(!f||!valid_size(w,h,d->current.count))return 0;
 memset(head,0,sizeof(head));head[0]='B';head[1]='M';put32(head+2,54+stride*(uint32_t)h);put32(head+10,54);put32(head+14,40);put32(head+18,(uint32_t)w);put32(head+22,(uint32_t)h);put16(head+26,1);put16(head+28,24);put32(head+34,stride*(uint32_t)h);
 if(!write_all(f,c,head,sizeof(head)))return 0;
 /* Fixed-size streaming chunks cover arbitrary valid widths without either
    a row-sized stack allocation or a second full-resolution frame. */
 for(y=h-1;y>=0;y--){
  cooperate(d);
  for(x=0;x<w;){int i,count=mini(w-x,(int)sizeof(chunk)/3);
   for(i=0;i<count;i++){uint32_t p=over(0xffffffffu,ie_composite(d,x+i,y),255);chunk[i*3]=(unsigned char)(p>>16);chunk[i*3+1]=(unsigned char)(p>>8);chunk[i*3+2]=(unsigned char)p;}
   if(!write_all(f,c,chunk,(size_t)count*3))return 0;
   x+=count;
  }
  if(!write_all(f,c,padding,stride-(uint32_t)w*3))return 0;
 }
 return 1;
}
static void layer_metadata(const IELayer*l,unsigned char*m){memset(m,0,28);m[0]=l->opacity;m[1]=l->visible;memcpy(m+4,l->name,24);m[27]=0;}
int ie_save_project(const IEDoc*d,IEWrite f,void*c){unsigned char head[32],meta[28],buf[1024];uint32_t hash=2166136261u;size_t n=(size_t)d->current.width*d->current.height,i,j;int l;if(!f||!valid_size(d->current.width,d->current.height,d->current.count))return 0;memset(head,0,sizeof(head));memcpy(head,"T5IMG001",8);put32(head+8,(uint32_t)d->current.width);put32(head+12,(uint32_t)d->current.height);put32(head+16,(uint32_t)d->current.count);put32(head+20,(uint32_t)d->current.active);hash=hash_bytes(hash,head,28);for(l=0;l<d->current.count;l++){layer_metadata(&d->current.layers[l],meta);hash=hash_bytes(hash,meta,sizeof(meta));for(i=0;i<n;i+=256){size_t count=n-i<256?n-i:256;cooperate(d);for(j=0;j<count;j++)put32(buf+j*4,d->current.layers[l].pixels[i+j]);hash=hash_bytes(hash,buf,count*4);}}memset(head,0,sizeof(head));memcpy(head,"T5IMG001",8);put32(head+8,(uint32_t)d->current.width);put32(head+12,(uint32_t)d->current.height);put32(head+16,(uint32_t)d->current.count);put32(head+20,(uint32_t)d->current.active);put32(head+28,hash);if(!write_all(f,c,head,sizeof(head)))return 0;for(l=0;l<d->current.count;l++){layer_metadata(&d->current.layers[l],meta);if(!write_all(f,c,meta,sizeof(meta)))return 0;for(i=0;i<n;i+=256){size_t count=n-i<256?n-i:256;cooperate(d);for(j=0;j<count;j++)put32(buf+j*4,d->current.layers[l].pixels[i+j]);if(!write_all(f,c,buf,count*4))return 0;}}return 1;}
int ie_load_project(IEDoc*d,IERead f,void*c){unsigned char head[32],meta[28],buf[1024];IEState s;uint32_t hash=2166136261u,w,h,count,active;size_t i,j,n;int l;if(!f||!read_all(f,c,head,sizeof(head)))return fail(d,"Projet tronque.");w=get32(head+8);h=get32(head+12);count=get32(head+16);active=get32(head+20);if(memcmp(head,"T5IMG001",8)||w>IE_MAX_DIMENSION||h>IE_MAX_DIMENSION||count>IE_MAX_LAYERS||active>=count||get32(head+24)!=0||!valid_size((int)w,(int)h,(int)count))return fail(d,"Projet T5I invalide.");hash=hash_bytes(hash,head,28);if(!state_make_import(d,&s,(int)w,(int)h,(int)count))return 0;s.active=(int)active;n=(size_t)w*h;for(l=0;l<s.count;l++){if(!read_all(f,c,meta,sizeof(meta))||meta[1]>1||meta[2]||meta[3]||meta[27]){state_free(d,&s);return fail(d,"Projet invalide/tronque; image conservee.");}hash=hash_bytes(hash,meta,sizeof(meta));s.layers[l].opacity=meta[0];s.layers[l].visible=meta[1];memcpy(s.layers[l].name,meta+4,24);for(i=0;i<n;i+=256){size_t amount=n-i<256?n-i:256;cooperate(d);if(!read_all(f,c,buf,amount*4)){state_free(d,&s);return fail(d,"Projet tronque; image conservee.");}hash=hash_bytes(hash,buf,amount*4);for(j=0;j<amount;j++)s.layers[l].pixels[i+j]=get32(buf+j*4);}}if(hash!=get32(head+28)){state_free(d,&s);return fail(d,"Projet altere; image conservee.");}install_loaded(d,&s);return 1;}

#include "image_selection.h"
