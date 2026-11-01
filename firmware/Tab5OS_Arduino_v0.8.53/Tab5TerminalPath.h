#pragma once
#include "Tab5FilePolicy.h"
namespace Tab5TerminalPath {
inline bool logical(const std::string& input,const std::string& cwd,std::string& result,bool write=false){
  if(input.empty())return false;std::string supplied=input;
  if(supplied.compare(0,4,"sd:/")==0)supplied=supplied.substr(3);
  else if(supplied.compare(0,5,"usb:/")==0)supplied="/USB"+supplied.substr(4);
  const std::string p=supplied[0]=='/'?supplied:cwd+"/"+supplied;
  std::string valid;if(!Tab5Files::canonical(p,valid)||valid.size()>250||(write&&(valid=="/"||valid=="/usb")))return false;
  result.clear();size_t at=0;
  while(at<p.size()){size_t end=p.find_first_of("/\\",at);if(end==std::string::npos)end=p.size();std::string part=p.substr(at,end-at);at=end+1;
    if(part.empty()||part==".")continue;if(part==".."){result=result.substr(0,result.rfind('/'));}else result+="/"+part;}
  if(result.empty())result="/";
  if(valid=="/usb"||valid.compare(0,5,"/usb/")==0)result.replace(0,4,"/USB");
  return true;
}
}
