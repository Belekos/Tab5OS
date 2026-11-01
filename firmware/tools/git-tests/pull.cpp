#include "ShellPlatform.h"
#include "Tab5GitPull.h"
#include <iostream>
#include <cstdlib>
static const char* faultStage=nullptr;
static const char* cancelStage=nullptr;
static bool cancelled=false;
static size_t faultFile=0;
static void inject(const char* stage,size_t file){if(faultStage&&!strcmp(stage,faultStage)&&(!faultFile||faultFile==file))std::_Exit(97);if(cancelStage&&!strcmp(stage,cancelStage))cancelled=true;}
int main(int argc,char** argv){
 if(argc<3)return 2;git_libgit2_init();git_libgit2_opts(GIT_OPT_ENABLE_FSYNC_GITDIR,1);
 faultStage=std::getenv("TAB5_PULL_CRASH");cancelStage=std::getenv("TAB5_PULL_CANCEL_STAGE");const char* f=std::getenv("TAB5_PULL_FILE");faultFile=f?strtoul(f,nullptr,10):0;Tab5GitPull::checkpoint(inject);
 git_repository* repo=nullptr;if(git_repository_open(&repo,argv[1])<0)return 3;
 std::string command=argv[2];int rc=0;const char* stop=std::getenv("TAB5_PULL_CANCEL_AFTER");int calls=0,limit=stop?atoi(stop):0;
 Tab5Git::Cancel cancel=[&]{return cancelled||(limit&&++calls>=limit);};
 if(command=="pending"){std::cout<<(Tab5GitPull::pending(repo)?"pending":"none");}
 else if(command=="resume")rc=Tab5GitPull::resume(repo,cancel);
 else if(command=="start"){
  git_reference* head=nullptr;git_commit* target=nullptr;git_oid oid{};
  if(argc!=4||git_oid_fromstr(&oid,argv[3])||git_repository_head(&head,repo)||git_commit_lookup(&target,repo,&oid))return 4;
  rc=Tab5GitPull::start(repo,head,target,cancel);git_reference_free(head);git_commit_free(target);
 }else return 2;
 if(rc<0){const char* explanation=Tab5GitPull::diagnostic(rc);const git_error* e=git_error_last();std::cerr<<rc<<" "<<(explanation?explanation:(e?e->message:"failure"))<<"\n";}
 git_repository_free(repo);git_libgit2_shutdown();return rc<0?1:0;
}
