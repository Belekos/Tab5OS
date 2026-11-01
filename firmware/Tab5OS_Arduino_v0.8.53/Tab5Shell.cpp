#include "Tab5Shell.h"
#include "Tab5ShellBuffer.h"
#include "Tab5FilePolicy.h"
#include "Tab5TerminalPath.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#ifndef ARDUINO
#include <sys/statvfs.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <ctime>
#include <cstdint>
#include <cmath>
#include "Tab5ShellHelp.h"
namespace Tab5Shell {
namespace {
struct File {FILE* f=nullptr;explicit File(const std::string& p,const char* m){f=fopen(p.c_str(),m);}~File(){if(f)fclose(f);}bool close(){if(!f)return true;FILE* p=f;f=nullptr;return fclose(p)==0;}};
struct Dir {DIR* d=nullptr;explicit Dir(const std::string& p){d=opendir(p.c_str());}~Dir(){if(d)closedir(d);}};
bool name(const std::string& n){if(n.empty()||n.size()>48||!(isalpha((unsigned char)n[0])||n[0]=='_'))return false;for(unsigned char c:n)if(!isalnum(c)&&c!='_')return false;return true;}
// Find a whole substitution before the outer lexer sees its spaces/operators.
bool substitution(const std::string& text,size_t start,size_t& end,std::string& body,unsigned depth=0){
 if(depth>=4)return false;const bool tick=text[start]=='`';const size_t first=start+(tick?1:2);char quote=0;
 for(size_t i=first;i<text.size();++i){char c=text[i];
  if(c=='\\'&&quote!='\''){if(i+1==text.size())return false;++i;continue;}
  if(tick&&c=='`'){end=i;body.clear();for(size_t j=first;j<i;++j){if(text[j]=='\\'&&j+1<i&&(text[j+1]=='`'||text[j+1]=='$'||text[j+1]=='\\'))++j;body+=text[j];}return true;}
  if(c=='\''||c=='"'){if(!quote){quote=c;continue;}if(quote==c){quote=0;continue;}}
  if(quote=='\'')continue;
  if(c=='`'||(c=='$'&&i+1<text.size()&&text[i+1]=='(')){
   size_t nested;std::string ignored;if(!substitution(text,i,nested,ignored,depth+1))return false;i=nested;continue;
  }
  if(!tick&&!quote&&c==')'){end=i;body=text.substr(first,i-first);return true;}
 }
 return false;
}
const char* commands[]={"help","pwd","cd","ls","cat","head","tail","wc","grep","find","mkdir","rmdir","touch","cp","mv","rm","echo","printf","clear","set","export","unset","env","test","true","false","source","sh","date","df","memory","uptime","wifi","netinfo","dns","ping","tcp","sha256","sleep","tree","stat","du","hexdump","cmp","basename","dirname","seq","ports","headers","wget","curl","nslookup","ifconfig","ip","nc","ssh","sftp","git","tasks","serial",nullptr};
}
Engine::Engine(std::string root,Output output,Cancel cancel):root_(std::move(root)),output_(output),errors_(output),cancel_(cancel){}
bool Engine::stopped(){return outputFailed_||(cancel_&&cancel_());}
int Engine::error(const std::string& msg){std::string text=tr("error: ")+tr(msg.c_str())+"\n";errors_(text.data(),text.size());return 1;}
const char* Engine::help(const char* command){
 const auto* entry=command?helpEntry(command):nullptr;
 return entry?entry->description:"help [commande]";
}
int Engine::printHelp(const char* command){
 if(command&&*command){const auto* e=helpEntry(command);if(!e)return error(tr("unknown command: ")+command);
  out(std::string(e->syntax)+"\n\n"+tr(e->description)+"\n\n"+tr("Exemple : ")+e->example+"\n");
  if(!strcmp(command,"git"))out(tr("recover --continue : reprendre explicitement un import interrompu ; refuse d'écraser les modifications locales.")+"\n");return 0;}
 out(tr("Terminal — aide")+"\n"+tr("help commande : paramètres et exemple. commande --help fonctionne aussi.")+"\n");
 for(const auto& e:helpEntries){
  if(!strcmp(e.command,"help"))out("\n"+tr("Fichiers et dossiers")+"\n");
  if(!strcmp(e.command,"echo"))out("\n"+tr("Texte et scripts")+"\n");
  if(!strcmp(e.command,"date"))out("\n"+tr("Système")+"\n");
  if(!strcmp(e.command,"wifi"))out("\n"+tr("Réseau et téléchargements")+"\n");
  out(std::string(e.command)+"  "+tr(e.description)+"\n");
 }
 out("\n"+tr("Chemins : /Documents sur SD, /USB/Documents sur USB ; téléchargements : sd:/ ou usb:/.")+"\n");
 out(tr("Placez les espaces entre guillemets. Motifs : * ? [abc]. Variables : $nom ${nom} $PWD $?.")+"\n");
 out(tr("Scripts : ; sépare les commandes, && continue si réussite, || si échec. > remplace un fichier, >> ajoute du texte.")+"\n");
 out(tr("Édition : Ctrl+A/C/X/V/Z/Y ; Ctrl+C copie la sélection, sinon interrompt. Haut/Bas : historique ; Tab : complétion.")+"\n");
 out(tr("Tubes : commande | filtre. Substitutions : $(commande) ou `commande`. Sans fichier, cat/head/tail/grep/wc lisent le tube ; < lit un fichier.")+"\n");
 out(tr("Limites : 16 commandes par tube, 256 Kio entre commandes ; substitutions : 4 niveaux et 8 Kio. Les commandes du tube sont exécutées successivement ; le statut est celui de la dernière.")+"\n");
 out(tr("Les guillemets doubles conservent les espaces des substitutions ; les apostrophes les désactivent. Les tâches en arrière-plan & ne sont pas disponibles.")+"\n");return 0;
}
bool Engine::lex(const std::string& text,std::vector<Token>& out){
 Token word;char quote=0;bool escape=false,has=false;
 auto flush=[&]{if(has){out.push_back(word);word={};has=false;}};
 for(size_t i=0;i<text.size();++i){char c=text[i];if(escape){word.raw+=c;escape=false;continue;}
  if(c=='\\'&&quote!='\''){has=true;word.raw+=c;escape=true;continue;}
  if(quote!='\''&&(c=='`'||(c=='$'&&i+1<text.size()&&text[i+1]=='('))){size_t end;std::string body;if(!substitution(text,i,end,body)){error("invalid command substitution");return false;}has=true;word.raw+=text.substr(i,end-i+1);i=end;continue;}
  if(quote){word.raw+=c;if(c==quote)quote=0;continue;}
  if(c=='\''||c=='"'){quote=c;has=true;word.raw+=c;continue;}
  if(c=='#'&&!has){while(i<text.size()&&text[i]!='\n')++i;if(i==text.size())break;c='\n';}
  if(c==';'||c=='\n'||c=='>'||c=='&'||c=='|'||c=='<'){
   flush();std::string op(1,c=='\n'?';':c);if(i+1<text.size()&&text[i+1]==c&&(c=='>'||c=='&'||c=='|')){op+=c;++i;}
   if(op=="&"){error(tr("unsupported operator: ")+op);return false;}out.push_back({op,true});
  }else if(isspace((unsigned char)c))flush();else{has=true;word.raw+=c;}
  if(out.size()>4096||word.raw.size()>4096){error("command too long");return false;}
 }
 if(quote||escape){error("unfinished quote or escape");return false;}flush();return true;
}
std::string Engine::value(const std::string& n)const{if(n=="PWD")return cwd_;if(n=="?")return std::to_string(last_);if(environment_.get&&name(n)){std::string v;int rc=environment_.get(n,v);if(rc>=0)return v;if(rc!=-ENOENT){environmentFailed_=true;return "";}}for(const auto& p:variables_)if(p.first==n)return p.second->data();return "";}
bool Engine::variable(const std::string& n,const std::string& v){if(v.size()>2048||n=="PWD"||n=="?")return false;auto data=std::make_shared<Buffer>(2048);if(!data->append(v.data(),v.size()))return false;for(auto& p:variables_)if(p.first==n){p.second=std::move(data);return true;}if(variables_.size()==32)return false;variables_.push_back({n,std::move(data)});return true;}
bool Engine::path(const std::string& input,std::string& result,bool write){
 return storageAvailable_&&Tab5TerminalPath::logical(input,cwd_,result,write);
}
bool Engine::substitute(const std::string& script,std::string& result){
 if(++substitutions_>4){--substitutions_;error("maximum substitution depth is 4");return false;}
 auto savedOutput=output_;auto savedInput=input_;auto savedVariables=variables_;auto savedCwd=cwd_;const int savedLast=last_;
 const bool savedFailure=outputFailed_;Buffer captured(8192);bool failed=false,binary=false;
 output_=[&](const char* p,size_t n){if(!failed){binary=memchr(p,0,n)!=nullptr;if(binary||!captured.append(p,n)){failed=true;outputFailed_=true;}}};
 // A substitution has its own shell state, but reads the same input stream.
 std::vector<Token> tokens;int rc=lex(script,tokens)?block(tokens,0,tokens.size()):2;
 output_=savedOutput;input_=savedInput;variables_=std::move(savedVariables);cwd_=std::move(savedCwd);last_=savedLast;outputFailed_=savedFailure;--substitutions_;
 if(failed){error(binary?tr("binary data: ")+"$(...)":tr("substitution exceeds 8 KiB or memory unavailable"));return false;}
 if(rc==130)return false;captured.trimNewlines();result.assign(captured.data(),captured.size());substitutionStatus_=rc;substituted_=true;return true;
}
bool Engine::expand(const Token& token,std::vector<std::string>& args,bool allowAssignment){
 struct Field{std::string text;bool wildcard=false,keep=false;};std::vector<Field> fields(1);char quote=0;size_t expandedBytes=0;for(const auto& a:args)expandedBytes+=a.size();
 const auto eq=token.raw.find('=');const bool assignment=allowAssignment&&eq!=std::string::npos&&name(token.raw.substr(0,eq));
 auto append=[&](const std::string& value,bool split){for(unsigned char c:value){if(split&&isspace(c)){if(!fields.back().text.empty()||fields.back().keep){if(fields.size()+args.size()>=512)return false;fields.emplace_back();}continue;}if(++expandedBytes>32768)return false;auto& f=fields.back();f.text+=char(c);if(!quote&&(c=='*'||c=='?'||c=='['))f.wildcard=true;if(f.text.size()>8192)return false;}return fields.size()<=512;};
 for(size_t i=0;i<token.raw.size();++i){char c=token.raw[i];
  if(c=='\\'&&quote!='\''){if(++i==token.raw.size())return false;char next=token.raw[i];if(quote=='"'&&next!='$'&&next!='`'&&next!='"'&&next!='\\'&&next!='\n')fields.back().text+='\\';if(next!='\n'){if(++expandedBytes>32768)return false;fields.back().text+=next;}fields.back().keep=true;continue;}
  if(c=='\''||c=='"'){if(!quote){quote=c;fields.back().keep=true;continue;}if(quote==c){quote=0;continue;}}
  if(quote!='\''&&(c=='`'||(c=='$'&&i+1<token.raw.size()&&token.raw[i+1]=='('))){size_t end;std::string script,value;if(!substitution(token.raw,i,end,script)||!substitute(script,value)||!append(value,!quote&&!assignment))return false;i=end;continue;}
  if(c=='$'&&quote!='\''){std::string key;if(i+1<token.raw.size()&&token.raw[i+1]=='{'){size_t end=token.raw.find('}',i+2);if(end==std::string::npos)return false;key=token.raw.substr(i+2,end-i-2);i=end;}else{size_t first=i+1;if(first<token.raw.size()&&(token.raw[first]=='?'||isdigit((unsigned char)token.raw[first])))++i;else while(i+1<token.raw.size()&&(isalnum((unsigned char)token.raw[i+1])||token.raw[i+1]=='_'))++i;key=token.raw.substr(first,i-first+1);}if(key.empty())fields.back().text+='$';else {auto expanded=value(key);if(environmentFailed_||!append(expanded,!quote&&!assignment))return false;}}
  else{auto& f=fields.back();if(!quote&&(c=='*'||c=='?'||c=='['))f.wildcard=true;if(++expandedBytes>32768)return false;f.text+=c;f.keep=true;}
 }
 for(const auto& f:fields){const auto& s=f.text;if(s.empty()&&!f.keep)continue;
  // URL query strings belong to the request, not the local filesystem.
  if(f.wildcard&&s.compare(0,7,"http://")!=0&&s.compare(0,8,"https://")!=0){size_t slash=s.rfind('/');std::string dir=slash==std::string::npos?".":slash==0?"/":s.substr(0,slash),pattern=slash==std::string::npos?s:s.substr(slash+1),base;
   if(!path(dir,base))return false;Dir d(disk(base));if(!d.d){error(tr("cannot list ")+base);return false;}std::vector<std::string> matches;
   while(auto* e=readdir(d.d)){if(stopped())return false;if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;if(e->d_name[0]=='.'&&pattern[0]!='.')continue;if(!fnmatch(pattern.c_str(),e->d_name,0)){if(matches.size()==256){error("more than 256 wildcard matches");return false;}expandedBytes+=strlen(e->d_name)+(slash==std::string::npos?0:slash+1);if(expandedBytes>32768||args.size()+matches.size()>=512)return false;matches.push_back(slash==std::string::npos?e->d_name:s.substr(0,slash+1)+e->d_name);}}
   if(!matches.empty()){std::sort(matches.begin(),matches.end());args.insert(args.end(),matches.begin(),matches.end());continue;}
  }args.push_back(s);
 }
 return true;
}
int Engine::run(const std::string& text){environmentFailed_=false;steps_=0;depth_=0;substitutions_=0;outputFailed_=false;std::vector<Token> tokens;if(text.size()>32768||!lex(text,tokens))return last_=error("invalid command");return last_=block(tokens,0,tokens.size());}
int Engine::block(const std::vector<Token>& tokens,size_t first,size_t end){
 // Substitutions add one block each, but never reset the global script budget.
 if(++depth_>6){--depth_;return error("execution limit reached");}if(depth_>4+substitutions_){--depth_;return error("maximum script/loop depth is 4");}struct Pop{unsigned& n;~Pop(){--n;}} pop{depth_};
 int rc=0;std::string previous=";";
 for(size_t i=first;i<end;){if(stopped())return 130;if(tokens[i].op){if(tokens[i].raw!=";")return error("missing command");++i;continue;}
  bool execute=previous==";"||(previous=="&&"&&rc==0)||(previous=="||"&&rc!=0);
  if(tokens[i].raw=="for"){
   if(i+3>=end||!name(tokens[i+1].raw)||tokens[i+2].raw!="in")return error("usage: for name in values; do commands; done");
   size_t values=i+3,semi=values;while(semi<end&&!tokens[semi].op)++semi;
   if(semi+1>=end||tokens[semi].raw!=";"||tokens[semi+1].raw!="do")return error("for: expected ; do");
   size_t body=semi+2,j=body;unsigned level=1;bool start=true;
   for(;j<end;++j){if(start&&tokens[j].raw=="for")++level;if(start&&tokens[j].raw=="done"&&!--level)break;start=tokens[j].op&&tokens[j].raw==";";}
   if(j==end)return error("for: missing done");
   if(execute){std::vector<std::string> list;for(size_t v=values;v<semi;++v)if(!expand(tokens[v],list))return error("invalid loop value");
    for(const auto& v:list){if(stopped())return 130;if(++steps_>10000)return error("execution limit reached");if(!variable(tokens[i+1].raw,v))return error("variable limit reached");rc=block(tokens,body,j);last_=rc;if(rc==130||steps_>10000)return rc;}}
   i=j+1;
  }else{size_t j=i;while(j<end&&!(tokens[j].op&&(tokens[j].raw==";"||tokens[j].raw=="&&"||tokens[j].raw=="||")))++j;
   if(execute){if(++steps_>10000)return error("execution limit reached");rc=pipeline(tokens,i,j);last_=rc;}i=j;
  }
  if(i<end){if(!tokens[i].op||(tokens[i].raw!=";"&&tokens[i].raw!="&&"&&tokens[i].raw!="||"))return error("missing command separator");previous=tokens[i++].raw;}else if(previous=="&&"||previous=="||")previous=";";
 }
 return rc;
}
int Engine::pipeline(const std::vector<Token>& tokens,size_t first,size_t end){
 std::vector<size_t> bounds{first};for(size_t i=first;i<end;++i)if(tokens[i].op&&tokens[i].raw=="|"){if(i==bounds.back()||i+1==end)return error("missing pipeline command");bounds.push_back(i+1);}
 if(bounds.size()==1)return command(tokens,first,end);if(bounds.size()>16)return error("maximum pipeline length is 16");
 const auto originalOutput=output_;const auto originalInput=input_;const auto originalCwd=cwd_;const auto originalVariables=variables_;const int originalLast=last_;
 Buffer previous(256*1024);int rc=0;
 for(size_t stage=0;stage<bounds.size();++stage){if(stopped()){rc=130;break;}Buffer next(256*1024);size_t position=0;bool failed=false;
  cwd_=originalCwd;variables_=originalVariables;last_=originalLast;
  input_=stage?Input([&](char* p,size_t n){n=std::min(n,previous.size()-position);if(n)memcpy(p,previous.data()+position,n);position+=n;return n;}):originalInput;
  output_=stage+1==bounds.size()?originalOutput:Output([&](const char* p,size_t n){if(!failed&&!next.append(p,n)){failed=true;outputFailed_=true;}});
  rc=command(tokens,bounds[stage],stage+1<bounds.size()?bounds[stage+1]-1:end);
  input_=originalInput;output_=originalOutput;outputFailed_=false;
  if(failed){rc=error("pipeline exceeds 256 KiB or memory unavailable");break;}if(rc==130)break;
  previous=std::move(next);
 }
 cwd_=originalCwd;variables_=originalVariables;last_=originalLast;input_=originalInput;output_=originalOutput;return rc;
}
int Engine::command(const std::vector<Token>& tokens,size_t first,size_t end){
 environmentFailed_=false;
 std::vector<std::string> args;std::string redirect,readPath;bool append=false;substituted_=false;substitutionStatus_=0;
 const auto savedInput=input_;struct RestoreInput{Input& target;Input saved;~RestoreInput(){target=std::move(saved);}} restore{input_,savedInput};
 FILE* readFile=nullptr;struct CloseInput{FILE*& f;~CloseInput(){if(f)fclose(f);}} closeInput{readFile};
 for(size_t i=first;i<end;++i){if(tokens[i].op){if(tokens[i].raw=="<"){if(!readPath.empty()||i+1==end||tokens[i+1].op)return error("invalid redirection");std::vector<std::string> paths;if(!expand(tokens[++i],paths)||paths.size()!=1||!path(paths[0],readPath))return error("invalid path");continue;}if((tokens[i].raw!=">"&&tokens[i].raw!=">>")||!redirect.empty()||i+1==end||tokens[i+1].op)return error("invalid redirection");append=tokens[i].raw==">>";std::vector<std::string> paths;if(!expand(tokens[++i],paths)||paths.size()!=1||!path(paths[0],redirect,true))return error("invalid output path");}
  else if(!expand(tokens[i],args,args.empty())||args.size()>512)return stopped()?130:error(environmentFailed_?"Variable protégée inaccessible.":"argument limit reached");}
 if(args.empty())return error("missing command");
 if(!readPath.empty()){readFile=fopen(disk(readPath).c_str(),"rb");struct stat st{};if(!readFile||fstat(fileno(readFile),&st)||!S_ISREG(st.st_mode))return error(tr("cannot read ")+readPath);input_=[&](char* p,size_t n){size_t read=fread(p,1,n,readFile);if(ferror(readFile)){outputFailed_=true;error("read failed");}return read;};}
 if(redirect.empty())return execute(args);
 // Spool redirected output separately. A command reading its own destination keeps the original intact.
 std::string temporary=disk(redirect)+".tab5-shell-part";if(access(temporary.c_str(),F_OK)==0)return error(tr("temporary output already exists: ")+temporary);
 int fd=open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL,0666);if(fd<0)return error(tr("cannot create output: ")+redirect);
 auto previous=output_;bool failed=false;output_=[&](const char* p,size_t n){while(n&&!failed){ssize_t done=write(fd,p,n);if(done<=0)failed=true;else{p+=done;n-=done;}}};
 int rc=execute(args);output_=previous;if(fsync(fd))failed=true;if(close(fd))failed=true;
 if(stopped())rc=130;if(rc||failed){unlink(temporary.c_str());return rc?rc:error("output write failed");}
 if(append)return appendOutput(temporary,redirect);
 else {std::string backup=disk(redirect)+".tab5-shell-previous";if(access(backup.c_str(),F_OK)==0){unlink(temporary.c_str());return error(tr("previous output recovery required: ")+backup);}bool old=access(disk(redirect).c_str(),F_OK)==0;if(old&&rename(disk(redirect).c_str(),backup.c_str())){unlink(temporary.c_str());return error("cannot preserve previous output");}if(rename(temporary.c_str(),disk(redirect).c_str())){if(old)rename(backup.c_str(),disk(redirect).c_str());unlink(temporary.c_str());return error("cannot commit output");}if(old)unlink(backup.c_str());}
 return 0;
}
int Engine::appendOutput(const std::string& temporary,const std::string& redirect){
 bool failed=false;File source(temporary,"rb"),target(disk(redirect),"ab");if(!source.f||!target.f){unlink(temporary.c_str());return error("cannot append output");}char buffer[512];size_t n;while((n=fread(buffer,1,sizeof(buffer),source.f))&&!stopped())if(fwrite(buffer,1,n,target.f)!=n){failed=true;break;}failed|=ferror(source.f)||fflush(target.f)||fsync(fileno(target.f));failed|=!target.close();source.close();unlink(temporary.c_str());if(failed||stopped())return error("append incomplete");return 0;
}
int Engine::copyFile(const std::string& source,const std::string& target){
 if(source==target)return error("source and destination are identical");File in(disk(source),"rb");struct stat st{};if(!in.f||fstat(fileno(in.f),&st)||!S_ISREG(st.st_mode))return error(tr("cannot read ")+source);
 std::string tmp=disk(target)+".tab5-shell-part";int fd=open(tmp.c_str(),O_WRONLY|O_CREAT|O_EXCL,0666);if(fd<0)return error(tr("cannot create ")+target);
 char bytes[1024];bool ok=true;size_t n;while(!stopped()&&(n=fread(bytes,1,sizeof(bytes),in.f))){if(write(fd,bytes,n)!=ssize_t(n)){ok=false;break;}}if(ferror(in.f)||stopped())ok=false;if(fsync(fd))ok=false;if(close(fd))ok=false;
 if(!ok){unlink(tmp.c_str());return error("copy interrupted or failed");}
 std::string bak=disk(target)+".tab5-shell-previous";bool exists=access(disk(target).c_str(),F_OK)==0;
 if(access(bak.c_str(),F_OK)==0||(exists&&rename(disk(target).c_str(),bak.c_str()))){unlink(tmp.c_str());return error("cannot preserve destination");}
 if(rename(tmp.c_str(),disk(target).c_str())){if(exists)rename(bak.c_str(),disk(target).c_str());unlink(tmp.c_str());return error("cannot commit destination");}if(exists)unlink(bak.c_str());return 0;
}
int Engine::visit(const std::string& p,const std::string& pattern,bool remove,bool hidden,unsigned depth){
 if(remove){std::string canonical;if(!Tab5Files::canonical(p,canonical)||canonical=="/"||canonical=="/usb")return error("La racine du support ne peut pas être supprimée.");}if(depth>16)return error("directory depth limit reached");if(stopped())return 130;struct stat st{};if(stat(disk(p).c_str(),&st))return error(tr("cannot access ")+p);
 if(!remove&&fnmatch(pattern.c_str(),p.substr(p.rfind('/')+1).c_str(),0)==0)out(p+"\n");
 if(S_ISDIR(st.st_mode)){Dir dir(disk(p));if(!dir.d)return error(tr("cannot list ")+p);while(auto* e=readdir(dir.d)){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")||(!hidden&&e->d_name[0]=='.'))continue;int rc=visit((p=="/"?"":p)+"/"+e->d_name,pattern,remove,hidden,depth+1);if(rc)return rc;}}
 if(remove){if(stopped())return 130;if((S_ISDIR(st.st_mode)?rmdir(disk(p).c_str()):unlink(disk(p).c_str())))return error(tr("cannot remove ")+p);}return 0;
}
int Engine::execute(const std::vector<std::string>& args){
 if((args[0]=="sh"||args[0]=="source")&&!(args.size()>1&&(args[1]=="--help"||args[1]=="-h")))return script(args);
 return builtin(args);
}
int Engine::readScript(const std::string& p,std::string& data){
 File f(disk(p),"rb");if(!f.f)return error("cannot read script");char buf[512];size_t n;while((n=fread(buf,1,sizeof(buf),f.f))){if(data.size()+n>32768)return error("script exceeds 32 KiB");data.append(buf,n);if(stopped())return 130;}if(ferror(f.f))return error("script read failed");return 0;
}
int Engine::script(const std::vector<std::string>& a){
if(a.size()<2){error("Paramètres invalides.");printHelp(a[0].c_str());return 2;}std::string p;if(!path(a[1],p))return error("invalid script path");std::string data;int loaded=readScript(p,data);if(loaded)return loaded;std::vector<Token> tokens;if(!lex(data,tokens))return 1;auto saved=variables_;for(unsigned i=0;i<10;++i)if(!variable(std::to_string(i),i+1<a.size()?a[i+1]:"")){variables_=saved;return error("not enough variable slots for script arguments");}int rc=block(tokens,0,tokens.size());for(unsigned i=0;i<10;++i){std::string key=std::to_string(i);variables_.erase(std::remove_if(variables_.begin(),variables_.end(),[&](const auto& v){return v.first==key;}),variables_.end());for(const auto& v:saved)if(v.first==key)variables_.push_back(v);}return rc;
}
int Engine::builtin(const std::vector<std::string>& a){
 const auto& c=a[0];auto usage=[&]{error("Paramètres invalides.");printHelp(c.c_str());return 2;};
 if(c=="help")return a.size()>2?usage():printHelp(a.size()==2?a[1].c_str():nullptr);
 if(a.size()>1&&(a[1]=="--help"||a[1]=="-h"))return printHelp(c.c_str());
 if(c=="true"||c=="false")return c=="false";
 if(c=="pwd"){out(cwd_+"\n");return 0;}
 if(c=="clear"){out("\f");return 0;}
 if(c=="echo"||c=="printf"){size_t start=1;bool newline=c=="echo";if(start<a.size()&&a[start]=="-n"&&newline){newline=false;++start;}if(c=="printf"){
   if(a.size()<2)return usage();size_t arg=2;for(size_t i=0;i<a[1].size();++i){char ch=a[1][i];if(ch=='%'&&i+1<a[1].size()){char f=a[1][++i];if(f=='s'){if(arg<a.size())out(a[arg++]);}else if(f=='%')out("%");else return error("printf supports %s and %%");}else if(ch=='\\'&&i+1<a[1].size()){char f=a[1][++i];out(f=='n'?"\n":f=='t'?"\t":std::string(1,f));}else out(std::string(1,ch));}}
  else for(size_t i=start;i<a.size();++i){if(i>start)out(" ");out(a[i]);}if(newline)out("\n");return 0;}
 if(c=="set"||(a.size()==1&&c.find('=')!=std::string::npos)){size_t eq=c.find('=');std::string key=c=="set"&&a.size()==3?a[1]:eq!=std::string::npos?c.substr(0,eq):"",v=c=="set"&&a.size()==3?a[2]:eq!=std::string::npos?c.substr(eq+1):"";return name(key)&&variable(key,v)?(substituted_?substitutionStatus_:0):usage();}
 if(c=="export"){
  if(!environment_.set)return error("Environnement indisponible.");
  if(a.size()>=3&&a[1]=="--secret")return a.size()<=4&&environment_.set(a[2],a.size()==4?a[3]:a[2],true)>=0?0:error("Variable protégée inaccessible.");
  if(a.size()!=2)return usage();size_t eq=a[1].find('=');std::string key=a[1].substr(0,eq),v=eq==std::string::npos?value(key):a[1].substr(eq+1);
  return !environmentFailed_&&environment_.set(key,v,false)>=0?0:error("Variable protégée inaccessible.");
 }
 if(c=="env"){if(a.size()!=1)return usage();std::vector<std::string> keys{"PWD"};for(const auto& v:variables_)keys.push_back(v.first);if(environment_.names){auto shared=environment_.names();keys.insert(keys.end(),shared.begin(),shared.end());}std::sort(keys.begin(),keys.end());keys.erase(std::unique(keys.begin(),keys.end()),keys.end());for(const auto& key:keys)out(key+"\n");return 0;}
 if(c=="unset"){if(a.size()!=2)return usage();if(environment_.erase&&environment_.erase(a[1])<0)return error("Variable protégée inaccessible.");variables_.erase(std::remove_if(variables_.begin(),variables_.end(),[&](const auto& v){return v.first==a[1];}),variables_.end());return 0;}
 if(c=="date"){time_t now=time(nullptr);struct tm t{};localtime_r(&now,&t);char text[80];strftime(text,sizeof(text),"%Y-%m-%d %H:%M:%S %Z\n",&t);out(text);return 0;}

#ifndef ARDUINO
 if(c=="df"){struct statvfs v{};if(statvfs(root_.c_str(),&v))return error("cannot query SD space");out("bytes free: "+std::to_string(uint64_t(v.f_bavail)*v.f_frsize)+" / "+std::to_string(uint64_t(v.f_blocks)*v.f_frsize)+"\n");return 0;}
#endif
 if(c=="sleep"){if(a.size()!=2)return usage();char* end;double seconds=strtod(a[1].c_str(),&end);if(*end||!std::isfinite(seconds)||seconds<0||seconds>3600)return usage();for(unsigned i=0;i<unsigned(seconds*100);++i){if(stopped())return 130;usleep(10000);}return 0;}
 if(c=="cd"){if(a.size()>2)return usage();std::string p;if(!path(a.size()==2?a[1]:"/",p))return error("invalid path");struct stat st{};if(stat(disk(p).c_str(),&st)||!S_ISDIR(st.st_mode))return error(tr("not a directory: ")+p);cwd_=p;return 0;}
 if(c=="basename"||c=="dirname"){
  if(a.size()!=2)return usage();std::string p=a[1];while(p.size()>1&&p.back()=='/')p.pop_back();size_t split=p.rfind('/');
  out((c=="basename"?(p=="/"?p:split==std::string::npos?p:p.substr(split+1)):(split==std::string::npos?".":split==0?"/":p.substr(0,split)))+"\n");return 0;
 }
 if(c=="seq"){
  if(a.size()<2||a.size()>3)return usage();auto number=[](const std::string& s,long& value){char* end=nullptr;errno=0;value=strtol(s.c_str(),&end,10);return !s.empty()&&!*end&&!errno&&value>=-1000000000&&value<=1000000000;};
  long first=1,last;if(!number(a.back(),last)||(a.size()==3&&!number(a[1],first))||last<first||last-first>=10000)return usage();
  for(long n=first;n<=last;++n){if(stopped())return 130;out(std::to_string(n)+"\n");}return 0;
 }
 if(c=="tree"){if(a.size()>2)return usage();std::string p;if(!path(a.size()==2?a[1]:".",p))return error("invalid path");return visit(p,"*",false,true);}
 if(c=="stat"){
  if(a.size()<2)return usage();for(size_t i=1;i<a.size();++i){if(stopped())return 130;std::string p;if(!path(a[i],p))return error("invalid path");struct stat st{};if(stat(disk(p).c_str(),&st))return error(tr("cannot access ")+p);
   char date[40];struct tm dateParts{};localtime_r(&st.st_mtime,&dateParts);strftime(date,sizeof(date),"%Y-%m-%d %H:%M:%S",&dateParts);
   out(p+"\n  "+tr(S_ISDIR(st.st_mode)?"Dossier":"Fichier")+"  "+std::to_string(uint64_t(st.st_size))+" "+tr("octets")+"  "+date+"\n");}return 0;
 }
 if(c=="du"){
  if(a.size()>2)return usage();std::string root;if(!path(a.size()==2?a[1]:".",root))return error("invalid path");uint64_t bytes=0,files=0;
  std::function<int(const std::string&,unsigned)> walk=[&](const std::string& p,unsigned depth){if(stopped())return 130;if(depth>16)return error("directory depth limit reached");struct stat st{};if(stat(disk(p).c_str(),&st))return error(tr("cannot access ")+p);if(!S_ISDIR(st.st_mode)){bytes+=st.st_size;++files;return 0;}Dir dir(disk(p));if(!dir.d)return error(tr("cannot list ")+p);while(auto* e=readdir(dir.d)){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;int rc=walk((p=="/"?"":p)+"/"+e->d_name,depth+1);if(rc)return rc;}return 0;};
  int rc=walk(root,0);if(!rc)out(std::to_string(bytes)+" "+tr("octets")+"  "+std::to_string(files)+" "+tr("fichiers")+"  "+root+"\n");return rc;
 }
 if(c=="hexdump"){
  unsigned count=256,offset=0;size_t i=1;while(i<a.size()&&(a[i]=="-n"||a[i]=="-s")){bool isCount=a[i++]=="-n";if(i==a.size())return usage();char* end;errno=0;unsigned long n=strtoul(a[i++].c_str(),&end,10);if(*end||errno||n>2147483647UL||(isCount&&(!n||n>65536)))return usage();if(isCount)count=n;else offset=n;}
  if(i+1!=a.size())return usage();std::string p;if(!path(a[i],p))return error("invalid path");File file(disk(p),"rb");if(!file.f||fseek(file.f,offset,SEEK_SET))return error(tr("cannot read ")+p);
  unsigned char bytes[16];while(count&&!stopped()){size_t got=fread(bytes,1,std::min(count,16U),file.f);if(!got)break;char line[96];int at=snprintf(line,sizeof(line),"%08x  ",offset);for(unsigned n=0;n<16;++n)at+=snprintf(line+at,sizeof(line)-at,n<got?"%02x ":"   ",n<got?bytes[n]:0);line[at++]=' ';for(unsigned n=0;n<got;++n)line[at++]=bytes[n]>=32&&bytes[n]<127?bytes[n]:'.';line[at++]='\n';out(std::string(line,at));count-=got;offset+=got;}return stopped()?130:ferror(file.f)?error("read failed"):0;
 }
 if(c=="cmp"){
  if(a.size()!=3)return usage();std::string first,second;if(!path(a[1],first)||!path(a[2],second))return error("invalid path");File left(disk(first),"rb"),right(disk(second),"rb");if(!left.f||!right.f)return error("cannot read file");uint64_t offset=0;unsigned char l[512],r[512];
  while(!stopped()){size_t n=fread(l,1,sizeof(l),left.f),m=fread(r,1,sizeof(r),right.f);if(ferror(left.f)||ferror(right.f))return error("read failed");size_t i=0;while(i<std::min(n,m)&&l[i]==r[i])++i;if(i<std::min(n,m)||n!=m){out(tr("Première différence à l'octet : ")+std::to_string(offset+i)+"\n");return 1;}if(!n){out(tr("Fichiers identiques.")+"\n");return 0;}offset+=n;}return 130;
 }
 if(c=="ls"){bool all=false,longer=false;std::string p=".";for(size_t i=1;i<a.size();++i){if(a[i][0]=='-'&&a[i].size()>1){for(size_t k=1;k<a[i].size();++k){if(a[i][k]=='a')all=true;else if(a[i][k]=='l')longer=true;else return usage();}}else p=a[i];}if(!path(p,p))return error("invalid directory");Dir dir(disk(p));if(!dir.d)return error(tr("cannot list ")+p);
  while(auto* e=readdir(dir.d)){if(stopped())return 130;if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")||(!all&&e->d_name[0]=='.'))continue;std::string full=disk(p)+(p=="/"?"":"/")+e->d_name;struct stat st{};if(stat(full.c_str(),&st))return error(tr("cannot stat ")+full);if(longer)out(std::string(S_ISDIR(st.st_mode)?"d ":"- ")+std::to_string(uint64_t(st.st_size))+"\t");out(std::string(e->d_name)+(S_ISDIR(st.st_mode)?"/\n":"\n"));}return 0;}
 if(c=="test"){if(a.size()==4&&(a[2]=="="||a[2]=="!="))return (a[1]==a[3])==(a[2]=="=")?0:1;if(a.size()!=3)return usage();std::string p;if(!path(a[2],p))return 1;struct stat st{};if(stat(disk(p).c_str(),&st))return 1;if(a[1]=="-e")return 0;if(a[1]=="-f")return !S_ISREG(st.st_mode);if(a[1]=="-d")return !S_ISDIR(st.st_mode);return usage();}
 if(c=="find"){std::string p="/",pattern="*";size_t i=1;if(i<a.size()&&a[i]!="-name")p=a[i++];if(i<a.size()){if(a[i]!="-name"||i+2!=a.size())return usage();pattern=a[i+1];}if(!path(p,p))return error("invalid path");return visit(p,pattern,false,true);}
 if(c=="cp"||c=="mv"){if(a.size()!=3)return usage();std::string from,to;if(!path(a[1],from,c=="mv")||!path(a[2],to))return error("invalid path");struct stat st{};if(!stat(disk(to).c_str(),&st)&&S_ISDIR(st.st_mode))to+=(to=="/"?"":"/")+from.substr(from.rfind('/')+1);if(!path(to,to,true))return error("invalid path");if(c=="cp")return copyFile(from,to);if(access(disk(to).c_str(),F_OK)==0)return error("destination exists");return rename(disk(from).c_str(),disk(to).c_str())?error("move failed"):0;}
 if(c=="mkdir"||c=="rmdir"||c=="rm"||c=="touch"){bool recursive=false;size_t first=1;if(first<a.size()&&((c=="mkdir"&&a[first]=="-p")||(c=="rm"&&a[first]=="-r"))){recursive=true;++first;}if(first==a.size())return usage();for(size_t i=first;i<a.size();++i){std::string p;if(!path(a[i],p,true))return error("invalid path");if(stopped())return 130;
   if(c=="rm"&&recursive){int rc=visit(p,"*",true,true);if(rc)return rc;}
   else if(c=="mkdir"&&recursive){for(size_t j=1;j<=p.size();++j)if(j==p.size()||p[j]=='/'){std::string part=disk(p.substr(0,j));struct stat st{};if(mkdir(part.c_str(),0777)&&(stat(part.c_str(),&st)||!S_ISDIR(st.st_mode)))return error(tr("cannot create directory: ")+p);}}
   else if(c=="touch"){File f(disk(p),"ab");if(!f.f||!f.close())return error(tr("cannot create ")+p);}
   else if((c=="rm"?unlink(disk(p).c_str()):c=="rmdir"?rmdir(disk(p).c_str()):mkdir(disk(p).c_str(),0777)))return error(tr("cannot change ")+p+": "+strerror(errno));}return 0;}

 if(c=="cat"||c=="head"||c=="tail"||c=="wc"||c=="grep"){
  size_t first=1;unsigned limit=10;bool ignore=false,numbers=false,wcLines=false,wcWords=false,wcBytes=false;std::string needle;
  if((c=="head"||c=="tail")&&first<a.size()&&a[first]=="-n"){if(first+1>=a.size())return usage();char* end;long number=strtol(a[first+1].c_str(),&end,10);if(*end||number<1||number>500)return usage();limit=number;first+=2;}
  if(c=="wc"){while(first<a.size()&&a[first].size()>1&&a[first][0]=='-'){for(size_t j=1;j<a[first].size();++j){if(a[first][j]=='l')wcLines=true;else if(a[first][j]=='w')wcWords=true;else if(a[first][j]=='c')wcBytes=true;else return usage();}++first;}if(!wcLines&&!wcWords&&!wcBytes)wcLines=wcWords=wcBytes=true;}
  if(c=="grep"){while(first<a.size()&&a[first].size()>1&&a[first][0]=='-'){for(size_t j=1;j<a[first].size();++j){if(a[first][j]=='i')ignore=true;else if(a[first][j]=='n')numbers=true;else return usage();}++first;}if(first==a.size())return usage();needle=a[first++];if(ignore)for(char& ch:needle)ch=tolower((unsigned char)ch);}
  bool matched=false;const bool defaultInput=first==a.size();const size_t count=defaultInput?1:a.size()-first;
  for(size_t i=0;i<count;++i){bool standard=defaultInput||a[first+i]=="-";std::string p;FILE* file=nullptr;
   if(!standard){if(!path(a[first+i],p))return error("invalid path");file=fopen(disk(p).c_str(),"rb");struct stat st{};if(!file||fstat(fileno(file),&st)||!S_ISREG(st.st_mode)){if(file)fclose(file);return error(tr("cannot read ")+p);}}
   struct Close{FILE* f;~Close(){if(f)fclose(f);}} close{file};
   uint64_t bytes=0,words=0,lines=0;bool word=false;std::string line;std::vector<Buffer> tail;size_t tailBytes=0;
   auto emitLine=[&](){if(c=="head"&&lines<=limit)out(line);else if(c=="tail"){if(tail.size()==limit){tailBytes-=tail.front().size();tail.erase(tail.begin());}if(line.size()>65536-tailBytes)return false;Buffer item(line.size());if(!item.append(line.data(),line.size()))return false;tailBytes+=line.size();tail.push_back(std::move(item));}else if(c=="grep"){auto copy=line;if(ignore)for(char& x:copy)x=tolower((unsigned char)x);if(copy.find(needle)!=std::string::npos){matched=true;if(numbers)out((standard?"":p+":")+std::to_string(lines)+":");out(line);}}line.clear();return true;};
   char buf[1024];size_t n;while((n=standard?(input_?input_(buf,sizeof(buf)):0):fread(buf,1,sizeof(buf),file))){if(stopped())return 130;bytes+=n;
    if(c=="cat"){output_(buf,n);continue;}
    for(size_t j=0;j<n;++j){unsigned char ch=buf[j];if(isspace(ch))word=false;else if(!word){word=true;++words;}if(c=="wc"){if(ch=='\n')++lines;continue;}
     if(ch==0)return error(tr("binary data: ")+p);line+=char(ch);if(line.size()>8192)return error("line exceeds 8 KiB");if(ch=='\n'){++lines;if(!emitLine())return error("tail output exceeds 64 KiB; reduce -n");}
    }if(c=="head"&&lines>=limit)break;
   }if(file&&ferror(file))return error(tr("read failed: ")+p);if(stopped())return 130;
   if(!line.empty()){++lines;if(!emitLine())return error("tail output exceeds 64 KiB; reduce -n");}
   if(c=="tail")for(const auto& v:tail)output_(v.data(),v.size());if(c=="wc"){std::string result;auto add=[&](uint64_t v){if(!result.empty())result+=' ';result+=std::to_string(v);};if(wcLines)add(lines);if(wcWords)add(words);if(wcBytes)add(bytes);out(result+(standard?"":" "+p)+"\n");}
  }return c=="grep"&&!matched?1:0;
 }
 if(admin_){int rc=admin_(a,output_);if(rc!=-1)return rc==2&&c=="git"?usage():rc;}
 return error(tr("unknown command: ")+c+tr(" (help lists commands)"));
}
std::string Engine::complete(const std::string& input){
 size_t start=input.find_last_of(" \t;");start=start==std::string::npos?0:start+1;std::string word=input.substr(start),common;unsigned matches=0;
 auto add=[&](const std::string& candidate){if(candidate.compare(0,word.size(),word))return;if(!matches++)common=candidate;else{size_t n=0;while(n<common.size()&&n<candidate.size()&&common[n]==candidate[n])++n;common.resize(n);}};
 if(start==0)for(const char** c=commands;*c;++c)add(*c);
 size_t slash=word.rfind('/');std::string dir=slash==std::string::npos?".":slash==0?"/":word.substr(0,slash),base;
 if(path(dir,base)){Dir d(disk(base));unsigned scanned=0;if(d.d)while(auto* e=readdir(d.d)){if(++scanned>256)return input;if(e->d_name[0]=='.')continue;std::string entry=slash==std::string::npos?e->d_name:word.substr(0,slash+1)+e->d_name;struct stat st{};if(!stat((disk(base)+(base=="/"?"":"/")+e->d_name).c_str(),&st)&&S_ISDIR(st.st_mode))entry+='/';add(entry);}}
 return matches?input.substr(0,start)+common:input;
}
}
