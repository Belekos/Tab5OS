#include "ShellPlatform.h"
#include "Tab5Shell.cpp"
#include <cassert>
static std::filesystem::path root;
static void put(const char* name,const std::string& value){std::ofstream(root/name,std::ios::binary)<<value;}
static std::string get(const char* name){std::ifstream f(root/name,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
 _set_fmode(_O_BINARY);
 assert(argc==2);root=argv[1];std::filesystem::create_directories(root/"USB");std::filesystem::create_directories(root/"Documents");
 put("Documents/one.txt","alpha\nbeta\n");put("Documents/two.txt","alpha\nBeta\n");
 std::string output;unsigned checks=0,cancelAt=0;
 Tab5Shell::Engine shell(root.string(),[&](const char* p,size_t n){output.append(p,n);},[&](){return cancelAt&&++checks>=cancelAt;});
 auto run=[&](const char* command){output.clear();checks=0;return shell.run(command);};
 assert(!run("help wget")&&output.find("--overwrite")!=std::string::npos&&output.find("usb:/")!=std::string::npos);
 assert(!run("help")&&output.find("Réseau et téléchargements")!=std::string::npos&&output.find("hexdump")!=std::string::npos&&output.size()<32768);
 assert(run("ls --invalid")==2&&output.find("ls [-a]")!=std::string::npos);
 assert(!run("cd usb:/; pwd")&&output=="/USB\n");
 assert(!run("cd sd:/Documents; pwd")&&output=="/Documents\n");
 assert(!run("tree .")&&output.find("one.txt")!=std::string::npos&&output.find("two.txt")!=std::string::npos);
 assert(!run("stat one.txt")&&output.find("11")!=std::string::npos);
 assert(!run("du .")&&output.find("22")!=std::string::npos);
 assert(!run("hexdump -n 5 one.txt")&&output.find("61 6c 70 68 61")!=std::string::npos);
 assert(run("cmp one.txt two.txt")==1&&output.find("6")!=std::string::npos);
 assert(!run("cmp one.txt one.txt")&&output.find("identiques")!=std::string::npos);
 assert(!run("basename /Documents/one.txt; dirname /Documents/one.txt")&&output=="one.txt\n/Documents\n");
 assert(!run("seq 3 5")&&output=="3\n4\n5\n");
 assert(run("seq 1 10001")==2);
 assert(run("sleep nan")==2);
 assert(!run("cp one.txt usb:/one.txt")&&get("USB/one.txt")==get("Documents/one.txt"));
 assert(!run("cp two.txt usb:/")&&get("USB/two.txt")==get("Documents/two.txt"));
 assert(!run("cp one.txt sd:/")&&get("one.txt")==get("Documents/one.txt"));
 put("Documents/moved.txt","move");assert(!run("mv moved.txt sd:/")&&get("moved.txt")=="move"&&!std::filesystem::exists(root/"Documents/moved.txt"));
 assert(run("mv usb:/ sd:/")==1&&std::filesystem::exists(root/"USB/one.txt"));
 assert(run("rm -r usb:/")==1&&std::filesystem::exists(root/"USB/one.txt"));
 assert(run("rm -r /USB/..")==1&&std::filesystem::exists(root/"USB/one.txt"));
 assert(!run("set word alpha; grep -n \"$word\" one.txt")&&output.find("/Documents/one.txt:1:alpha")!=std::string::npos);
 assert(!run("echo saved > output.txt")&&get("Documents/output.txt")=="saved\n");
 put("Documents/large",std::string(100000,'x'));put("Documents/keep","original");cancelAt=3;
 assert(run("cp large keep")!=0&&get("Documents/keep")=="original"&&!std::filesystem::exists(root/"Documents/keep.tab5-shell-part"));
 cancelAt=0;std::vector<std::string> received;shell.admin([&](const std::vector<std::string>& args,Tab5Shell::Engine::Output){received=args;return 0;});
 assert(!run("wget https://example.org/file?download=1")&&received.size()==2&&received[1]=="https://example.org/file?download=1");
 assert(!run("curl http://example.org/path/[file]*?q=1")&&received.size()==2&&received[1]=="http://example.org/path/[file]*?q=1");
 shell.storageAvailable(false);assert(!run("help ping"));assert(shell.complete("head")=="head");
 assert(!run("headers https://example.org/?x=1")&&received.size()==2&&received[1]=="https://example.org/?x=1");
 puts("PASS: actual shell help/options; file metadata, sizes, binary inspection, comparison, SD/USB paths, root protection, scripts, redirection and cancelled-copy preservation.");
}
