#pragma once
#include <string>
#include <vector>
#include <cctype>
namespace Tab5Url {
inline bool valid(const std::string& s){if(s.size()>2048||s.compare(0,8,"https://"))return false;size_t end=s.find_first_of("/?#",8);std::string host=s.substr(8,end==std::string::npos?end:end-8);if(host.empty()||host.size()>259||host.find('@')!=std::string::npos)return false;auto port=host.find(':');if(port!=std::string::npos){if(host.substr(port)!=":443")return false;host.resize(port);}if(host.empty())return false;for(unsigned char c:host)if(!isalnum(c)&&c!='.'&&c!='-')return false;for(unsigned char c:s)if(c<=32||c==127||c=='\\')return false;return true;}
inline std::string resolve(const std::string& base,std::string ref){
 if(ref.empty()||ref.size()>2048)return {};if(ref.compare(0,2,"//")==0)ref="https:"+ref;
 if(ref.compare(0,8,"https://")==0)return valid(ref)?ref:std::string();
 size_t colon=ref.find(':');size_t slash=ref.find_first_of("/?#");if(colon!=std::string::npos&&(slash==std::string::npos||colon<slash))return {};
 if(!valid(base))return {};auto hostEnd=base.find('/',8);auto origin=base.substr(0,base.find_first_of("/?#",8));std::string path=hostEnd==std::string::npos?"/":base.substr(hostEnd);auto hash=path.find('#');if(hash!=std::string::npos)path.resize(hash);
 if(ref[0]=='#')return valid(origin+path+ref)?origin+path+ref:std::string();
 if(ref[0]=='?'){path.resize(path.find('?')==std::string::npos?path.size():path.find('?'));return valid(origin+path+ref)?origin+path+ref:std::string();}
 if(ref[0]=='/')path=ref;else{path.resize(path.find('?')==std::string::npos?path.size():path.find('?'));path=path.substr(0,path.rfind('/')+1)+ref;}
 auto suffixAt=path.find_first_of("?#");std::string suffix=suffixAt==std::string::npos?"":path.substr(suffixAt);if(suffixAt!=std::string::npos)path.resize(suffixAt);
 std::vector<std::string> parts;size_t begin=1;while(begin<=path.size()){auto end=path.find('/',begin);auto part=path.substr(begin,end==std::string::npos?end:end-begin);if(part==".."){if(!parts.empty())parts.pop_back();}else if(part!=".")parts.push_back(part);if(end==std::string::npos)break;begin=end+1;}
 std::string out=origin;for(auto& p:parts)out+="/"+p;if(parts.empty())out+="/";out+=suffix;return valid(out)?out:std::string();
}
}
