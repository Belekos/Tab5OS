#include "Tab5LanguageReader.h"
#include "Tab5TranslationFormat.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

static unsigned checks=0,fixtureCases=0,mutationCases=0,acceptedMutations=0;
static void check(bool value,const char* message){++checks;if(!value){std::cerr<<"FAIL "<<checks<<": "<<message<<'\n';std::abort();}}
struct Parsed { bool ok=false;std::string id,name;std::map<std::string,std::string> translations; };
struct Counter:ArduinoJson::Allocator{
 size_t calls=0,allocated=0;
 void* allocate(size_t n) override{++calls;allocated+=n;return std::malloc(n);}
 void deallocate(void* p) override{std::free(p);}
 void* reallocate(void* p,size_t n) override{++calls;allocated+=n;return std::realloc(p,n);}
};
static Parsed parseOld(const std::string& bytes,Counter* allocator=nullptr){
 Parsed result;if(bytes.size()>512*1024)return result;
 JsonDocument document(allocator?allocator:ArduinoJson::detail::DefaultAllocator::instance());
 if(deserializeJson(document,bytes))return result;
 const char* id=document["id"]|"";const char* name=document["name"]|id;
 if(!*id||std::strlen(id)>63||!*name||std::strlen(name)>127)return result;
 auto translations=document["translations"].as<JsonObjectConst>();
 if(translations.isNull()||translations.size()>4096)return result;
 for(JsonPairConst item:translations){
  const char* value=item.value().as<const char*>();if(!value||!*value)continue;
  if(!Tab5TranslationFormat::compatible(item.key().c_str(),value))return result;
  if(std::strcmp(item.key().c_str(),value))result.translations[item.key().c_str()]=value;
 }
 result.id=id;result.name=name;result.ok=true;return result;
}

// Exercise the production parser and normalize only after it has accepted the entire input.
static Parsed parseNew(const std::string& bytes){
 Parsed result;if(bytes.size()>512*1024)return result;
 std::vector<char> input(bytes.size()+1,0);std::memcpy(input.data(),bytes.data(),bytes.size());
 std::vector<Tab5LanguageReader::Entry> entries(4096);
 Tab5LanguageReader::Pack pack;
 if(!Tab5LanguageReader::parse(input.data(),bytes.size(),entries.data(),entries.size(),pack))return result;
 result.ok=true;result.id=pack.id;result.name=pack.name;
 for(size_t i=0;i<pack.count;++i){
  check(entries[i].source!=nullptr&&entries[i].translated!=nullptr,"parser returns valid pointers");
  if(i)check(std::strcmp(entries[i-1].source,entries[i].source)<0,"output strictly sorted without duplicates");
  result.translations[entries[i].source]=entries[i].translated;
 }
 return result;
}

