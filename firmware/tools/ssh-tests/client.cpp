#include "SshPlatform.h"
#include "Tab5Ssh.h"
#include "Tab5SshPolicy.h"
#include <iostream>
#include <iterator>
#include <crtdbg.h>
int main(int argc,char** argv){for(int type:{_CRT_WARN,_CRT_ERROR,_CRT_ASSERT}){_CrtSetReportMode(type,_CRTDBG_MODE_FILE);_CrtSetReportFile(type,_CRTDBG_FILE_STDERR);}_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);_setmode(_fileno(stderr),_O_BINARY);WSADATA data;WSAStartup(MAKEWORD(2,2),&data);std::vector<std::string> args;for(int n=1;n<argc;++n)args.push_back(argv[n]);
 std::string input((std::istreambuf_iterator<char>(std::cin)),{});size_t pos=0;const char* cancel=std::getenv("SSH_CANCEL_MS");unsigned deadline=cancel?static_cast<unsigned>(atoi(cancel)):0;
 const char* repeated=std::getenv("SSH_REPEAT");int repeats=repeated?atoi(repeated):1;int result=0;_CrtMemState baseline{},after{};
 for(int iteration=0;iteration<repeats;++iteration){pos=0;sshHostAllocationCalls=0;result=Tab5Ssh::run(args,[](const char* p,size_t n){std::cout.write(p,n);},[&]{return deadline&&millis()>=deadline;},{},"/",[](const char* p,size_t n){std::cerr.write(p,n);},[&](char* p,size_t n){size_t amount=std::min(n,input.size()-pos);memcpy(p,input.data()+pos,amount);pos+=amount;return amount;});if(!iteration)_CrtMemCheckpoint(&baseline);}
 _CrtMemCheckpoint(&after);long retained=static_cast<long>(after.lSizes[_NORMAL_BLOCK])-static_cast<long>(baseline.lSizes[_NORMAL_BLOCK]);
 if(const char* report=std::getenv("SSH_REPORT")){std::ofstream out(report);out<<"{\"returnCode\":"<<result<<",\"retainedSessionBytes\":"<<sshHostRetained<<",\"liveAllocations\":"<<sshHostAllocations<<",\"peakSessionBytes\":"<<sshHostPeak<<",\"sessions\":"<<sshHostSessions<<",\"retainedCrtBytes\":"<<retained<<",\"prematureTcpWrites\":"<<sshPrematureWrites<<",\"allocationCalls\":"<<sshHostAllocationCalls<<",\"allocationFailureInjected\":"<<(sshHostAllocationFailed?"true":"false")<<"}";}
 WSACleanup();return result;
}
