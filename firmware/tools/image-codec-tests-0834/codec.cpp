#include "Tab5ImageCodec.h"
#include "Tab5ImageWrite.h"
#include "FS.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <limits>

static size_t checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << "failed line " << __LINE__ << ": " << #x << '\n'; std::abort(); } } while (0)
struct Heap {
 std::unordered_map<void*,size_t> blocks;
 size_t bytes=0,peak=0,calls=0,failAt=SIZE_MAX;
 static void* alloc(void* opaque,size_t n) {
  auto& h=*static_cast<Heap*>(opaque); if(h.calls++==h.failAt)return nullptr;
  void*p=std::malloc(n);if(p){h.blocks[p]=n;h.bytes+=n;h.peak=std::max(h.peak,h.bytes);}return p;
 }
 static void free(void* opaque,void*p){auto&h=*static_cast<Heap*>(opaque);if(!p)return;auto i=h.blocks.find(p);CHECK(i!=h.blocks.end());h.bytes-=i->second;h.blocks.erase(i);std::free(p);}
};
struct Reader {
 std::vector<uint8_t> bytes;size_t at=0,chunk=SIZE_MAX,stopAt=SIZE_MAX;bool lie=false;
 static size_t read(void*opaque,void*out,size_t n){auto&r=*static_cast<Reader*>(opaque);if(r.lie)return n+1;if(r.at>=r.stopAt)return 0;size_t got=std::min({n,r.bytes.size()-r.at,r.chunk,r.stopAt-r.at});if(got)std::memcpy(out,r.bytes.data()+r.at,got);r.at+=got;return got;}
};
struct Writer {
 std::vector<uint8_t> bytes;size_t chunk=SIZE_MAX,stopAt=SIZE_MAX;bool lie=false;
 static size_t write(void*opaque,const void*data,size_t n){auto&w=*static_cast<Writer*>(opaque);if(w.lie)return n+1;if(w.bytes.size()>=w.stopAt)return 0;size_t got=std::min({n,w.chunk,w.stopAt-w.bytes.size()});auto*p=static_cast<const uint8_t*>(data);w.bytes.insert(w.bytes.end(),p,p+got);return got;}
};
struct Cancel {size_t calls=0,limit;static bool test(void*p){auto&c=*static_cast<Cancel*>(p);return ++c.calls>=c.limit;}};
static size_t yields=0;static void cooperate(void*){++yields;}
static std::vector<uint8_t> file(const std::string&p){std::ifstream f(p,std::ios::binary);CHECK(bool(f));return {std::istreambuf_iterator<char>(f),{}};}
static IEDoc make(Heap& h){IEDoc d{};CHECK(ie_init(&d,Heap::alloc,Heap::free,&h,32,24));ie_set_yield(&d,cooperate,nullptr);CHECK(ie_new(&d,32,24,IE_RGBA(11,27,63,255)));CHECK(ie_begin(&d));d.current.layers[0].pixels[3]=IE_RGBA(99,88,77,255);return d;}
struct Old {IEState current;unsigned undo,redo;IEState*first;int dirty;explicit Old(IEDoc&d):current(d.current),undo(d.undo_count),redo(d.redo_count),first(d.undo[0]),dirty(d.dirty){}void check(IEDoc&d){CHECK(!std::memcmp(&current,&d.current,sizeof current));CHECK(d.undo_count==undo&&d.redo_count==redo&&d.undo[0]==first&&d.dirty==dirty);CHECK(d.current.layers[0].pixels[3]==IE_RGBA(99,88,77,255));}};

