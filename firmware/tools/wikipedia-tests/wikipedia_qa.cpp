#include "Tab5Zim.h"
#include "Tab5ReaderLines.h"
#include "Tab5WikiMarkup.h"
extern "C" {
#define XZ_USE_CRC64
#include "src/xz/xz.h"
}
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
#include <sstream>

using Bytes=std::vector<uint8_t>;
Bytes load(const std::string& path){std::ifstream f(path,std::ios::binary);assert(f);return Bytes(std::istreambuf_iterator<char>(f),{});}
struct Source {Bytes bytes;size_t calls=0,maximum=0;bool shortRead=false;
 static bool read(void* p,uint64_t at,uint8_t* out,size_t n){auto& s=*static_cast<Source*>(p);++s.calls;s.maximum=std::max(s.maximum,n);if(s.shortRead||at>s.bytes.size()||n>s.bytes.size()-at)return false;memcpy(out,s.bytes.data()+at,n);return true;}
 static size_t lines(void* p,uint32_t at,uint8_t* out,size_t n){return read(p,at,out,n)?n:0;}
};
struct Memory {std::map<void*,size_t> blocks;size_t current=0,peak=0,limit=32*1024*1024;unsigned ticks=0,cancelAt=0;
 static void* allocate(void* p,size_t n){auto& m=*static_cast<Memory*>(p);if(n>m.limit||m.current>m.limit-n)return nullptr;void* b=std::malloc(n);assert(b);m.blocks[b]=n;m.current+=n;m.peak=std::max(m.peak,m.current);return b;}
 static void release(void* p,void* b){if(!b)return;auto& m=*static_cast<Memory*>(p);assert(m.blocks.count(b));m.current-=m.blocks[b];m.blocks.erase(b);std::free(b);}
 static void yield(void* p){++static_cast<Memory*>(p)->ticks;}
 static bool cancelled(void* p){auto& m=*static_cast<Memory*>(p);return m.cancelAt&&m.ticks>=m.cancelAt;}
 Tab5Zim::Callbacks callbacks(){return {this,cancelled,yield,allocate,release};}
};
bool write(void* p,const uint8_t* bytes,size_t n){static_cast<std::string*>(p)->append(reinterpret_cast<const char*>(bytes),n);return true;}
bool full(void*,const uint8_t*,size_t){return false;}
std::vector<Tab5WikiTable::Cell> tableCells(const std::string& links){std::vector<Tab5WikiTable::Cell> cells;std::istringstream input(links);std::string row;while(std::getline(input,row)){Tab5WikiTable::Cell c;if(Tab5WikiTable::cell(row.c_str(),c))cells.push_back(c);}return cells;}
void tableCheck(const std::string& text,const std::string& links){auto cells=tableCells(links);assert(cells.size()==4);assert(cells[0].row==0&&cells[0].column==0&&cells[0].rows==2&&cells[0].heading);assert(cells[1].row==0&&cells[1].column==1&&cells[1].columns==2);assert(cells[2].row==1&&cells[2].column==1);assert(cells[3].row==1&&cells[3].column==2);for(auto c:cells)assert(c.begin<=c.end&&c.end<=text.size());assert(text.substr(cells[2].begin,cells[2].end-cells[2].begin).find("Next")!=std::string::npos);assert(text.find("  |  ")==std::string::npos);}
void markupTest(){
 struct Output {std::string text,links;};
 auto text=[](void* p,const char* d,size_t n){static_cast<Output*>(p)->text.append(d,n);return true;};
 auto links=[](void* p,const char* d,size_t n){static_cast<Output*>(p)->links.append(d,n);return true;};
 std::string html="<p>Before</p><table><tr><th rowspan='2'>Region</th><th colspan='2'>Population</th></tr><tr><td><a href='./Next'>Next</a></td><td>Second</td></tr></table><p>After</p>";
 std::string expectedText,expectedLinks;
 for(size_t chunk:{size_t(1),size_t(7),size_t(4096)}){Output out;Tab5WikiMarkup::Parser parser(text,links,&out,"en");for(size_t at=0;at<html.size();at+=chunk)assert(parser.feed(html.data()+at,std::min(chunk,html.size()-at)));assert(parser.feed("",0,true));tableCheck(out.text,out.links);assert(out.text.find("After")!=std::string::npos);if(expectedText.empty()){expectedText=out.text;expectedLinks=out.links;}else{assert(expectedText==out.text&&expectedLinks==out.links);}}
 Output out;Tab5WikiMarkup::Parser failure(text,[](void*,const char*,size_t){return false;},&out,"en");assert(!failure.feed(html.data(),html.size(),true));assert(failure.error==5);
 Tab5WikiTable::Cell bad;assert(!Tab5WikiTable::cell("C\t0\t9\t1\t0\t63\t1\t2\t0\t1",bad));assert(!Tab5WikiTable::cell("C\t9\t0\t1\t0\t0\t1\t1\t0\t1",bad));
 std::cout<<"HTML tables: stream chunk independence, headers, rowspan/colspan, exact cell offsets, sink failure and malformed metadata passed\n";
}
struct Ticks {unsigned count=0,stop=0;};
int cooperate(void* p){auto& t=*static_cast<Ticks*>(p);++t.count;return !t.stop||t.count<t.stop;}

