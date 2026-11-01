#define main gesture_main
#include "gesture.c"
#undef main
static uint32_t rng=0x812ab74u;
static unsigned next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(void){IEDoc d;setup(&d,67,41,3);for(unsigned k=0;k<4000;k++){
 unsigned choice=next()%20;int w=d.current.width,h=d.current.height,x=next()%w,y=next()%h;d.current.active=next()%d.current.count;
 switch(choice){
 case 0:case 1:case 2:case 3:{IEBrush b={next(),1+(int)(next()%23),(int)(next()%101),(int)(next()%101),(int)(next()%101),(int)(next()%6),(int)(next()%2)};C(ie_brush_begin(&d));C(ie_brush(&d,x,y,next()%w,next()%h,&b));if(choice==3)C(ie_cancel(&d));else ie_commit(&d);break;}
 case 4:case 5:C(ie_warp_begin(&d,31));C(ie_warp_brush(&d,x,y,next()%w,next()%h,31,80,next()%3));if(choice==5)C(ie_cancel(&d));else ie_commit(&d);break;
 case 6:if(d.undo_count)C(ie_undo(&d));break;
 case 7:if(d.redo_count)C(ie_redo(&d));break;
 case 8:if(d.current.count<4)C(ie_layer_add(&d,"Added"));break;
 case 9:if(d.current.count>1)C(ie_layer_delete(&d));break;
 case 10:C(ie_layer_style(&d,next()%256,next()%2));break;
 case 11:C(ie_resize(&d,17+next()%71,11+next()%43));break;
 case 12:C(ie_begin(&d));ie_shape(&d,next()%3,x,y,next()%w,next()%h,next(),80,1+next()%7,next()%2,1);ie_commit(&d);break;
 case 13:selection(&d);break;
 case 14:ie_deselect(&d);break;
 case 15:ie_select(&d,x,y,next()%w,next()%h);break;
 case 16:C(ie_adjust(&d,(int)(next()%51)-25,(int)(next()%31)-15,15,95,105,100,0));break;
 case 17:C(ie_transform(&d,100,100,0,next()%4));break;
 case 18:C(ie_gesture_begin(&d));{IEBrush b={next(),9,55,80,0,0,0};C(ie_brush(&d,x,y,x,y,&b));}C(ie_cancel(&d));break;
 default:C(ie_clear(&d));break;
 }
 printf("%u %016llx\n",k,(unsigned long long)hash(&d));
 }done(&d);return 0;}