static bool same(const Parsed&a,const Parsed&b){return a.ok==b.ok&&(!a.ok||(a.id==b.id&&a.name==b.name&&a.translations==b.translations));}
static std::string read(const std::filesystem::path&path){std::ifstream f(path,std::ios::binary);check(bool(f),"fixture opens");return std::string(std::istreambuf_iterator<char>(f),{});}
static void good(const char*name,const std::string&json){++fixtureCases;auto old=parseOld(json),now=parseNew(json);if(!old.ok||!same(old,now)){std::cerr<<"Fixture "<<name<<" old="<<old.ok<<" new="<<now.ok<<'\n';}check(old.ok,name);check(same(old,now),name);}
static void bad(const char*name,const std::string&json){++fixtureCases;const auto now=parseNew(json);if(now.ok)std::cerr<<"Accepted bad fixture: "<<name<<'\n';check(!now.ok,name);}
static std::string wrap(const std::string&entries){return "{\"id\":\"test\",\"name\":\"Custom language\",\"translations\":{"+entries+"}}";}
static std::string many(unsigned n){std::string s;for(unsigned i=0;i<n;++i){if(i)s+=',';s+='"'+std::to_string(i)+"\":\"value"+std::to_string(i)+'"';}return wrap(s);}
static void fixtures(){
 good("empty translations",wrap(""));
 good("identity skipped, blank translation ignored",wrap("\"same\":\"same\",\"empty\":\"\",\"label\":\"translated\""));
 good("escaped strings and surrogate pair",R"({"id":"t\u0065st","name":"Fran\u00e7ais \ud83d\ude03","translations":{"étiquette":"ligne\n\t\r\b\f\\\"\/","\u00e9":"accent","\ud83d\ude80":"fusée"}})");
 good("all metadata after translations",R"({"translations":{"A":"B"},"ignored":{"nested":[true,false,null,-12.5e+4,{"translations":"ignored"}]},"name":"Right order","id":"test"})");
 good("metadata mixed types",R"({"version":2,"nil":null,"n":-0.125e-2,"a":[{},[],0,"x"],"id":"test","translations":{"A":"B"},"name":"Language"})");
 good("non-string name defaults to ID",R"({"id":"test","name":["ignored"],"translations":{"A":"B"}})");
 good("name defaults to ID",R"({"translations":{"A":"B"},"id":"test"})");
 good("non-string translations ignored",wrap("\"a\":true,\"b\":false,\"c\":null,\"d\":123.4,\"e\":[],\"f\":{},\"g\":\"value\""));
 good("printf widths and precision",wrap("\"%04d %s %.3f\":\"%d %s %f\""));
 good("printf dynamic widths",wrap("\"%*.*f %% %lu\":\"%*.*f %% %lu\""));
 good("ordinary percentages",wrap("\"Ready: 100%\":\"Disponible: 100%\""));
 good("maximum translations",many(4096));
 bad("too many translations",many(4097));
 bad("missing translations",R"({"id":"test","name":"x"})");
 bad("wrong translations type",R"({"id":"test","translations":[]})");
 bad("missing ID",R"({"translations":{}})");
 bad("empty ID",R"({"id":"","translations":{}})");
 bad("non-string ID",R"({"id":17,"translations":{}})");
 bad("empty name",R"({"id":"test","name":"","translations":{}})");
 bad("ID too long","{\"id\":\""+std::string(64,'x')+"\",\"translations\":{}}");
 bad("name too long","{\"id\":\"test\",\"name\":\""+std::string(128,'x')+"\",\"translations\":{}}");
 bad("argument type changed",wrap("\"%d\":\"%s\""));
 bad("argument order changed",wrap("\"%d %s\":\"%s %d\""));
 bad("forbidden percent n",wrap("\"%n\":\"%n\""));
 bad("forbidden positional specifier",wrap("\"%1$s\":\"%1$s\""));
 bad("duplicate translated label",wrap("\"A\":\"first\",\"A\":\"second\""));
 bad("duplicate escaped label",wrap("\"A\":\"first\",\"\\u0041\":\"second\""));
 bad("duplicate ignored label",wrap("\"A\":null,\"A\":\"second\""));
 bad("duplicate identity label",wrap("\"A\":\"A\",\"A\":\"second\""));
 bad("duplicate name",R"({"id":"test","name":"x","name":"y","translations":{}})");
 bad("duplicate translations",R"({"id":"test","translations":{},"translations":{"A":"B"}})");
 bad("duplicate ID",R"({"id":"test","id":"other","translations":{}})");
 bad("trailing bytes",wrap("\"A\":\"B\"")+"garbage");
 good("trailing JSON whitespace",wrap("\"A\":\"B\"")+" \r\n\t");
 bad("trailing second JSON object",wrap("\"A\":\"B\"")+"{}");
 bad("missing colon",R"({"id" "test","translations":{}})");
 bad("trailing object comma",R"({"id":"test","translations":{"A":"B",}})");
 bad("invalid number",R"({"id":"test","number":01,"translations":{}})");
 bad("invalid exponent",R"({"id":"test","number":1e+,"translations":{}})");
 bad("invalid token",R"({"id":"test","value":tru,"translations":{}})");
 bad("array trailing comma",R"({"id":"test","value":[1,],"translations":{}})");
 bad("bad unicode hex",wrap("\"A\":\"\\u12x4\""));
 bad("orphan high surrogate",wrap("\"A\":\"\\ud800\""));
 bad("orphan low surrogate",wrap("\"A\":\"\\udc00\""));
 bad("wrong surrogate companion",wrap("\"A\":\"\\ud800\\u1234\""));
 bad("invalid escape",wrap("\"A\":\"\\q\""));
 bad("unescaped line break",wrap("\"A\":\"one\ntwo\""));
 bad("embedded NUL",wrap("\"A\":\"\\u0000\""));
 bad("raw NUL",wrap(std::string("\"A\":\"x")+char(0)+"y\""));
 bad("UTF8 lone continuation",wrap("\"A\":\""+std::string("\x80")+"\""));
 bad("UTF8 overlong two bytes",wrap("\"A\":\""+std::string("\xc0\xaf")+"\""));
 bad("UTF8 overlong three bytes",wrap("\"A\":\""+std::string("\xe0\x80\xaf")+"\""));
 bad("UTF8 overlong four bytes",wrap("\"A\":\""+std::string("\xf0\x80\x80\xaf")+"\""));
 bad("UTF8 raw surrogate",wrap("\"A\":\""+std::string("\xed\xa0\x80")+"\""));
 bad("UTF8 above Unicode maximum",wrap("\"A\":\""+std::string("\xf4\x90\x80\x80")+"\""));
 bad("UTF8 invalid continuation",wrap("\"A\":\""+std::string("\xe2x\xac")+"\""));
 bad("UTF8 truncated code point",wrap("\"A\":\""+std::string("\xe2\x82")+"\""));
 bad("object under array root","["+wrap("")+"]");
 bad("empty input","");
 const std::string prefix="{\"id\":\"test\",\"translations\":{},\"data\":";
 good("nesting limit with scalar",prefix+std::string(9,'[')+"0"+std::string(9,']')+"}");
 good("nesting limit with empty container",prefix+std::string(9,'[')+std::string(9,']')+"}");
 bad("one beyond nesting limit",prefix+std::string(10,'[')+"0"+std::string(10,']')+"}");
 std::string deep="{\"id\":\"test\",\"translations\":{},\"data\":"+std::string(40,'[')+"0"+std::string(40,']')+"}";
 bad("bounded nesting",deep);
 auto within=wrap("");within+=std::string(512*1024-within.size(),' ');good("exact maximum file",within);within+=' ';bad("file exceeds maximum",within);
 const std::string sample=R"({"id":"test","name":"test \ud83d\ude03","metadata":[{"x":"escaped\""},false],"translations":{"A":"B","C":"D"}})";
 for(size_t i=0;i<sample.size();++i)bad("truncated JSON at every byte",sample.substr(0,i));
 // Reproducible corruption exercise: accepted inputs must still match the
 // established parser. Rejected inputs exercise bounds under both sanitizers.
 uint32_t random=0x5840537u;
 auto next=[&random](){random=random*1664525u+1013904223u;return random;};
 for(unsigned n=0;n<2000;++n){
  std::string changed=sample;const size_t at=next()%changed.size();
  switch(n%3){case 0:changed[at]=char(next()&255);break;case 1:changed.erase(at,1);break;default:changed.insert(at,1,char(next()&255));break;}
  const auto now=parseNew(changed);++mutationCases;
  if(now.ok){++acceptedMutations;check(same(parseOld(changed),now),"accepted mutated JSON preserves semantics");}
 }
}
static std::vector<std::filesystem::path> packs(const std::filesystem::path& directory){std::vector<std::filesystem::path> result;for(auto& entry:std::filesystem::directory_iterator(directory))if(entry.path().extension()==".json")result.push_back(entry.path());std::sort(result.begin(),result.end());return result;}
static void benchmark(const std::filesystem::path&directory){
 std::cout<<"{\"hostOnly\":true,\"method\":\"Steady-clock median of 5 runs per parser, optimized host build; includes semantic validation and normalized map construction, excludes filesystem reads\",\"packs\":[";
 bool comma=false;
 for(const auto&path:packs(directory)){
  if(path.filename()!="fr-FR.json"&&path.filename()!="en-US.json")continue;
  auto bytes=read(path);std::vector<double> oldMs,newMs;Counter allocation;parseOld(bytes,&allocation);
  size_t retained=0;for(unsigned i=0;i<5;++i){auto begin=std::chrono::steady_clock::now();auto old=parseOld(bytes);auto middle=std::chrono::steady_clock::now();auto now=parseNew(bytes);auto end=std::chrono::steady_clock::now();check(same(old,now),"benchmark outputs match");retained=now.translations.size();oldMs.push_back(std::chrono::duration<double,std::milli>(middle-begin).count());newMs.push_back(std::chrono::duration<double,std::milli>(end-middle).count());}
  std::sort(oldMs.begin(),oldMs.end());std::sort(newMs.begin(),newMs.end());
  if(comma)std::cout<<',';comma=true;std::cout<<"{\"file\":\""<<path.filename().string()<<"\",\"bytes\":"<<bytes.size()<<",\"retainedTranslations\":"<<retained<<",\"oldMedianMs\":"<<oldMs[2]<<",\"newMedianMs\":"<<newMs[2]<<",\"oldAllocationCalls\":"<<allocation.calls<<",\"oldCumulativeAllocatedBytes\":"<<allocation.allocated<<'}';
 }
 std::cout<<"]}\n";
}
int main(int argc,char**argv){
 if(argc==3&&std::string(argv[1])=="--benchmark"){benchmark(argv[2]);return 0;}
 check(argc==2,"language directory argument");fixtures();
 auto paths=packs(argv[1]);check(paths.size()==14,"all fourteen current language packs");size_t compared=0;
 for(auto&path:paths){auto bytes=read(path);auto old=parseOld(bytes);auto now=parseNew(bytes);check(old.ok,"shipped pack baseline valid");if(!same(old,now))std::cerr<<"Pack mismatch "<<path<<'\n';check(same(old,now),"shipped pack translations preserved");compared+=old.translations.size();}
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"fixtureCases\":"<<fixtureCases<<",\"mutationCases\":"<<mutationCases<<",\"acceptedMutations\":"<<acceptedMutations<<",\"packs\":"<<paths.size()<<",\"retainedTranslationsCompared\":"<<compared<<"}\n";
}