void linesTest(){
 Source s;std::string records="10\t20\tC/One\r\n"+std::string(900,'x')+"\n30\t40\tC/Two\n";s.bytes.assign(records.begin(),records.end());
 Tab5Reader::Lines lines(&s,Source::lines,uint32_t(s.bytes.size()));char row[100];
 assert(lines.next(row,sizeof(row))&&std::string(row)=="10\t20\tC/One");const uint32_t resume=lines.position();
 assert(lines.next(row,sizeof(row))&&!*row);assert(lines.next(row,sizeof(row))&&std::string(row)=="30\t40\tC/Two");assert(!lines.next(row,sizeof(row)));assert(s.calls<=3&&s.maximum<=512);
 Tab5Reader::Lines again(&s,Source::lines,uint32_t(s.bytes.size()),resume);assert(again.next(row,sizeof(row))&&!*row);
 s.bytes.assign(1000000,'x');s.calls=0;Tab5Reader::Lines damaged(&s,Source::lines,uint32_t(s.bytes.size()));assert(!damaged.next(row,sizeof(row)));assert(s.calls<=9);assert(!damaged.next(row,sizeof(row)));
 s.calls=0;Tab5Reader::Lines huge(&s,Source::lines,Tab5Reader::Lines::MaxBytes+1);assert(!huge.next(row,sizeof(row))&&s.calls==0);
 s.bytes.assign({'a','\n'});s.shortRead=true;Tab5Reader::Lines removed(&s,Source::lines,2);assert(!removed.next(row,sizeof(row)));
 std::cout<<"sidecars: buffered I/O, restart cursor, long/corrupt lines, size limit, removed SD passed\n";
}

void xzTest(const std::string& dir){
 auto encoded=load(dir+"/cluster.xz"),plain=load(dir+"/cluster.raw");Bytes out(plain.size());
 xz_crc32_init();xz_crc64_init();
 for(bool hook:{false,true}){xz_dec* dec=xz_dec_init(XZ_SINGLE,0);assert(dec);Ticks ticks;if(hook)xz_dec_set_cooperate(dec,cooperate,&ticks);
  xz_buf b{};b.in=encoded.data();b.in_size=encoded.size();b.out=out.data();b.out_size=out.size();assert(xz_dec_run(dec,&b)==XZ_STREAM_END);assert(b.out_pos==plain.size()&&out==plain);if(hook)assert(ticks.count>100);xz_dec_end(dec);
 }
 xz_dec* dec=xz_dec_init(XZ_SINGLE,0);Ticks stop;stop.stop=8;xz_dec_set_cooperate(dec,cooperate,&stop);xz_buf b{};b.in=encoded.data();b.in_size=encoded.size();b.out=out.data();b.out_size=out.size();assert(xz_dec_run(dec,&b)==XZ_DATA_ERROR);assert(stop.count==8&&b.out_pos==0);xz_dec_end(dec);
 for(size_t cut:{size_t(1),encoded.size()/2,encoded.size()-4}){dec=xz_dec_init(XZ_SINGLE,0);Ticks ticks;xz_dec_set_cooperate(dec,cooperate,&ticks);b={};b.in=encoded.data();b.in_size=cut;b.out=out.data();b.out_size=out.size();assert(xz_dec_run(dec,&b)!=XZ_STREAM_END);assert(b.out_pos==0);xz_dec_end(dec);}
 std::cout<<"XZ: upstream behavior vs cooperative output identical; mid-stream cancellation and truncated input passed\n";
}

void zimTest(const std::string& dir){
 std::string expected;
 for(auto name:{"plain.zim","xz.zim","zstd.zim"}){Source source{load(dir+"/"+name)};Memory memory;std::string text,links;uint64_t size=0;
  {Tab5Zim::Reader reader({&source,source.bytes.size(),Source::read},memory.callbacks());assert(reader.open());Tab5Zim::RichOptions rich;rich.links={&links,write};assert(reader.extractText(0,{&text,write},nullptr,&size,&rich));assert(size==text.size());assert(links.find("C/Next")!=std::string::npos);}
  assert(memory.current==0&&memory.blocks.empty());assert(source.maximum<=32768);tableCheck(text,links);if(expected.empty())expected=text;else assert(text==expected);
  std::cout<<name<<": text and links OK, allocation peak "<<memory.peak<<", cooperations "<<memory.ticks<<"\n";
 }
 for(unsigned mode=0;mode<4;++mode){Source source{load(dir+"/xz.zim")};Memory memory;std::string text;uint64_t size=999;
  {Tab5Zim::Reader reader({&source,source.bytes.size(),Source::read},memory.callbacks());assert(reader.open());
   if(mode==0)memory.cancelAt=memory.ticks+80;
   if(mode==1)memory.limit=100;
   if(mode==2)source.shortRead=true;
   assert(!reader.extractText(0,{&text,mode==3?full:write},nullptr,&size));assert(size==0);assert(reader.error());
   if(mode==0){assert(std::string(reader.error())=="Cancelled");assert(text.empty());}
  }
  assert(memory.current==0&&memory.blocks.empty());
 }
 std::cout<<"ZIM: cancellation, memory refusal, SD read failure, full output sink: no leaked allocation or successful partial result\n";
}
int main(int argc,char** argv){assert(argc==2);markupTest();linesTest();xzTest(argv[1]);zimTest(argv[1]);}