int main(int argc,char**argv){
 CHECK(argc>=2);const std::string root=argv[1];Heap heap;
 struct Fixture {const char*name;unsigned sw,sh,w,h,reduction;};
 const Fixture fixtures[]={{"gradient.jpg",127,95,127,95,1},{"noise.jpg",129,131,129,131,1},{"low.jpg",127,127,127,127,1},{"grey.jpg",71,59,71,59,1},{"half.jpg",1600,1200,800,600,2},{"quarter.jpg",4001,3001,1000,750,4},{"eighth.jpg",6000,4000,750,500,8},{"thin.jpg",8192,16,1024,2,8},{"app.jpg",127,95,127,95,1}};
 for(const auto&f:fixtures){IEDoc d=make(heap);Reader r{file(root+"/"+f.name)};r.chunk=17;Tab5ImageCodec::ImportInfo info;CHECK(Tab5ImageCodec::loadJpeg(&d,Reader::read,&r,{nullptr,nullptr,&info}));CHECK(info.sourceWidth==f.sw&&info.sourceHeight==f.sh&&info.width==f.w&&info.height==f.h&&info.reduction==f.reduction);CHECK(d.current.width==int(f.w)&&d.current.height==int(f.h)&&d.current.count==1&&d.current.active==0);CHECK(d.undo_count==0&&d.redo_count==0&&!d.dirty);CHECK(d.yield==cooperate);for(size_t i=0;i<size_t(f.w)*f.h;i+=31)CHECK((d.current.layers[0].pixels[i]>>24)==255);ie_destroy(&d);CHECK(heap.bytes==0);if(argc>2)break;}
 if(argc>2)return 0;
 const auto good=file(root+"/gradient.jpg");
 for(const auto*name:{"progressive.jpg","overside.jpg","not.jpg"}){IEDoc d=make(heap);Old old(d);Reader r{file(root+"/"+name)};CHECK(!Tab5ImageCodec::loadJpeg(&d,Reader::read,&r));old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
 for(size_t at:{size_t(0),size_t(10),good.size()/2,good.size()-2,good.size()-1}){IEDoc d=make(heap);Old old(d);Reader r{good};r.stopAt=at;CHECK(!Tab5ImageCodec::loadJpeg(&d,Reader::read,&r));old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
 {IEDoc d=make(heap);Old old(d);Reader r{good};r.lie=true;CHECK(!Tab5ImageCodec::loadJpeg(&d,Reader::read,&r));old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
 {IEDoc d=make(heap);Old old(d);Reader r{good};r.bytes.resize(16u*1024u*1024u+1,0);CHECK(!Tab5ImageCodec::loadJpeg(&d,Reader::read,&r));old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
 {IEDoc d=make(heap);Reader r; r.bytes={255,216};size_t padding=16u*1024u*1024u-good.size();while(padding){size_t n=std::min<size_t>(padding,65537);if(padding-n&&padding-n<4)n-=4;r.bytes.push_back(255);r.bytes.push_back(239);r.bytes.push_back(uint8_t((n-2)>>8));r.bytes.push_back(uint8_t(n-2));r.bytes.insert(r.bytes.end(),n-4,0);padding-=n;}r.bytes.insert(r.bytes.end(),good.begin()+2,good.end());CHECK(r.bytes.size()==16u*1024u*1024u);CHECK(Tab5ImageCodec::loadJpeg(&d,Reader::read,&r));ie_destroy(&d);CHECK(heap.bytes==0);}
 for(size_t fault=0;fault<2;++fault){IEDoc d=make(heap);Old old(d);Reader r{good};heap.failAt=heap.calls+fault;CHECK(!Tab5ImageCodec::loadJpeg(&d,Reader::read,&r));heap.failAt=SIZE_MAX;old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
 const auto large=file(root+"/half.jpg");
 for(size_t point:{size_t(1),size_t(2),size_t(10),size_t(50)}){IEDoc d=make(heap);Old old(d);Reader r{large};Cancel c{0,point};CHECK(!Tab5ImageCodec::loadJpeg(&d,Reader::read,&r,{Cancel::test,&c,nullptr}));old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
 for(unsigned cycle=0;cycle<30;++cycle){IEDoc d=make(heap);CHECK(ie_new(&d,65,47,0));for(int y=0;y<47;y++)for(int x=0;x<65;x++)d.current.layers[0].pixels[y*65+x]=IE_RGBA(x*3,y*5,80,128);Writer w;w.chunk=7;CHECK(Tab5ImageCodec::saveJpeg(&d,Writer::write,&w));CHECK(w.bytes.size()>100&&w.bytes[0]==255&&w.bytes[1]==216&&w.bytes[w.bytes.size()-2]==255&&w.bytes.back()==217);if(!cycle){std::ofstream out(root+"/export.jpg",std::ios::binary);out.write(reinterpret_cast<const char*>(w.bytes.data()),w.bytes.size());}Reader r{w.bytes};CHECK(Tab5ImageCodec::loadJpeg(&d,Reader::read,&r));CHECK(d.current.width==65&&d.current.height==47);ie_destroy(&d);CHECK(heap.bytes==0);}
 for(size_t fault=0;fault<3;++fault){IEDoc d=make(heap);Old old(d);Writer w;heap.failAt=heap.calls+fault;CHECK(!Tab5ImageCodec::saveJpeg(&d,Writer::write,&w));heap.failAt=SIZE_MAX;old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
 for(size_t at:{size_t(0),size_t(30),size_t(600)}){IEDoc d=make(heap);Old old(d);Writer w;w.stopAt=at;CHECK(!Tab5ImageCodec::saveJpeg(&d,Writer::write,&w));old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
 {IEDoc d=make(heap);Old old(d);Writer w;w.lie=true;CHECK(!Tab5ImageCodec::saveJpeg(&d,Writer::write,&w));old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
 for(size_t point:{size_t(1),size_t(2),size_t(10),size_t(50)}){IEDoc d=make(heap);Old old(d);Writer w;Cancel c{0,point};CHECK(!Tab5ImageCodec::saveJpeg(&d,Writer::write,&w,92,{Cancel::test,&c,nullptr}));old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
 using Legacy=Tab5ImageWrite<Fs,File>;using Native=Tab5ImageWrite<Fs,File,true>;
 for(const char*p:{"/Images/a.jpg","/Images/a.jpeg","/Images/A.JPG","/Images/A.JPEG"}){CHECK(!Legacy::valid(p));CHECK(Native::valid(p));}
 for(const char*p:{"/Images/a.bmp","/Images/a.t5i"}){CHECK(Legacy::valid(p));CHECK(Native::valid(p));}
 for(const char*p:{"/Images/.a.jpg","/Images/sub/a.jpg","/Images/a.jpg.tab5tmp","/Images/../a.jpeg","/Images/a.png"})CHECK(!Native::valid(p));
 for(unsigned fault=0;fault<7;++fault){Fs fs;Native w;fs.put("/Images/a.jpg","original");int handle=w.begin(fs,"/Images/a.jpg","wb");CHECK(handle>0);if(fault==1)fs.shortWrite=1;if(fault==2)fs.failFlush=true;if(fault==3)fs.failRename=1;if(fault==4)fs.failRename=2;if(fault==5){fs.failRename=2;fs.failRename2=3;}int written=w.chunk(handle,good.data(),std::min(size_t(4096),good.size()));CHECK(fault==1?written<0:written>0);int done=w.finish(fs,handle,fault!=6);CHECK(fault==0?done==0:fault==6?done==0:done<0);if(fault==5){CHECK(fs.text("/Images/a.jpg.tab5old")=="original");fs.failRename=fs.failRename2=0;CHECK(Native::recover(fs,"/Images/a.jpg"));}if(fault)CHECK(fs.text("/Images/a.jpg")=="original");CHECK(!fs.exists("/Images/a.jpg.tab5tmp"));}
 CHECK(heap.bytes==0&&heap.blocks.empty());
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"peakBytes\":"<<heap.peak<<",\"remainingBytes\":"<<heap.bytes<<",\"cooperations\":"<<yields<<"}\n";
}
