#include "Tab5Git.h"
#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <string>
#include <vector>
int main(int argc, char** argv) {
 std::signal(SIGPIPE,SIG_IGN);
 if (argc < 4) return 2;
 std::vector<std::string> args;
 for (int i = 3; i < argc; ++i) args.emplace_back(argv[i]);
 auto out=[](const char* p,size_t n){fwrite(p,1,n,stdout);};
 auto err=[](const char* p,size_t n){fwrite(p,1,n,stderr);};
 Tab5Git::Observer observer;
 observer.worktreeChanged=[](){fprintf(stderr,"WORKTREE_CHANGED\n");};
 observer.project=[](const char* p){fprintf(stderr,"PROJECT %s\n",p);};
 const char* count=getenv("TAB5_GIT_REPEAT");
 int repeat=count?atoi(count):1,result=0;
 for(int i=0;i<repeat;++i) {
  result=Tab5Git::run(argv[1],argv[2],args,out,{},{},err,observer);
  const auto memory=Tab5Git::memoryStats();
  fprintf(stderr,"MEMORY %zu %zu %zu\n",memory.current,memory.peak,memory.reclaimedBlocks);
  if(memory.current || (!result && memory.reclaimedBlocks)) return 99;
  if(result) break;
 }
 return result;
}
