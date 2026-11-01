#include "Tab5ImeDictionary.h"
#include "Tab5VolumeFS.h"
#include <cassert>
#include <iostream>
#include <set>
#include <vector>
std::string testSdRoot,testOverride;size_t testReads=0,testReadBytes=0;TestVolume tab5FS;
int main(int argc,char** argv){
  assert(argc==2);testSdRoot=argv[1];using tab5ime::Mode;
  auto p=Tab5ImeWords.lookup(Mode::Pinyin,"nihao");assert(p.available&&p.count);bool found=false;
  for(size_t i=0;i<p.count;++i)if(p.words[i]=="你好")found=true;assert(found);
  p=Tab5ImeWords.lookup(Mode::Japanese,"にほんご");assert(p.available&&p.count);found=false;
  for(size_t i=0;i<p.count;++i)if(p.words[i]=="日本語")found=true;assert(found);
  auto first=Tab5ImeWords.lookup(Mode::Pinyin,"yi");assert(first.total>6);std::set<std::string> words;
  size_t worstReads=0,worstBytes=0;
  for(uint32_t offset=0;offset<first.total;offset+=6){testReads=testReadBytes=0;p=Tab5ImeWords.lookup(Mode::Pinyin,"yi",offset);assert(p.available&&p.total==first.total&&p.count);
    for(size_t i=0;i<p.count;++i)assert(words.insert(p.words[i].c_str()).second);
    worstReads=std::max(worstReads,testReads);worstBytes=std::max(worstBytes,testReadBytes);
  }
  assert(words.size()==first.total);p=Tab5ImeWords.lookup(Mode::Pinyin,"yi",first.total);assert(p.available&&!p.count&&p.total==first.total);
  p=Tab5ImeWords.lookup(Mode::Japanese,"not-a-kana-key");assert(p.available&&!p.count);
  p=Tab5ImeWords.lookup(Mode::Korean,"abc");assert(!p.available);
  std::ifstream input(testSdRoot+"/System/ime/zh-pinyin.dat",std::ios::binary);std::vector<char> header(64);input.read(header.data(),header.size());
  testOverride="corrupt-ime.dat";
  for(size_t n=0;n<64;++n){std::ofstream out(testOverride,std::ios::binary);out.write(header.data(),n);out.close();p=Tab5ImeWords.lookup(Mode::Pinyin,"nihao");assert(!p.available&&!p.count);}
  std::cout<<"PASS: actual SD dictionary reader, nihao / 日本語, all "<<words.size()<<" homophones through last page, absent keys, 64 truncated files; maximum "<<worstReads<<" reads / "<<worstBytes<<" bytes per page.\n";
}
