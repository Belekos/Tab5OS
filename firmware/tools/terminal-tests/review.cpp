#include "ShellPlatform.h"
#include "Tab5Shell.cpp"
#include <iostream>
static unsigned checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n";}}while(0)
int main(int argc,char** argv){
 _set_fmode(_O_BINARY);if(argc!=2)return 2;std::filesystem::path root=argv[1];std::filesystem::create_directories(root/"Documents");
 std::string output;unsigned ticks=0,cancelAt=0;Tab5Shell::Engine shell(root.string(),[&](const char* p,size_t n){output.append(p,n);},[&](){return cancelAt&&++ticks>=cancelAt;});
 auto run=[&](const char* text){output.clear();ticks=0;return shell.run(text);};
 auto contents=[&](const char* path){std::ifstream file(root/path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(file),{});};
 CHECK(!run("echo \"$(printf 'a; touch sentinel')\"")&&output=="a; touch sentinel\n"&&!std::filesystem::exists(root/"sentinel"));
 CHECK(!run("echo $(printf 'a|cat') | cat")&&output=="a|cat\n");
 CHECK(!run("echo $(printf 'safe > sentinel')")&&output=="safe > sentinel\n"&&!std::filesystem::exists(root/"sentinel"));
 CHECK(!run("$(printf echo) $(printf 'left; right')")&&output=="left; right\n");
 CHECK(!run("printf '[%s][%s][%s]' \"$(printf ' ')\" $(printf 'a  b')")&&output=="[ ][a][b]");
 CHECK(!run("printf '[%s][%s]' \"\" \"$(true)\"")&&output=="[][]");
 CHECK(!run("echo \\| \\> \\; | cat")&&output=="| > ;\n");
 CHECK(!run("echo '$(touch sentinel)' '`touch sentinel`'")&&!std::filesystem::exists(root/"sentinel"));
 CHECK(!run("x=$(missing); echo \"X:$x\"")&&output.find("unknown command")!=std::string::npos&&output.substr(output.size()-3)=="X:\n");
 CHECK(!run("printf '[%s][%s]' x=$(printf 'a b')")&&output=="[x=a][b]");
 CHECK(!run("echo original > saved")&&contents("saved")=="original\n");
 CHECK(run("false > saved")!=0&&contents("saved")=="original\n");
 CHECK(run("cat < missing > saved")!=0&&contents("saved")=="original\n");
 CHECK(run("printf '%q' > saved")!=0&&contents("saved")=="original\n");
 cancelAt=10;CHECK(run("seq 1 10000 > saved")==130&&contents("saved")=="original\n");cancelAt=0;
 cancelAt=10;CHECK(run("seq 1 10000 | cat > saved")==130&&contents("saved")=="original\n");cancelAt=0;
 cancelAt=10;CHECK(run("echo $(seq 1 10000) > saved")==130&&contents("saved")=="original\n");cancelAt=0;
 CHECK(!std::filesystem::exists(root/"saved.tab5-shell-part")&&!std::filesystem::exists(root/"saved.tab5-shell-previous"));
 CHECK(!run("echo \"$(echo \"$(printf 'x) y')\")\"")&&output=="x) y\n");
 CHECK(!run("echo $(printf '\"literal quotes\"')")&&output=="\"literal quotes\"\n");
 CHECK(!run("echo after | cat")&&output=="after\n");
 std::cout<<"{\"passed\":"<<(failures?"false":"true")<<",\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";return failures?1:0;
}
