#include "Tab5EpubPageIndex.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <vector>
#include <string>
#include <type_traits>

static bool failExternal=false,failInternal=false;
static size_t allocations=0,frees=0,checks=0;
static std::vector<uint32_t> attempts;
static std::map<void*,size_t> live;
void* heap_caps_malloc(size_t bytes,uint32_t caps) {
 attempts.push_back(caps);
 assert(bytes==sizeof(epub_pages::Index<256>));
 if((caps&MALLOC_CAP_SPIRAM)?failExternal:failInternal)return nullptr;
 void* ptr=std::malloc(bytes);assert(ptr);live[ptr]=bytes;++allocations;return ptr;
}
void heap_caps_free(void* ptr) {
 if(!ptr)return;
 assert(live.erase(ptr)==1);++frees;std::free(ptr);
}
static void check(bool value){++checks;assert(value);}
static void same(epub_pages::Status a,epub_pages::Status b) {
 check(a.page==b.page&&a.total==b.total&&a.percent==b.percent&&a.exact==b.exact&&a.pageKnown==b.pageKnown);
}

// Only renderer, file-cache and LVGL boundaries are stubs below. The three
// lifecycle functions are extracted verbatim from the delivered source.
struct String:std::string {
 using std::string::string;
 String(const std::string& text):std::string(text){}
 String(unsigned value):std::string(std::to_string(value)){}
};
static const char* T(const char* text){return text;}
static const char* readerCurrentSectionName(){return "Chapter";}
static Tab5EpubPageIndex<256> readerBookPages;
static void* readerIndexContent=nullptr;
static unsigned canceled=0,released=0,deleted=0,cacheReads=0;
static bool readerIndexFailed=false,readerIndexSaved=false,readerIndexCacheTried=false;
static uint32_t readerIndexLastService=0,readerIndexLayoutKey=0;
static struct {uint16_t chapterCount=3;uint32_t chapterBytes[256]={100,900,1000};} readerBook;
static unsigned millis(){return 1234;}
static void readerSeekCancel(){++canceled;}
static void readerIndexReleaseChapter(){++released;}
static void lv_obj_delete(void*){++deleted;}
static uint32_t readerBookLayoutKey(){return 0x12345678;}
static void readerBookIndexReadCache(){++cacheReads;}
static epub_pages::Status readerBookIndexStatus(){return readerBookPages.status(1,0,10,100);}
#include "lifecycle.inc"

int main() {
 static_assert(!std::is_copy_constructible<Tab5EpubPageIndex<256>>::value,"single owner");
 check(live.empty()&&allocations==0);
 {
  Tab5EpubPageIndex<256> book;epub_pages::Index<256> original;
  check(!book.available()&&!book.complete()&&book.count()==0&&book.pages(0)==0&&book.nextUnknown()==0);
  uint16_t chapter=77;uint32_t local=88;
  check(!book.locate(1,chapter,local)&&chapter==77&&local==88);
  check(!book.record(0,1));same(book.status(0,0,0,0),{});
  for(unsigned cycle=0;cycle<40;++cycle){
   uint32_t weights[256];for(unsigned i=0;i<256;++i)weights[i]=cycle==39?UINT32_MAX:((i*37+cycle*101)%9001);
   unsigned count=cycle==39?256:1+cycle*6;
   check(book.reset(count,weights));original.reset(count,weights);
   for(unsigned c=0;c<count;++c){
    same(book.status(c,0,0,100),original.status(c,0,0,100));
    same(book.status(c,2,73,100),original.status(c,2,73,100));
    check(book.record(c,1+(c+cycle)%19)==original.record(c,1+(c+cycle)%19));
    same(book.status(c,0,0,0),original.status(c,0,0,0));
    check(book.pages(c)==original.pages(c)&&book.nextUnknown()==original.nextUnknown()&&book.complete()==original.complete());
   }
   for(unsigned page=0;page<5000;page+=7){uint16_t ac=1,bc=1;uint32_t ap=2,bp=2;check(book.locate(page,ac,ap)==original.locate(page,bc,bp));check(ac==bc&&ap==bp);}
   check(!book.record(count,1)&&!book.record(0,0)&&!book.record(0,1000001));
  }
  check(allocations==1);check(attempts[0]==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  failExternal=failInternal=true;size_t tried=attempts.size();check(book.reset(3));check(attempts.size()==tried);
  book.clear();book.clear();check(live.empty());
  check(!book.reset(3));check(!book.available());same(book.status(0,0,0,0),{});
  failInternal=false;check(book.reset(3));check(attempts.back()==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
  check(!book.reset(257)&&!book.available());check(book.reset(0));
 }
 failExternal=failInternal=false;
 for(unsigned n=0;n<1000;++n){Tab5EpubPageIndex<256> book;check(book.reset(256));check(book.record(255,4));check(book.pages(255)==4);}
 check(live.empty());
 // Opening, relayout, closing, no repeated retry after exhaustion and explicit recovery.
 readerBookIndexStart();check(readerBookPages.available()&&!readerIndexFailed&&cacheReads==1);
 readerIndexContent=reinterpret_cast<void*>(1);size_t tried=attempts.size();failExternal=failInternal=true;
 readerBookIndexStart();check(readerBookPages.available()&&!readerIndexFailed&&attempts.size()==tried&&deleted==1);
 check(readerIndexLayoutKey==0x12345678&&readerIndexLastService==1234);
 readerBookIndexStop();check(!readerBookPages.available()&&live.empty());
 unsigned reads=cacheReads;readerBookIndexStart();check(readerIndexFailed&&!readerBookPages.available()&&cacheReads==reads);
 const auto previousBook=readerBook;tried=attempts.size();
 for(int n=0;n<100;++n)check(readerBookIndexStatusText()=="Mémoire insuffisante pour la pagination  ·  Chapter");
 check(attempts.size()==tried&&readerBook.chapterCount==previousBook.chapterCount&&readerBook.chapterBytes[1]==previousBook.chapterBytes[1]);
 failExternal=failInternal=false;readerBookIndexStart();check(readerBookPages.available()&&!readerIndexFailed&&cacheReads==reads+1);
 check(readerBookIndexStatusText().find("insuffisante")==std::string::npos);
 readerBookIndexStop();check(live.empty()&&allocations==frees);
 std::printf("{\"passed\":true,\"checks\":%zu,\"allocations\":%zu,\"frees\":%zu,\"remainingBytes\":0}\n",checks,allocations,frees);
}
