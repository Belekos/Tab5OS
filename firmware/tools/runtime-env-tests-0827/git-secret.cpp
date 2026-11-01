#define TAB5_GIT_HOST 1
#include "ShellPlatform.h"
#include "Tab5Git.cpp"
#include "Tab5GitPort.cpp"
#include <iostream>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
int main(int argc,char** argv){if(argc!=2)return 2;bool unlocked=false;unsigned reads=0;
 Tab5Git::secretResolver([&](const char* name,char* out,size_t cap){++reads;if(!unlocked)return false;const char* value=!strcmp(name,"GIT_USER")?"fixture-user":!strcmp(name,"GIT_TOKEN")?"fixture-secret":nullptr;if(!value||strlen(value)>=cap)return false;strcpy(out,value);return true;});
 CHECK(git_libgit2_init()>0);{Tab5Git::Session s;s.networkOrigin="https://example.invalid";s.usernameSecret="GIT_USER";s.tokenSecret="GIT_TOKEN";git_credential* out=nullptr;
 CHECK(Tab5Git::Session::credentials(&out,"https://example.invalid/repo",nullptr,GIT_CREDENTIAL_USERPASS_PLAINTEXT,&s)==GIT_EAUTH&&!out);unlocked=true;s.credentialAttempts=0;
 CHECK(!Tab5Git::Session::credentials(&out,"https://example.invalid/repo",nullptr,GIT_CREDENTIAL_USERPASS_PLAINTEXT,&s)&&out);CHECK(!strcmp(git_credential_get_username(out),"fixture-user"));git_credential_free(out);out=nullptr;
 unsigned before=reads;s.credentialAttempts=0;CHECK(Tab5Git::Session::credentials(&out,"https://attacker.invalid/repo",nullptr,GIT_CREDENTIAL_USERPASS_PLAINTEXT,&s)==GIT_EAUTH&&reads==before);
 unlocked=false;s.credentialAttempts=0;CHECK(Tab5Git::Session::credentials(&out,"https://example.invalid/repo",nullptr,GIT_CREDENTIAL_USERPASS_PLAINTEXT,&s)==GIT_EAUTH&&!out);CHECK(!s.username[0]&&!s.token[0]);
 }git_libgit2_shutdown();
 std::filesystem::create_directories(argv[1]);std::string output;
 auto run=[&](std::vector<std::string> args){output.clear();int rc=Tab5Git::run(argv[1],"/",args,[&](const char* p,size_t n){output.append(p,n);});if(rc)std::cerr<<"Git rc="<<rc<<" "<<output;return rc;};
 CHECK(!run({"git","init","project"}));CHECK(!run({"git","-C","/project","config","tab5.usernameSecret","GIT_USER"}));CHECK(!run({"git","-C","/project","config","tab5.tokenSecret","GIT_TOKEN"}));CHECK(!run({"git","-C","/project","config","tab5.tokenSecret"})&&output=="GIT_TOKEN\n");
 CHECK(run({"git","-C","/project","config","tab5.tokenSecret","not a secret name"})==2);CHECK(!run({"git","-C","/project","config","--unset","tab5.tokenSecret"}));CHECK(!run({"git","-C","/project","config","--unset","tab5.tokenSecret"}));
 std::ifstream f(std::filesystem::path(argv[1])/"project/.git/config");std::string config((std::istreambuf_iterator<char>(f)),{});CHECK(config.find("fixture-secret")==std::string::npos&&config.find("fixture-user")==std::string::npos);CHECK(Tab5Git::memoryStats().current==0);
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"secretReads\":"<<reads<<"}\n";
}
