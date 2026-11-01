#include "src/tab5git/include/git2.h"
#include "src/tab5git/include/git2/sys/alloc.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <iostream>
extern "C" {
struct git_config_list;
int git_config_list_new(git_config_list**);
int git_config_list_dup(git_config_list**,git_config_list*);
int git_config_list_dup_entry(git_config_list*,const git_config_entry*);
int git_config_list_get(void**,git_config_list*,const char*);
void git_config_list_free(git_config_list*);
}
static long attempt=0,failAfter=-1;
static size_t retained=0,peak=0,checks=0,failures=0,cycles=0;
static std::map<void*,size_t> blocks;
static void check(bool ok,const char* text){++checks;if(!ok){std::cerr<<text<<" failAfter="<<failAfter<<" attempt="<<attempt<<" retained="<<retained<<"\n";std::exit(2);}}
static void* allocate(size_t n,const char*,int){long index=attempt++;if(failAfter>=0&&index>=failAfter)return nullptr;void*p=std::malloc(n?n:1);if(p){blocks[p]=n;retained+=n;if(retained>peak)peak=retained;}return p;}
static void release(void*p){if(!p)return;auto f=blocks.find(p);check(f!=blocks.end(),"foreign/double free");retained-=f->second;blocks.erase(f);std::free(p);}
static void* resize(void*p,size_t n,const char*f,int line){if(!p)return allocate(n,f,line);if(!n){release(p);return nullptr;}auto item=blocks.find(p);check(item!=blocks.end(),"foreign realloc");size_t old=item->second;void*q=allocate(n,f,line);if(q){memcpy(q,p,old<n?old:n);release(p);}return q;}
static git_config_entry entry(const char*name,const char*value){git_config_entry e{};e.name=name;e.value=value;e.backend_type="file";e.origin_path="/sd/project/.git/config";e.level=GIT_CONFIG_LEVEL_LOCAL;return e;}
static const char* value(git_config_list*l,const char*key){void*p=nullptr;int rc=git_config_list_get(&p,l,key);return rc==GIT_ENOTFOUND?nullptr:static_cast<git_config_entry*>(p)->value;}
int main(){
 git_allocator a{allocate,resize,release};check(git_libgit2_opts(GIT_OPT_SET_ALLOCATOR,&a)==0,"allocator");
 for(int round=0;round<3;++round)for(int mode=0;mode<3;++mode)for(int n=0;n<48;++n){
  ++cycles;failAfter=-1;check(git_libgit2_init()==1,"init");git_config_list*l=nullptr,*copy=nullptr;check(git_config_list_new(&l)==0,"list");
  auto alpha=entry("core.alpha","first"),beta=entry("core.beta","second");check(git_config_list_dup_entry(l,&alpha)==0,"seed alpha");check(git_config_list_dup_entry(l,&beta)==0,"seed beta");
  auto incoming=entry(mode==0?"core.gamma":"core.alpha","incoming");attempt=0;failAfter=n;
  int rc=mode==2?git_config_list_dup(&copy,l):git_config_list_dup_entry(l,&incoming);
  failAfter=-1;
  if(rc<0){++failures;check(copy==nullptr,"failed duplicate published output");check(!strcmp(value(l,"core.alpha"),"first"),"failed append changed existing");check(value(l,"core.gamma")==nullptr,"failed append published new");}
  else if(mode==2){check(copy!=nullptr,"duplicate success null");check(!strcmp(value(copy,"core.alpha"),"first"),"duplicate alpha");check(!strcmp(value(copy,"core.beta"),"second"),"duplicate beta");}
  else check(!strcmp(value(l,incoming.name),"incoming"),"append value");
  check(!strcmp(value(l,"core.beta"),"second"),"unrelated entry");git_config_list_free(copy);git_config_list_free(l);check(git_libgit2_shutdown()==0,"shutdown");check(retained==0&&blocks.empty(),"retained config payload");
 }
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"cycles\":"<<cycles<<",\"allocationFailures\":"<<failures<<",\"failurePointsPerOperation\":48,\"rounds\":3,\"retainedPayloadBytes\":"<<retained<<",\"peakPayloadBytes\":"<<peak<<"}\n";
}
