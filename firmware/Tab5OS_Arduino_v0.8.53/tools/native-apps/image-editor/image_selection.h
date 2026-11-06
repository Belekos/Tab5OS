/* Selection/fill engine. Included once by image_core.c (native and firmware).
   Work buffers are document-sized and use the document's bounded allocator.
   Jobs never call LVGL; the host services a finite amount of work per frame. */
struct IEJob {
 IEDoc *doc;
 IESelectionOptions select;
 IEFillOptions fill;
 uint8_t *mask,*temporary;
 uint32_t *queue,*sums;
 uint16_t *relief;
 int32_t *envelope;
 int relief_w,relief_h,relief_max,relief_limit,relief_step,light_x,light_y;
 IEPoint *points;
 int32_t *crossings;
 size_t n,at,head,tail;
 int stage,fill_job,transaction,dirty,complete,cancelled;
 int y,minx,miny,maxx,maxy;
 uint32_t reference;
};
enum { JOB_GENERATE,JOB_FLOOD,JOB_CLEAN,JOB_HORIZONTAL,JOB_VERTICAL,JOB_COMBINE,JOB_RELIEF_FORWARD,JOB_RELIEF_BACKWARD,JOB_RELIEF_HEIGHT,JOB_PAINT,JOB_DONE };
static unsigned color_distance(uint32_t a,uint32_t b){
 unsigned aa=a>>24,ba=b>>24,k,d=(unsigned)absi((int)aa-(int)ba);
 for(k=0;k<24;k+=8){unsigned x=(((a>>k)&255)*aa+127)/255,y=(((b>>k)&255)*ba+127)/255;d=(unsigned)maxi((int)d,absi((int)x-(int)y));}return d;
}
static uint32_t job_sample(const IEJob*j,int x,int y){const IEState*s=&j->doc->current;return j->select.sample_merged?ie_composite(j->doc,x,y):s->layers[s->active].pixels[(size_t)y*s->width+x];}
static void job_buffers(IEJob*j){
 IEDoc*d=j->doc;void*p[]={j->mask,j->temporary,j->queue,j->sums,j->points,j->crossings,j->relief,j->envelope};unsigned i;for(i=0;i<sizeof(p)/sizeof(p[0]);i++)if(p[i])d->release(d->memory_context,p[i]);j->mask=j->temporary=0;j->queue=j->sums=0;j->points=0;j->crossings=0;j->relief=0;j->envelope=0;
}
void ie_job_cancel(IEJob*j){if(!j||j->complete)return;if(j->transaction){ie_cancel(j->doc);j->doc->dirty=j->dirty;j->transaction=0;}j->cancelled=1;j->complete=-1;job_buffers(j);}
void ie_job_destroy(IEJob*j){IEDoc*d;if(!j)return;d=j->doc;if(!j->complete)ie_job_cancel(j);job_buffers(j);d->release(d->memory_context,j);}
static IEJob*job_make(IEDoc*d,const IESelectionOptions*options){
 IEJob*j;size_t n;if(!d||!options||!d->current.count||d->gesture)return 0;n=(size_t)d->current.width*d->current.height;
 j=d->alloc(d->memory_context,sizeof(*j));if(!j){fail(d,"Mémoire insuffisante pour la sélection ; image conservée");return 0;}memset(j,0,sizeof(*j));j->doc=d;j->select=*options;j->n=n;j->dirty=d->dirty;j->minx=d->current.width;j->miny=d->current.height;j->maxx=j->maxy=-1;
 j->select.tolerance=clampi(options->tolerance,0,255);j->select.feather=clampi(options->feather,-32,32);j->select.edge=clampi(options->edge,0,255);j->select.combine=clampi(options->combine,0,3);
 j->select.x0=clampi(options->x0,0,d->current.width-1);j->select.y0=clampi(options->y0,0,d->current.height-1);j->select.x1=clampi(options->x1,0,d->current.width-1);j->select.y1=clampi(options->y1,0,d->current.height-1);
 j->mask=d->alloc(d->memory_context,n);if(!j->mask)goto failed;memory_zero(d,j->mask,n);
 if(j->select.feather){j->temporary=d->alloc(d->memory_context,n);j->sums=d->alloc(d->memory_context,(size_t)d->current.width*sizeof(*j->sums));if(!j->temporary||!j->sums)goto failed;}
 if(options->method==IE_SELECT_LASSO){
  if(!options->points||options->point_count<3||options->point_count>2048){fail(d,"Tracer au moins trois points pour le lasso");goto invalid;}
  j->points=d->alloc(d->memory_context,(size_t)options->point_count*sizeof(*j->points));j->crossings=d->alloc(d->memory_context,(size_t)options->point_count*sizeof(*j->crossings));if(!j->points||!j->crossings)goto failed;
  {unsigned i;for(i=0;i<options->point_count;i++){j->points[i].x=clampi(options->points[i].x,0,d->current.width-1);j->points[i].y=clampi(options->points[i].y,0,d->current.height-1);}}j->select.points=j->points;
 }
 if(options->method==IE_SELECT_COLOR||options->method==IE_SELECT_CONTIGUOUS)j->reference=job_sample(j,j->select.x0,j->select.y0);
 if(options->method==IE_SELECT_CONTIGUOUS){j->queue=d->alloc(d->memory_context,n*sizeof(*j->queue));if(!j->queue)goto failed;j->stage=JOB_FLOOD;}
 d->error=0;return j;
 failed:fail(d,"Mémoire insuffisante pour la sélection ; image conservée");
 invalid:j->complete=-1;ie_job_destroy(j);return 0;
}
IEJob*ie_selection_start(IEDoc*d,const IESelectionOptions*options){IEJob*j;if(!d||!d->current.count||!options||options->method<0||options->method>IE_SELECT_INVERT)return 0;if(pixel_bytes(&d->current)*(size_t)d->current.count+(size_t)d->current.width*d->current.height>IE_HISTORY_MAX_BYTES){fail(d,"Mémoire insuffisante pour annuler la sélection ; image conservée");return 0;}j=job_make(d,options);if(j&&j->stage==JOB_FLOOD){size_t at=(size_t)j->select.y0*d->current.width+j->select.x0;j->mask[at]=255;j->queue[j->tail++]=(uint32_t)at;}return j;}
IEJob*ie_fill_start(IEDoc*d,const IEFillOptions*options){
 IESelectionOptions q;IEJob*j;if(!d||!d->current.count||!options||options->kind<0||options->kind>IE_FILL_RELIEF||options->region<0||options->region>IE_FILL_SELECTION)return 0;
 memset(&q,0,sizeof(q));q.method=options->region==IE_FILL_CONTIGUOUS?IE_SELECT_CONTIGUOUS:options->region==IE_FILL_MATCHING?IE_SELECT_COLOR:IE_SELECT_RECT;q.x0=options->x;q.y0=options->y;q.x1=d->current.width-1;q.y1=d->current.height-1;q.tolerance=options->tolerance;q.feather=options->feather;q.sample_merged=options->sample_merged;q.edge=options->edge;
 if(options->region==IE_FILL_SELECTION){q.method=IE_SELECT_EXISTING;q.x0=q.y0=0;}
 j=job_make(d,&q);if(!j)return 0;j->fill_job=1;j->fill=*options;j->fill.x=clampi(options->x,0,d->current.width-1);j->fill.y=clampi(options->y,0,d->current.height-1);j->fill.opacity=clampi(options->opacity,0,100);j->fill.density=clampi(options->density,0,100);j->fill.scale=clampi(options->scale,2,64);j->fill.direction=clampi(options->direction,0,3);
 j->fill.relief_profile=clampi(options->relief_profile,IE_RELIEF_LINEAR,IE_RELIEF_EXTRUDE);
 j->fill.relief_unit=clampi(options->relief_unit,IE_RELIEF_PIXELS,IE_RELIEF_PERCENT);
 j->fill.relief_extent=clampi(options->relief_extent,1,j->fill.relief_unit==IE_RELIEF_PERCENT?100:IE_MAX_DIMENSION);
 j->fill.relief_height=clampi(options->relief_height,-256,256);
 j->fill.relief_azimuth=options->relief_azimuth%360;if(j->fill.relief_azimuth<0)j->fill.relief_azimuth+=360;
 j->fill.relief_intensity=clampi(options->relief_intensity,0,200);
 if(j->stage==JOB_FLOOD&&ie_selection_alpha(&d->current,q.x0,q.y0)){size_t at=(size_t)q.y0*d->current.width+q.x0;j->mask[at]=255;j->queue[j->tail++]=(uint32_t)at;}return j;
}
static void job_after_generation(IEJob*j){if(j->queue){j->doc->release(j->doc->memory_context,j->queue);j->queue=0;}j->at=0;j->y=0;j->stage=j->select.feather?JOB_HORIZONTAL:JOB_COMBINE;}
/* In-place heapsort: bounded stack, no hidden libc allocation or ELF import. */
static void crossing_sift(int32_t*a,unsigned root,unsigned n){
 while(root*2+1<n){unsigned child=root*2+1;int32_t tmp;if(child+1<n&&a[child]<a[child+1])++child;if(a[root]>=a[child])return;tmp=a[root];a[root]=a[child];a[child]=tmp;root=child;}
}
static void crossings_sort(int32_t*a,unsigned n){
 unsigned i;for(i=n/2;i>0;){--i;crossing_sift(a,i,n);}for(i=n;i>1;){int32_t tmp=a[0];--i;a[0]=a[i];a[i]=tmp;crossing_sift(a,0,i);}
}
static void job_lasso_row(IEJob*j,int y){
 unsigned i,n=0;const int w=j->doc->current.width;const IEPoint*points=j->points;int64_t py=(int64_t)y*2;
 for(i=0;i<j->select.point_count;i++){IEPoint a=points[i],b=points[(i+1)%j->select.point_count];int64_t ay=(int64_t)a.y*2,by=(int64_t)b.y*2;if(a.y==y&&b.y==y){int l=mini(a.x,b.x),r=maxi(a.x,b.x);memset(j->mask+(size_t)y*w+l,255,(size_t)(r-l+1));}if((ay<=py&&by>py)||(by<=py&&ay>py)){int64_t x=(int64_t)a.x*65536+(py-ay)*(b.x-a.x)*65536/(by-ay);j->crossings[n++]=(int32_t)x;}}
 crossings_sort(j->crossings,n);
 for(i=0;i+1<n;i+=2){int l=(int)floor_div((int64_t)j->crossings[i]+65535,65536),r=(int)floor_div((int64_t)j->crossings[i+1],65536)+1;l=clampi(l,0,w);r=clampi(r,0,w);if(r>l)memset(j->mask+(size_t)y*w+l,255,(size_t)(r-l));}
}
static unsigned ellipse_coverage(int x,int y,int l,int t,int r,int b){
 int64_t rx=2*(int64_t)(r-l+1),ry=2*(int64_t)(b-t+1),rx2=rx*rx,ry2=ry*ry;unsigned count=0;int yy,xx;
 for(yy=1;yy<=3;yy+=2)for(xx=1;xx<=3;xx+=2){int64_t dx=4*(int64_t)x+xx-2*(l+r+1),dy=4*(int64_t)y+yy-2*(t+b+1);if(dx*dx*ry2+dy*dy*rx2<=rx2*ry2)++count;}
 return(count*255+2)/4;
}
static void job_generate_row(IEJob*j,int y){
 IEDoc*d=j->doc;const int w=d->current.width;int x,l=mini(j->select.x0,j->select.x1),r=maxi(j->select.x0,j->select.x1),t=mini(j->select.y0,j->select.y1),b=maxi(j->select.y0,j->select.y1);uint8_t*out=j->mask+(size_t)y*w;
 if(j->select.method==IE_SELECT_LASSO){job_lasso_row(j,y);return;}
 for(x=0;x<w;x++){
  unsigned a=0;switch(j->select.method){
   case IE_SELECT_RECT:a=x>=l&&x<=r&&y>=t&&y<=b?255:0;break;
   case IE_SELECT_ELLIPSE:a=x>=l&&x<=r&&y>=t&&y<=b?ellipse_coverage(x,y,l,t,r,b):0;break;
   case IE_SELECT_COLOR:a=color_distance(job_sample(j,x,y),j->reference)<=(unsigned)j->select.tolerance?255:0;break;
   case IE_SELECT_EXISTING:a=ie_selection_alpha(&d->current,x,y);break;
   case IE_SELECT_INVERT:a=255-ie_selection_alpha(&d->current,x,y);break;
   default:break;
  }out[x]=(uint8_t)a;
 }
}
static void job_neighbor(IEJob*j,int x,int y,uint32_t adjacent){
 const int w=j->doc->current.width,h=j->doc->current.height;size_t at;uint32_t c;if(x<0||y<0||x>=w||y>=h)return;at=(size_t)y*w+x;if(j->mask[at])return;
 if(j->fill_job&&!ie_selection_alpha(&j->doc->current,x,y)){j->mask[at]=128;return;}
 c=job_sample(j,x,y);if(color_distance(c,j->reference)>(unsigned)j->select.tolerance){j->mask[at]=128;return;}
 if(j->select.edge&&color_distance(c,adjacent)>(unsigned)j->select.edge)return;
 j->mask[at]=255;/* Every pixel is enqueued only once. */j->queue[j->tail++]=(uint32_t)at;
}
static void job_horizontal(IEJob*j,int y){
 const int w=j->doc->current.width,r=absi(j->select.feather),k=2*r+1;const uint8_t*in=j->mask+(size_t)y*w;uint8_t*out=j->temporary+(size_t)y*w;unsigned sum=0;int x,i;const int inner=j->select.feather<0;
 /* For an inner edge, soften the nonzero support then retain its original
    alpha. This avoids destroying partially selected interiors. */
 for(i=-r;i<=r;i++){unsigned a=in[clampi(i,0,w-1)];sum+=inner?(a?255:0):a;}
 for(x=0;x<w;x++){unsigned a,b;out[x]=(uint8_t)((sum+k/2)/(unsigned)k);a=in[clampi(x-r,0,w-1)];b=in[clampi(x+r+1,0,w-1)];sum-=inner?(a?255:0):a;sum+=inner?(b?255:0):b;}
}
static void job_vertical(IEJob*j,int y){
 const int w=j->doc->current.width,h=j->doc->current.height,r=absi(j->select.feather),k=2*r+1;int x,i;uint8_t*out=j->mask+(size_t)y*w;
 if(!y){memset(j->sums,0,(size_t)w*sizeof(*j->sums));for(i=-r;i<=r;i++){const uint8_t*row=j->temporary+(size_t)clampi(i,0,h-1)*w;for(x=0;x<w;x++)j->sums[x]+=row[x];}}
 /* The original row is still present here. A negative radius maps the
    inner half of the same blur to 0..255 and clips it to that original mask.
    Thus inner feathering can never add even one outside pixel, including
    anti-aliased edges, concave shapes and holes; no extra mask is allocated. */
 for(x=0;x<w;x++){unsigned a=(j->sums[x]+k/2)/(unsigned)k;if(j->select.feather<0)a=(out[x]*(unsigned)maxi(0,(int)a*2-255)+127)/255;out[x]=(uint8_t)a;j->sums[x]-=j->temporary[(size_t)clampi(y-r,0,h-1)*w+x];j->sums[x]+=j->temporary[(size_t)clampi(y+r+1,0,h-1)*w+x];}
}
static void job_combine(IEJob*j,int y){
 const int w=j->doc->current.width;int x;for(x=0;x<w;x++){size_t at=(size_t)y*w+x;unsigned a=j->mask[at],old=ie_selection_alpha(&j->doc->current,x,y);
  if(j->fill_job){
   /* Filling the selection already generated that alpha: intersect once,
      otherwise a semi-selected pixel would be attenuated a second time. */
   if(j->fill.region==IE_FILL_SELECTION)a=(unsigned)mini((int)a,(int)old);
   else a=(a*old+127)/255;
  }
  /* No active selection means an unrestricted paint canvas, not a full
     pre-existing selection to subtract from. The first traced shape creates
     its own mask in every combine mode; explicit inversion stays explicit. */
  else if(!j->doc->current.selection.enabled){}
  else if(j->select.combine==IE_SELECT_ADD)a=(unsigned)maxi((int)old,(int)a);
  else if(j->select.combine==IE_SELECT_SUBTRACT)a=old>a?old-a:0;
  else if(j->select.combine==IE_SELECT_INTERSECT)a=(unsigned)mini((int)old,(int)a);
  j->mask[at]=(uint8_t)a;if(a){j->minx=mini(j->minx,x);j->maxx=maxi(j->maxx,x);j->miny=mini(j->miny,y);j->maxy=maxi(j->maxy,y);}
 }
}
/* Exact squared Euclidean transform of the raster support. The horizontal
   distances and final Q4 distances share one uint16_t bounding-box field.
   Each column uses an integer lower envelope and one output column, keeping
   the original horizontal samples intact until that column is finished. */
