#include <lvgl.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include "EpubPageIndex.h"
static unsigned checks=0,failures=0,reads=0,measures=0,renders=0,saves=0,notices=0,docs=0;
static void check(bool value,const char* message){++checks;if(!value){++failures;printf("FAIL %s\n",message);}}
struct String:std::string {using std::string::string;String(uint32_t n):std::string(std::to_string(n)){}String(int32_t n):std::string(std::to_string(n)){}String(const std::string& value):std::string(value){}};
static const char* T(const char* s){return s;}static uint32_t millis(){static uint32_t now=0;return now+=10;}
static bool failMeasure=false,failParse=false,failArray=false;
static unsigned failRenders=0;
static void heap_caps_free(void* pointer){free(pointer);}
template<class T,size_t N> struct Tab5PsramTable{std::vector<T> values;bool ensure(size_t count){if(failArray){failArray=false;return false;}if(count>N)return false;if(values.size()<count)values.resize(count);return true;}void clear(){values.clear();values.shrink_to_fit();}T& operator[](size_t i){return values[i];}};
namespace epub_render {struct Document {char* pool=nullptr;uint16_t blockCount=0;int viewportW=0,viewportH=0;unsigned pages=0;void clear(){if(pool){free(pool);--docs;}pool=nullptr;blockCount=0;}void allocate(unsigned count){clear();pool=static_cast<char*>(malloc(1));++docs;blockCount=1;pages=count;}};}
struct RichLoc{uint16_t block=0;uint32_t offset=0;};
static bool richLocLE(const RichLoc& a,const RichLoc& b){return a.block<b.block||(a.block==b.block&&a.offset<=b.offset);}
static constexpr unsigned READER_PAGE_CACHE_MAX=2048;
static epub_pages::Index<8> readerBookPages;
static epub_render::Document readerRichDoc,readerIndexDocument;
static char* readerText=nullptr;static size_t readerTextLen=0;
static char* readerIndexText=nullptr;static size_t readerIndexTextLen=0;
static bool readerSeekDragging=false,readerIndexLoaded=false,readerRichPaginationComplete=false,readerChapterPaginationComplete=false,readerResumePending=false;
static uint16_t readerChapter=0,readerPageIndex=0,readerRichPageStartCount=0,readerPageStartCount=0;
static uint32_t readerIndexPageCount=0,readerIndexOffset=0;static RichLoc readerIndexLoc;
static Tab5PsramTable<RichLoc,2049> readerRichPageStarts;
static Tab5PsramTable<uint32_t,2049> readerPageStarts;
static lv_obj_t* readerSeekSlider=nullptr;static lv_obj_t* readerStatusLabel=nullptr;
static float readerPageAspect=2.f/3,readerIndexAspect=2.f/3,readerDisplayedAspect=2.f/3;static struct{bool fixedLayout=false;}readerBook;
static unsigned chapterPages[]={3,5,2};
static void readerIndexReleaseChapter(){readerIndexDocument.clear();free(readerIndexText);readerIndexText=nullptr;readerIndexTextLen=0;readerIndexLoaded=false;readerIndexPageCount=0;readerIndexOffset=0;readerIndexLoc={};}
static bool readerIndexLoadChapter(uint16_t chapter){++reads;if(failParse){failParse=false;return false;}readerIndexDocument.allocate(chapterPages[chapter]);readerIndexLoaded=true;return true;}
struct ReaderIndexBorrow{epub_render::Document old=readerRichDoc;ReaderIndexBorrow(){readerRichDoc=readerIndexDocument;}~ReaderIndexBorrow(){readerRichDoc=old;}};
static RichLoc richComputeNextPageStart(RichLoc at){++measures;if(failMeasure){failMeasure=false;return at;}return at.offset+1>=readerRichDoc.pages?RichLoc{1,0}:RichLoc{0,at.offset+1};}
static size_t readerSkipPageWhitespace(size_t value){return value;}
static bool readerImageMarkerAt(size_t,void*,size_t*){return false;}static size_t readerFindTextPageEnd(size_t at){return at+1;}
static void readerApplyEpubGeometry(float aspect){readerDisplayedAspect=aspect;}
static void freeReaderText(){free(readerText);readerText=nullptr;readerTextLen=0;readerPageIndex=0;readerPageStartCount=0;}
static bool readerRenderEpubPage(){++renders;if(failRenders){--failRenders;return false;}return true;}static void markReaderProgressDirty(){++saves;}
static void toast(const char*,unsigned){++notices;}
static void readerSeekSync();static void readerUpdateEpubStatus(){readerSeekSync();}
#include "Tab5EpubSeek.inc"
static void drive(){for(unsigned i=0;i<100&&readerSeekPending;++i)readerSeekService();check(!readerSeekPending,"seek finishes in bounded service calls");}
static void choose(unsigned page){lv_slider_set_value(readerSeekSlider,page,LV_ANIM_OFF);lv_obj_send_event(readerSeekSlider,LV_EVENT_VALUE_CHANGED,nullptr);}
int main(){lv_init();lv_display_create(720,1280);auto* screen=lv_obj_create(nullptr);readerSeekSlider=lv_slider_create(screen);readerStatusLabel=lv_label_create(screen);lv_obj_add_event_cb(readerSeekSlider,readerSeekEvent,LV_EVENT_ALL,nullptr);
 readerBookPages.reset(3);readerSeekSync();check(lv_obj_has_state(readerSeekSlider,LV_STATE_DISABLED),"no invented whole-book page while indexing");
 for(unsigned c=0;c<3;++c)readerBookPages.record(c,chapterPages[c]);readerRichDoc.allocate(3);readerRichPageStarts.ensure(4);readerRichPageStarts[0]={};readerRichPageStarts[1]={0,1};readerRichPageStartCount=2;readerSeekSync();
 check(!lv_obj_has_state(readerSeekSlider,LV_STATE_DISABLED)&&lv_slider_get_max_value(readerSeekSlider)==10,"slider covers all chapters");
 uint16_t chapter;uint32_t page;check(readerBookPages.locate(4,chapter,page)&&chapter==1&&page==0,"global page maps to correct section");check(!readerBookPages.locate(0,chapter,page)&&!readerBookPages.locate(11,chapter,page),"invalid targets rejected");
 for(unsigned i=1;i<=10;++i)choose(i);check(reads==0&&renders==0&&measures==0&&saves==0,"drag preview does no IO, layout, render or bookmark save");check(strstr(lv_label_get_text(readerStatusLabel),"10 / 10")!=nullptr,"preview displays final target page");
 lv_obj_send_event(readerSeekSlider,LV_EVENT_RELEASED,nullptr);check(reads==0&&renders==0,"release queues work without immediate parse");drive();check(readerChapter==2&&readerPageIndex==1,"last whole-book page reached");check(reads==1&&measures==2&&renders==1&&saves==1,"one parse then progressive measures and one final render");check(docs==1,"staged document transferred, not copied or reparsed");
 choose(9);lv_obj_send_event(readerSeekSlider,LV_EVENT_RELEASED,nullptr);drive();check(reads==1&&readerPageIndex==0,"cached page seek avoids parse");
 {const auto before=saves;choose(10);lv_obj_send_event(readerSeekSlider,LV_EVENT_RELEASED,nullptr);failRenders=1;drive();check(readerPageIndex==0&&saves==before,"cached seek render failure restores page and does not write bookmark");}
 const unsigned oldRenders=renders,oldSaves=saves;auto* oldPool=readerRichDoc.pool;choose(5);lv_obj_send_event(readerSeekSlider,LV_EVENT_RELEASED,nullptr);readerSeekService();failMeasure=true;drive();check(readerChapter==2&&readerPageIndex==0&&readerRichDoc.pool==oldPool,"measurement OOM preserves visible chapter");check(renders==oldRenders&&saves==oldSaves&&docs==1,"failure neither renders nor overwrites bookmark; staged memory released");
 choose(6);lv_obj_send_event(readerSeekSlider,LV_EVENT_RELEASED,nullptr);failArray=true;drive();check(readerRichDoc.pool==oldPool&&docs==1,"destination allocation failure preserves current page");
 {const auto before=saves;choose(5);lv_obj_send_event(readerSeekSlider,LV_EVENT_RELEASED,nullptr);failRenders=1;drive();check(readerRichDoc.pool==oldPool&&docs==1&&readerChapter==2&&readerPageIndex==0,"target render failure restores previous document and page cache");check(saves==before,"failed target render never saves target bookmark");}
 {const auto before=saves;choose(5);lv_obj_send_event(readerSeekSlider,LV_EVENT_RELEASED,nullptr);failRenders=2;drive();check(readerRichDoc.pool==oldPool&&docs==1&&saves==before,"even failure to repaint old page retains document and bookmark");}
 choose(6);lv_obj_send_event(readerSeekSlider,LV_EVENT_RELEASED,nullptr);readerSeekService();lv_obj_send_event(readerSeekSlider,LV_EVENT_PRESSED,nullptr);check(!readerSeekPending&&docs==1,"new drag cancels pending preparation");
 choose(8);uint32_t esc=LV_KEY_ESC;lv_obj_send_event(readerSeekSlider,LV_EVENT_KEY,&esc);check(!readerSeekDragging&&!readerSeekPending&&readerChapter==2,"Escape cancels preview");
 choose(6);uint32_t enter=LV_KEY_ENTER;lv_obj_send_event(readerSeekSlider,LV_EVENT_KEY,&enter);drive();check(readerChapter==1&&readerPageIndex==2,"keyboard Enter commits selected global page");
 const auto box=epub_pages::geometry(720,1280,2.f/3);check(box.y+box.height<1280-62,"page rectangle leaves room for slider and status");
 readerSeekCancel();readerIndexReleaseChapter();readerRichDoc.clear();readerRichPageStarts.clear();readerPageStarts.clear();check(docs==0,"all staged and active document storage freed");lv_obj_delete(screen);
 printf("checks=%u failures=%u reads=%u measures=%u renders=%u bookmarkWrites=%u\n",checks,failures,reads,measures,renders,saves);return failures?1:0;}
