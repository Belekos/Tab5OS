#include <lvgl.h>
#include <src/misc/lv_text_ap.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <type_traits>
using std::min; using std::max;
using String=std::string;
static unsigned checks=0,failed=0,measureCalls=0,groups=0,maxMeasured=0;
static size_t allocated=0,peakAllocated=0;
static bool failNextAllocation=false;
static int failAllocationAfter=-1;
static std::map<void*,size_t> allocations;
static void check(bool value,const char* what){++checks;if(!value){++failed;if(failed<14)std::fprintf(stderr,"FAIL %s\n",what);}}
static void* heap_caps_malloc(size_t n,int){if(failNextAllocation){failNextAllocation=false;return nullptr;}if(failAllocationAfter==0){failAllocationAfter=-1;return nullptr;}if(failAllocationAfter>0)--failAllocationAfter;void* p=std::malloc(n);if(p){allocations[p]=n;allocated+=n;peakAllocated=max(allocated,peakAllocated);}return p;}
static void* heap_caps_calloc(size_t n,size_t size,int flags){void* p=heap_caps_malloc(n*size,flags);if(p)std::memset(p,0,n*size);return p;}
static void heap_caps_free(void* p){if(p){allocated-=allocations[p];allocations.erase(p);std::free(p);}}
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
#include "document.generated.inc"
#include "EpubPageIndex.h"
struct RichLoc{uint16_t block=0;uint32_t offset=0;};
static epub_render::Document readerRichDoc;
static lv_obj_t *readerContent=nullptr,*readerRichHost=nullptr,*readerMeasureHost=nullptr,*readerTextLabel=nullptr,*readerImageObj=nullptr;
struct Locations:std::vector<RichLoc>{using std::vector<RichLoc>::vector;using std::vector<RichLoc>::operator=;bool ensure(size_t n){if(size()<n)resize(n);return true;}};
static Locations readerRichPageStarts;
static uint16_t readerRichPageStartCount=0,readerPageIndex=0;
static bool readerRichPaginationComplete=false;
static constexpr unsigned READER_PAGE_CACHE_MAX=2048;
static unsigned notices=0;
static void toast(const char*,unsigned){++notices;}
static std::vector<unsigned char> fontBytes;
static std::vector<unsigned char> cjkBytes,koBytes;
static std::map<unsigned,lv_font_t*> fonts;
static const lv_font_t* richFontFor(uint16_t px,uint8_t){if(!fonts.count(px)){fonts[px]=lv_tiny_ttf_create_data_ex(fontBytes.data(),fontBytes.size(),px,LV_FONT_KERNING_NORMAL,192);auto* cjk=lv_tiny_ttf_create_data_ex(cjkBytes.data(),cjkBytes.size(),px,LV_FONT_KERNING_NONE,96);auto* ko=lv_tiny_ttf_create_data_ex(koBytes.data(),koBytes.size(),px,LV_FONT_KERNING_NONE,96);check(cjk&&ko,"shipped CJK fonts loaded");fonts[px]->fallback=cjk;cjk->fallback=ko;}return fonts[px];}
static const lv_font_t* readerFont(){return richFontFor(26,0);}
static lv_color_t C_TEXT(){return lv_color_hex(0xeeeeee);}static lv_color_t C_BORDER(){return lv_color_hex(0x556677);}static lv_color_t C_SURFACE(){return lv_color_hex(0x20252a);}static lv_color_t C_RAISED(){return lv_color_hex(0x30353a);}
static struct{uint8_t fontIndex=1;} settings;
static unsigned fontSizeForIndex(uint8_t){return 24;}
static bool readerRenderImage(const String&){return true;}
static int measuredHeight(lv_obj_t* obj,int width){++measureCalls;unsigned n=0;for(unsigned i=0;i<lv_spangroup_get_span_count(obj);++i)n+=unsigned(std::strlen(lv_span_get_text(lv_spangroup_get_child(obj,i))));maxMeasured=max(maxMeasured,n);return lv_spangroup_get_expand_height(obj,width);}
static lv_obj_t* measuredGroup(lv_obj_t* obj){++groups;return lv_spangroup_create(obj);}
#define lv_spangroup_get_expand_height measuredHeight
#define lv_spangroup_create measuredGroup
#include "reader.generated.inc"
#undef lv_spangroup_get_expand_height
#undef lv_spangroup_create
static bool readerIsEpub=true;
static char* readerText=nullptr;
static unsigned readerTextLen=0,readerPageStartCount=0,readerPageStarts[4]={};
static unsigned marked=0,changedChapter=0;
static uint16_t readerChapter=0;
static struct {uint16_t chapterCount=2;} readerBook;
template<class F> static bool renderResult(F&& function){if constexpr(std::is_void_v<decltype(function())>){function();return true;}else return function();}
static bool readerRenderEpubPage(){return renderResult([](){return readerRichRenderPage();});}
static void markReaderProgressDirty(){++marked;}
static bool readerEnsureNextPageStart(uint16_t){return false;}
static bool readerLoadEpubChapter(uint16_t,bool){++changedChapter;return false;}
static void readerSeekCancel() {}
#include "navigation.generated.inc"
static void resetDocument(){lv_obj_clean(readerRichHost);readerRichDoc.clear();check(allocated==0,"all paragraph/measurement storage released");check(readerRichDoc.alloc(256*1024),"document PSRAM allocation");}
static void addBlock(const std::vector<std::pair<std::string,unsigned>>& runs,epub_render::BlockKind kind=epub_render::BlockKind::Paragraph,int mt=0,int mb=8,int indent=0){
 auto& b=readerRichDoc.blocks[readerRichDoc.blockCount++];b={};b.kind=kind;b.runStart=readerRichDoc.runCount;b.marginTop=mt;b.marginBottom=mb;b.indent=indent;b.lineSpace=6;
 for(const auto& value:runs){auto& r=readerRichDoc.runs[readerRichDoc.runCount++];r={};r.textOff=readerRichDoc.poolLen;r.textLen=value.first.size();r.style.fontPx=value.second;std::memcpy(readerRichDoc.pool+readerRichDoc.poolLen,value.first.c_str(),value.first.size()+1);readerRichDoc.poolLen+=value.first.size()+1;++b.runCount;b.textLen+=r.textLen;}
}
static std::string repeated(const char* text,unsigned n){std::string out;while(n--)out+=text;return out;}
static unsigned paginate(int width,int height,bool cost=false){
 lv_obj_set_size(readerContent,width,height);lv_obj_set_size(readerRichHost,width,height);lv_obj_update_layout(readerContent);
 readerRichPageStarts={RichLoc{0,0}};unsigned bytes=0,expected=0;for(unsigned i=0;i<readerRichDoc.blockCount;++i)expected+=readerRichDoc.blocks[i].textLen;
 unsigned oldFailure=failed;auto begin=std::chrono::steady_clock::now();unsigned maxCalls=0,maxGroups=0;
 for(unsigned guard=0;guard<2000;++guard){
   const auto start=readerRichPageStarts.back();measureCalls=groups=maxMeasured=0;const auto next=richComputeNextPageStart(start);
   maxCalls=max(maxCalls,measureCalls);maxGroups=max(maxGroups,groups);
   const bool progress=next.block>start.block||(next.block==start.block&&next.offset>start.offset);check(progress,"every page advances");if(!progress)break;
   check(next.block<=readerRichDoc.blockCount,"page end within document");
   if(next.block<readerRichDoc.blockCount)check(next.offset==readerRichDoc.blocks[next.block].textLen||(richByteAt(readerRichDoc.blocks[next.block],next.offset)&0xc0)!=0x80,"page split is UTF-8 boundary");
   for(unsigned b=start.block;b<readerRichDoc.blockCount&&b<=next.block;++b){unsigned first=b==start.block?start.offset:0;unsigned last=b==next.block?next.offset:readerRichDoc.blocks[b].textLen;if(last>first)bytes+=last-first;}
   readerRichPageStarts.push_back(next);readerRichPageStartCount=readerRichPageStarts.size();readerPageIndex=readerRichPageStarts.size()-2;
   readerRichRenderPage();lv_obj_update_layout(readerRichHost);
   lv_area_t parent;lv_obj_get_content_coords(readerRichHost,&parent);
   std::string actual,expectedText;
   for(unsigned i=0;i<lv_obj_get_child_count(readerRichHost);++i){auto* child=lv_obj_get_child(readerRichHost,i);lv_area_t box;lv_obj_get_coords(child,&box);check(box.y1>=parent.y1&&box.y2<=parent.y2,"actual rendered last line fits the page");for(unsigned span=0;span<lv_spangroup_get_span_count(child);++span)actual+=lv_span_get_text(lv_spangroup_get_child(child,span));}
   for(unsigned bi=start.block;bi<readerRichDoc.blockCount&&bi<=next.block;++bi){auto& b=readerRichDoc.blocks[bi];unsigned first=bi==start.block?start.offset:0,last=bi==next.block?next.offset:b.textLen,logical=0;for(unsigned ri=0;ri<b.runCount;++ri){auto& r=readerRichDoc.runs[b.runStart+ri];unsigned a=max(first,logical),z=min(last,logical+r.textLen);if(z>a){std::string raw(readerRichDoc.ptr(r.textOff)+a-logical,z-a);std::vector<char> shaped(raw.size()*2+1);lv_text_ap_proc(raw.c_str(),shaped.data());expectedText+=shaped.data();}logical+=r.textLen;}}
   check(actual==expectedText,"visible span text exactly equals page range after shaping");
   if(cost){check(measureCalls<=18,"bounded span reflows per long-paragraph page");check(maxMeasured<=4096,"long paragraph never laid out in full per page");check(maxGroups<=1,"one reusable measuring widget per paragraph page");}
   if(next.block>=readerRichDoc.blockCount)break;
 }
 check(bytes==expected,"all logical text bytes consumed exactly once across pages");check(readerRichPageStarts.back().block==readerRichDoc.blockCount,"pagination reaches document end");
 const unsigned count=readerRichPageStarts.size()-1;epub_pages::Index<2> index;index.reset(2);index.record(0,count);index.record(1,3);auto status=index.status(1,2,0,1);check(status.exact&&status.total==count+3&&status.page==count+3&&status.percent==100,"whole-book count and percent retained");
 for(unsigned i=0;i<count;++i){auto loc=readerRichPageStarts[i];unsigned restored=0;while(restored+1<count&&(readerRichPageStarts[restored+1].block<loc.block||(readerRichPageStarts[restored+1].block==loc.block&&readerRichPageStarts[restored+1].offset<=loc.offset)))++restored;check(restored==i,"stored block/offset restores same page");}
 if(cost)std::printf("long paragraph: %u pages, max %u reflows/page, max %u measurement groups/page, %.1f ms host\n",count,maxCalls,maxGroups,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
 return failed-oldFailure;
}
int main(int argc,char** argv){
 lv_init();check(argc==2,"shipped font argument");std::ifstream f(argv[1],std::ios::binary);fontBytes.assign(std::istreambuf_iterator<char>(f),{});check(!fontBytes.empty(),"shipped font loaded");auto folder=std::filesystem::path(argv[1]).parent_path()/"fonts";std::ifstream jf(folder/"cjk-ja.ttf",std::ios::binary),kf(folder/"cjk-ko.ttf",std::ios::binary);cjkBytes.assign(std::istreambuf_iterator<char>(jf),{});koBytes.assign(std::istreambuf_iterator<char>(kf),{});lv_display_create(1280,1280);auto* screen=lv_screen_active();lv_obj_set_style_text_font(screen,readerFont(),0);
 readerContent=lv_obj_create(screen);lv_obj_remove_style_all(readerContent);lv_obj_remove_flag(readerContent,LV_OBJ_FLAG_SCROLLABLE);
 readerRichHost=lv_obj_create(readerContent);lv_obj_remove_style_all(readerRichHost);lv_obj_remove_flag(readerRichHost,LV_OBJ_FLAG_SCROLLABLE);readerTextLabel=lv_label_create(readerContent);readerImageObj=lv_label_create(readerContent);
 readerMeasureHost=lv_obj_create(screen);lv_obj_remove_style_all(readerMeasureHost);lv_obj_set_pos(readerMeasureHost,-4000,0);lv_obj_set_size(readerMeasureHost,1000,1200);
 for(unsigned cp:{0x00e9U,0x4e2dU,0x65e5U,0xd55cU,0xfe8eU}){lv_font_glyph_dsc_t glyph{};check(lv_font_get_glyph_dsc(readerFont(),&glyph,cp,0)&&!glyph.is_placeholder,"Latin/CJK/Korean/shaped-Arabic test glyph exists");}
 for(int px:{20,24,28,32})for(int width:{380,640,684}){
  resetDocument();for(unsigned n=0;n<24;++n)addBlock({{repeated("Une phrase française : élève, été et façade. ",n%5+1),unsigned(px)}},n%3==0?epub_render::BlockKind::Quote:epub_render::BlockKind::Paragraph,n%4,8,n%2?22:0);paginate(width,710);
  resetDocument();addBlock({{repeated("長い段落日本語漢字中文한국어문장",220),unsigned(px)}});paginate(width,470);
  resetDocument();addBlock({{repeated("مرحبا بالعالم فارسی متن خواندن کتاب سلام. ",150),unsigned(px)}});paginate(width,470);
  resetDocument();addBlock({{repeated("abcdefghijklmno",400),unsigned(px)}});paginate(width,490);
  resetDocument();addBlock({{repeated("Première ligne.\nDeuxième\r\n\tTroisième ligne.\n\n",40),unsigned(px)}});paginate(width,490);
  resetDocument();addBlock({{repeated("A moderately sized paragraph. ",4),unsigned(px)}});addBlock({{"Important next paragraph at page bottom",58}});paginate(width,160);
  resetDocument();for(unsigned n=0;n<10;++n)addBlock({{"Petit ",22},{"grand titre ",58},{"et encore des mots français. ",unsigned(px)},{repeated(" Long text with line break.\n",4),unsigned(px)}},epub_render::BlockKind::Quote,2,8,26);paginate(width,600);
 }
 // Margin alone must not make end={same block,textLen}: that noncanonical
 // location previously let rendering continue past the requested final block.
 resetDocument();addBlock({{"First line",26}},epub_render::BlockKind::Paragraph,0,100);addBlock({{"Must be on another page",26}});paginate(640,80);
 resetDocument();addBlock({{repeated("LongparagraphwithmanylongwordsandUTF8éèà漢字 ",1800),26}});paginate(640,750,true);
 // Rendering must reject the whole block when a later run cannot be copied.
 resetDocument();addBlock({{"First run",26},{"Second run",26}});
 const size_t renderAllocated=allocated;const unsigned renderChildren=lv_obj_get_child_count(readerRichHost);
 failAllocationAfter=1;
 auto* incomplete=richCreateSpanBlock(readerRichHost,readerRichDoc.blocks[0],0,readerRichDoc.blocks[0].textLen,640);
 check(incomplete==nullptr,"render allocation failure rejects incomplete text block");
 if(incomplete)lv_obj_delete(incomplete);
 check(allocated==renderAllocated,"failed text block releases every temporary");
 check(lv_obj_get_child_count(readerRichHost)==renderChildren,"failed text block destroys partial LVGL group");
 failAllocationAfter=-1;
 // Real cache/navigation functions, failing the new measuring span array.
 resetDocument();addBlock({{repeated("長い段落日本語漢字中文한국어문장",220),26}});lv_obj_set_size(readerContent,640,470);lv_obj_set_size(readerRichHost,640,470);lv_obj_update_layout(readerContent);
 readerRichPageStarts={RichLoc{0,0}};readerRichPageStartCount=1;readerPageIndex=0;readerRichPaginationComplete=false;
 failNextAllocation=true;check(!readerRichEnsureNextPage(0),"allocation failure rejects unmeasured first-page boundary");check(readerRichPageStartCount==1&&!readerRichPaginationComplete,"allocation failure never caches EOF or advances page count");failNextAllocation=false;
 check(readerRichEnsureNextPage(0),"pagination recovers when allocation succeeds");readerRichRenderPage();unsigned children=lv_obj_get_child_count(readerRichHost);
 marked=changedChapter=0;failNextAllocation=true;check(!readerGoNextPage(),"target page allocation failure refuses navigation");check(readerPageIndex==0&&marked==0&&changedChapter==0,"allocation failure preserves visible page and bookmark/chapter");check(lv_obj_get_child_count(readerRichHost)==children,"failed target-page pagination preserves rendered objects");failNextAllocation=false;
 readerRichPageStartCount=1;readerRichPageStarts.resize(1);failNextAllocation=true;readerRichRenderPage();check(lv_obj_get_child_count(readerRichHost)==children,"render without measurable page end preserves previous page");check(readerRichPageStartCount==1,"render failure does not publish a fabricated endpoint");failNextAllocation=false;
 resetDocument();lv_obj_clean(readerRichHost);readerRichDoc.clear();check(allocated==0,"all custom PSRAM released at reader close");
 std::printf("%s %u checks, %u failures; peak tracked PSRAM %zu bytes (document plus temporary storage)\n",failed?"FAIL":"PASS",checks,failed,peakAllocated);return failed?1:0;
}