static unsigned relief_sqrt64(uint64_t n){uint64_t x=0,bit=(uint64_t)1<<62;while(bit>n)bit>>=2;while(bit){if(n>=x+bit){n-=x+bit;x=(x>>1)+bit;}else x>>=1;bit>>=2;}return(unsigned)x;}
static int relief_sine(int angle){
 static const uint16_t sine[91]={0,18,36,54,71,89,107,125,143,160,178,195,213,230,248,265,282,299,316,333,350,367,384,400,416,433,449,465,481,496,512,527,543,558,573,587,602,616,630,644,658,672,685,698,711,724,737,749,761,773,784,796,807,818,828,839,849,859,868,878,887,896,904,912,920,928,935,943,949,956,962,968,974,979,984,989,994,998,1002,1005,1008,1011,1014,1016,1018,1020,1022,1023,1023,1024,1024};int sign=1;angle%=360;if(angle<0)angle+=360;if(angle>180){sign=-1;angle-=180;}if(angle>90)angle=180-angle;return sign*(int)sine[angle];
}
static int job_relief_allocate(IEJob*j){
 IEDoc*d=j->doc;size_t n;j->relief_w=j->maxx-j->minx+1;j->relief_h=j->maxy-j->miny+1;n=(size_t)j->relief_w*j->relief_h;
 j->relief=d->alloc(d->memory_context,n*sizeof(*j->relief));
 j->envelope=d->alloc(d->memory_context,(2*(size_t)j->relief_h+5)*sizeof(*j->envelope)+(size_t)j->relief_h*sizeof(uint16_t));
 if(!j->relief||!j->envelope)return fail(d,"Mémoire insuffisante pour la sélection ; image conservée");
 j->light_x=relief_sine(j->fill.relief_azimuth+90)*724/1024;j->light_y=relief_sine(j->fill.relief_azimuth)*724/1024;return 1;
}
static void job_relief_forward(IEJob*j,int y){
 const int w=j->relief_w;int x;uint16_t*row=j->relief+(size_t)y*w;const uint8_t*mask=j->mask+(size_t)(y+j->miny)*j->doc->current.width+j->minx;
 for(x=0;x<w;x++)row[x]=(uint16_t)(mask[x]?(x?row[x-1]:0)+1:0);
 for(x=w-1;x>=0;x--)if(row[x])row[x]=(uint16_t)mini(row[x],(x+1<w?row[x+1]:0)+1);
}
static int64_t job_relief_cost(const IEJob*j,int x,int y){int d;if(y<0||y>=j->relief_h)return 0;d=j->relief[(size_t)y*j->relief_w+x];return(int64_t)d*d;}
static void job_relief_backward(IEJob*j,int x){
 int h=j->relief_h,w=j->relief_w,q,k=0,y;int32_t*v=j->envelope,*z=v+h+2;uint16_t*out=(uint16_t*)(z+h+3);
 v[0]=-1;z[0]=INT32_MIN;z[1]=INT32_MAX;
 for(q=0;q<=h;q++){
  int64_t boundary;
  for(;;){int p=v[k];boundary=floor_div(job_relief_cost(j,x,q)+(int64_t)q*q-job_relief_cost(j,x,p)-(int64_t)p*p,2*(q-p))+1;if(boundary>z[k]||!k)break;--k;}
  ++k;v[k]=q;z[k]=(int32_t)boundary;z[k+1]=INT32_MAX;
 }
 k=0;
 for(y=0;y<h;y++){int p,dy;unsigned distance;while(z[k+1]<=y)++k;p=v[k];dy=y-p;distance=relief_sqrt64((uint64_t)(job_relief_cost(j,x,p)+(int64_t)dy*dy)*256u);out[y]=(uint16_t)mini((int)distance,65535);j->relief_max=maxi(j->relief_max,out[y]);}
 for(y=0;y<h;y++)j->relief[(size_t)y*w+x]=out[y];
}
static void job_relief_prepare_height(IEJob*j){
 j->relief_limit=j->fill.relief_unit==IE_RELIEF_PERCENT?maxi(1,(j->relief_max*j->fill.relief_extent+50)/100):j->fill.relief_extent*16;
 j->relief_step=j->fill.relief_profile==IE_RELIEF_EXTRUDE?1:clampi(j->relief_limit/(16*24),1,3);
}
static void job_relief_height_row(IEJob*j,int y){
 int x,amplitude=absi(j->fill.relief_height)*64;uint16_t*row=j->relief+(size_t)y*j->relief_w;
 for(x=0;x<j->relief_w;x++){
  unsigned t,profile;if(!row[x])continue;
  t=(unsigned)mini(4096,(int)(((int64_t)row[x]*4096+j->relief_limit/2)/j->relief_limit));
  if(j->fill.relief_profile==IE_RELIEF_EXTRUDE)profile=4096;
  else if(j->fill.relief_profile==IE_RELIEF_CIRCULAR)profile=isqrtu(4096u*4096u-(4096u-t)*(4096u-t));
  else profile=t;
  row[x]=(uint16_t)((amplitude*profile+2048)/4096);
 }
}
static int job_relief_height(const IEJob*j,int x,int y){
 int value;x-=j->minx;y-=j->miny;if(x<0||y<0||x>=j->relief_w||y>=j->relief_h)return 0;value=j->relief[(size_t)y*j->relief_w+x];return j->fill.relief_height<0?-value:value;
}
static uint32_t job_relief_color(IEJob*j,int x,int y){
 static const int weights[5]={1,4,6,4,1},derivative[5]={1,2,0,-2,-1};int xx,yy,nx=0,ny=0,shade;unsigned norm,k;uint32_t base=j->fill.foreground,result=base&0xff000000u;
 /* Relief is a fill: shade the colour captured when the gesture started. */
 if(!j->fill.relief_height||!j->fill.relief_intensity)return base;
 /* Smoothed derivatives affect only lighting; neither coverage nor image
    alpha is blurred. Sample spacing grows to at most 3 pixels on wider
    profiles, retaining 25 reads and reducing discrete-contour normal bands.
    Height is Q6, with a fixed 45-degree light elevation. */
 for(yy=-2;yy<=2;yy++)for(xx=-2;xx<=2;xx++){int value=job_relief_height(j,x+xx*j->relief_step,y+yy*j->relief_step);nx+=value*derivative[xx+2]*weights[yy+2];ny+=value*weights[xx+2]*derivative[yy+2];}
 nx/=128*j->relief_step;ny/=128*j->relief_step;norm=isqrtu((unsigned)(nx*nx)+(unsigned)(ny*ny)+64u*64u);
 shade=clampi((((nx*j->light_x+ny*j->light_y+64*724)/(int)norm)-724)*j->fill.relief_intensity/100,-1024,1024);
 for(k=0;k<24;k+=8){int v=(base>>k)&255;v=shade<0?(v*(1024+shade)+512)/1024:v+((255-v)*shade+512)/1024;result|=(uint32_t)v<<k;}return result;
}
static uint32_t gradient_color(uint32_t a,uint32_t b,unsigned weight){unsigned k;uint32_t c=0;for(k=0;k<32;k+=8)c|=((((a>>k)&255)*(255-weight)+((b>>k)&255)*weight+127)/255)<<k;return c;}
static uint32_t job_fill_color(IEJob*j,int x,int y,unsigned*alpha){
 const IEFillOptions*f=&j->fill;unsigned weight=0;int w=maxi(1,j->maxx-j->minx),h=maxi(1,j->maxy-j->miny),u=x-j->minx,v=y-j->miny,scale=f->scale;
 if(!f->density){*alpha=0;return 0;}
 if(f->kind==IE_FILL_HATCH){int threshold=(scale*f->density+99)/100;if((x+y)%scale>=threshold)*alpha=0;return f->foreground;}
 if(f->kind==IE_FILL_DOTS){int xx=2*(x%scale)+1-scale,yy=2*(y%scale)+1-scale;if((xx*xx+yy*yy)*100>scale*scale*f->density)*alpha=0;return f->foreground;}
 if(f->density<100&&spatial_random(0x7e451b23u,x,y)%100u>=(unsigned)f->density)*alpha=0;
 if(f->kind==IE_FILL_RELIEF)return job_relief_color(j,x,y);
 if(f->kind==IE_FILL_CHECKER)return((x/scale+y/scale)&1)?f->background:f->foreground;
 if(f->kind==IE_FILL_LINEAR){if(f->direction==0)weight=(unsigned)(u*255/w);else if(f->direction==1)weight=(unsigned)(v*255/h);else if(f->direction==2)weight=(unsigned)((u*255/w+v*255/h)/2);else weight=(unsigned)((u*255/w+255-v*255/h)/2);}
 else if(f->kind==IE_FILL_RADIAL){int dx=x-f->x,dy=y-f->y,rx=maxi(absi(j->minx-f->x),absi(j->maxx-f->x)),ry=maxi(absi(j->miny-f->y),absi(j->maxy-f->y));unsigned radius=isqrtu((unsigned)(rx*rx+ry*ry));weight=radius?isqrtu((unsigned)(dx*dx+dy*dy))*255/radius:0;if(weight>255)weight=255;}
 return f->kind==IE_FILL_LINEAR||f->kind==IE_FILL_RADIAL?gradient_color(f->foreground,f->background,weight):f->foreground;
}
unsigned ie_job_progress(const IEJob*j){unsigned p;if(!j)return 0;if(j->complete)return 100;switch(j->stage){case JOB_FLOOD:return (unsigned)(j->head*35/maxi(1,(int)j->n));case JOB_GENERATE:p=(unsigned)(j->y*35/j->doc->current.height);break;case JOB_CLEAN:p=35;break;case JOB_HORIZONTAL:p=40+(unsigned)(j->y*15/j->doc->current.height);break;case JOB_VERTICAL:p=55+(unsigned)(j->y*15/j->doc->current.height);break;case JOB_COMBINE:p=70+(unsigned)(j->y*(j->fill_job?10:29)/j->doc->current.height);break;case JOB_RELIEF_FORWARD:p=80+(unsigned)(j->y*2/j->relief_h);break;case JOB_RELIEF_BACKWARD:p=82+(unsigned)(j->y*2/j->relief_w);break;case JOB_RELIEF_HEIGHT:p=84+(unsigned)(j->y*2/j->relief_h);break;case JOB_PAINT:p=(j->fill.kind==IE_FILL_RELIEF?86:80)+(unsigned)(j->y*(j->fill.kind==IE_FILL_RELIEF?13:19)/j->doc->current.height);break;default:p=99;}return p;}
int ie_job_step(IEJob*j,size_t budget){
 IEDoc*d;int w,h;size_t processed=0;if(!j)return -1;if(j->complete)return j->complete;d=j->doc;w=d->current.width;h=d->current.height;if(!budget)budget=1;
 while(processed<budget&&!j->complete){
  if(j->stage==JOB_FLOOD){
   if(j->head<j->tail){size_t at=j->queue[j->head++];int x=(int)(at%(size_t)w),y=(int)(at/(size_t)w);uint32_t c=job_sample(j,x,y);job_neighbor(j,x-1,y,c);job_neighbor(j,x+1,y,c);job_neighbor(j,x,y-1,c);job_neighbor(j,x,y+1,c);processed+=4;}
   else {j->stage=JOB_CLEAN;j->at=0;}
  }else if(j->stage==JOB_CLEAN){
   size_t amount=budget-processed,end;if(amount>j->n-j->at)amount=j->n-j->at;end=j->at+amount;processed+=amount;while(j->at<end){if(j->mask[j->at]!=255)j->mask[j->at]=0;++j->at;}if(j->at==j->n)job_after_generation(j);
  }else if(j->stage==JOB_GENERATE){job_generate_row(j,j->y++);processed+=(size_t)w+(j->select.method==IE_SELECT_LASSO?(size_t)j->select.point_count*12:0);if(j->y==h)job_after_generation(j);}
  else if(j->stage==JOB_HORIZONTAL){job_horizontal(j,j->y++);processed+=(size_t)w;if(j->y==h){j->stage=JOB_VERTICAL;j->y=0;}}
  else if(j->stage==JOB_VERTICAL){job_vertical(j,j->y++);processed+=(size_t)w;if(j->y==h){d->release(d->memory_context,j->temporary);j->temporary=0;d->release(d->memory_context,j->sums);j->sums=0;j->stage=JOB_COMBINE;j->y=0;}}
  else if(j->stage==JOB_COMBINE){
   job_combine(j,j->y++);processed+=(size_t)w;if(j->y==h){
    if(j->fill_job){
     if(j->maxx<0||!j->fill.opacity||!j->fill.density){j->complete=1;job_buffers(j);break;}
     /* A flat relief needs only the normal fill buffers, even on a large region. */
     const int shaded_relief=j->fill.kind==IE_FILL_RELIEF&&j->fill.relief_height&&j->fill.relief_intensity;
     if(shaded_relief&&!job_relief_allocate(j)){j->complete=-1;job_buffers(j);break;}
     if(!ie_gesture_begin(d)){j->complete=-1;job_buffers(j);break;}j->transaction=1;j->stage=shaded_relief?JOB_RELIEF_FORWARD:JOB_PAINT;j->y=0;
    }else{
     floating_finish(d);
     if(d->current.selection_mask)d->release(d->memory_context,d->current.selection_mask);
     d->current.selection_mask=j->mask;j->mask=0;
     d->current.selection=(IERect){j->maxx<0?0:j->minx,j->maxy<0?0:j->miny,j->maxx<0?0:j->maxx-j->minx+1,j->maxy<0?0:j->maxy-j->miny+1,1};j->complete=1;d->error=0;job_buffers(j);
    }
   }
  }else if(j->stage==JOB_RELIEF_FORWARD){job_relief_forward(j,j->y++);processed+=(size_t)j->relief_w;if(j->y==j->relief_h){j->stage=JOB_RELIEF_BACKWARD;j->y=0;}}
  else if(j->stage==JOB_RELIEF_BACKWARD){job_relief_backward(j,j->y++);processed+=(size_t)j->relief_h*3;if(j->y==j->relief_w){d->release(d->memory_context,j->envelope);j->envelope=0;job_relief_prepare_height(j);j->stage=JOB_RELIEF_HEIGHT;j->y=0;}}
  else if(j->stage==JOB_RELIEF_HEIGHT){job_relief_height_row(j,j->y++);processed+=(size_t)j->relief_w*2;if(j->y==j->relief_h){j->stage=JOB_PAINT;j->y=0;}}
  else if(j->stage==JOB_PAINT){
   int x,y=j->y++;for(x=0;x<w;x++){size_t at=(size_t)y*w+x;unsigned a=(j->mask[at]*(unsigned)j->fill.opacity+50)/100;uint32_t c;if(!a)continue;c=job_fill_color(j,x,y,&a);if(a){uint32_t*pixel=&d->current.layers[d->current.active].pixels[at];*pixel=over(*pixel,c,a);}}processed+=(size_t)w*(j->fill.kind==IE_FILL_RELIEF?3u:1u);
   if(j->y==h){ie_commit(d);j->transaction=0;j->complete=1;d->error=0;job_buffers(j);}
  }else {j->complete=-1;job_buffers(j);}
 }
 return j->complete;
}
/* Outline only: never changes image pixels or allocates a full-screen mask.
   Sampling follows the same signed fixed-point viewport as the image renderer. */
