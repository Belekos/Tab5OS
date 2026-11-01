#include "Tab5Git.h"
#ifndef TAB5_GIT_HOST
#include "Tab5Secrets.h"
#endif
#include "Tab5GitPull.h"
#include "Tab5TerminalPath.h"
#include "Tab5GitPort.h"
#include "src/tab5git/include/git2.h"
#include "src/tab5git/include/git2/sys/alloc.h"
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include <memory>
#ifndef TAB5_GIT_HOST
#include "Tab5Storage.h"
#include "Tab5ResourceManager.h"
#include "Tab5GitMemory.h"
#include "Tab5NetworkGate.h"
#include "Tab5VolumeFS.h"
#include "Tab5Task.h"
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace Tab5Git {
namespace {SecretResolver secretLookup;
 bool secretName(const std::string& name){if(name.empty()||name.size()>48||!(isalpha((unsigned char)name[0])||name[0]=='_'))return false;for(unsigned char c:name)if(!isalnum(c)&&c!='_')return false;return true;}
 bool resolveSecret(const char* key,char* output,size_t capacity){
  if(output&&capacity)memset(output,0,capacity);if(secretLookup)return secretLookup(key,output,capacity);
#ifndef TAB5_GIT_HOST
  return Tab5Secrets::get(key,output,capacity);
#else
  return false;
#endif
 }
 void wipeCredential(char* p,size_t n){volatile char* v=p;while(n--)*v++=0;}
}
void secretResolver(SecretResolver resolver){secretLookup=std::move(resolver);}

namespace {
std::atomic_flag active=ATOMIC_FLAG_INIT;
std::atomic<bool> occupied{false};
std::atomic<size_t> current{0},peak{0};
size_t reclaimedBlocks=0,reclaimedBytes=0;
#ifdef TAB5_GIT_HOST
long failAllocation=-1,allocationAttempts=0;
#endif
struct alignas(std::max_align_t) Allocation {size_t size;Allocation* previous;Allocation* next;};
Allocation* allocations=nullptr;
#ifndef TAB5_GIT_HOST
AllocationBudget* allocationBudget=nullptr;
bool allocationDenied=false;
Tab5Resources::AdmissionReport allocationFailure;
#endif
void* allocate(size_t n,const char* file,int line){
#ifdef TAB5_GIT_HOST
 if(std::getenv("TAB5_GIT_TRACE_ALLOC"))std::fprintf(stderr,"ALLOC %ld %zu %s:%d\n",allocationAttempts,n,file,line);
 if(failAllocation>=0&&allocationAttempts++>=failAllocation)return nullptr;
#endif
 if(tab5_git_cancelled()||n>SIZE_MAX-sizeof(Allocation)||n+sizeof(Allocation)>MemoryLimit-current.load())return nullptr;
#ifndef TAB5_GIT_HOST
 if(allocationBudget&&!allocationBudget->admit(n+sizeof(Allocation))){allocationDenied=true;allocationFailure=allocationBudget->lastReport();return nullptr;}
 auto* a=static_cast<Allocation*>(heap_caps_malloc(n+sizeof(Allocation),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#else
 auto* a=static_cast<Allocation*>(std::malloc(n+sizeof(Allocation)));
#endif
 if(!a)return nullptr;a->size=n;a->previous=nullptr;a->next=allocations;if(allocations)allocations->previous=a;allocations=a;size_t used=current.fetch_add(n+sizeof(Allocation))+n+sizeof(Allocation);peak.store(std::max(peak.load(),used));
#ifndef TAB5_GIT_HOST
 if(allocationBudget)allocationBudget->allocated(n+sizeof(Allocation));
#endif
 return a+1;
}
void release(void* p){if(!p)return;auto* a=static_cast<Allocation*>(p)-1;current.fetch_sub(a->size+sizeof(Allocation));
 if(a->previous)a->previous->next=a->next;else allocations=a->next;if(a->next)a->next->previous=a->previous;
#ifdef TAB5_GIT_HOST
 if(std::getenv("TAB5_GIT_TRACE_ALLOC"))std::fprintf(stderr,"FREE %zu\n",a->size);
#endif
#ifndef TAB5_GIT_HOST
 heap_caps_free(a);
#else
 std::free(a);
#endif
}
void* resize(void* p,size_t n,const char* f,int l){if(!p)return allocate(n,f,l);if(!n){release(p);return nullptr;}
 auto* old=static_cast<Allocation*>(p)-1;
 // Both blocks count during relocation. A failed growth leaves the old block
 // intact and cannot exceed the physical session quota transiently.
 void* replacement=allocate(n,f,l);if(!replacement)return nullptr;
 memcpy(replacement,p,std::min(n,old->size));release(p);return replacement;
}
template<class T,void(*Free)(T*)> struct Ptr {T* p=nullptr;~Ptr(){Free(p);}T** put(){return &p;}operator T*()const{return p;}T* operator->()const{return p;}void reset(){Free(p);p=nullptr;}};
using Repo=Ptr<git_repository,git_repository_free>;using Config=Ptr<git_config,git_config_free>;
using Index=Ptr<git_index,git_index_free>;using Tree=Ptr<git_tree,git_tree_free>;using Commit=Ptr<git_commit,git_commit_free>;
using Ref=Ptr<git_reference,git_reference_free>;using Remote=Ptr<git_remote,git_remote_free>;
using Signature=Ptr<git_signature,git_signature_free>;using Diff=Ptr<git_diff,git_diff_free>;
using Walk=Ptr<git_revwalk,git_revwalk_free>;using Object=Ptr<git_object,git_object_free>;
struct Str {git_buf b=GIT_BUF_INIT;~Str(){git_buf_dispose(&b);}};
bool allowedURL(const std::string& url){
 if(url.compare(0,8,"https://")||url.size()>1024)return false;
 size_t slash=url.find('/',8);if(slash==std::string::npos||slash==8)return false;
 std::string authority=url.substr(8,slash-8);if(authority.find('@')!=std::string::npos)return false;
 for(unsigned char c:url)if(c<=32||c==127||c=='\\')return false;
 return url.find('#')==std::string::npos&&url.find('?')==std::string::npos;
}
std::string origin(const std::string& url){auto n=url.find('/',8);std::string r=url.substr(0,n);for(char& c:r)c=char(std::tolower((unsigned char)c));return r;}
bool exists(const std::string& p){struct stat st{};return stat(p.c_str(),&st)==0;}
bool configKey(const std::string& s){return s=="user.name"||s=="user.email"||s=="tab5.credentialFile"||s=="tab5.usernameSecret"||s=="tab5.tokenSecret";}
std::string redact(std::string s){size_t pos=0;while((pos=s.find("://",pos))!=std::string::npos){size_t start=pos+3,end=s.find_first_of("/ \t\r\n\"'",start),at=s.find('@',start);if(at!=std::string::npos&&(end==std::string::npos||at<end)){s.replace(start,at-start,"***");pos=start+4;}else pos=start;}return s;}
struct Session {
 std::string root,cwd,project,networkOrigin;Output out,err;Cancel cancel;Translate translate;Observer observer;
 Repo repo;Config config;char username[256]{},token[1024]{};std::string usernameSecret,tokenSecret;unsigned credentialAttempts=0;bool pushRejected=false,networking=false;
 ~Session(){Tab5GitPort::cancellation({});volatile char* p=token;for(size_t n=0;n<sizeof(token);++n)p[n]=0;}
 std::string tr(const char* s){return translate?translate(s):std::string(s);}
 void print(const std::string& s){if(out)out(s.data(),s.size());}
 int fail(const char* key){std::string s=tr(key)+"\n";if(err)err(s.data(),s.size());return stopped()?130:1;}
 bool stopped(){
#ifndef TAB5_GIT_HOST
  if(networking&&!tab5NetworkEnabled())return true;
#endif
  return cancel&&cancel();}
 int result(int rc){if(stopped())return 130;if(rc>=0)return 0;const git_error* e=git_error_last();
  std::string s=tr("Opération Git impossible.");if(e&&e->message)s+=" "+redact(e->message);s+='\n';if(err)err(s.data(),s.size());return 1;}
 bool logical(const std::string& p,std::string& value){return Tab5TerminalPath::logical(p,cwd,value,true);}
 bool open(){
  std::string candidate=cwd;const std::string ceiling=cwd=="/USB"||cwd.compare(0,5,"/USB/")==0?"/USB":"/";
  while(candidate!=ceiling){struct stat st{};if(!stat((root+candidate+"/.git").c_str(),&st)){if(!S_ISDIR(st.st_mode))return false;break;}if(errno!=ENOENT)return false;auto slash=candidate.rfind('/');candidate=slash?candidate.substr(0,slash):"/";}
  if(candidate==ceiling){if(observer.notRepository)observer.notRepository();return false;}
  if(exists(root+candidate+"/.git/commondir")||exists(root+candidate+"/.git/objects/info/alternates"))return false;
  if(!Tab5GitPort::project(root+candidate))return false;
  if(git_repository_open_ext(repo.put(),(root+candidate).c_str(),GIT_REPOSITORY_OPEN_NO_SEARCH,nullptr)<0)return false;
  const char* work=git_repository_workdir(repo);if(!work)return false;std::string path=work;
  while(path.size()>1&&path.back()=='/')path.pop_back();
  if(path.size()<=root.size()||path.compare(0,root.size(),root)||path[root.size()]!='/')return false;
  project=path.substr(root.size());if(observer.project)observer.project(project.c_str());
  return git_repository_config(config.put(),repo)==0;
 }
 int head(Ref& ref){int rc=git_repository_head(ref.put(),repo);bool unborn=rc==GIT_EUNBORNBRANCH||rc==GIT_ENOTFOUND;
  if(observer.repository)observer.repository(unborn?"main":(ref.p?git_reference_shorthand(ref):""),unborn,git_repository_head_detached(repo)==1);
  return unborn?0:rc;
 }
 void configuration(){for(const char* k:{"user.name","user.email","tab5.credentialFile","tab5.usernameSecret","tab5.tokenSecret"}){Str v;if(git_config_get_string_buf(&v.b,config,k)==0&&observer.config)observer.config(k,v.b.ptr);}
  git_strarray names{};if(!git_remote_list(&names,repo)){for(size_t n=0;n<names.count;++n){Remote remote;if(!git_remote_lookup(remote.put(),repo,names.strings[n])){const char* url=git_remote_url(remote);if(url&&observer.remote){auto safe=redact(url);observer.remote(names.strings[n],safe.c_str());}}}git_strarray_dispose(&names);}}
 bool loadCredential(const std::string& explicitFile=""){
  Str value;std::string file=explicitFile;
  if(config.p){Str u,t;if(usernameSecret.empty()&&git_config_get_string_buf(&u.b,config,"tab5.usernameSecret")==0)usernameSecret=u.b.ptr;if(tokenSecret.empty()&&git_config_get_string_buf(&t.b,config,"tab5.tokenSecret")==0)tokenSecret=t.b.ptr;}
  if(!usernameSecret.empty()||!tokenSecret.empty())return file.empty()&&secretName(usernameSecret)&&secretName(tokenSecret);
  if(file.empty()&&config.p&&git_config_get_string_buf(&value.b,config,"tab5.credentialFile")==0)file=value.b.ptr;
  if(file.empty())return true;std::string path;if(!logical(file,path))return false;
  FILE* f=fopen((root+path).c_str(),"rb");if(!f)return false;
  bool ok=fgets(username,sizeof(username),f)&&strchr(username,'\n')&&fgets(token,sizeof(token),f);
  if(ok){username[strcspn(username,"\r\n")]=0;token[strcspn(token,"\r\n")]=0;int tail=fgetc(f);ok=tail==EOF&&!ferror(f)&&username[0]&&token[0];}
  fclose(f);return ok;
 }
 static int credentials(git_credential** out,const char* url,const char*,unsigned types,void* ctx){auto& s=*static_cast<Session*>(ctx);
  if(s.stopped()||!url||origin(url)!=s.networkOrigin||++s.credentialAttempts>2||!(types&GIT_CREDENTIAL_USERPASS_PLAINTEXT))return GIT_EAUTH;
  if(!s.usernameSecret.empty()){
   char user[513]{},token[513]{};bool ok=resolveSecret(s.usernameSecret.c_str(),user,sizeof(user))&&resolveSecret(s.tokenSecret.c_str(),token,sizeof(token))&&user[0]&&token[0]&&!strpbrk(user,"\r\n")&&!strpbrk(token,"\r\n");
   int rc=ok?git_credential_userpass_plaintext_new(out,user,token):GIT_EAUTH;wipeCredential(user,sizeof(user));wipeCredential(token,sizeof(token));return rc;
  }
  return s.username[0]?git_credential_userpass_plaintext_new(out,s.username,s.token):GIT_EAUTH;
 }
 static int certificate(git_cert*,int valid,const char*,void*){Tab5GitPort::phase(valid?"tls-verified":"tls-rejected");return valid?0:GIT_ECERTIFICATE;}
 static int progress(const git_indexer_progress* p,void* ctx){Tab5GitPort::phase("receive-pack");auto& s=*static_cast<Session*>(ctx);return s.stopped()||p->received_bytes>128u*1024u*1024u?GIT_EUSER:0;}
 static int sideband(const char* text,int n,void* ctx){auto& s=*static_cast<Session*>(ctx);if(s.stopped())return GIT_EUSER;if(s.out)s.out(text,size_t(n));return 0;}
 static int pushResult(const char*,const char* status,void* ctx){auto& s=*static_cast<Session*>(ctx);if(status){s.pushRejected=true;s.print(std::string(status)+"\n");}return s.stopped()?GIT_EUSER:0;}
 git_remote_callbacks callbacks(){git_remote_callbacks c=GIT_REMOTE_CALLBACKS_INIT;c.payload=this;c.credentials=credentials;c.certificate_check=certificate;c.transfer_progress=progress;c.sideband_progress=sideband;c.push_update_reference=pushResult;return c;}
 static int checkout(git_checkout_notify_t,const char*,const git_diff_file*,const git_diff_file*,const git_diff_file*,void* ctx){Tab5GitPort::phase("checkout");return static_cast<Session*>(ctx)->stopped()?GIT_EUSER:0;}
 void checkoutOptions(git_checkout_options& opts){opts.checkout_strategy=GIT_CHECKOUT_SAFE;opts.notify_flags=GIT_CHECKOUT_NOTIFY_ALL;opts.notify_cb=checkout;opts.notify_payload=this;}
 static int status(const char* path,unsigned flags,void* ctx){auto& s=*static_cast<Session*>(ctx);if(s.stopped())return GIT_EUSER;
  if(s.observer.status)s.observer.status(path,flags);char a=' ',b=' ';
  if(flags&GIT_STATUS_INDEX_NEW)a='A';else if(flags&GIT_STATUS_INDEX_DELETED)a='D';else if(flags&GIT_STATUS_INDEX_RENAMED)a='R';else if(flags&GIT_STATUS_INDEX_MODIFIED)a='M';
  if(flags&GIT_STATUS_WT_NEW)a=b='?';else if(flags&GIT_STATUS_WT_DELETED)b='D';else if(flags&GIT_STATUS_WT_MODIFIED)b='M';else if(flags&GIT_STATUS_WT_RENAMED)b='R';
  if(flags&GIT_STATUS_CONFLICTED)a=b='U';s.print(std::string(1,a)+b+" "+path+"\n");return 0;
 }
 int status(){Ref ref;int rc=head(ref);if(rc<0)return result(rc);configuration();git_status_options opts=GIT_STATUS_OPTIONS_INIT;
  opts.flags=GIT_STATUS_OPT_INCLUDE_UNTRACKED|GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS|GIT_STATUS_OPT_SORT_CASE_SENSITIVELY;
  return result(git_status_foreach_ext(repo,&opts,status,this));
 }
 static int matched(const char*,const char*,void* ctx){return static_cast<Session*>(ctx)->stopped()?GIT_EUSER:0;}
 struct AddScope{Session* session;const std::string* path;size_t matches=0;};
 static int inScope(const char* path,const char*,void* context){auto& scope=*static_cast<AddScope*>(context);if(scope.session->stopped())return GIT_EUSER;
  const auto& prefix=*scope.path;if(!prefix.empty()&&(strncmp(path,prefix.c_str(),prefix.size())||!(path[prefix.size()]=='/'||path[prefix.size()]==0)))return 1;++scope.matches;return 0;}
 int add(const std::vector<std::string>& a,size_t n){if(n==a.size())return 2;Index index;if(git_repository_index(index.put(),repo)<0)return result(-1);
  // -A stages the complete repository; a literal . or directory stages only
  // that subtree. Explorer may keep its current folder inside a repository.
  bool all=a[n]=="-A";if(all&&n+1!=a.size())return 2;
  int rc=0;if(all){git_strarray paths{};rc=git_index_add_all(index,&paths,GIT_INDEX_ADD_DEFAULT,matched,this);if(!rc)rc=git_index_update_all(index,&paths,matched,this);}
  else{if(a[n]=="--")++n;if(n==a.size())return 2;for(;n<a.size()&&!rc&&!stopped();++n){std::string logicalPath;if(!logical(a[n],logicalPath)||!Tab5Files::sameOrDescendant(logicalPath,project))return fail("Chemin Git invalide.");
    std::string path=logicalPath.size()==project.size()?std::string():logicalPath.substr(project.size()+1);if(path==".git"||path.compare(0,5,".git/")==0)return fail("Chemin Git invalide.");
    struct stat info{};const bool present=stat((root+logicalPath).c_str(),&info)==0;
    if((present&&S_ISDIR(info.st_mode))||(!present&&!git_index_get_bypath(index,path.c_str(),0))){AddScope scope{this,&path};git_strarray paths{};
     rc=git_index_add_all(index,&paths,GIT_INDEX_ADD_DEFAULT,inScope,&scope);if(!rc)rc=git_index_update_all(index,&paths,inScope,&scope);if(!rc&&!scope.matches&&!present)rc=GIT_ENOTFOUND;
    }else if(present)rc=git_index_add_bypath(index,path.c_str());else rc=git_index_remove_bypath(index,path.c_str());}}
  if(stopped())return 130;if(!rc)rc=git_index_write(index);return result(rc);
 }
 int commit(const std::vector<std::string>& a,size_t n){if(n+2!=a.size()||a[n]!="-m"||a[n+1].empty()||a[n+1].size()>4096)return 2;
  Index index;Tree tree;Commit parent;Ref ref;Signature sig;git_oid treeOid,oid;
  if(git_repository_state(repo)!=GIT_REPOSITORY_STATE_NONE)return fail("Le dépôt Git nécessite une résolution sur ordinateur.");
  if(git_signature_default(sig.put(),repo)<0)return fail("Renseignez user.name et user.email avant de valider.");
  if(git_repository_index(index.put(),repo)<0)return result(-1);if(git_index_has_conflicts(index))return fail("Le dépôt Git contient des conflits.");
  if(git_index_write_tree(&treeOid,index)<0||git_tree_lookup(tree.put(),repo,&treeOid)<0)return result(-1);
  int rc=head(ref);if(rc<0)return result(rc);if(ref.p){if(git_commit_lookup(parent.put(),repo,git_reference_target(ref))<0)return result(-1);if(git_oid_equal(&treeOid,git_commit_tree_id(parent)))return fail("Aucune modification préparée.");}
  if(stopped())return 130;const git_commit* parents[]={parent.p};rc=git_commit_create(&oid,repo,"HEAD",sig,sig,nullptr,a[n+1].c_str(),tree,parent.p?1:0,parents);
  if(!rc){char hex[GIT_OID_SHA1_HEXSIZE+1];git_oid_tostr(hex,sizeof(hex),&oid);print(std::string(hex)+"\n");}return result(rc);
 }
 static int diffLine(const git_diff_delta*,const git_diff_hunk*,const git_diff_line* line,void* ctx){auto& s=*static_cast<Session*>(ctx);if(s.stopped())return GIT_EUSER;
  if(line->origin==' '||line->origin=='+'||line->origin=='-')s.out(&line->origin,1);s.out(line->content,line->content_len);return 0;}
 int diff(bool cached,const std::string* selected=nullptr){Diff diff;git_diff_options opts=GIT_DIFF_OPTIONS_INIT;opts.max_size=2*1024*1024;opts.flags=GIT_DIFF_INCLUDE_TYPECHANGE;
  std::string path;char* pathValue=nullptr;
  if(selected){std::string logicalPath;if(!logical(*selected,logicalPath)||!Tab5Files::sameOrDescendant(logicalPath,project))return fail("Chemin Git invalide.");
   if(logicalPath.size()!=project.size()){path=logicalPath.substr(project.size()+1);if(path==".git"||path.compare(0,5,".git/")==0)return fail("Chemin Git invalide.");
    pathValue=&path[0];opts.pathspec={&pathValue,1};opts.flags|=GIT_DIFF_DISABLE_PATHSPEC_MATCH;}}
  int rc;if(cached){Ref ref;Commit commit;Tree tree;rc=head(ref);if(rc<0)return result(rc);if(ref.p&&(git_commit_lookup(commit.put(),repo,git_reference_target(ref))<0||git_commit_tree(tree.put(),commit)<0))return result(-1);rc=git_diff_tree_to_index(diff.put(),repo,tree,nullptr,&opts);}
  else rc=git_diff_index_to_workdir(diff.put(),repo,nullptr,&opts);if(!rc)rc=git_diff_print(diff,GIT_DIFF_FORMAT_PATCH,diffLine,this);return result(rc);
 }
 int log(unsigned count){Walk walk;if(git_revwalk_new(walk.put(),repo)<0)return result(-1);git_revwalk_sorting(walk,GIT_SORT_TOPOLOGICAL|GIT_SORT_TIME);int rc=git_revwalk_push_head(walk);if(rc==GIT_ENOTFOUND)return 0;if(rc<0)return result(rc);
  git_oid oid;for(unsigned i=0;i<count&&!stopped()&&(rc=git_revwalk_next(&oid,walk))==0;++i){Commit commit;if(git_commit_lookup(commit.put(),repo,&oid)<0)return result(-1);char hex[41];git_oid_tostr(hex,sizeof(hex),&oid);const char* summary=git_commit_summary(commit);if(!summary)return result(-1);print(std::string(hex)+" "+summary+"\n");}return stopped()?130:(rc==GIT_ITEROVER?0:result(rc));
 }
 int remote(const std::vector<std::string>& a,size_t n){if(n==a.size()||(n+1==a.size()&&a[n]=="-v")){git_strarray names{};int rc=git_remote_list(&names,repo);if(rc<0)return result(rc);for(size_t i=0;i<names.count;++i){Remote remote;if(!git_remote_lookup(remote.put(),repo,names.strings[i])){const char* url=git_remote_url(remote);auto safe=redact(url?url:"");print(std::string(names.strings[i])+" "+safe+"\n");if(observer.remote)observer.remote(names.strings[i],safe.c_str());}}git_strarray_dispose(&names);return 0;}
  if(n+3!=a.size()||(a[n]!="add"&&a[n]!="set-url")||!allowedURL(a[n+2]))return 2;
  Remote remote;int rc=git_remote_lookup(remote.put(),repo,a[n+1].c_str());if(rc==GIT_ENOTFOUND)rc=git_remote_create(remote.put(),repo,a[n+1].c_str(),a[n+2].c_str());else if(!rc&&a[n]=="set-url")rc=git_remote_set_url(repo,a[n+1].c_str(),a[n+2].c_str());else if(!rc)rc=GIT_EEXISTS;return result(rc);
 }
 int setConfig(const std::vector<std::string>& a,size_t n){if(n+2==a.size()&&a[n]=="--unset"&&configKey(a[n+1])){int rc=git_config_delete_entry(config,a[n+1].c_str());return rc==GIT_ENOTFOUND?0:result(rc);}if(n>=a.size()||!configKey(a[n])||a.size()>n+2)return 2;
  if(n+1==a.size()){Str value;int rc=git_config_get_string_buf(&value.b,config,a[n].c_str());if(!rc)print(std::string(value.b.ptr)+"\n");return result(rc);}
  if(a[n+1].empty()||a[n+1].size()>250||a[n+1].find_first_of("\r\n")!=std::string::npos)return 2;
  if(a[n]=="tab5.usernameSecret"||a[n]=="tab5.tokenSecret"){if(!secretName(a[n+1]))return 2;return result(git_config_set_string(config,a[n].c_str(),a[n+1].c_str()));}
  if(a[n]=="tab5.credentialFile"){std::string path;if(!logical(a[n+1],path))return 2;return result(git_config_set_string(config,a[n].c_str(),path.c_str()));}
  return result(git_config_set_string(config,a[n].c_str(),a[n+1].c_str()));
 }
 int transfer(const std::string& command,const std::string& name){Remote remote;if(git_remote_lookup(remote.put(),repo,name.c_str())<0)return result(-1);
  const char* url=command=="push"?git_remote_pushurl(remote):git_remote_url(remote);if(!url)url=git_remote_url(remote);
  if(!url||!allowedURL(url))return fail("Git accepte uniquement une adresse HTTPS sans identifiants.");networkOrigin=origin(url);
  if(!loadCredential())return fail("Fichier d'identifiants Git invalide.");networking=true;
#ifndef TAB5_GIT_HOST
  Tab5NetworkLease network("git",1500,[](void* p){return static_cast<Session*>(p)->stopped();},this);if(!network)return fail("Réseau indisponible.");
  if(time(nullptr)<1700000000)return fail("Réglez la date avant une connexion sécurisée.");
#endif
  if(command=="push"){Ref headRef;int rc=git_repository_head(headRef.put(),repo);if(rc<0)return result(rc);if(git_repository_head_detached(repo))return fail("Sélectionnez une branche Git.");
   std::string spec=std::string(git_reference_name(headRef))+":"+git_reference_name(headRef);char* text=&spec[0];git_strarray refs{&text,1};git_push_options opts=GIT_PUSH_OPTIONS_INIT;opts.pb_parallelism=1;opts.callbacks=callbacks();opts.follow_redirects=GIT_REMOTE_REDIRECT_NONE;opts.proxy_opts.type=GIT_PROXY_NONE;
   rc=git_remote_push(remote,&refs,&opts);if(!rc&&pushRejected)rc=GIT_ERROR;return result(rc);
  }
  git_fetch_options opts=GIT_FETCH_OPTIONS_INIT;opts.callbacks=callbacks();opts.download_tags=GIT_REMOTE_DOWNLOAD_TAGS_NONE;opts.follow_redirects=GIT_REMOTE_REDIRECT_NONE;opts.proxy_opts.type=GIT_PROXY_NONE;
  int rc=git_remote_fetch(remote,nullptr,&opts,nullptr);if(rc<0||command=="fetch")return result(rc);
  Ref headRef,target;Commit targetCommit;if(git_repository_head(headRef.put(),repo)<0)return result(-1);if(git_repository_head_detached(repo))return fail("Sélectionnez une branche Git.");
  std::string tracking="refs/remotes/"+name+"/"+git_reference_shorthand(headRef);if(git_reference_lookup(target.put(),repo,tracking.c_str())<0)return result(-1);
  const git_oid* old=git_reference_target(headRef);const git_oid* next=git_reference_target(target);if(git_oid_equal(old,next))return 0;
  if(git_graph_descendant_of(repo,next,old)!=1)return fail("Les branches divergent. Fusion nécessaire sur ordinateur.");
  git_status_options statusOpts=GIT_STATUS_OPTIONS_INIT;statusOpts.flags=GIT_STATUS_OPT_INCLUDE_UNTRACKED;git_status_list* list=nullptr;rc=git_status_list_new(&list,repo,&statusOpts);bool clean=rc==0&&git_status_list_entrycount(list)==0;git_status_list_free(list);if(!clean)return fail("Validez les modifications locales avant l'import.");
  if(git_commit_lookup(targetCommit.put(),repo,next)<0)return result(-1);
  rc=Tab5GitPull::start(repo,headRef,targetCommit,[this]{return stopped();});
  if(const char* message=Tab5GitPull::diagnostic(rc))return fail(message);return result(rc);
 }
 int initialize(const std::string& path){std::string logicalPath;if(!logical(path,logicalPath))return fail("Chemin Git invalide.");if(exists(root+logicalPath+"/.git"))return fail("Ce dossier contient déjà un dépôt Git.");
  if(!Tab5GitPort::project(root+logicalPath))return fail("Une récupération Git est nécessaire.");
  git_repository_init_options opts=GIT_REPOSITORY_INIT_OPTIONS_INIT;opts.flags=GIT_REPOSITORY_INIT_MKPATH;opts.initial_head="main";opts.mode=GIT_REPOSITORY_INIT_SHARED_UMASK;
  int rc=git_repository_init_ext(repo.put(),(root+logicalPath).c_str(),&opts);if(!rc){Config cfg;if(git_repository_config(cfg.put(),repo)<0)return result(-1);rc=git_config_set_bool(cfg,"core.filemode",0);if(!rc)rc=git_config_set_bool(cfg,"core.symlinks",0);if(!rc)rc=git_config_set_bool(cfg,"core.ignorecase",1);}return result(rc);
 }
 int clone(const std::string& url,const std::string& destination,const std::string& credentialFile,unsigned depth){std::string path;if(!logical(destination,path))return fail("Chemin Git invalide.");if(exists(root+path))return fail("Le dossier de destination doit être nouveau.");
  if(!allowedURL(url))return fail("Git accepte uniquement une adresse HTTPS sans identifiants.");networkOrigin=origin(url);if(!loadCredential(credentialFile))return fail("Fichier d'identifiants Git invalide.");networking=true;
#ifndef TAB5_GIT_HOST
  Tab5NetworkLease network("git",1500,[](void* p){return static_cast<Session*>(p)->stopped();},this);if(!network)return fail("Réseau indisponible.");if(time(nullptr)<1700000000)return fail("Réglez la date avant une connexion sécurisée.");
#endif
  const std::string temporary=root+path+".git-import";if(exists(temporary))return fail("Un import Git incomplet existe déjà.");
  if(!Tab5GitPort::project(temporary))return fail("Une récupération Git est nécessaire.");
  git_clone_options opts=GIT_CLONE_OPTIONS_INIT;opts.fetch_opts.callbacks=callbacks();opts.fetch_opts.depth=depth;opts.fetch_opts.download_tags=GIT_REMOTE_DOWNLOAD_TAGS_NONE;opts.fetch_opts.follow_redirects=GIT_REMOTE_REDIRECT_NONE;opts.fetch_opts.proxy_opts.type=GIT_PROXY_NONE;checkoutOptions(opts.checkout_opts);
  Tab5GitPort::phase("clone");int rc=git_clone(repo.put(),url.c_str(),temporary.c_str(),&opts);if(rc<0)return result(rc);
  Tab5GitPort::phase("clone-config");
  {Config cfg;if(git_repository_config(cfg.put(),repo)<0)return result(-1);
   if(git_config_set_bool(cfg,"core.filemode",0)<0||git_config_set_bool(cfg,"core.symlinks",0)<0||git_config_set_bool(cfg,"core.ignorecase",1)<0)return result(-1);
   if(!usernameSecret.empty()&&(git_config_set_string(cfg,"tab5.usernameSecret",usernameSecret.c_str())<0||git_config_set_string(cfg,"tab5.tokenSecret",tokenSecret.c_str())<0))return result(-1);
   if(!credentialFile.empty()){std::string logicalFile;if(!logical(credentialFile,logicalFile))return 2;if(git_config_set_string(cfg,"tab5.credentialFile",logicalFile.c_str())<0)return result(-1);}}
  Tab5GitPort::phase("clone-publish");repo.reset();if(stopped())return 130;
  if(rename(temporary.c_str(),(root+path).c_str()))return fail("Impossible de terminer l'import Git.");return 0;
 }
};
}
bool busy(){return occupied.load();}
MemoryStats memoryStats(){return {current.load(),peak.load(),MemoryLimit,reclaimedBlocks,reclaimedBytes};}
// Syntax/help never enters libgit2, initializes TLS, touches a volume or
// requests a 40 KiB worker stack. In particular `git` is a useful command.
static int immediate(const std::vector<std::string>& argv,const Output& output,const Translate& translate){
 if(argv.empty()||argv[0]!="git")return -1;
 if(argv.size()==2&&argv[1]=="--version"){const char* version="libgit2 1.9.7 / Tab5OS\n";if(output)output(version,strlen(version));return 0;}
 if(argv.size()==1||(argv.size()==2&&(argv[1]=="--help"||argv[1]=="help"||argv[1]=="-h"))){
  const char* key="Git : cloner un dépôt, enregistrer les modifications et synchroniser.";
  std::string line=translate?translate(key):key;line+='\n';if(output){output(line.data(),line.size());const char* help="git clone <https://...> <dossier>\ngit init [dossier]\ngit status | add -A | commit -m \"message\" | log | diff\ngit fetch | pull --ff-only | push\ngit config user.name | user.email | tab5.usernameSecret | tab5.tokenSecret\n";output(help,strlen(help));}return 0;
 }
 return -2;
}
static int executeSession(const std::string& root,const std::string& cwd,const std::vector<std::string>& argv,Output output,Cancel cancel,Translate translate,Output diagnostics,Observer observer,Tab5Resources::Reservation* startupAdmission){
 peak=current.load();
 const size_t commandIndex=argv.size()>1&&argv[1]=="-C"?3:1;
 if(commandIndex>=argv.size())return 2;
 const auto& operation=argv[commandIndex];
 const bool networkOperation=operation=="clone"||operation=="fetch"||operation=="pull"||operation=="push";
 Tab5GitPort::networking(networkOperation);
#ifdef TAB5_GIT_HOST
 const char* injected=std::getenv("TAB5_GIT_FAIL_ALLOC");failAllocation=injected?std::strtol(injected,nullptr,10):-1;allocationAttempts=0;
#endif
#ifndef TAB5_GIT_HOST
 Tab5StorageUse storage;if(!storage){const std::string text=translate?translate("Stockage indisponible."):"Stockage indisponible.";diagnostics(text.data(),text.size());return 1;}
 // TLS and untracked adapter buffers need startup headroom. The bounded
 // libgit2 arena is admitted in batches as allocations occur, not as an 8 MiB
 // minimum before even a small repository can start.
 const auto budget=startupBudget(networkOperation);
 Tab5Resources::AdmissionReport admission;
 auto memory=startupAdmission&&*startupAdmission?std::move(*startupAdmission):tab5ReserveMemory(Tab5Resources::Owner::Tools,budget,"git",&admission);if(!memory){const char* key="Mémoire insuffisante pour démarrer Git.";std::string text=translate?translate(key):key;text+='\n';char detail[512];Tab5Resources::describeAdmission(admission,detail,sizeof(detail));text+=detail;text+='\n';diagnostics(text.data(),text.size());return 1;}
 AllocationBudget batches;allocationBudget=&batches;allocationDenied=false;
 struct ReservationGuard{Output& diagnostics;~ReservationGuard(){
  if(allocationDenied&&allocationBudget){char detail[512];Tab5Resources::describeAdmission(allocationFailure,detail,sizeof(detail));diagnostics(detail,strlen(detail));diagnostics("\n",1);}
  allocationBudget=nullptr;
 }} reservationGuard{diagnostics};
#endif
 git_allocator allocator{allocate,resize,release};if(git_libgit2_opts(GIT_OPT_SET_ALLOCATOR,&allocator)<0)return 1;
 Tab5GitPort::phase(networkOperation?"init-tls":"init-local");
 int init=git_libgit2_init();struct Shutdown{~Shutdown(){
  Tab5GitPort::phase("shutdown");
  // No repository/config/output object survives Session. Once global shutdown
  // reaches zero, the session arena can reclaim orphaned blocks from upstream
  // allocation-failure paths as well. Successful operations may not hide them.
  if(git_libgit2_shutdown()==0)while(allocations){++reclaimedBlocks;reclaimedBytes+=allocations->size+sizeof(Allocation);
#ifdef TAB5_GIT_HOST
   if(std::getenv("TAB5_GIT_TRACE_ALLOC"))std::fprintf(stderr,"ORPHAN %zu\n",allocations->size);
#endif
   release(allocations+1);}
 }} shutdown;if(init<0)return 1;
 const bool configured=
  git_libgit2_opts(GIT_OPT_SET_MWINDOW_SIZE,size_t(128*1024))==0&&
  git_libgit2_opts(GIT_OPT_SET_MWINDOW_MAPPED_LIMIT,size_t(256*1024))==0&&
  git_libgit2_opts(GIT_OPT_SET_MWINDOW_FILE_LIMIT,size_t(4))==0&&
  git_libgit2_opts(GIT_OPT_SET_CACHE_MAX_SIZE,ssize_t(256*1024))==0&&
  git_libgit2_opts(GIT_OPT_SET_PACK_MAX_OBJECTS,size_t(32768))==0&&
  git_libgit2_opts(GIT_OPT_ENABLE_STRICT_HASH_VERIFICATION,1)==0&&
  git_libgit2_opts(GIT_OPT_ENABLE_FSYNC_GITDIR,1)==0&&
  git_libgit2_opts(GIT_OPT_SET_SERVER_CONNECT_TIMEOUT,5000)==0&&
  git_libgit2_opts(GIT_OPT_SET_SERVER_TIMEOUT,10000)==0&&
  git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH,GIT_CONFIG_LEVEL_SYSTEM,"")==0&&
  git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH,GIT_CONFIG_LEVEL_GLOBAL,"")==0&&
  git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH,GIT_CONFIG_LEVEL_XDG,"")==0;
 Session s;s.root=root;s.cwd=cwd;s.out=output;s.err=diagnostics;s.cancel=cancel;s.translate=translate;s.observer=observer;
 if(!configured)return s.result(-1);
#ifdef TAB5_GIT_HOST
 if(const char* ca=std::getenv("TAB5_GIT_TEST_CA"))if(git_libgit2_opts(GIT_OPT_SET_SSL_CERT_LOCATIONS,ca,nullptr)<0)return s.result(-1);
#endif
 Tab5GitPort::cancellation([&s]{return s.stopped();});
 size_t n=1;if(n<argv.size()&&argv[n]=="-C"){if(n+1>=argv.size()||!s.logical(argv[n+1],s.cwd))return 2;n+=2;}
 if(n>=argv.size())return 2;const std::string& command=argv[n++];
 if(command=="--version"){s.print("libgit2 1.9.7 / Tab5OS\n");return 0;}
 if(command=="init")return n+1<argv.size()?2:s.initialize(n<argv.size()?argv[n]:".");
 if(command=="clone"){std::string credential;unsigned depth=1;while(n<argv.size()&&argv[n].compare(0,2,"--")==0){if(n+1>=argv.size())return 2;if(argv[n]=="--credential-file")credential=argv[n+1];else if(argv[n]=="--username-secret")s.usernameSecret=argv[n+1];else if(argv[n]=="--token-secret")s.tokenSecret=argv[n+1];else if(argv[n]=="--depth"){char* end;unsigned long x=strtoul(argv[n+1].c_str(),&end,10);if(*end||x>10000)return 2;depth=unsigned(x);}else return 2;n+=2;}return n+2==argv.size()?s.clone(argv[n],argv[n+1],credential,depth):2;}
 if(!s.open())return s.fail("Aucun dépôt Git compatible dans ce dossier.");
 if(command=="recover"){
  if(n+1!=argv.size()||argv[n]!="--continue")return 2;
  int rc=Tab5GitPull::resume(s.repo,[&s]{return s.stopped();});
  if(const char* message=Tab5GitPull::diagnostic(rc))return s.fail(message);return s.result(rc);
 }
 const bool readOnly=command=="status"||command=="log"||command=="diff"||
  (command=="remote"&&(n==argv.size()||(n+1==argv.size()&&argv[n]=="-v")))||
  (command=="config"&&n+1==argv.size());
 if(!readOnly&&Tab5GitPull::pending(s.repo))return s.fail("Un import Git est interrompu. Utilisez git recover --continue.");
 if(command=="status"&&n==argv.size())return s.status();
 if(command=="add")return s.add(argv,n);
 if(command=="commit")return s.commit(argv,n);
 if(command=="log"){unsigned count=20;if(n<argv.size()){if(n+2!=argv.size()||argv[n]!="-n")return 2;char* end;unsigned long x=strtoul(argv[n+1].c_str(),&end,10);if(*end||x>1000)return 2;count=unsigned(x);}return s.log(count);}
 if(command=="diff"){bool cached=false;if(n<argv.size()&&argv[n]=="--cached"){cached=true;++n;}if(n==argv.size())return s.diff(cached);return n+2==argv.size()&&argv[n]=="--"?s.diff(cached,&argv[n+1]):2;}
 if(command=="remote")return s.remote(argv,n);
 if(command=="config")return s.setConfig(argv,n);
 if(command=="fetch"||command=="push")return n+1<argv.size()?2:s.transfer(command,n<argv.size()?argv[n]:"origin");
 if(command=="pull"){if(n<argv.size()&&argv[n]=="--ff-only")++n;return n+1<argv.size()?2:s.transfer(command,n<argv.size()?argv[n]:"origin");}
 return 2;
}
int run(const std::string& root,const std::string& cwd,const std::vector<std::string>& argv,Output output,Cancel cancel,Translate translate,Output diagnostics,Observer observer,Tab5Resources::Reservation* startupAdmission){
#ifndef TAB5_GIT_HOST
 // Consume before any early return (busy, syntax/help, cancellation). The
 // caller-owned handoff is empty before a second launch can reuse it.
 auto admitted=startupAdmission?std::move(*startupAdmission):Tab5Resources::Reservation{};
 startupAdmission=&admitted;
#else
 (void)startupAdmission;
#endif
 if(const int quick=immediate(argv,output,translate);quick!=-2)return quick;
 if(argv.empty()||argv[0]!="git")return -1;if(!diagnostics)diagnostics=output;
 if(active.test_and_set()){std::string text=translate?translate("Une opération Git est déjà en cours."):"Une opération Git est déjà en cours.";text+='\n';diagnostics(text.data(),text.size());return 1;}
 struct Guard{Guard(){occupied=true;Tab5GitPort::phase("admission");}~Guard(){Tab5GitPort::clear();Tab5GitPort::idle();occupied=false;active.clear();}} guard;
 reclaimedBlocks=reclaimedBytes=0;
 const int result=executeSession(root,cwd,argv,output,cancel,translate,diagnostics,observer,startupAdmission);
 if(result==0&&(reclaimedBlocks||current.load())){const char* key="Opération Git impossible.";std::string text=translate?translate(key):key;text+='\n';diagnostics(text.data(),text.size());return 1;}
 return result;
}
int runIsolated(const std::string& root,const std::string& cwd,const std::vector<std::string>& argv,Output out,Cancel cancel,Translate translate,Output err){
 if(const int quick=immediate(argv,out,translate);quick!=-2)return quick;
 if(argv.empty()||argv[0]!="git")return -1;
#ifdef TAB5_GIT_HOST
 return run(root,cwd,argv,out,cancel,translate,err);
#else
 if(!err)err=out;
 struct Job {const std::string& root;const std::string& cwd;const std::vector<std::string>& args;Output out;Cancel cancel;Translate translate;Output err;Tab5Resources::Reservation startup;int result=1;std::atomic<bool> finished{false};};
 Job job{root,cwd,argv,out,cancel,translate,err};
 const auto worker=[](void* pointer){auto* job=static_cast<Job*>(pointer);
  job->result=run(job->root,job->cwd,job->args,job->out,job->cancel,job->translate,job->err,{},&job->startup);
  tab5TaskCheckpoint("git");job->finished.store(true,std::memory_order_release);
  // This is the final access to caller-owned Job. RTOS stack reclamation may
  // occur later; the admission manager observes its real allocation meanwhile.
  vTaskDelete(nullptr);
 };
 Tab5Resources::AdmissionReport admission;
 job.startup=reserveStartup(argv,&admission);
 if(!job.startup||tab5CreateTask(worker,"tab5_git_terminal",WorkerStackBytes,&job,1,nullptr,0,&admission)!=pdPASS){const char* key=admission.reason==Tab5Resources::Denial::None?"Impossible de créer la tâche Git.":"Mémoire insuffisante pour démarrer Git.";std::string text=translate?translate(key):key;text+='\n';if(admission.reason!=Tab5Resources::Denial::None){char detail[512];Tab5Resources::describeAdmission(admission,detail,sizeof(detail));text+=detail;text+='\n';}err(text.data(),text.size());return 1;}
 while(!job.finished.load(std::memory_order_acquire))vTaskDelay(1);
 return job.result;
#endif
}
}
