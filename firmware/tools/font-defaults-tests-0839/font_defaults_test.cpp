#include "Tab5FontDefaults.h"
#include <cassert>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
static unsigned checks=0;
static void check(bool b){++checks;if(!b){std::cerr<<"failed at "<<checks<<"\n";std::abort();}}
static std::vector<uint8_t> read(const char* p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
 using namespace Tab5FontDefaults;
 check(validPath("/System/font.ttf"));check(validPath("/USB/Fonts/DejaVu.TTF"));check(validPath("/Fonts/漢字.ttf"));
 for(const char* p:{"","relative.ttf","//font.ttf","/../font.ttf","/Fonts/./font.ttf","/Fonts//font.ttf","/Fonts/a.ttf/","/Fonts/a.otf","/Fonts/a\\b.ttf","/Fonts/a\nb.ttf","/Fonts/a:b.ttf"})check(!validPath(p));
 check(!validPath(nullptr));check(!validPath(("/"+std::string(257,'a')+".ttf").c_str()));
 for(int i=1;i<argc;++i){auto data=read(argv[i]);check(!data.empty());check(validate(data.data(),data.size()));
  for(size_t cut=0;cut<std::min(data.size(),size_t(4097));++cut)check(!validate(data.data(),cut));
  auto broken=data;broken[4]=0xff;broken[5]=0xff;check(!validate(broken.data(),broken.size()));
  broken=data;broken[20]=0xff;broken[21]=0xff;broken[22]=0xff;broken[23]=0xff;check(!validate(broken.data(),broken.size()));
  broken=data;broken[24]=0xff;broken[25]=0xff;broken[26]=0xff;broken[27]=0xff;check(!validate(broken.data(),broken.size()));
  broken=data;memcpy(broken.data()+28,broken.data()+12,4);check(!validate(broken.data(),broken.size()));
  for(unsigned j=0;j<4096;++j){broken=data;const size_t at=(size_t(j)*7919)%broken.size();broken[at]^=uint8_t(1u<<(j%8));(void)validate(broken.data(),broken.size());++checks;}
 }
 check(!validate(nullptr,MaxFontBytes));std::cout<<"PASS "<<checks<<" font path/structure checks\n";
}
