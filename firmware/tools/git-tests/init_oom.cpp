#include "src/tab5git/include/git2.h"
#include "src/tab5git/include/git2/sys/alloc.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <iostream>
static long attempt=0,failAfter=-1;
static size_t retained=0,peak=0,checks=0;
static std::map<void*,size_t> blocks;
static void check(bool ok,const char* text){++checks;if(!ok){std::cerr<<text<<" failAfter="<<failAfter<<" attempt="<<attempt<<" retained="<<retained<<"\n";std::exit(2);}}
static void* allocate(size_t n,const char*,int){long index=attempt++;if(failAfter>=0&&index>=failAfter)return nullptr;void* p=std::malloc(n?n:1);if(p){blocks[p]=n;retained+=n;if(retained>peak)peak=retained;}return p;}
static void release(void* p){if(!p)return;auto found=blocks.find(p);check(found!=blocks.end(),"foreign or double free");retained-=found->second;blocks.erase(found);std::free(p);}
static void* resize(void* p,size_t n,const char* file,int line){if(!p)return allocate(n,file,line);if(!n){release(p);return nullptr;}auto found=blocks.find(p);check(found!=blocks.end(),"foreign realloc");size_t old=found->second;void* next=allocate(n,file,line);if(next){memcpy(next,p,old<n?old:n);release(p);}return next;}
static long healthy(){failAfter=-1;attempt=0;check(git_libgit2_init()==1,"healthy initialization");long count=attempt;check(git_libgit2_shutdown()==0,"healthy shutdown");check(retained==0&&blocks.empty(),"healthy leak");return count;}
int main(){
 git_allocator allocator{allocate,resize,release};check(git_libgit2_opts(GIT_OPT_SET_ALLOCATOR,&allocator)==0,"install allocator");long normal=healthy();size_t failures=0;
 for(int round=0;round<3;++round)for(long n=0;n<=normal+2;++n){
  failAfter=n;attempt=0;int initialized=git_libgit2_init();if(initialized<0)++failures;
  check(git_libgit2_shutdown()==0,"failed initialization reference count");
  check(retained==0&&blocks.empty(),"failed initialization leaked");healthy();
 }
 std::cout<<"{\"passed\":true,\"assertions\":"<<checks<<",\"failurePoints\":"<<(normal+3)<<",\"rounds\":3,\"initializationFailures\":"<<failures<<",\"retainedPayloadBytes\":"<<retained<<",\"peakPayloadBytes\":"<<peak<<"}\n";
}
