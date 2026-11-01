#include "HttpPlatform.h"
// write is also a Stream member, so don't macro-redirect the source globally.
#undef write
#include "Tab5NetworkHttp.cpp"
#include <iostream>
static std::string get(const char* path){std::ifstream f(HttpFake::root/path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
 assert(argc==2);HttpFake::root=argv[1];std::filesystem::create_directories(HttpFake::root/"USB");
 std::string output;auto emit=[&](const char* p,size_t n){output.append(p,n);};
 auto cancel=[](){return HttpFake::cancelAt&&HttpFake::tick>=HttpFake::cancelAt;};
 auto run=[&](std::vector<std::string> args){output.clear();Tab5NetworkArgs::Options o;assert(Tab5NetworkArgs::parse(args,o)&&o.error.empty());int rc=Tab5NetworkTools::http(o,emit,cancel,[](const char* p){return std::string(p);},"/");assert(HttpFake::fdCount==0);assert(!std::filesystem::exists(HttpFake::root/"file.tab5-download-part"));return rc;};
 HttpFake::reset();HttpFake::replies.push_back({200,"new"});assert(run({"wget","-O","sd:/file","https://example.org"})==0&&get("file")=="new"&&HttpFake::secureRequests==1);
 HttpFake::reset();HttpFake::replies.push_back({200,"replace"});assert(run({"wget","-O","sd:/file","https://example.org"})==1&&get("file")=="new"&&HttpFake::urls.empty());
 HttpFake::reset();HttpFake::replies.push_back({200,std::string(200,'x'),{},true});assert(run({"wget","--overwrite","-O","sd:/file","https://example.org"})==1&&get("file")=="new");
 HttpFake::reset();HttpFake::failWriteAfter=32;HttpFake::replies.push_back({200,std::string(200,'x')});assert(run({"wget","--overwrite","-O","sd:/file","https://example.org"})==1&&get("file")=="new");
 HttpFake::reset();HttpFake::cancelAt=3;HttpFake::replies.push_back({200,std::string(200,'x')});assert(run({"wget","--overwrite","-O","sd:/file","https://example.org"})==130&&get("file")=="new");
 HttpFake::reset();HttpFake::step=600;HttpFake::replies.push_back({200,std::string(200,'x')});assert(run({"wget","-T","1","--overwrite","-O","sd:/file","https://example.org"})==1&&get("file")=="new");
 HttpFake::reset();HttpFake::clock=false;assert(run({"wget","--overwrite","-O","sd:/file","https://example.org"})==1&&get("file")=="new"&&HttpFake::urls.empty());
 HttpFake::reset();HttpFake::replies.push_back({302,"",{{"Location","http://example.org/file"}}});assert(run({"wget","-L","--overwrite","-O","sd:/file","https://example.org"})==1&&HttpFake::urls.size()==1&&get("file")=="new");
 HttpFake::reset();HttpFake::replies.push_back({302,"",{{"Location","/next"}}});HttpFake::replies.push_back({200,"usb"});assert(run({"wget","-L","-T","3600","-O","usb:/file","https://example.org"})==0&&get("USB/file")=="usb"&&HttpFake::urls.back()=="https://example.org/next"&&HttpFake::largestTimeout<=30000);
 HttpFake::reset();HttpFake::replies.push_back({200,"",{{"Server","fixture"}}});assert(run({"headers","https://example.org"})==0&&output.find("TLS :")!=std::string::npos&&output.find("fixture")!=std::string::npos);
 HttpFake::reset();HttpFake::replies.push_back({200,std::string(40000,'x')});assert(run({"curl","https://example.org"})==1&&output.size()<34000);
 HttpFake::reset();HttpFake::replies.push_back({200,"replacement"});assert(run({"wget","--overwrite","-O","sd:/file","https://example.org"})==0&&get("file")=="replacement"&&!std::filesystem::exists(HttpFake::root/"file.tab5-download-previous"));
 HttpFake::reset();HttpFake::replies.push_back({200,""});assert(run({"headers","https://example.org?x=1#section"})==0&&HttpFake::urls.back()=="https://example.org/?x=1");
 HttpFake::reset();HttpFake::replies.push_back({302,"",{{"Location","?download=1#fragment"}}});HttpFake::replies.push_back({200,""});assert(run({"headers","-L","https://example.org/releases/file.bin"})==0&&HttpFake::urls.back()=="https://example.org/releases/file.bin?download=1");
 HttpFake::reset();HttpFake::replies.push_back({302,"",{{"Location","/next"}}});HttpFake::replies.push_back({200,""});assert(run({"headers","-L","https://example.org?old=1"})==0&&HttpFake::urls.back()=="https://example.org/next");
 HttpFake::reset();HttpFake::step=2000;HttpFake::replies.push_back({200,""});assert(run({"headers","-T","1","https://example.org"})==1&&output.find("Délai du transfert dépassé.")!=std::string::npos);
 std::string errors;auto diagnostic=[&](const char* p,size_t n){errors.append(p,n);};
 auto separated=[&](std::vector<std::string> args){output.clear();errors.clear();Tab5NetworkArgs::Options o;assert(Tab5NetworkArgs::parse(args,o)&&o.error.empty());int rc=Tab5NetworkTools::http(o,emit,cancel,[](const char* p){return std::string(p);},"/",diagnostic);assert(HttpFake::fdCount==0);return rc;};
 HttpFake::reset();HttpFake::replies.push_back({403,"denied"});assert(separated({"curl","https://example.org"})==1&&output.empty()&&errors.find("HTTP 403")!=std::string::npos);
 HttpFake::reset();HttpFake::clock=false;assert(separated({"curl","https://example.org"})==1&&output.empty()&&!errors.empty());
 HttpFake::reset();HttpFake::replies.push_back({200,"body"});assert(separated({"curl","https://example.org"})==0&&output=="body\n"&&errors.empty());
 HttpFake::reset();HttpFake::replies.push_back({404,""});assert(separated({"headers","https://example.org"})==1&&output.find("HTTP 404")!=std::string::npos&&errors.empty());
 puts("PASS: actual HTTP transfer code; atomic replacement, truncation, cancellation, deadline, HTTPS clock check, downgrade rejection, SD/USB routing, redirects, timeout width, bounded console output.");
 puts("PASS: optional stderr for HTTP refusal and TLS clock failure; successful bodies and requested header results remain stdout; historical shared callback preserved.");
}
