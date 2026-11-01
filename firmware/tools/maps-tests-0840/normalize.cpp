#include "Tab5GeoIndex.h"
#include <iostream>
#include <string>
static std::string decode(const std::string& s){std::string o;for(size_t i=0;i+1<s.size();i+=2)o+=char(std::stoul(s.substr(i,2),nullptr,16));return o;}
int main(){std::string s;unsigned checks=0;while(std::getline(std::cin,s)){auto at=s.find(':');if(at==std::string::npos)return 2;auto input=decode(s.substr(0,at)),expected=decode(s.substr(at+1));char out[160];bool ok=Tab5GeoIndex::normalize(input.c_str(),out,sizeof(out));if(ok!=!expected.empty()||(ok&&out!=expected)){std::cerr<<"normalization mismatch "<<checks<<" input="<<s.substr(0,at)<<" actual="<<out<<" expected="<<expected<<"\n";return 1;}++checks;}std::cout<<"checks="<<checks<<" PASS\n";}
