/* Reuse the shipped native API/filesystem fixture, but exercise real app.c path UI transitions. */
#define main shipped_native_ui_main
#include "test_ui.c"
#undef main
int main(void){
 struct tab5_app_api* b=&fake.base.base.base.base.base;b->version=6;b->size=sizeof(fake);b->language="en";b->allocate=allocate_host;b->release=release_host;b->read=read_host;b->open_read=open_host;b->close=close_host;b->should_stop=stop_host;b->millis=millis_host;b->sleep_ms=sleep_host;fake.base.base.base.base.video_present=present_host;fake.write_begin=begin_host;fake.write_chunk=write_host;fake.write_finish=finish_host;api=&fake;base=b;video=&fake.base.base.base.base;
 pixels=allocate_host(W*H*2);check(ie_init(&doc,memory_alloc,memory_free,NULL,16,16));
 for(fr=0;fr<=1;++fr){
  doc.dirty=0;project_path[0]=0;activate(A_OPEN,0);check(modal==1&&!strcmp(input,"/Picture/"));modal=0;
  activate(A_SAVE,0);check(modal==1&&!strcmp(input,"/Picture/Image.t5i"));modal=0;
  activate(A_BMP,0);check(modal==1&&!strcmp(input,"/Picture/Image.bmp"));modal=0;
  const char* roots[]={"/Picture","/Images"};
  for(unsigned i=0;i<2;i++){
   char path[256];snprintf(path,sizeof(path),"%s/legacy-%d.t5i",roots[i],fr);activate(A_SAVE,0);copy(input,sizeof(input),path);input_submit();check(!modal&&saved_size>0&&!strcmp(project_path,path)&&!strcmp(saved_path,path));
   check(ie_new(&doc,8,8,0xffffffff));open_file(path);check(doc.current.width==16&&!doc.dirty&&!strcmp(project_path,path));
   activate(A_SAVE,0);check(!strcmp(input,path));input_submit();check(modal==2&&confirm_action==A_SAVE);activate(A_CANCEL,0);check(!modal&&!strcmp(project_path,path));
   activate(A_SAVE,0);input_submit();check(modal==2);activate(A_CONFIRM,0);check(!modal&&!strcmp(saved_path,path));
   snprintf(path,sizeof(path),"%s/export-%d.bmp",roots[i],fr);activate(A_BMP,0);copy(input,sizeof(input),path);input_submit();check(!modal&&!strcmp(saved_path,path)&&saved_size>0);open_file(path);check(doc.current.width==16&&project_path[0]==0);
  }
  const char* bad[]={"/ImagesOther/a.t5i","/PictureOther/a.t5i","/Pictures/a.t5i","/Images/sub/a.t5i","/Picture/sub/a.t5i","/USB/Picture/a.t5i"};
  for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){char previous[256];copy(previous,sizeof(previous),saved_path);activate(A_SAVE,0);copy(input,sizeof(input),bad[i]);input_submit();check(!modal&&!strcmp(saved_path,previous));}
  activate(A_SAVE,0);copy(input,sizeof(input),fr?"relative-fr":"relative-en");input_submit();check(!modal&&!strcmp(project_path,fr?"/Picture/relative-fr.t5i":"/Picture/relative-en.t5i"));
 }
 ie_destroy(&doc);release_host(pixels);pixels=NULL;free(saved);saved=NULL;check(!pending&&live==0&&allocations==0&&memory_bytes==0);printf("{\"checks\":%u,\"remainingPsram\":%zu,\"languages\":[\"en\",\"fr\"]}\n",checks,live);return 0;
}
