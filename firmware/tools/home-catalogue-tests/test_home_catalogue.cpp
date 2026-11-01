#include "Tab5AppCataloguePolicy.h"
#include "Tab5Ico.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>
using namespace Tab5AppCatalogue;
static void put16(std::vector<uint8_t>& b,size_t p,unsigned v){b[p]=uint8_t(v);b[p+1]=uint8_t(v>>8);}
static void put32(std::vector<uint8_t>& b,size_t p,unsigned v){put16(b,p,v);put16(b,p+2,v>>16);}
static std::vector<uint8_t> makeIcon(unsigned depth){
  const unsigned w=2,h=2,stride=((w*depth+31)/32)*4,maskStride=4,length=40+stride*h+maskStride*h;
  std::vector<uint8_t> b(22+length);put16(b,2,1);put16(b,4,1);b[6]=w;b[7]=h;
  put16(b,10,1);put16(b,12,depth);put32(b,14,length);put32(b,18,22);
  put32(b,22,40);put32(b,26,w);put32(b,30,h*2);put16(b,34,1);put16(b,36,depth);
  for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){size_t p=62+y*stride+x*(depth/8);b[p]=uint8_t(10+y*20+x);b[p+1]=42;b[p+2]=84;}
  return b;
}
int main(){
  Choices config;assert(config.valid());assert(keyFor("files")==keyFor("files"));assert(keyFor("files")!=keyFor("editor"));
  auto* first=config.get(keyFor("files"),true);assert(first&&first->pinned&&first->order==0);
  assert(config.get(keyFor("files"),false)==first&&config.count==1);
  for(unsigned i=1;i<MaxApps;++i)assert(config.get(i,false));
  assert(config.count==MaxApps&&config.valid());assert(!config.get(1000,false));
  config.personal=2;assert(!config.valid());config.personal=1;
  config.apps[0].icon=SymbolCount;assert(!config.valid());config.apps[0].icon=0;
  config.apps[1].key=config.apps[0].key;assert(!config.valid());
  config=Choices{};assert(config.valid()&&config.count==0&&config.get(1000,false));
  // The existing NVS blob upgrades in place without losing order or visibility.
  config=Choices{};config.version=1;config.personal=1;
  auto* legacy=config.get(keyFor("/Apps/test.app.elf"),true);
  legacy->icon=12;legacy->pinned=0;legacy->order=7;
  assert(!config.valid()&&config.upgrade()&&config.version==2);
  assert(config.count==1&&config.apps[0].icon==12&&!config.apps[0].pinned&&config.apps[0].order==7);
  assert(!config.list()&&config.density()==0&&config.labels());
  for(unsigned density=0;density<3;++density)for(unsigned list=0;list<2;++list)for(unsigned labels=0;labels<2;++labels){
    config.setDensity(density);config.setList(list);config.setLabels(labels);
    assert(config.valid()&&config.density()==density&&config.list()==bool(list)&&config.labels()==bool(labels));
    Choices persisted;memcpy(&persisted,&config,sizeof(config));
    assert(persisted.valid()&&persisted.display==config.display&&persisted.apps[0].order==7);
  }
  config.display=6;assert(!config.valid());config.display=16;assert(!config.valid());
  config.display=0;config.version=99;assert(!config.upgrade());
  config=Choices{};auto* custom=config.get(1,true);custom->icon=24;assert(config.valid());
  custom->icon=25;assert(!config.valid());
  for(unsigned depth:{24U,32U}){
    auto b=makeIcon(depth);Tab5Ico::Frame frame;assert(Tab5Ico::inspect(b.data(),b.size(),frame));
    uint8_t pixels[20];std::fill(std::begin(pixels),std::end(pixels),0xaa);
    assert(!Tab5Ico::decode(frame,pixels,15));assert(Tab5Ico::decode(frame,pixels,16));
    assert(pixels[0]==30&&pixels[3]==255&&pixels[8]==10&&pixels[16]==0xaa);
    // A set AND-mask bit makes the corresponding top pixel transparent.
    b[size_t(frame.mask-b.data())+frame.maskStride]=0x80;
    assert(Tab5Ico::decode(frame,pixels,16)&&pixels[3]==0&&pixels[7]==255);
    // Every truncated prefix must be rejected without reading past the input.
    for(size_t n=0;n<b.size();++n)assert(!Tab5Ico::inspect(b.data(),n,frame));
  }
  auto b=makeIcon(32);Tab5Ico::Frame frame;uint8_t pixels[16];
  b[65]=127;assert(Tab5Ico::inspect(b.data(),b.size(),frame));assert(Tab5Ico::decode(frame,pixels,sizeof(pixels)));assert(pixels[11]==127&&pixels[3]==0);
  auto corrupt=b;put32(corrupt,18,0xfffffff0);assert(!Tab5Ico::inspect(corrupt.data(),corrupt.size(),frame));
  corrupt=b;put32(corrupt,14,0xffffffff);assert(!Tab5Ico::inspect(corrupt.data(),corrupt.size(),frame));
  corrupt=b;put32(corrupt,26,4096);assert(!Tab5Ico::inspect(corrupt.data(),corrupt.size(),frame));
  corrupt=b;put32(corrupt,30,0xfffffffc);assert(!Tab5Ico::inspect(corrupt.data(),corrupt.size(),frame));
  corrupt=b;put16(corrupt,36,8);assert(!Tab5Ico::inspect(corrupt.data(),corrupt.size(),frame));
  corrupt=b;put32(corrupt,38,1);assert(!Tab5Ico::inspect(corrupt.data(),corrupt.size(),frame));
  corrupt=b;put16(corrupt,4,17);assert(!Tab5Ico::inspect(corrupt.data(),corrupt.size(),frame));
  // PNG-compressed ICO frames are intentionally rejected and use the UI symbol.
  corrupt=b;put32(corrupt,22,0x474e5089);assert(!Tab5Ico::inspect(corrupt.data(),corrupt.size(),frame));
  puts("PASS: home preference migration, display choices, bounds and ICO validation/alpha/masks/truncation");
}
