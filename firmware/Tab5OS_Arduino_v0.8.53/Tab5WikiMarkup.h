#pragma once
#include "Tab5Url.h"
#include "Tab5WikiTable.h"
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <algorithm>
namespace Tab5WikiMarkup {
struct Image {size_t offset;std::string url,description,alt;};
using Sink=bool(*)(void*,const char*,size_t);
inline std::string utf8(uint32_t c){std::string s;if(c<128)s+=char(c);else if(c<2048){s+=char(192|(c>>6));s+=char(128|(c&63));}else if(c<65536){s+=char(224|(c>>12));s+=char(128|((c>>6)&63));s+=char(128|(c&63));}else{s+=char(240|(c>>18));s+=char(128|((c>>12)&63));s+=char(128|((c>>6)&63));s+=char(128|(c&63));}return s;}
inline std::string unescape(const std::string& input){
 static const char* names[]={"amp","lt","gt","quot","apos","nbsp","ndash","mdash","hellip","eacute","Eacute","agrave","egrave","rsquo","lsquo","rdquo","ldquo","times","minus","middot","bull"};
 static const char* values[]={"&","<",">","\"","'"," ","–","—","…","é","É","à","è","’","‘","”","“","×","−","·","•"};
 std::string out;out.reserve(input.size());for(size_t i=0;i<input.size();){if(input[i]!='&'){out+=input[i++];continue;}size_t end=input.find(';',i+1);if(end==std::string::npos||end-i>18){out+=input[i++];continue;}auto name=input.substr(i+1,end-i-1);std::string decoded;
  if(name.size()>1&&name[0]=='#'){char* tail=nullptr;bool hex=name.size()>2&&(name[1]=='x'||name[1]=='X');unsigned long c=strtoul(name.c_str()+(hex?2:1),&tail,hex?16:10);if(tail&&!*tail&&c>=32&&c<=0x10ffff&&!(c>=0xd800&&c<=0xdfff))decoded=utf8(c);}
  else for(unsigned n=0;n<sizeof(names)/sizeof(*names);++n)if(name==names[n]){decoded=values[n];break;}
  if(decoded.empty())out.append(input,i,end-i+1);else out+=decoded;i=end+1;
 }return out;
}
inline bool starts(const std::string& s,const std::string& prefix){return s.compare(0,prefix.size(),prefix)==0;}
inline std::string target(std::string href,const std::string& language){
 href=unescape(href);if(starts(language,"https://"))return Tab5Url::resolve(language,href);for(auto& prefix: {std::string("https://")+language+".wikipedia.org",std::string("//")+language+".wikipedia.org"})if(starts(href,prefix+"/")){href.erase(0,prefix.size());break;}
 if(starts(href,"/wiki/"))href.erase(0,6);else if(starts(href,"./"))href.erase(0,2);else return {};
 auto hash=href.find('#');if(hash!=std::string::npos)href.resize(hash);std::string out;
 for(size_t i=0;i<href.size();++i){if(href[i]=='%'&&i+2<href.size()&&isxdigit((unsigned char)href[i+1])&&isxdigit((unsigned char)href[i+2])){char h[]={href[i+1],href[i+2],0};out+=char(strtoul(h,nullptr,16));i+=2;}else out+=href[i]=='_'?' ':href[i];}
 while(!out.empty()&&isspace((unsigned char)out.front()))out.erase(0,1);while(!out.empty()&&isspace((unsigned char)out.back()))out.pop_back();
 if(out.empty()||out.size()>700)return {};for(unsigned char c:out)if(c<32||c==127||c==':'||c=='?'||c=='\\'||c=='<'||c=='>')return {};return out;
}
struct Attrs {std::string href,src,alt,width="80",height="80",classes;unsigned colspan=1,rowspan=1;};
inline Attrs attributes(const std::string& s){Attrs a;size_t i=0;while(i<s.size()&&!isspace((unsigned char)s[i]))++i;
 while(i<s.size()){while(i<s.size()&&(isspace((unsigned char)s[i])||s[i]=='/'))++i;size_t b=i;while(i<s.size()&&!isspace((unsigned char)s[i])&&s[i]!='='&&s[i]!='/')++i;std::string key=s.substr(b,i-b);for(char& c:key)c=tolower((unsigned char)c);while(i<s.size()&&isspace((unsigned char)s[i]))++i;if(key.empty()){++i;continue;}std::string value;
  if(i<s.size()&&s[i]=='='){++i;while(i<s.size()&&isspace((unsigned char)s[i]))++i;if(i<s.size()&&(s[i]=='\''||s[i]=='"')){char quote=s[i++];b=i;while(i<s.size()&&s[i]!=quote)++i;value=s.substr(b,i-b);if(i<s.size())++i;}else{b=i;while(i<s.size()&&!isspace((unsigned char)s[i]))++i;value=s.substr(b,i-b);}}
  if(key=="href")a.href=unescape(value);else if(key=="src")a.src=unescape(value);else if(key=="alt")a.alt=unescape(value);else if(key=="width")a.width=value;else if(key=="height")a.height=value;else if(key=="class")a.classes=" "+value+" ";else if(key=="colspan")a.colspan=std::max(1,atoi(value.c_str()));else if(key=="rowspan")a.rowspan=std::max(1,atoi(value.c_str()));
 }return a;
}
class Parser {
 Sink text_,links_;void* context_;Tab5WikiTable::Recorder table_;std::string language_,pending_,skipped_,anchor_,picture_,space_;unsigned depth_=0;
 size_t anchorStart_=0;char last_='\n',quote_=0;bool longTag_=false,comment_=false;
 uint32_t utfCode_=0,utfMin_=0;unsigned utfNeed_=0;
 bool write(const std::string& value){if(value.empty())return true;if(offset+value.size()>262144){error=27;return false;}if(!text_(context_,value.data(),value.size())){error=5;return false;}offset+=value.size();last_=value.back();return true;}
 void space(){if(!space_.empty()&&offset&&last_!='\n')write(space_);space_.clear();}
 void words(const std::string& raw){if(!skipped_.empty())return;auto s=unescape(raw);size_t b=0;for(size_t i=0;i<s.size();++i){unsigned n=0;unsigned char c=s[i];if(isspace(c))n=1;else if(c==0xc2&&i+1<s.size()&&(unsigned char)s[i+1]==0xa0)n=2;else if(c==0xe2&&i+2<s.size()&&(unsigned char)s[i+1]==0x80&&((unsigned char)s[i+2]>=0x80&&(unsigned char)s[i+2]<=0x8a))n=3;
  if(n){if(i>b){space();write(s.substr(b,i-b));content=true;}if(space_!="\n\n")space_=" ";i+=n-1;b=i+1;}}
  if(b<s.size()){space();write(s.substr(b));content=true;}
 }
 void anchorEnd(){if(!anchor_.empty()&&offset>anchorStart_&&linkCount<2048){char numbers[48];snprintf(numbers,sizeof(numbers),"%u\t%u\t",unsigned(anchorStart_),unsigned(offset));std::string row=std::string(numbers)+anchor_+"\n";if(!links_(context_,row.data(),row.size()))error=5;else ++linkCount;}anchor_.clear();picture_.clear();}
 void tag(std::string body){size_t first=body.find_first_not_of(" \r\n\t");if(first==std::string::npos)return;body.erase(0,first);if(body[0]=='!'||body[0]=='?')return;bool closing=body[0]=='/';if(closing)body.erase(0,1);size_t end=body.find_first_of(" \r\n\t/");std::string name=body.substr(0,end);for(char& c:name)c=tolower((unsigned char)c);
  if(!skipped_.empty()){if(name==skipped_){if(closing){if(!--depth_)skipped_.clear();}else ++depth_;}return;}
  if(!closing&&(name=="script"||name=="style"||name=="noscript"||name=="head"||name=="svg"||name=="math")){skipped_=name;depth_=1;return;}
  Attrs a;if(!closing)a=attributes(body);
  if(!closing)for(auto* hide:{"mw-editsection","navbox","metadata","noprint","sistersitebox"})if(a.classes.find(std::string(" ")+hide+" ")!=std::string::npos){skipped_=name;depth_=1;return;}
  if(name=="a"){anchorEnd();if(!closing){space();anchor_=target(a.href,language_);anchorStart_=offset;if(a.href.find("/wiki/File:")!=std::string::npos||starts(a.href,"./File:"))picture_=a.href;}}
  if(name=="img"&&!closing&&images.size()<8){auto url=a.src;if(starts(url,"//"))url="https:"+url;auto mark=url.find_first_of("?#");if(mark!=std::string::npos)url.resize(mark);int w=atoi(a.width.c_str()),h=atoi(a.height.c_str());bool duplicate=false;for(auto& img:images)if(img.url==url)duplicate=true;
   if(!duplicate&&(starts(url,"https://upload.wikimedia.org/wikipedia/")||starts(url,"https://thumb.wikimedia.org/wikipedia/"))&&url.size()<=1024&&w>=48&&h>=40&&url.find_first_of("\t\r\n\\?#")==std::string::npos){space_="\n\n";space();auto alt=a.alt;size_t end=0;unsigned chars=0;for(;end<alt.size();++end)if(((unsigned char)alt[end]&0xc0)!=0x80&&chars++==180)break;alt.resize(end);images.push_back({offset,url,picture_,alt});}}
  if(name=="table"){
   if(closing){anchorEnd();table_.end(offset);}
   else {space_="\n\n";space();table_.begin(offset);}
  }
  if(name=="tr"&&table_.depth()==1){anchorEnd();table_.closeCell(offset);space_="\n";space();if(!closing)table_.row(offset);}
  if((name=="td"||name=="th")&&table_.depth()==1){anchorEnd();table_.closeCell(offset);space_=" ";space();if(!closing)table_.openCell(offset,name=="th",a.colspan,a.rowspan);}
  if(name=="p"||name=="div"||name=="section"||(name.size()==2&&name[0]=='h'&&name[1]>='1'&&name[1]<='6')||name=="table"||name=="tr"||name=="figure"||name=="figcaption"||name=="blockquote"||name=="ul"||name=="ol")space_="\n\n";
  else if(name=="br"||name=="li")space_="\n";else if((name=="td"||name=="th")&&closing)space_=" ";
 }
 bool unicode(const char* data,size_t n){for(size_t i=0;i<n;++i){unsigned char c=data[i];if(utfNeed_){if((c&192)!=128)return false;utfCode_=(utfCode_<<6)|(c&63);if(!--utfNeed_&&(utfCode_<utfMin_||utfCode_>0x10ffff||(utfCode_>=0xd800&&utfCode_<=0xdfff)))return false;}else if(c>=128){if(c>=0xc2&&c<=0xdf){utfNeed_=1;utfCode_=c&31;utfMin_=128;}else if(c>=0xe0&&c<=0xef){utfNeed_=2;utfCode_=c&15;utfMin_=2048;}else if(c>=0xf0&&c<=0xf4){utfNeed_=3;utfCode_=c&7;utfMin_=65536;}else return false;}else if(!c)return false;}return true;}
 public:
 size_t offset=0;unsigned linkCount=0;std::vector<Image> images;int error=0;bool content=false;
 Parser(Sink text,Sink links,void* context,const char* language):text_(text),links_(links),context_(context),table_(links,context),language_(language){pending_.reserve(8192);images.reserve(8);}
 bool feed(const char* data,size_t n,bool final=false){
  if(error)return false;if(!unicode(data,n)){error=84;return false;}pending_.append(data,n);size_t pos=0;
  while(pos<pending_.size()&&!error){
   if(comment_){size_t end=pending_.find("-->",pos);if(end==std::string::npos){pos=pending_.size()>2?pending_.size()-2:0;break;}pos=end+3;comment_=false;continue;}
   if(longTag_){size_t i=pos;for(;i<pending_.size();++i){char c=pending_[i];if(quote_){if(c==quote_)quote_=0;}else if(c=='\''||c=='"')quote_=c;else if(c=='>'){longTag_=false;++i;break;}}pos=i;continue;}
   if(pending_.compare(pos,4,"<!--")==0){comment_=true;pos+=4;continue;}
   if(pending_[pos]=='<'){size_t i=pos+1;char quote=0;for(;i<pending_.size();++i){char c=pending_[i];if(quote){if(c==quote)quote=0;}else if(c=='\''||c=='"')quote=c;else if(c=='>')break;}
    if(i==pending_.size()){if(i-pos>4096){size_t end=pending_.find_first_of(" \t\r\n",pos+1);tag(pending_.substr(pos+1,end==std::string::npos?std::min(size_t(40),i-pos-1):end-pos-1));longTag_=true;quote_=quote;pos=i;}else if(final)pos=i;break;}
    tag(pending_.substr(pos+1,i-pos-1));pos=i+1;continue;
   }
   size_t end=pending_.find('<',pos);if(end==std::string::npos){end=pending_.size();if(!final){size_t amp=pending_.rfind('&');if(amp>=pos&&amp!=std::string::npos&&end-amp<=18&&pending_.find(';',amp)==std::string::npos)end=amp;
     // Keep the last UTF-8 codepoint until the next block if incomplete.
     size_t lead=end;while(lead>pos&&((unsigned char)pending_[lead-1]&0xc0)==0x80)--lead;if(lead>pos){unsigned char c=pending_[lead-1];unsigned need=c<128?1:c<224?2:c<240?3:4;if(end-lead+1<need)end=lead-1;}
    }}
   if(end==pos)break;words(pending_.substr(pos,end-pos));pos=end;
  }
  pending_.erase(0,pos);if(final){if(utfNeed_)error=84;anchorEnd();table_.finish(offset);if(!content&&!error)error=61;}if(!table_.good())error=5;return !error;
 }
};
}
