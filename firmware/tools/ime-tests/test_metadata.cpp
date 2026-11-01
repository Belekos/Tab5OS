#include "Tab5LanguageMetadata.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
static std::string prefix(const std::string& source){const size_t n=Tab5LanguageMetadata::prefixLength(source.data(),source.size());return n?source.substr(0,n)+"}":"";}
int main(int argc,char** argv){
  assert(prefix("{\"id\":\"ja-JP\",\"name\":\"日本語\",\"translations\":{")=="{\"id\":\"ja-JP\",\"name\":\"日本語\"}");
  assert(prefix("{\"id\":\"x\",\"custom\":{\"translations\":1},\"name\":\"A \\\"title\\\"\", \"translations\" : {")=="{\"id\":\"x\",\"custom\":{\"translations\":1},\"name\":\"A \\\"title\\\"\"}");
  assert(prefix("{\"name\":\"translations\\\\\",\"id\":\"x\",\"translations\":{}")=="{\"name\":\"translations\\\\\",\"id\":\"x\"}");
  assert(prefix("{\"translations\":{},\"id\":\"x\"}").empty());
  assert(prefix("{\"id\":\"x\",\"translations\"").empty());
  assert(prefix("{\"id\":\"x\",\"translations\" ").empty());
  assert(prefix("{\"id\":\"x\",\"nested\":[1,{\"translations\":{}}]}").empty());
  assert(prefix("{\"id\":\"x\",\"name\":\"quoted, \\\"translations\\\":\"}").empty());
  for(int arg=1;arg<argc;++arg){std::ifstream f(argv[arg],std::ios::binary);assert(f);char buf[1024];f.read(buf,sizeof(buf));const auto n=Tab5LanguageMetadata::prefixLength(buf,size_t(f.gcount()));assert(n>0&&n<1024);const auto header=std::string(buf,n)+"}";assert(header.find("\"id\"")!=std::string::npos&&header.find("\"name\"")!=std::string::npos);}
  std::cout<<"PASS language metadata: bounded real-pack headers, escaped/nested metadata, truncated/custom-order fallback.\n";
}
