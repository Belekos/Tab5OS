#include "Tab5GeoIndex.h"
#include <cassert>
#include <fstream>
#include <vector>
#include <iostream>
#include <random>
#include <chrono>
using namespace Tab5GeoIndex;
static size_t checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";std::abort();}}while(0)
struct Reader{std::vector<uint8_t> bytes;size_t reads=0,readBytes=0;int fail=-1;uint32_t size()const{return bytes.size();}bool read(uint32_t at,void* out,size_t n){++reads;readBytes+=n;if(fail==0)return false;if(fail>0)--fail;if(at>bytes.size()||n>bytes.size()-at)return false;memcpy(out,bytes.data()+at,n);return true;}};
int main(int argc,char**argv){
 CHECK(argc==2);Reader r;std::ifstream f(argv[1],std::ios::binary);CHECK(bool(f));r.bytes.assign(std::istreambuf_iterator<char>(f),{});Header h;CHECK(header(r,h));CHECK(h.places>100000);CHECK(h.keys>h.places);
 char out[160];CHECK(normalize("  Évry-Courcouronnes ",out,sizeof(out))&&!strcmp(out,"evry courcouronnes"));CHECK(normalize("L’Haÿ-les-Roses",out,sizeof(out))&&!strcmp(out,"l hay les roses"));CHECK(normalize("Łódź",out,sizeof(out))&&!strcmp(out,"lodz"));CHECK(normalize("МОСКВА",out,sizeof(out))&&!strcmp(out,"москва"));CHECK(!normalize("\xC0\xAF",out,sizeof(out)));CHECK(!normalize("\xED\xA0\x80",out,sizeof(out)));CHECK(!normalize("abc\n",out,sizeof(out)));CHECK(!normalize("too long",out,4));CHECK(normalize("ab\xC2\xA8" "cd",out,sizeof(out))&&!strcmp(out,"ab cd"));
 for(auto query:{"Paris","paris","PARIS","Évry","Lorient","Londres","Tokyo","東京","München","Muenchen","São Paulo","sao paulo","Москва","Reykjavík"}){
  Result result[MaxResults];r.reads=r.readBytes=0;auto start=std::chrono::steady_clock::now();auto o=search(r,query,48.85,2.35,result,MaxResults,[](){return false;});CHECK(o.status==Status::Ok);CHECK(o.count>0);CHECK(o.count<=MaxResults);for(unsigned i=0;i<o.count;++i){CHECK(utf8(result[i].name));CHECK(validCoordinate(result[i].latitude,result[i].longitude));}
  std::cout<<query<<" -> "<<result[0].name<<" "<<result[0].country<<" count="<<int(o.count)<<" scanned="<<o.scanned<<" bytes="<<r.readBytes<<" us="<<std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count()<<"\n";
 }
 Result results[MaxResults];auto a=search(r,"Paris",48.85,2.35,results,MaxResults,[](){return false;});CHECK(!strcmp(results[0].country,"FR"));CHECK(fabs(results[0].latitude-48.8534)<0.1);
 auto b=search(r,"Paris",33.66,-95.55,results,MaxResults,[](){return false;});CHECK(!strcmp(results[0].country,"US"));
 auto broad=search(r,"s",0,0,results,MaxResults,[](){return false;});CHECK(broad.status==Status::Ok&&broad.truncated&&broad.scanned==50001&&broad.count==MaxResults);
 CHECK(search(r,"zzzzzzznonexistent",0,0,results,MaxResults,[](){return false;}).count==0);
 CHECK(search(r,"Paris",0,0,results,0,[](){return false;}).status==Status::Invalid);
 CHECK(search(r,"Paris",91,0,results,MaxResults,[](){return false;}).status==Status::Invalid);
 CHECK(search(r,"Paris",0,0,results,MaxResults,[](){return true;}).status==Status::Cancelled);
 unsigned calls=0;CHECK(search(r,"sa",0,0,results,MaxResults,[&](){return ++calls>30;}).status==Status::Cancelled);
 r.fail=5;CHECK(search(r,"Paris",0,0,results,MaxResults,[](){return false;}).status==Status::Io);r.fail=-1;
 // Malformed headers and random tiny inputs are never read out of bounds.
 const auto valid=r.bytes;std::mt19937 gen(840);
 for(unsigned i=0;i<5000;++i){r.bytes.resize(gen()%256);for(auto& x:r.bytes)x=uint8_t(gen());auto q=search(r,"Paris",0,0,results,MaxResults,[](){return false;});CHECK(q.status==Status::Invalid);}
 r.bytes=valid;
 for(unsigned byte=0;byte<48;++byte){auto saved=r.bytes[byte];r.bytes[byte]^=0xff;auto q=search(r,"Paris",0,0,results,MaxResults,[](){return false;});CHECK(q.count<=MaxResults);r.bytes[byte]=saved;}
 // Corrupt a record offset and ensure failure without use of stale results.
 uint32_t saved=u32(r.bytes.data()+h.keyIndex+(h.keys/2)*4);memset(r.bytes.data()+h.keyIndex+(h.keys/2)*4,0xff,4);
 CHECK(search(r,"Paris",0,0,results,MaxResults,[](){return false;}).status==Status::Io);
 memcpy(r.bytes.data()+h.keyIndex+(h.keys/2)*4,&saved,4);
 std::cout<<"checks="<<checks<<" PASS\n";
}
