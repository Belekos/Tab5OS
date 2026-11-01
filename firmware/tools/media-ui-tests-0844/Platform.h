#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cstdint>
class String {
 std::string text;
 public:
 String()=default;String(const char* p):text(p?p:""){}String(const std::string& p):text(p){}
 const char* c_str()const{return text.c_str();}size_t length()const{return text.size();}
 char operator[](size_t i)const{return text[i];}
 int lastIndexOf(char c)const{auto n=text.rfind(c);return n==std::string::npos?-1:int(n);}
 int indexOf(char c,size_t from=0)const{auto n=text.find(c,from);return n==std::string::npos?-1:int(n);}
 String substring(size_t n)const{return text.substr(n);}String substring(size_t n,size_t end)const{return text.substr(n,end-n);}
 bool startsWith(const String& p)const{return text.compare(0,p.text.size(),p.text)==0;}
 String operator+(const String& s)const{return text+s.text;}String& operator+=(const String& s){text+=s.text;return *this;}
 bool operator==(const String& s)const{return text==s.text;}bool operator!=(const String& s)const{return text!=s.text;}
 void toLowerCase(){for(char& c:text)if(c>='A'&&c<='Z')c+=32;}int compareTo(const String& other)const{return text.compare(other.text);}
 friend String operator+(const char* a,const String& b){return String(a)+b;}
};
namespace Fake {
inline unsigned tick=0;
inline std::map<std::string,std::pair<uint64_t,int64_t>> metadata;
using Entry=std::pair<std::string,bool>;inline std::map<std::string,std::vector<Entry>> folders;
}
class File {
 struct Data{std::string path;bool directory=false;size_t next=0;};std::shared_ptr<Data> data;
 public:
 File()=default;File(const std::string& path,bool directory):data(std::make_shared<Data>(Data{path,directory,0})){}
 operator bool()const{return bool(data);}bool isDirectory()const{return data&&data->directory;}
 const char* name()const{return data?data->path.c_str():"";}void close(){data.reset();}
 uint64_t size()const{return data?Fake::metadata[data->path].first:0;}int64_t getLastWrite()const{return data?Fake::metadata[data->path].second:0;}
 File openNextFile(){if(!isDirectory())return {};auto& entries=Fake::folders[data->path];if(data->next>=entries.size())return {};auto e=entries[data->next++];return {data->path+(data->path=="/"?"":"/")+e.first,e.second};}
};
struct FS {
 File open(const String& p){const auto path=std::string(p.c_str());if(Fake::folders.count(path))return {path,true};if(Fake::metadata.count(path))return {path,false};return {};}
 bool exists(const String& p){return Fake::folders.count(p.c_str())||Fake::metadata.count(p.c_str());}
 bool mkdir(const String& p){Fake::folders[p.c_str()]={};return true;}
};
inline FS tab5FS;
struct Locale{const char* text(const char* p)const{return p;}};
inline Locale Tab5Locale;
inline uint32_t millis(){return Fake::tick;}
