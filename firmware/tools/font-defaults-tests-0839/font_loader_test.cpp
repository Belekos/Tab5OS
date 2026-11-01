#include "Tab5FontDefaults.h"
#include "font_test_platform.h"
#include <fstream>
#include <iostream>
static unsigned checks=0;
static void check(bool b){++checks;if(!b){std::cerr<<"failed "<<checks<<"\n";std::abort();}}
int main(int argc,char**argv){
 using namespace Tab5FontDefaults;check(argc==2);std::ifstream f(argv[1],std::ios::binary);files["/Fonts/a.ttf"].bytes={std::istreambuf_iterator<char>(f),{}};files["/Fonts/b.ttf"]=files["/Fonts/a.ttf"];
 const size_t n=files["/Fonts/a.ttf"].bytes.size();check(n>4096);
 check(inspect("/Fonts/a.ttf")==Result::Ok);check(aliveBytes==0);check(readCalls==(n+16383)/16384);
 check(inspect("bad")==Result::InvalidPath);check(inspect("/Missing.ttf")==Result::Unavailable);
 leaseOk=false;check(inspect("/Fonts/a.ttf")==Result::Unavailable);leaseOk=true;
 admitOk=false;check(inspect("/Fonts/a.ttf")==Result::NoMemory);admitOk=true;
 allocationOk=false;check(inspect("/Fonts/a.ttf")==Result::NoMemory);allocationOk=true;
 files["/Fonts/a.ttf"].failRead=true;check(inspect("/Fonts/a.ttf")==Result::ReadFailure);check(aliveBytes==0);files["/Fonts/a.ttf"].failRead=false;
 files["/Fonts/a.ttf"].logical=MaxFontBytes+1;check(inspect("/Fonts/a.ttf")==Result::TooLarge);files["/Fonts/a.ttf"].logical=0;
 files["/Fonts/a.ttf"].directory=true;check(inspect("/Fonts/a.ttf")==Result::Unavailable);files["/Fonts/a.ttf"].directory=false;
 Blob blob;check(load("/Fonts/a.ttf",blob,n-1)==Result::TooLarge);check(!blob.data);check(load("/Fonts/a.ttf",blob)==Result::Ok);check(load("/Fonts/b.ttf",blob)==Result::InvalidFont);check(blob.size==n);release(blob);check(!blob.data&&!blob.size&&aliveBytes==0);
 {Sources sources;sources.begin("/Fonts/a.ttf","/Fonts/a.ttf");check(sources.residentBytes()==n);check(sources.custom(Role::Interface)==sources.custom(Role::Document));check(sources.result(Role::Interface)==Result::Ok);sources.begin("/Fonts/b.ttf","");check(sources.residentBytes()==n);}
 check(aliveBytes==0);
 {Sources sources;sources.begin("/Fonts/a.ttf","/Fonts/b.ttf");check(sources.residentBytes()==n*2);check(sources.custom(Role::Interface)!=sources.custom(Role::Document));}
 check(aliveBytes==0);
 {Sources sources;sources.begin("",StandardPath);check(!sources.custom(Role::Interface)&&!sources.custom(Role::Document)&&sources.residentBytes()==0);}
 {Sources sources;sources.begin("/Missing.ttf","/Fonts/a.ttf");check(!sources.custom(Role::Interface)&&sources.custom(Role::Document));check(sources.result(Role::Interface)==Result::Unavailable);}
 check(aliveBytes==0);
 files["/Fonts/a.ttf"].bytes.resize(5u*1024u*1024u);files["/Fonts/b.ttf"].bytes=files["/Fonts/a.ttf"].bytes;
 {Sources sources;sources.begin("/Fonts/a.ttf","/Fonts/b.ttf");check(sources.residentBytes()==5u*1024u*1024u);check(sources.custom(Role::Interface)&&!sources.custom(Role::Document));check(sources.result(Role::Document)==Result::TooLarge);}
 check(aliveBytes==0);
 check(checkSelectionBudget("/Fonts/a.ttf","/Fonts/b.ttf")==Result::CombinedTooLarge);
 check(checkSelectionBudget("/Fonts/a.ttf","/Fonts/a.ttf")==Result::Ok);
 check(checkSelectionBudget("/Fonts/a.ttf",StandardPath)==Result::Ok);
 check(checkSelectionBudget("/Fonts/a.ttf","")==Result::Ok);
 check(checkSelectionBudget("/Fonts/a.ttf","/Missing.ttf")==Result::Ok);
 check(checkSelectionBudget("/Fonts/a.ttf","relative.ttf")==Result::Ok);
 check(checkSelectionBudget("/Missing.ttf","/Fonts/a.ttf")==Result::Unavailable);
 check(checkSelectionBudget("relative.ttf","/Fonts/a.ttf")==Result::InvalidPath);
 files["/Fonts/b.ttf"].logical=MaxFontBytes+1;
 check(checkSelectionBudget("/Fonts/a.ttf","/Fonts/b.ttf")==Result::Ok);
 check(checkSelectionBudget("/Fonts/b.ttf","/Fonts/a.ttf")==Result::TooLarge);
 files["/Fonts/b.ttf"].logical=0;
 check(aliveBytes==0);
 files["/Fonts/a.ttf"].bytes[0]=0xff;check(inspect("/Fonts/a.ttf")==Result::InvalidFont);check(aliveBytes==0);
 std::cout<<"PASS "<<checks<<" font loading/fallback/allocation checks\n";
}
