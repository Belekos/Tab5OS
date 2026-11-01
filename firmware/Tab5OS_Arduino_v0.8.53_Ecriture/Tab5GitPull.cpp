#include "Tab5GitPull.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace Tab5GitPull {
namespace {
enum { Pending=-4001, Invalid=-4002, Changed=-4003, ReferenceChanged=-4004,
       Storage=-4005, Unsupported=-4006, Nothing=-4007 };
constexpr uint32_t Magic=0x31504c54u;
struct Record {uint32_t magic=Magic;char oldOid[41]{},newOid[41]{},reference[256]{};uint16_t reserved=0;uint32_t sum=0;};
static_assert(offsetof(Record,sum)==344,"Pull journal contains no implicit padding");
template<class T,void(*Free)(T*)> struct Owned {
 T* p=nullptr;~Owned(){Free(p);}T** put(){return &p;}operator T*()const{return p;}
};
using Commit=Owned<git_commit,git_commit_free>;
using Tree=Owned<git_tree,git_tree_free>;
using Index=Owned<git_index,git_index_free>;
using Diff=Owned<git_diff,git_diff_free>;
using Ref=Owned<git_reference,git_reference_free>;
uint32_t checksum(const Record& record){uint32_t h=2166136261u;auto p=reinterpret_cast<const unsigned char*>(&record);for(size_t n=0;n<offsetof(Record,sum);++n)h=(h^p[n])*16777619u;return h;}
std::string intent(git_repository* repo){const char* p=git_repository_path(repo);return p?std::string(p)+"tab5-pull.pending":std::string();}
int exists(const std::string& path){struct stat st{};if(!stat(path.c_str(),&st))return 1;return errno==ENOENT?0:-1;}
bool stopped(const Tab5Git::Cancel& cancel){return cancel&&cancel();}
#ifdef TAB5_GIT_PULL_TEST
Checkpoint fault=nullptr;
void point(const char* stage,size_t completed=0){if(fault)fault(stage,completed);}
#else
void point(const char*,size_t=0){}
#endif
int read(git_repository* repo,Record& record){
 const std::string path=intent(repo);if(path.empty())return Invalid;
 FILE* file=fopen(path.c_str(),"rb");if(!file)return errno==ENOENT?Nothing:Storage;
 bool ok=fread(&record,1,sizeof(record),file)==sizeof(record)&&fgetc(file)==EOF&&!ferror(file);
 if(fclose(file))ok=false;
 if(!ok||record.magic!=Magic||record.reserved||record.sum!=checksum(record)||record.oldOid[40]||record.newOid[40]||!memchr(record.reference,0,sizeof(record.reference)))return Invalid;
 git_oid old{},next{};int valid=0;
 if(git_oid_fromstr(&old,record.oldOid)||git_oid_fromstr(&next,record.newOid)||git_reference_name_is_valid(&valid,record.reference)||!valid||strncmp(record.reference,"refs/heads/",11))return Invalid;
 return 0;
}
int write(git_repository* repo,Record& record){
 const std::string path=intent(repo);if(path.empty())return Invalid;
 record.sum=checksum(record);int flags=O_WRONLY|O_CREAT|O_EXCL;
#ifdef O_BINARY
 flags|=O_BINARY;
#endif
 int fd=open(path.c_str(),flags,0600);if(fd<0)return errno==EEXIST?Pending:Storage;
 bool ok=::write(fd,&record,sizeof(record))==ssize_t(sizeof(record));if(ok)ok=fsync(fd)==0;if(close(fd))ok=false;
 // A partial intent remains a blocker; it is never silently discarded.
 return ok?0:Storage;
}
bool regular(uint32_t mode){return !mode||mode==GIT_FILEMODE_BLOB||mode==GIT_FILEMODE_BLOB_EXECUTABLE;}
int sameIndex(git_repository* repo,git_index* index,git_tree* tree){Diff diff;int rc=git_diff_tree_to_index(diff.put(),repo,tree,index,nullptr);return rc<0?rc:(git_diff_num_deltas(diff)?0:1);}
int inspectFiles(git_repository* repo,git_tree* before,git_tree* after,git_index* baseline,const Tab5Git::Cancel& cancel,bool completed){
 Diff diff;int rc=git_diff_tree_to_tree(diff.put(),repo,before,after,nullptr);if(rc<0)return rc;
 const char* root=git_repository_workdir(repo);if(!root)return Unsupported;
 for(size_t n=0;n<git_diff_num_deltas(diff);++n){
  if(stopped(cancel))return GIT_EUSER;
  const git_diff_delta* delta=git_diff_get_delta(diff,n);
  if(!regular(delta->old_file.mode)||!regular(delta->new_file.mode))return Unsupported;
  const char* path=delta->new_file.path?delta->new_file.path:delta->old_file.path;
  if(!path||!*path)return Unsupported;
  const std::string full=std::string(root)+path;struct stat st{};
  if(stat(full.c_str(),&st)){
   if(errno!=ENOENT)return Storage;
   // Missing newly added or already deleted files are valid intermediate
   // states. A missing modified file is ambiguous: preserve it for inspection.
   if(delta->old_file.mode&&delta->new_file.mode)return Changed;
   if(completed&&delta->new_file.mode)return Changed;
   if(delta->old_file.mode){rc=git_index_remove_bypath(baseline,path);if(rc<0&&rc!=GIT_ENOTFOUND)return rc;}
   continue;
  }
  if(!S_ISREG(st.st_mode))return Changed;
  git_oid actual{};rc=git_repository_hashfile(&actual,repo,path,GIT_OBJECT_BLOB,path);if(rc<0)return rc;
  bool isNew=delta->new_file.mode&&git_oid_equal(&actual,&delta->new_file.id);
  bool isOld=delta->old_file.mode&&git_oid_equal(&actual,&delta->old_file.id);
  if((completed&&!isNew)||(!isNew&&!isOld))return Changed;
  if(completed){
   // Checkout closes worktree files, but the branch must not become durable
   // before their contents. The filesystem adapter must report flush errors.
   int fd=open(full.c_str(),O_RDWR);if(fd<0)return Storage;
   bool flushed=fsync(fd)==0;if(close(fd))flushed=false;if(!flushed)return Storage;
  }
  if(isNew){git_index_entry entry{};entry.path=path;entry.mode=delta->new_file.mode;entry.id=delta->new_file.id;rc=git_index_add(baseline,&entry);if(rc<0)return rc;}
 }
 return 0;
}
int notify(git_checkout_notify_t,const char*,const git_diff_file*,const git_diff_file*,const git_diff_file*,void* payload){return stopped(*static_cast<const Tab5Git::Cancel*>(payload))?GIT_EUSER:0;}
void progress(const char*,size_t completed,size_t,void*){point("checkout-file",completed);}
int apply(git_repository* repo,const Record& record,const Tab5Git::Cancel& cancel){
 git_oid old{},next{};if(git_oid_fromstr(&old,record.oldOid)||git_oid_fromstr(&next,record.newOid))return Invalid;
 Ref head;int rc=git_repository_head(head.put(),repo);if(rc<0)return rc;
 if(git_repository_head_detached(repo)||strcmp(git_reference_name(head),record.reference))return ReferenceChanged;
 const git_oid* current=git_reference_target(head);if(!current)return ReferenceChanged;
 bool committed=git_oid_equal(current,&next);if(!committed&&!git_oid_equal(current,&old))return ReferenceChanged;
 Commit previous,target;Tree before,after;Index baseline,index;
 if((rc=git_commit_lookup(previous.put(),repo,&old))<0||(rc=git_commit_lookup(target.put(),repo,&next))<0||
    (rc=git_commit_tree(before.put(),previous))<0||(rc=git_commit_tree(after.put(),target))<0||
    (rc=git_index_new(baseline.put()))<0||(rc=git_index_read_tree(baseline,before))<0||
    (rc=git_repository_index(index.put(),repo))<0||(rc=git_index_read(index,1))<0)return rc;
 if(git_index_has_conflicts(index))return Changed;
 int oldIndex=sameIndex(repo,index,before);if(oldIndex<0)return oldIndex;
 int newIndex=sameIndex(repo,index,after);if(newIndex<0)return newIndex;
 if((!oldIndex&&!newIndex)||(committed&&!newIndex))return Changed;
 if((rc=inspectFiles(repo,before,after,baseline,cancel,committed))<0)return rc;
 if(stopped(cancel))return GIT_EUSER;
 if(!committed){
  git_checkout_options options=GIT_CHECKOUT_OPTIONS_INIT;
  options.checkout_strategy=GIT_CHECKOUT_SAFE|GIT_CHECKOUT_DONT_UPDATE_INDEX|GIT_CHECKOUT_DONT_OVERWRITE_IGNORED;
  options.baseline_index=baseline;options.notify_flags=GIT_CHECKOUT_NOTIFY_ALL;options.notify_cb=notify;options.notify_payload=const_cast<Tab5Git::Cancel*>(&cancel);options.progress_cb=progress;
  if((rc=git_checkout_tree(repo,reinterpret_cast<git_object*>(target.p),&options))<0)return rc;
  point("checkout");if(stopped(cancel))return GIT_EUSER;
  // Verify the complete target before advancing the index or branch. This
  // also catches missing files that checkout SAFE intentionally preserves.
  if((rc=inspectFiles(repo,before,after,baseline,cancel,true))<0)return rc;
  if((rc=git_index_read_tree(index,after))<0||(rc=git_index_write(index))<0)return rc;
  point("index");if(stopped(cancel))return GIT_EUSER;
  Ref updated;
  // Matching update prevents overwriting a branch moved by another process.
  if((rc=git_reference_create_matching(updated.put(),repo,record.reference,&next,1,&old,"Tab5OS: fast-forward"))<0)return rc;
  point("head");
 }
 if(stopped(cancel))return GIT_EUSER;
 if(remove(intent(repo).c_str()))return Storage;
 point("cleanup");return 0;
}
}
bool pending(git_repository* repo){std::string path=intent(repo);return path.empty()||exists(path)!=0;}
int start(git_repository* repo,git_reference* previous,git_commit* target,const Tab5Git::Cancel& cancel){
 if(pending(repo))return Pending;if(stopped(cancel))return GIT_EUSER;
 const char* ref=git_reference_name(previous);const git_oid* old=git_reference_target(previous);
 if(!old||!ref||strlen(ref)>=sizeof(Record::reference)||strncmp(ref,"refs/heads/",11))return Unsupported;
 // Reject unsupported object kinds before creating an intent. This firmware
 // does not materialize links or submodules during a recoverable pull.
 int rc=0;{
  Commit parent;Tree before,after;Diff changes;rc=git_commit_lookup(parent.put(),repo,old);
  if(rc<0||(rc=git_commit_tree(before.put(),parent))<0||(rc=git_commit_tree(after.put(),target))<0||
     (rc=git_diff_tree_to_tree(changes.put(),repo,before,after,nullptr))<0)return rc;
  for(size_t n=0;n<git_diff_num_deltas(changes);++n){const auto* delta=git_diff_get_delta(changes,n);if(!regular(delta->old_file.mode)||!regular(delta->new_file.mode))return Unsupported;}
 }
 Record record;git_oid_tostr(record.oldOid,sizeof(record.oldOid),old);git_oid_tostr(record.newOid,sizeof(record.newOid),git_commit_id(target));strcpy(record.reference,ref);
 rc=write(repo,record);if(rc<0)return rc;point("intent");return apply(repo,record,cancel);
}
int resume(git_repository* repo,const Tab5Git::Cancel& cancel){Record record;int rc=read(repo,record);return rc<0?rc:apply(repo,record,cancel);}
const char* diagnostic(int result){switch(result){
 case Pending:return "Un import Git est interrompu. Utilisez git recover --continue.";
 case Invalid:return "Journal d'import Git invalide. Conservez le dossier et vérifiez-le sur ordinateur.";
 case Changed:return "Reprise Git refusée : des fichiers ou l'index ont changé. Vos modifications sont conservées.";
 case ReferenceChanged:return "Reprise Git refusée : la branche a changé.";
 case Storage:return "Impossible d'enregistrer la reprise Git. Vérifiez le stockage.";
 case Unsupported:return "Cette modification Git nécessite une intervention sur ordinateur.";
 case Nothing:return "Aucun import Git à reprendre.";
 default:return nullptr;
}}
#ifdef TAB5_GIT_PULL_TEST
void checkpoint(Checkpoint callback){fault=callback;}
#endif
}