void ie_render_selection_outline(const IEDoc*d,uint16_t*out,size_t stride,int ow,int oh,int32_t ox,int32_t oy,int zoom,int x,int y,int width,int height){
 int yy,xx,l=maxi(0,x),t=maxi(0,y),r,b,denom,step,remstep,first,firstrem;int64_t n,ex=(int64_t)x+width,ey=(int64_t)y+height;r=ex>ow?ow:ex<0?0:(int)ex;b=ey>oh?oh:ey<0?0:(int)ey;
 if(!d||!out||!d->current.selection.enabled||!d->current.selection_mask||zoom<1||zoom>6400||ow<1||ow>2048||oh<1||oh>2048||stride<(size_t)ow||stride>SIZE_MAX/sizeof(*out)/(size_t)oh||width<=0||height<=0||l>=r||t>=b)return;
 denom=zoom*65536;step=6553600/denom;remstep=6553600%denom;n=(int64_t)ox*zoom+(int64_t)l*6553600;first=(int)floor_div(n,denom);firstrem=(int)(n-(int64_t)first*denom);
 for(yy=t;yy<b;yy++){int sy=ie_view_sample(yy,oy,zoom),up=ie_view_sample(yy-1,oy,zoom),down=ie_view_sample(yy+1,oy,zoom),sx=first,rem=firstrem;for(xx=l;xx<r;xx++){
  int previous=sx-step-(rem<remstep),next=sx+step+(rem+remstep>=denom);
  if(ie_selection_alpha(&d->current,sx,sy)>0&&(!ie_selection_alpha(&d->current,previous,sy)||!ie_selection_alpha(&d->current,next,sy)||!ie_selection_alpha(&d->current,sx,up)||!ie_selection_alpha(&d->current,sx,down)))out[(size_t)yy*stride+xx]=((xx+yy)/5&1)?0xffff:0;
  sx+=step;rem+=remstep;if(rem>=denom){rem-=denom;++sx;}
 }if((yy&31)==31)cooperate(d);}
}
