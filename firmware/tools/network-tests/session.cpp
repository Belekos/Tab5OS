#include "Platform.h"
#include "Tab5NetworkTools.cpp"
#include <cassert>
#include <cstdio>
// HTTP has its own transfer fixture; this suite exercises the real DNS,
// socket, parser and cancellation implementation without an HTTP server.
namespace Tab5NetworkTools {
int http(const Tab5NetworkArgs::Options&,Output,Cancel,Translate,const std::string&,Output){return 0;}
}

int main() {
  std::string text;
  const auto output = [&](const char* p,size_t n) { text.append(p,n); };
  const auto cancel = [] { return Fake::cancelAt && Fake::tick >= Fake::cancelAt; };
  const auto tr = [](const char* p) { return std::string(p); };
  auto run = [&](std::vector<std::string> args) {
    text.clear(); int rc=Tab5NetworkTools::run(args,output,cancel,tr); assert(Fake::open==0); return rc;
  };
  Fake::reset(); assert(run({"other"})==-1);
  assert(run({"tcp","192.0.2.1","65536"})==2 && Fake::opened==0);
  assert(run({"netinfo","extra"})==2);
  assert(run({"netinfo"})==0 && text.find("192.0.2.1")!=std::string::npos && Fake::opened==0);
  Fake::wifi=false; assert(run({"ping","192.0.2.1"})==1 && Fake::opened==0);
  Fake::reset(); Fake::gate=false; assert(run({"dns","example.test"})==1 && !Fake::pendingDns);
  Fake::reset(); assert(run({"dns","example.test"})==0 && Fake::tick<5000);
  Fake::reset(); Fake::dnsResult=ERR_OK; assert(run({"dns","cached.test"})==0);
  Fake::reset(); Fake::dnsResult=-1; assert(run({"dns","missing.test"})==1);
  Fake::reset(); Fake::enqueueFails=true; assert(run({"dns","queued.test"})==1);
  Fake::reset(); assert(run({"dns","224.0.0.1"})==1);
  assert(run({"dns","255.255.255.255"})==1);
  assert(run({"dns","0.0.0.0"})==1);
  assert(run({"dns","https://example.test"})==1);
  // The callback is deliberately delivered after timeout/cancellation. Its
  // storage remains valid and a new command cannot create unbounded lookups.
  Fake::reset(); Fake::dnsDelay=6000;
  assert(run({"dns","slow.test"})==1 && Fake::tick==5000);
  assert(run({"dns","next.test"})==1 && Fake::tick==5000);
  Fake::advance(1000); Fake::dnsDelay=10;
  assert(run({"dns","next.test"})==0);
  Fake::reset(); Fake::dnsDelay=2000; Fake::cancelAt=50;
  assert(run({"dns","cancel.test"})==130 && Fake::tick<=60);
  Fake::advance(3000); Fake::cancelAt=0;
  assert(run({"dns","after-cancel.test"})==0);
  Fake::reset(); assert(run({"tcp","192.0.2.1","80"})==0 && Fake::opened==1);
  Fake::reset(); Fake::tcpError=ECONNREFUSED; assert(run({"tcp","192.0.2.1","80"})==1);
  Fake::reset(); Fake::tcpTimeout=true;
  assert(run({"tcp","192.0.2.1","80"})==1 && Fake::tick<=1520);
  Fake::reset(); Fake::tcpTimeout=true; Fake::cancelAt=50;
  assert(run({"tcp","192.0.2.1","80"})==130 && Fake::tick<=80);
  Fake::reset(); Fake::echo=true;
  assert(run({"ping","192.0.2.1"})==0 && Fake::sent==4);
  Fake::reset(); Fake::echo=true;
  assert(run({"ping","-c","2","-W","0.2","192.0.2.1"})==0 && Fake::sent==2);
  Fake::reset(); Fake::tcpTimeout=true;
  assert(run({"tcp","-w","0.35","192.0.2.1","80"})==1 && Fake::tick<=380);
  Fake::reset();
  assert(run({"ports","-w","0.35","192.0.2.1","22,80,443,8000-8002"})==0 && Fake::opened==6);
  assert(text.find("ports testés")!=std::string::npos && text.find("6")!=std::string::npos);
  Fake::reset(); Fake::cancelAt=25;
  assert(run({"ports","192.0.2.1","1-100"})==130 && Fake::opened<100);
  Fake::reset(); assert(run({"ping","192.0.2.1"})==1 && Fake::sent==4 && Fake::tick<=4080);
  Fake::reset(); Fake::cancelAt=50;
  assert(run({"ping","192.0.2.1"})==130 && Fake::sent==1 && Fake::tick<=80);
  Fake::reset();
  // An optional error callback separates diagnostics without reclassifying
  // useful results merely because the command returned a nonzero status.
  std::string errors;auto diagnostic=[&](const char* p,size_t n){errors.append(p,n);};
  auto separated=[&](std::vector<std::string> args){text.clear();errors.clear();int rc=Tab5NetworkTools::run(args,output,cancel,tr,"/",diagnostic);assert(Fake::open==0);return rc;};
  assert(separated({"tcp","192.0.2.1","65536"})==2&&text.empty()&&!errors.empty());
  Fake::reset();Fake::wifi=false;assert(separated({"dns","example.test"})==1&&text.empty()&&!errors.empty());
  Fake::reset();Fake::dnsResult=-1;assert(separated({"dns","missing.test"})==1&&text.empty()&&!errors.empty());
  Fake::reset();Fake::tcpError=ECONNREFUSED;assert(separated({"tcp","192.0.2.1","80"})==1&&text.empty()&&errors.find("192.0.2.1:80")!=std::string::npos);
  Fake::reset();Fake::tcpTimeout=true;assert(separated({"tcp","192.0.2.1","80"})==1&&text.empty()&&!errors.empty());
  Fake::reset();assert(separated({"ping","192.0.2.1"})==1&&errors.empty()&&text.find("PING")!=std::string::npos);
  Fake::reset();assert(separated({"netinfo"})==0&&errors.empty()&&!text.empty());
  puts("PASS: real command implementation with simulated sockets/DNS; validation, network gate, deadline, late DNS callback, cancellation, socket cleanup, exactly four echoes.");
  puts("PASS: optional stderr for invalid arguments, disconnected Wi-Fi, DNS, refused/timeout TCP; ping results remain stdout; historical shared callback preserved.");
}
