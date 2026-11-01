#pragma once
#include <string>
#include <cstdint>
#include <vector>
namespace tab5search {
// Latin case/diacritic folding for names; never changes a filesystem path.
inline std::string fold(const std::string& source) {
  std::string out;
  for(size_t i=0;i<source.size();) {
    unsigned char c=source[i++];unsigned cp=c;
    if(c>=0xc2&&c<=0xdf&&i<source.size()&&(uint8_t(source[i])&0xc0)==0x80)cp=((c&31)<<6)|(uint8_t(source[i++])&63);
    else if(c>=0xe0&&c<=0xef&&i+1<source.size()&&(uint8_t(source[i])&0xc0)==0x80&&(uint8_t(source[i+1])&0xc0)==0x80){cp=((c&15)<<12)|((uint8_t(source[i])&63)<<6)|(uint8_t(source[i+1])&63);i+=2;}
    if(cp>='A'&&cp<='Z')cp+=32;
    if(cp>=0xc0&&cp<=0xde&&cp!=0xd7)cp+=32;
    if(cp>=0x300&&cp<=0x36f)continue;
    if(cp==0x152||cp==0x153){out+="oe";continue;}
    if(cp==0xe6){out+="ae";continue;}
    if(cp==0xdf){out+="ss";continue;}
    if(cp>=0xe0&&cp<=0xe5)cp='a';else if(cp==0xe7)cp='c';
    else if(cp>=0xe8&&cp<=0xeb)cp='e';else if(cp>=0xec&&cp<=0xef)cp='i';
    else if(cp==0xf1)cp='n';else if((cp>=0xf2&&cp<=0xf6)||cp==0xf8)cp='o';
    else if(cp>=0xf9&&cp<=0xfc)cp='u';else if(cp==0xfd||cp==0xff||cp==0x178)cp='y';
    if(cp<128)out+=char(cp);
    else if(cp<2048){out+=char(0xc0|(cp>>6));out+=char(0x80|(cp&63));}
    else {out+=char(0xe0|(cp>>12));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}
  }
  return out;
}
class Query {
  std::vector<std::string> words;
 public:
  explicit Query(const std::string& text="") {auto normalized=fold(text);size_t start=0;
    while(start<normalized.size()){start=normalized.find_first_not_of(" \t\r\n",start);if(start==std::string::npos)break;size_t end=normalized.find_first_of(" \t\r\n",start);words.push_back(normalized.substr(start,end-start));if(end==std::string::npos)break;start=end;}
  }
  bool empty()const{return words.empty();}
  bool matches(const std::string& text)const{auto normalized=fold(text);for(const auto& word:words)if(normalized.find(word)==std::string::npos)return false;return true;}
};
}
