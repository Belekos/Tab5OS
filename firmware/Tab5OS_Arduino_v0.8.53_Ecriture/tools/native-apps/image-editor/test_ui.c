/* Actual production UI compiled on host; SDK is replaced by bounded file/input doubles. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "app.c"
static struct tab5_app_api6 fake;
static unsigned checks,present_count,allocations;
static size_t live,peak;
struct Allocation {size_t size;};
static void* allocate_host(size_t n){struct Allocation* a=malloc(sizeof(*a)+n);if(!a)return NULL;a->size=n;live+=n;if(live>peak)peak=live;allocations++;return a+1;}
static void release_host(void* p){if(p){struct Allocation* a=(struct Allocation*)p-1;live-=a->size;allocations--;free(a);}}
static void check_line(int ok,int line){++checks;if(!ok){fprintf(stderr,"UI assertion %u failed at line %d\n",checks,line);abort();}}
#define check(ok) check_line((ok),__LINE__)
static int present_host(const uint16_t* data,size_t n){check(data&&n==800*480);present_count++;return 0;}
static uint32_t millis_host(void){static uint32_t t;return t++;}
static int stop_host(void){return 0;}static int sleep_host(uint32_t ms){(void)ms;return 0;}
static unsigned char *saved,*pending;static size_t saved_size,position,pending_size;static char saved_path[256];
static int open_host(const char* path){if(!saved||strcmp(path,saved_path))return -1;position=0;return 1;}
static int read_host(int fd,void* out,size_t n){check(fd==1);if(n>saved_size-position)n=saved_size-position;memcpy(out,saved+position,n);position+=n;return(int)n;}
static int close_host(int fd){check(fd==1);return 0;}
static int begin_host(const char* path){check(!pending);copy(saved_path,sizeof(saved_path),path);pending=malloc(16*1024*1024);pending_size=0;return pending?2:-1;}
static int write_host(int fd,const void* data,size_t n){check(fd==2&&n<=4096);if(pending_size+n>16*1024*1024)return -1;memcpy(pending+pending_size,data,n);pending_size+=n;return(int)n;}
static int finish_host(int fd,int commit){check(fd==2);if(commit){free(saved);saved=pending;saved_size=pending_size;}else free(pending);pending=NULL;return 0;}
const struct tab5_app_api6* tab5_app_api_v6(void){return &fake;}
static void press(int x,int y){pointer_down(x,y);pointer_up(x,y);render();}
static void key(unsigned code,const char* value,unsigned mods){struct tab5_app_input_event e={0};e.key=code;e.modifiers=mods;if(value)copy(e.text,sizeof(e.text),value);key_event(&e);render();}
static void shot(const char* directory,const char* name){char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);FILE* f=fopen(path,"wb");assert(f);fprintf(f,"P6\n800 480\n255\n");for(unsigned i=0;i<W*H;i++){uint16_t c=pixels[i];unsigned char p[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(p,1,3,f);}fclose(f);}
int main(int argc,char** argv){check(argc==2);struct tab5_app_api* b=&fake.base.base.base.base.base;b->version=6;b->size=sizeof(fake);b->language="fr";b->allocate=allocate_host;b->release=release_host;b->read=read_host;b->open_read=open_host;b->close=close_host;b->should_stop=stop_host;b->millis=millis_host;b->sleep_ms=sleep_host;fake.base.base.base.base.video_present=present_host;fake.write_begin=begin_host;fake.write_chunk=write_host;fake.write_finish=finish_host;api=&fake;base=b;video=&fake.base.base.base.base;fr=1;
 pixels=allocate_host(W*H*2);check(ie_init(&doc,memory_alloc,memory_free,NULL,640,360));check(memory_bytes<=2*1024*1024);render();shot(argv[1],"01-tools");
 /* A physical/touch stroke is exactly one undo step; Escape returns clean pixels. */
 uint32_t initial=ie_composite(&doc,100,100);pointer_down(dx+90,dy+90);pointer_move(dx+150,dy+120);check(doc.undo_count==1&&doc.dirty);key(TAB5_KEY_ESCAPE,NULL,0);check(doc.undo_count==0&&doc.dirty==0&&ie_composite(&doc,100,100)==initial);
 pointer_down(dx+90,dy+90);pointer_move(dx+150,dy+120);pointer_up(dx+150,dy+120);check(doc.undo_count==1&&doc.dirty);key(TAB5_KEY_TEXT,"z",TAB5_MOD_CTRL);check(doc.undo_count==0&&doc.redo_count==1);key(TAB5_KEY_TEXT,"y",TAB5_MOD_CTRL);check(doc.undo_count==1&&doc.redo_count==0);
 /* A shortcut mid-stroke cancels that transaction before undoing the previous one. */
 pointer_down(dx+200,dy+100);pointer_move(dx+220,dy+120);check(doc.undo_count==2);key(TAB5_KEY_TEXT,"z",TAB5_MOD_CTRL);check(!gesture&&!dragging&&doc.undo_count==0);pointer_up(dx+220,dy+120);check(doc.undo_count==0);
 /* Lost UP/new DOWN cannot carry old gesture ownership onto a new toolbar action. */
 pointer_down(dx+100,dy+100);check(gesture);pointer_down(8+4*99+20,20);check(!gesture&&!dragging);pointer_up(420,20);render();
 /* Actual UI layer actions, not direct engine-only tests. */
 press(8+3*99+20,460);check(tab==3);press(625,288);check(doc.current.count==2&&doc.current.active==1);press(690,322);check(doc.current.active==0);press(625,288);check(doc.current.count==3);press(675,288);check(doc.current.count==4);press(755,288);check(doc.current.count==3);render();shot(argv[1],"02-layers");
 /* Slider drag has a single transaction and restores both pixels/state on cancellation. */
 int opacity=doc.current.layers[doc.current.active].opacity;unsigned before=doc.undo_count;pointer_down(650,368);pointer_move(710,368);check(doc.undo_count>0&&gesture);key(TAB5_KEY_ESCAPE,NULL,0);check(doc.current.layers[doc.current.active].opacity==opacity&&doc.undo_count<=before);
 /* Selection and crop through UI. */
 press(20,460);press(625,118);check(tool==TOOL_SELECT);pointer_down(dx+50,dy+40);pointer_move(dx+200,dy+140);pointer_up(dx+200,dy+140);check(doc.current.selection.enabled);int cw=doc.current.selection.w,ch=doc.current.selection.h;press(8+6*99+20,460);press(690,266);check(doc.current.width==cw&&doc.current.height==ch);key(TAB5_KEY_TEXT,"z",TAB5_MOD_CTRL);check(doc.current.width==640&&doc.current.height==360);
 /* UTF8 input has bounded append/backspace, Enter commits, Escape cancels. */
 press(8+7*99+20,460);press(690,82);check(modal==1&&modal_action==A_TEXT);input[0]=0;key(TAB5_KEY_TEXT,"É",0);key(TAB5_KEY_TEXT,"té",0);check(!strcmp(input,"Été"));key(TAB5_KEY_BACKSPACE,NULL,0);check(!strcmp(input,"Ét"));key(TAB5_KEY_TEXT,"é",0);key(TAB5_KEY_ENTER,NULL,0);check(!modal&&!strcmp(text_value,"Été")&&tool==TOOL_TEXT);press(dx+30,dy+40);check(doc.dirty);
 /* Save native project, overwrite requires explicit confirmation, load round-trip. */
 key(TAB5_KEY_TEXT,"s",TAB5_MOD_CTRL);check(modal==1);copy(input,sizeof(input),"ui-test");key(TAB5_KEY_ENTER,NULL,0);check(!modal&&!doc.dirty&&saved_size>0&&!strcmp(project_path,"/Picture/ui-test.t5i"));key(TAB5_KEY_TEXT,"s",TAB5_MOD_CTRL);key(TAB5_KEY_ENTER,NULL,0);check(modal==2);key(TAB5_KEY_ESCAPE,NULL,0);check(!modal);
 int oldw=doc.current.width;activate(A_OPEN,0);copy(input,sizeof(input),"/Picture/ui-test.t5i");key(TAB5_KEY_ENTER,NULL,0);check(doc.current.width==oldw&&!doc.dirty);
 /* Reject an overlong relative save instead of silently changing its name. */
 activate(A_SAVE,0);memset(input,'x',250);input[250]=0;key(TAB5_KEY_ENTER,NULL,0);check(!modal&&strstr(status,"long"));check(!strcmp(project_path,"/Picture/ui-test.t5i"));
 /* New/quit cancellation preserves the entire document. */
 check(ie_begin(&doc));ie_brush(&doc,10,10,30,30,&brush);activate(A_NEW,0);check(modal==2);key(TAB5_KEY_ESCAPE,NULL,0);check(doc.dirty&&doc.current.width==oldw);activate(A_QUIT,0);check(modal==2&&!exit_requested);key(TAB5_KEY_ESCAPE,NULL,0);check(!exit_requested);
 /* State render examples from real drawing functions. */
 check(ie_new(&doc,640,360,IE_RGBA(235,240,245,255)));check(ie_begin(&doc));ie_shape(&doc,IE_SHAPE_ELLIPSE,80,60,300,270,IE_RGBA(25,126,164,255),100,5,1,0);ie_shape(&doc,IE_SHAPE_RECT,240,150,510,300,IE_RGBA(243,151,47,255),85,8,1,1);ie_text(&doc,35,20,"Atelier Image",4,IE_RGBA(25,45,75,255),100);check(ie_layer_add(&doc,"Details"));check(ie_begin(&doc));ie_text(&doc,300,80,"Creer",4,IE_RGBA(180,45,95,255),100);tab=1;render();shot(argv[1],"03-brush");tab=4;render();shot(argv[1],"04-adjust");modal=3;render();shot(argv[1],"05-help");modal=0;
 /* History/doc replacement releases complete PSRAM chunks while app remains open. */
 check(ie_new(&doc,1024,1000,0xffffffff));check(ie_layer_add(&doc,"Large"));check(ie_begin(&doc));check(memory_bytes<=ARENA_BYTES);check(!ie_layer_add(&doc,"Over limit"));check(doc.current.count==2);
 size_t large=memory_bytes;check(ie_new(&doc,16,16,0xffffffff));check(memory_bytes<large&&memory_bytes<=524288);ie_destroy(&doc);check(chunks==NULL&&memory_bytes==0&&chunk_count==0);release_host(pixels);pixels=NULL;check(live==0&&allocations==0);free(saved);
 printf("{\"checks\":%u,\"frames\":%u,\"peakPsram\":%zu,\"remainingPsram\":%zu}\n",checks,present_count,peak,live);return 0;
}
