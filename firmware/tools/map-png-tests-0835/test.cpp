#include "Tab5TileValidation.h"
#include "Tab5TileRaster.h"
#include "Tab5TilePolicy.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

static size_t assertions=0, calls=0, failAt=0, active=0, peak=0;
static std::unordered_map<void*,size_t> allocations;
static void check(bool ok,const char* message){++assertions;if(!ok){std::cerr<<message<<"\n";std::abort();}}
extern "C" void* tab5png_test_alloc(size_t n){
 if(++calls==failAt)return nullptr;
 void* p=std::malloc(n);if(p){allocations[p]=n;active+=n;peak=std::max(peak,active);}return p;
}
extern "C" void tab5png_test_free(void* p){
 if(!p)return;auto i=allocations.find(p);check(i!=allocations.end(),"unknown PNG free");active-=i->second;allocations.erase(i);std::free(p);
}
static bool failTile=false;
extern "C" void* tab5tile_test_alloc(size_t n){return failTile?nullptr:std::malloc(n);}
static std::vector<uint8_t> readFile(const std::string& p){std::ifstream f(p,std::ios::binary);check(bool(f),"fixture read");return {std::istreambuf_iterator<char>(f),{}};}
static unsigned cancelCalls=0,cancelAt=0;
static bool cancelled(){return cancelAt&&++cancelCalls>=cancelAt;}
struct Source{const std::vector<uint8_t>& bytes;size_t at=0,limit=32768;};
static size_t readSource(void* v,uint8_t* dst,size_t n){auto& s=*static_cast<Source*>(v);n=std::min({n,s.limit,s.bytes.size()-s.at});std::memcpy(dst,s.bytes.data()+s.at,n);s.at+=n;return n;}
static void empty(){check(active==0&&allocations.empty(),"PNG allocation leaked");}
int main(int argc,char** argv){
 using R=Tab5TileValidation::Result;
 check(argc==2,"fixture directory required");const std::string dir=argv[1];std::ifstream names(dir+"/names.txt");std::string name;size_t fixtures=0,failures=0;
 std::vector<uint8_t> first;
 while(std::getline(names,name)){
  auto bytes=readFile(dir+"/"+name+".png"),raw=readFile(dir+"/"+name+".rgb565");check(raw.size()==65536*2,"expected tile size");
  std::vector<uint16_t> expected(65536),out(65536,0xdead);for(size_t i=0;i<65536;++i)expected[i]=uint16_t(raw[i*2]|unsigned(raw[i*2+1])<<8);
  check(tab5tile::validPng(bytes.data(),bytes.size()),"tile structural validation");
  calls=0;check(Tab5TileValidation::decode(bytes.data(),bytes.size(),out.data())==R::Valid,"production tile decode");const size_t allocCalls=calls;
  for(size_t i=0;i<out.size();++i)check(out[i]==expected[i],"decoded tile pixel differs");empty();
  check(Tab5TileValidation::validate(bytes.data(),bytes.size())==R::Valid,"production tile validate");empty();
  for(size_t n=1;n<=allocCalls;++n){calls=0;failAt=n;check(Tab5TileValidation::decode(bytes.data(),bytes.size(),out.data())==R::NoMemory,"allocation failure must be NoMemory");failAt=0;empty();++failures;}
  for(unsigned at: {1u,2u,4u,8u,16u,64u}){cancelCalls=0;cancelAt=at;check(Tab5TileValidation::decode(bytes.data(),bytes.size(),out.data(),cancelled)==R::Unavailable,"cancelled tile must be Unavailable");cancelAt=0;empty();}
  Source source{bytes,0,113};Tab5TileValidation::Source io{&source,bytes.size(),readSource};check(Tab5TileValidation::validateSource(io)==R::Valid,"short source reads");empty();
  source.at=0;source.limit=0;check(Tab5TileValidation::validateSource(io)==R::Unavailable,"source read failure");empty();
  failTile=true;check(Tab5TileValidation::validateSource(io)==R::NoMemory,"source allocation failure");failTile=false;empty();
  for(int size: {128,256,512}){
   std::vector<uint16_t> dest(320*160,0xabc7);R result=R::Invalid;check(Tab5TileRaster::draw(dest.data(),320,160,bytes.data(),bytes.size(),-50,10,size,20,15,240,120,&result)&&result==R::Valid,"production raster render");
   for(int y=0;y<160;++y)for(int x=0;x<320;++x){uint16_t want=0xabc7;if(x>=20&&x<260&&y>=15&&y<135&&x<(-50+size)&&y<(10+size)){auto c=expected[size_t((y-10)*256/size)*256+unsigned((x+50)*256/size)];want=uint16_t(c>>8|c<<8);}check(dest[size_t(y)*320+x]==want,"raster clipped scaled pixel differs");}
   empty();
  }
  for(size_t cut: {size_t(0),size_t(7),size_t(33),bytes.size()/2,bytes.size()-1})check(Tab5TileValidation::validate(bytes.data(),cut)==R::Invalid,"truncated tile accepted");
  auto corrupt=bytes;corrupt[corrupt.size()/2]^=1;check(Tab5TileValidation::validate(corrupt.data(),corrupt.size())==R::Invalid,"corrupt tile accepted");
  std::vector<uint16_t> dest(64*64,0xabc7);R result=R::Valid;check(!Tab5TileRaster::draw(dest.data(),64,64,corrupt.data(),corrupt.size(),0,0,64,0,0,-1,-1,&result)&&result==R::Invalid,"corrupt raster accepted");check(std::all_of(dest.begin(),dest.end(),[](uint16_t p){return p==0xabc7;}),"failed tile changed raster");empty();
  if(first.empty())first=bytes;++fixtures;
 }
 check(fixtures==30,"all PNG fixtures executed");
 for(unsigned i=0;i<512;++i){check(Tab5TileValidation::validate(first.data(),first.size())==R::Valid,"repeated tile validation");empty();}
 std::cout<<"{\"fixtures\":"<<fixtures<<",\"checks\":"<<assertions<<",\"allocationFailures\":"<<failures<<",\"repeatCycles\":512,\"peakDecoderBytes\":"<<peak<<",\"liveDecoderBytes\":"<<active<<"}\n";
}
