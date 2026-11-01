#include "Platform.h"
#include <lvgl.h>
#include "Tab5TextInput.cpp"
#include "Tab5UiStyle.cpp"
#define private public
#include "Tab5MediaList.h"
#undef private
#include "Tab5MediaList.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks=0,activations=0,formatted=0,metadataReads=0,domain=0,last=0;
static bool lastFavorite=false, favoriteOverride=false;
static uint64_t flushedPixels=0,flushes=0,frames=0;
static uint16_t drawBuffer[1280*40],screenPixels[1280*720];
static void check(bool value,const char* what){if(!value){fprintf(stderr,"FAIL %s\n",what);exit(1);}++checks;}
static uint64_t digest(){uint64_t h=1469598103934665603ULL;for(auto x:screenPixels){h^=x;h*=1099511628211ULL;}return h;}
static void flush(lv_display_t* d,const lv_area_t* a,uint8_t* pixels){
 const int w=lv_area_get_width(a),h=lv_area_get_height(a);auto* src=reinterpret_cast<uint16_t*>(pixels);
 for(int y=0;y<h;++y)std::memcpy(screenPixels+(a->y1+y)*1280+a->x1,src+y*w,size_t(w)*2);
 flushedPixels+=uint64_t(w)*h;++flushes;if(lv_display_flush_is_last(d))++frames;lv_display_flush_ready(d);
}
static void settle(){for(unsigned i=0;i<12;++i){lv_tick_inc(34);lv_timer_handler();}}
static void format(Tab5MediaList::Entry e,String& label,const char*& icon,bool& favorite,bool& allowed,bool& trash){
 char text[96];snprintf(text,sizeof(text),"%s %04u",domain?"Music":"Station",e.index);label=text;
 icon=domain?LV_SYMBOL_AUDIO:LV_SYMBOL_WIFI;favorite=(e.index%2)!=favoriteOverride;allowed=true;trash=false;++formatted;
}
static void action(Tab5MediaList::Entry e,bool favorite){++activations;last=e.index;lastFavorite=favorite;}
static void metadata(Tab5MediaList::Entry e,String& name,uint64_t& bytes,int64_t& modified){
 char text[96];snprintf(text,sizeof(text),"Station %04u",e.index);name=text;bytes=800-e.index;modified=e.index;++metadataReads;
}
static void populate(Tab5MediaList& v,unsigned n=800){for(unsigned i=0;i<n;++i)check(v.add(Tab5MediaList::Media,i),"populate model");}
int main(){
 lv_init();auto* display=lv_display_create(1280,720);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
 lv_display_set_buffers(display,drawBuffer,nullptr,sizeof(drawBuffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);
 auto* screen=lv_screen_active();Tab5Focus.begin(screen,LV_FONT_DEFAULT);
 uint32_t palette[]={0x101010,0x202020,0x303030,0x777777,0xffffff,0x999999,0x55aaff,0xaaaaaa,0x345678};Tab5UiStyle::configure(palette,12);
 auto* list=lv_obj_create(screen);lv_obj_set_pos(list,220,80);lv_obj_set_size(list,540,500);lv_obj_update_layout(screen);
 uint64_t unchangedPixels=0,stagedPixels=0,changedPixels=0;size_t sortAllocations=0,steadyBytes=0;
 {
  Tab5MediaList view(list,LV_FONT_DEFAULT,format,action);populate(view);view.show(0,5);settle();
  check(view.count()==800&&lv_obj_get_child_count(list)==25,"800 records use 24 rows");check(view.selected_==5,"selection restored without playback");
  const auto initialDigest=digest();
  // Warm both reusable model buffers before measuring unchanged commits.
  view.reset();populate(view);view.appearance(LV_FONT_DEFAULT);view.show(0,5);settle();
  const auto idleStart=flushedPixels;
  for(unsigned cycle=0;cycle<100;++cycle){view.reset();populate(view);view.appearance(LV_FONT_DEFAULT);view.show(0,5);settle();check(digest()==initialDigest,"unchanged model has identical pixels");}
  unchangedPixels=flushedPixels-idleStart;
#if CURRENT
  check(unchangedPixels==0,"unchanged commits publish no image");
#endif
  const auto priorDigest=digest(),priorPixels=flushedPixels;
  view.reset();populate(view,7);settle();stagedPixels=flushedPixels-priorPixels;
#if CURRENT
  check(digest()==priorDigest&&stagedPixels==0,"incomplete model never blanks displayed rows");
  check(view.count()==800,"old model remains active until commit");view.cancel();
  check(view.count()==800&&digest()==priorDigest,"cancel retains complete displayed catalogue");
#else
  view.reset();populate(view);view.show(0,5);settle();
#endif
  // A changed model still redraws and actions use its new index mapping.
  const auto changedStart=flushedPixels;
  for(unsigned cycle=0;cycle<40;++cycle){domain=cycle%2;view.reset();populate(view,cycle%2?200:800);view.show(0,3);settle();check(view.count()==(cycle%2?200:800),"changed catalogue commits all references");}
  changedPixels=flushedPixels-changedStart;check(changedPixels>0,"changed contents are actually rendered");
  domain=0;view.reset();populate(view);view.show(0,5);settle();
  view.focus(5);settle();auto* focused=Tab5Focus.focused();
  view.reset();populate(view);view.show(0,5);settle();
#if CURRENT
  check(Tab5Focus.focused()==focused,"unchanged row retains keyboard focus");
#endif
  view.focus(0);auto* pressed=view.slots_[0].row;lv_obj_send_event(pressed,LV_EVENT_PRESSED,nullptr);const auto before=activations;
  view.reset();view.add(Tab5MediaList::Media,700);view.show(0);lv_obj_send_event(pressed,LV_EVENT_RELEASED,nullptr);lv_obj_send_event(pressed,LV_EVENT_CLICKED,nullptr);
  check(activations==before,"model commit rejects stale press");
  view.reset();populate(view);view.show(0);settle();
  const auto allocationStart=HeapCheck::calls;const auto readStart=metadataReads;
  check(view.sort(Tab5FileSort::Order::SizeAscending,metadata),"size sort succeeds");
  sortAllocations=HeapCheck::calls-allocationStart;
  check(metadataReads-readStart==800,"metadata read once per record outside comparator");
  check(view.entries_[0].index==799&&view.entries_[799].index==0,"size sorting matches file metadata");
  view.show(0);settle();
  const auto reads=metadataReads;lv_obj_scroll_to_y(list,30000,LV_ANIM_OFF);settle();check(metadataReads==reads,"scroll reads no file metadata");
  view.focus(0);writing::KeyEvent end;end.key=writing::Key::End;check(view.key(end),"End crosses pool");
  writing::KeyEvent enter;enter.key=writing::Key::Enter;check(view.key(enter)&&last==0&&!lastFavorite,"keyboard action resolves sorted entry");
  writing::KeyEvent right;right.key=writing::Key::Right;check(view.key(right)&&view.key(enter)&&lastFavorite,"favorite keyboard action preserved");
  // Every actual sort allocation failure must preserve the complete model.
  std::vector<Tab5MediaList::Entry> saved(view.entries_,view.entries_+view.count_);
  for(size_t fail=0;fail<sortAllocations;++fail){
   const auto resident=HeapCheck::live;HeapCheck::failAfter=int(fail);
   check(!view.sort(Tab5FileSort::Order::NameAscending,metadata),"each sort OOM is reported");HeapCheck::failAfter=-1;
   check(HeapCheck::live==resident,"sort OOM releases all scratch memory");
   for(size_t i=0;i<saved.size();++i)check(saved[i].kind==view.entries_[i].kind&&saved[i].index==view.entries_[i].index,"sort OOM preserves references");
  }
#if CURRENT
  view.reset();HeapCheck::failAfter=0;while(view.pendingCount_<view.pendingCapacity_)check(view.add(Tab5MediaList::Media,1),"fill reusable model without allocation");
  check(!view.add(Tab5MediaList::Media,2),"model growth OOM reported");HeapCheck::failAfter=-1;view.cancel();
  check(view.count()==saved.size(),"cancel after OOM preserves active model length");
  for(size_t i=0;i<saved.size();++i)check(saved[i].kind==view.entries_[i].kind&&saved[i].index==view.entries_[i].index,"cancel after OOM preserves active identity");
#endif
  // Appearance still responds to live palette, font and radius changes.
  Tab5Focus.clear();view.show(0);palette[1]=0xeeeeee;palette[4]=0x111111;Tab5UiStyle::configure(palette,0);view.appearance(LV_FONT_DEFAULT);settle();
  check(lv_color_eq(lv_obj_get_style_bg_color(view.slots_[0].row,LV_PART_MAIN),lv_color_hex(palette[1])),"theme updates row surface");
  check(lv_obj_get_style_radius(view.slots_[0].row,LV_PART_MAIN)==0,"theme updates row geometry");
  const auto themePixels=flushedPixels;view.appearance(LV_FONT_DEFAULT);settle();
#if CURRENT
  check(flushedPixels==themePixels,"same appearance causes no invalidation");
#endif
  steadyBytes=HeapCheck::live;view.release();check(view.count()==0&&view.entries_==nullptr&&view.capacity_==0,"release retires active references");
#if CURRENT
  check(view.pending_==nullptr&&view.pendingCapacity_==0,"release retires staged references");
#endif
 }
 check(HeapCheck::live==0&&HeapCheck::allocations.empty(),"all explicit PSRAM allocations freed");check(lv_obj_get_child_count(list)==0,"pooled LVGL children freed");
 lv_obj_delete(list);lv_display_delete(display);
 printf("{\"checks\":%u,\"unchangedPixels\":%llu,\"stagedPixels\":%llu,\"changedPixels\":%llu,\"sortAllocations\":%zu,\"steadyModelBytes\":%zu,\"trackedLiveBytes\":%zu,\"peakTrackedBytes\":%zu,\"frames\":%llu}\n",checks,(unsigned long long)unchangedPixels,(unsigned long long)stagedPixels,(unsigned long long)changedPixels,sortAllocations,steadyBytes,HeapCheck::live,HeapCheck::peak,(unsigned long long)frames);
}
