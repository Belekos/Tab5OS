#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <string>
extern "C" {
typedef pcre2_code* git_regexp;
struct git_regmatch {ptrdiff_t start,end;};
int git_regexp_compile(git_regexp*,const char*,int);
void git_regexp_dispose(git_regexp*);
int git_regexp_match(const git_regexp*,const char*);
int git_regexp_search(const git_regexp*,const char*,size_t,git_regmatch*);
}
static size_t current=0,peak=0,allocations=0,attempt=0,failAt=SIZE_MAX;
static unsigned checks=0,callouts=0,cancelAt=0;
static void check(bool c,const char* why){++checks;if(!c){std::fprintf(stderr,"FAIL %s (current=%zu)\n",why,current);std::exit(1);}}
struct alignas(std::max_align_t) Header{size_t size;};
extern "C" void* quota_malloc(size_t n){if(attempt++==failAt||n>8*1024*1024-current-sizeof(Header))return nullptr;auto* p=static_cast<Header*>(std::malloc(n+sizeof(Header)));if(!p)return nullptr;p->size=n+sizeof(Header);current+=p->size;if(current>peak)peak=current;++allocations;return p+1;}
extern "C" void quota_free(void* p){if(!p)return;auto* h=static_cast<Header*>(p)-1;check(current>=h->size,"tracked PCRE2 allocation released through same allocator");current-=h->size;std::free(h);}
extern "C" int tab5_git_cancelled(){++callouts;return cancelAt&&callouts>=cancelAt;}
extern "C" void regexp_error(const char*){}
int main(){
 for(unsigned cycle=0;cycle<500;++cycle){
  git_regexp re=nullptr;failAt=SIZE_MAX;attempt=0;callouts=cancelAt=0;
  check(git_regexp_compile(&re,"^([a-z]+)-([0-9]+)$",0)==0,"normal pattern compiles");
  check(current>0,"compiled code is inside quota");const size_t held=current;
  check(git_regexp_match(&re,"hello-123")==0,"matching subject");check(current==held,"match contexts/frames freed");
  check(git_regexp_match(&re,"invalid")<0,"nonmatching subject");
  git_regmatch spans[4];check(git_regexp_search(&re,"hello-123",4,spans)==0,"capture search");check(spans[1].start==0&&spans[1].end==5&&spans[2].start==6&&spans[2].end==9&&spans[3].start==-1,"capture offsets preserved");
  cancelAt=1;callouts=0;check(git_regexp_match(&re,"hello-123")<0&&callouts>0,"profile callout cancels matching");cancelAt=0;
  git_regexp_dispose(&re);check(re==nullptr&&current==0,"whole compile/match lifecycle releases quota");
 }
 const std::string named="(?<a>a)(?<b>b)(?<c>c)(?<d>d)(?<e>e)(?<f>f)(?<g>g)(?<h>h)";
 const std::string longPattern="^(?:"+std::string(700,'a')+")$";
 for(const auto& pattern:{named,longPattern}){
  git_regexp re=nullptr;failAt=SIZE_MAX;attempt=0;check(git_regexp_compile(&re,pattern.c_str(),0)==0,"small stack workspaces grow through checked heap");const size_t calls=attempt;git_regexp_dispose(&re);check(current==0,"grown compiler buffers released");
  for(size_t point=0;point<calls+3;++point){failAt=point;attempt=0;re=nullptr;int rc=git_regexp_compile(&re,pattern.c_str(),0);if(rc==0)git_regexp_dispose(&re);check(current==0,"every injected compile allocation failure rolls back");}
 }
 failAt=SIZE_MAX;attempt=0;git_regexp re=nullptr;
 const std::string deep=std::string(17,'(')+"x"+std::string(17,')');check(git_regexp_compile(&re,deep.c_str(),0)<0&&current==0,"parenthesis nesting rejected beyond 16");
 const std::string oversized(4097,'a');check(git_regexp_compile(&re,oversized.c_str(),0)<0&&current==0,"pattern longer than 4096 rejected");
 check(git_regexp_compile(&re,"^(a+)+$",0)==0,"backtracking pattern compiles within nesting limit");
 std::string subject(500,'a');subject+='!';check(git_regexp_match(&re,subject.c_str())<0,"hostile backtracking is bounded");git_regexp_dispose(&re);check(current==0,"backtracking limit cleanup");
 check(git_regexp_compile(&re,"([a-z]+)",0)==0,"OOM match setup");const size_t held=current;
 for(size_t point=0;point<10;++point){failAt=point;attempt=0;git_regmatch span[2];git_regexp_match(&re,"abc");check(current==held,"injected match OOM cleanup");attempt=0;git_regexp_search(&re,"abc",2,span);check(current==held,"injected search OOM cleanup");}
 failAt=SIZE_MAX;git_regexp_dispose(&re);check(current==0,"final retained bytes zero");
 std::printf("PASS %u real-PCRE2 quota/lifecycle/limits checks; cycles=500; retained_bytes=%zu; peak_bytes=%zu; tracked_allocations=%zu\n",checks,current,peak,allocations);
}
