#include "ShellPlatform.h"
#include "Tab5Shell.cpp"
#include "Tab5TerminalBuffer.h"
#include <cassert>
#include <crtdbg.h>
#include <iostream>
static unsigned assertions=0;
#define CHECK(x) do{++assertions;if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
int main(int argc,char** argv){
 _set_fmode(_O_BINARY);assert(argc==2);std::filesystem::path root=argv[1];std::filesystem::create_directories(root/"Documents");
 std::ofstream(root/"one",std::ios::binary)<<"alpha\nbeta\nGamma";
 std::ofstream(root/"big",std::ios::binary)<<std::string(270000,'x');
 std::ofstream(root/"binary",std::ios::binary)<<std::string("abc\0def",7);
 std::ofstream(root/"a.sh")<<"source b.sh";std::ofstream(root/"b.sh")<<"source c.sh";std::ofstream(root/"c.sh")<<"echo $(source a.sh)";
 std::string output;unsigned ticks=0,cancelAt=0;
 Tab5Shell::Engine shell(root.string(),[&](const char* p,size_t n){output.append(p,n);},[&](){return cancelAt&&++ticks>=cancelAt;});
 auto run=[&](const char* s){output.clear();ticks=0;return shell.run(s);};
 CHECK(!run("echo hello | cat")&&output=="hello\n");
 CHECK(!run("printf 'alpha\\nbeta\\nGamma' | grep -in a | head -n 2 | wc -l")&&output=="2\n");
 CHECK(!run("cat < one | tail -n 1")&&output=="Gamma");
 CHECK(!run("cat one | wc")&&output=="2 3 16\n");
 CHECK(!run("cat binary | wc -c")&&output=="7\n");
 CHECK(!run("echo pipe | cat - one")&&output=="pipe\nalpha\nbeta\nGamma");
 CHECK(!run("echo pipe > saved | wc -c")&&output=="0\n");
 CHECK(!run("echo pipe | cat > saved; cat saved")&&output=="pipe\n");
 CHECK(!run("echo pipe | cat < one")&&output=="alpha\nbeta\nGamma");
 CHECK(!run("false | true; echo $?")&&output=="0\n");
 CHECK(!run("true | false; echo $?")&&output=="1\n");
 CHECK(!run("false | true && echo yes")&&output=="yes\n");
 CHECK(!run("true | false || echo yes")&&output=="yes\n");
 CHECK(!run("echo x | grep missing; echo $?")&&output=="1\n");
 CHECK(!run("echo 'a|b' | cat")&&output=="a|b\n");
 CHECK(run("echo x |")!=0);CHECK(run("| cat")!=0);CHECK(run("echo x | | cat")!=0);
 CHECK(!run("echo $(echo hello)")&&output=="hello\n");
 CHECK(!run("echo `echo hello`")&&output=="hello\n");
 CHECK(!run("echo `echo \\`echo nested\\``")&&output=="nested\n");
 CHECK(!run("set value hello; echo ${value}-$value-${PWD}; false; echo $?")&&output=="hello-hello-/\n1\n");
 CHECK(!run("echo \"$(printf 'hello\\n\\n')\"")&&output=="hello\n");
 CHECK(!run("echo \"$(echo \"$(echo nested)\")\"")&&output=="nested\n");
 CHECK(!run("echo '$(echo unchanged)' '\\n' '\"'")&&output=="$(echo unchanged) \\n \"\n");
 CHECK(!run("echo \"\\$(echo unchanged)\"")&&output=="$(echo unchanged)\n");
 CHECK(!run("set a \"$(printf 'a\\nb')\"; echo \"$a\"")&&output=="a\nb\n");
 CHECK(!run("for a in $(printf 'a b\\nc'); do echo $a; done")&&output=="a\nb\nc\n");
 CHECK(!run("for a in \"$(printf 'a b\\nc')\"; do echo \"$a\"; done")&&output=="a b\nc\n");
 CHECK(!run("echo $(seq 1 10 | tail -n 2 | wc -l)")&&output=="2\n");
 CHECK(!run("set x parent; echo $(set x child; cd Documents; pwd); echo $x; pwd")&&output=="/Documents\nparent\n/\n");
 CHECK(!run("set x parent; set x child | cat; echo $x")&&output=="parent\n");
 CHECK(!run("cd Documents | pwd; pwd")&&output=="/\n/\n");
 CHECK(run("a=$(false)")==1);CHECK(!run("echo $?")&&output=="1\n");
 CHECK(!run("echo $(false); echo $?")&&output=="\n0\n");
 CHECK(!run("x=$(printf 'a b'); echo \"$x\"")&&output=="a b\n");
 CHECK(!run("printf '[%s][%s]' x=$(echo a b)")&&output=="[x=a][b]");
 CHECK(!run("for v in x=$(echo a b); do echo $v; done")&&output=="x=a\nb\n");
 CHECK(!run("echo $(missing) | wc -c")&&output.find("unknown command")!=std::string::npos&&output.substr(output.size()-2)=="1\n");
 CHECK(run("echo $(echo unfinished")!=0);CHECK(run("echo `echo unfinished")!=0);
 CHECK(run("echo $(cat big)")!=0&&output.find("substitution exceeds")!=std::string::npos);
 CHECK(run("cat big | wc -c")!=0&&output.find("pipeline exceeds")!=std::string::npos);
 CHECK(!run("echo recovered | wc -c")&&output=="10\n");
 CHECK(run("echo $(cat binary)")!=0);
 CHECK(!run("echo $(echo $(echo $(echo $(echo deep))))")&&output=="deep\n");
 CHECK(run("echo $(echo $(echo $(echo $(echo $(echo deep)))))")!=0);
 run("source a.sh");CHECK(output.find("maximum script/loop depth is 4")!=std::string::npos&&output.find("maximum substitution depth is 4")==std::string::npos);
 CHECK(run("echo x | cat | cat | cat | cat | cat | cat | cat | cat | cat | cat | cat | cat | cat | cat | cat | cat")!=0);
 CHECK(!run("seq 1 3 | cat >> saved; cat saved")&&output=="pipe\n1\n2\n3\n");
 cancelAt=5;CHECK(run("seq 1 10000 | cat | wc -c")==130);cancelAt=0;
 cancelAt=5;CHECK(run("echo $(seq 1 10000)")==130);cancelAt=0;
 CHECK(!run("echo after-cancel | cat")&&output=="after-cancel\n");
 CHECK(!run("for n in 1 2 3; do echo $n | wc -c; done")&&output=="2\n2\n2\n");
 CHECK(!run("echo \"$(cat one | grep beta)\" > substitution.txt; cat substitution.txt")&&output=="beta\n");
 CHECK(!run("echo \"\" $(true) end | wc -w")&&output=="1\n");
 CHECK(run("echo $(seq 1 2000)$(seq 1 2000)$(seq 1 2000)$(seq 1 2000)$(seq 1 2000)")!=0);
 shell.storageAvailable(false);CHECK(!run("echo without-SD | wc -c")&&output=="11\n");CHECK(!run("echo $(echo without-SD)")&&output=="without-SD\n");shell.storageAvailable(true);
 shell.admin([&](const std::vector<std::string>& a,Tab5Shell::Engine::Output out){if(a[0]!="network-test")return -1;out("HTTP 200\n",9);return 0;});
 CHECK(!run("network-test | grep 200")&&output=="HTTP 200\n");
 CHECK(!run("echo $(network-test | wc -l)")&&output=="1\n");
 CHECK(!run("help")&&output.find("Substitutions")!=std::string::npos);
 Tab5Terminal::OutputRing<64> ring;char snapshot[64];ring.append("before\fafter",12);ring.copy(snapshot,sizeof(snapshot));CHECK(std::string(snapshot)=="after");
 ring.clear();for(int i=0;i<10000;++i)ring.append("écrire\n",8);ring.copy(snapshot,sizeof(snapshot));CHECK((uint8_t(snapshot[0])&0xc0)!=0x80&&strlen(snapshot)<64);
 Tab5Terminal::YieldCadence cadence;unsigned yields=0;for(unsigned i=0;i<100000;++i)yields+=cadence.due(i/100);CHECK(yields==199);
 // CRT tracks ordinary allocations too, including vectors/strings and pipe buffers.
 auto cycle=[&](){Tab5Terminal::OutputRing<32768> ring;Tab5Shell::Engine transient(root.string(),[&](const char* p,size_t n){ring.append(p,n);});transient.run("set x value; for n in 1 2 3; do echo $(seq 1 100 | tail -n 5) | wc -c; done");};
 for(int i=0;i<20;++i)cycle();_CrtMemState before{},after{},delta{};_CrtMemCheckpoint(&before);
 auto begin=std::chrono::steady_clock::now();for(int i=0;i<2000;++i)cycle();auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
 _CrtMemCheckpoint(&after);_CrtMemDifference(&delta,&before,&after);CHECK(delta.lSizes[_NORMAL_BLOCK]==0&&delta.lCounts[_NORMAL_BLOCK]==0);
 // Persistent shell variables overwritten/unset must also return to the same baseline.
 auto repeat=[&](){shell.run("set x \"$(seq 1 100)\"; unset x; seq 1 500 | tail -n 2 | wc -l");output.clear();};for(int i=0;i<20;++i)repeat();_CrtMemCheckpoint(&before);for(int i=0;i<2000;++i)repeat();_CrtMemCheckpoint(&after);_CrtMemDifference(&delta,&before,&after);CHECK(delta.lSizes[_NORMAL_BLOCK]==0&&delta.lCounts[_NORMAL_BLOCK]==0);
 std::cout<<"{\"passed\":true,\"assertions\":"<<assertions<<",\"repeated_sessions\":2000,\"repeated_commands\":2000,\"retained_heap_bytes\":"<<delta.lSizes[_NORMAL_BLOCK]<<",\"session_stress_ms\":"<<ms<<",\"physical_device_tested\":false}"<<std::endl;
}
