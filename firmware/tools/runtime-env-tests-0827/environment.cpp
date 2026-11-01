#include "ShellPlatform.h"
#include "Tab5Shell.cpp"
#include "Tab5Environment.cpp"
#include <map>
#include <iostream>
static bool unlocked=false;static std::map<std::string,std::string> secrets;
namespace Tab5Secrets {
bool available(){return unlocked;}
bool get(const char* key,char* out,size_t cap){if(out&&cap)memset(out,0,cap);if(!unlocked||!secrets.count(key)||secrets[key].size()>=cap)return false;memcpy(out,secrets[key].c_str(),secrets[key].size()+1);return true;}
std::vector<String> names(){std::vector<String> out;if(unlocked)for(const auto& e:secrets)out.push_back(e.first);return out;}
}
namespace Tab5Terminal {static bool occupied=false;static std::string submitted;bool busy(){return occupied;}bool submit(const char* p){submitted=p;return true;}
#include "script-entrypoint.inc"
}
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
int main(int argc,char** argv){if(argc!=2)return 2;std::filesystem::path root=argv[1];std::filesystem::create_directories(root/"Scripts d'essai");
 std::ofstream(root/"Scripts d'essai/a.sh")<<"echo $PWD; echo \"$1\"; echo \"$TOKEN\"";
 std::string output;Tab5Shell::Engine shell(root.string(),[&](const char* p,size_t n){output.append(p,n);});
 shell.environment({[](const std::string& key,std::string& out){char value[513]{};int rc=tab5_env_get(key.c_str(),value,sizeof(value));if(rc>=0)out.assign(value,size_t(rc));return rc;},[](const std::string& key,const std::string& value,bool secret){return secret?tab5_env_bind(key.c_str(),value.c_str()):tab5_env_set(key.c_str(),value.c_str());},[](const std::string& key){return tab5_env_unset(key.c_str());},[]{return Tab5Environment::names();}});
 auto run=[&](const char* line){output.clear();return shell.run(line);};char value[513];
 CHECK(!run("export PUBLIC=hello; echo $PUBLIC")&&output=="hello\n");
 CHECK(!run("set LOCAL world; echo ${LOCAL}")&&output=="world\n");
 secrets["TOKEN"]="test-secret a$'\"b";CHECK(!tab5_env_bind("TOKEN","TOKEN"));
 CHECK(run("echo \"$TOKEN\"")!=0&&output.find("test-secret")==std::string::npos);
 CHECK(run("echo $TOKEN")!=0);CHECK(!run("echo '$TOKEN'")&&output=="$TOKEN\n");
 CHECK(!run("echo $TOKEN || echo recovered")&&output.find("recovered")!=std::string::npos);
 unlocked=true;CHECK(tab5_env_import_secrets()==1);CHECK(!run("echo \"${TOKEN}\"")&&output==secrets["TOKEN"]+"\n");
 CHECK(!run("env")&&output.find("TOKEN")!=std::string::npos&&output.find("test-secret")==std::string::npos&&output.find("hello")==std::string::npos);
 CHECK(tab5_env_set("TOKEN","shadow")==-EACCES);CHECK(!run("set TOKEN shadow; echo \"$TOKEN\"")&&output==secrets["TOKEN"]+"\n");
 CHECK(!run("export --secret ALIAS TOKEN; echo \"$ALIAS\"")&&output==secrets["TOKEN"]+"\n");
 CHECK(!Tab5Terminal::runScript("/Scripts/a.py"));CHECK(Tab5Terminal::runScript("/Scripts d'essai/a.sh"));CHECK(!run(Tab5Terminal::submitted.c_str())&&output=="/Scripts d'essai\n\n"+secrets["TOKEN"]+"\n");
 CHECK(!run("sh a.sh parameter")&&output=="/Scripts d'essai\nparameter\n"+secrets["TOKEN"]+"\n");
 CHECK(Tab5Terminal::runScript("usb:/Scripts/a.SH")&&Tab5Terminal::submitted=="cd '/USB/Scripts' && sh '/USB/Scripts/a.SH'");Tab5Terminal::occupied=true;CHECK(!Tab5Terminal::runScript("/a.sh"));Tab5Terminal::occupied=false;
 for(int i=0;i<1000;++i){unlocked=false;memset(value,'x',sizeof(value));CHECK(tab5_env_get("ALIAS",value,sizeof(value))==-EACCES&&value[0]==0);CHECK(!run("echo $PUBLIC")&&output=="hello\n");unlocked=true;CHECK(tab5_env_get("ALIAS",value,sizeof(value))==int(secrets["TOKEN"].size()));}
 secrets.erase("TOKEN");CHECK(tab5_env_get("TOKEN",value,sizeof(value))<0&&value[0]==0);CHECK(run("echo $TOKEN")!=0&&output.find("shadow")==std::string::npos);
 const char set[]="LANG\0en-GB\0";CHECK(!tab5_env_call(108,0,set,sizeof(set)-1,nullptr,0));CHECK(tab5_env_call(107,0,"LANG",5,value,sizeof(value))==5&&!strcmp(value,"en-GB"));
 const char bind[]="GIT_TOKEN\0TOKEN\0";CHECK(!tab5_env_call(108,1,bind,sizeof(bind)-1,nullptr,0));CHECK(tab5_env_call(107,0,"GIT_TOKEN",10,value,sizeof(value))<0);CHECK(tab5_env_call(109,1,nullptr,0,value,sizeof(value))>0&&strstr(value,"GIT_TOKEN"));
 CHECK(tab5_env_call(107,0,"PUBLIC",7,value,4097)==-EINVAL);
 CHECK(tab5_env_call(108,0,"X\0bad\0extra\0",12,nullptr,0)==-EINVAL);CHECK(tab5_env_call(107,0,"LANG",4,value,sizeof(value))==-EINVAL);CHECK(tab5_env_call(109,0,"LANG",5,nullptr,0)==0);CHECK(tab5_env_get("LANG",value,sizeof(value))==-ENOENT);
 CHECK(tab5_env_set("1BAD","x")==-EINVAL);CHECK(tab5_env_set(std::string(49,'A').c_str(),"x")==-EINVAL);CHECK(tab5_env_set("BIG",std::string(513,'x').c_str())==-EINVAL);
 CHECK(tab5_env_set("EMPTY","")==0&&tab5_env_get("EMPTY",value,sizeof(value))==0);CHECK(tab5_env_get("PUBLIC",value,2)==-ENOSPC&&value[0]==0);
 for(const char* line:{"TOKEN=secret","export TOKEN=secret","set TOKEN secret","\"set\" TOKEN secret","\'export\' TOKEN","echo a=b","curl -u user:password url"})CHECK(Tab5Environment::privateCommand(line));
 for(const char* line:{"echo $TOKEN","env","git config tab5.tokenSecret TOKEN","sh '/Scripts/a.sh'"})CHECK(!Tab5Environment::privateCommand(line));
 CHECK(!run("unset ALIAS")&&tab5_env_get("ALIAS",value,sizeof(value))==-ENOENT);Tab5Environment::clear();CHECK(Tab5Environment::names().empty());
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"lockCycles\":1000}\n";return 0;
}
