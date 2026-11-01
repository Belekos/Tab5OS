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
 size_t bytes=0,peak=0,calls=0,failAt=SIZE_MAX,limit=18u*1024u*1024u;
 static void* alloc(void* opaque,size_t n) {
  auto& h=*static_cast<Heap*>(opaque); if(h.calls++==h.failAt||n>h.limit-h.bytes)return nullptr;
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
 const Fixture fixtures[]={{"gradient.jpg",127,95,127,95,1},{"noise.jpg",129,131,129,131,1},{"low.jpg",127,127,127,127,1},{"grey.jpg",71,59,71,59,1},{"half.jpg",1600,1200,1600,1200,1},{"fullhd.jpg",1920,1080,1920,1080,1},{"thin.jpg",8192,16,8192,16,1},{"app.jpg",127,95,127,95,1}};
 for(const auto&f:fixtures){IEDoc d=make(heap);Reader r{file(root+"/"+f.name)};r.chunk=17;Tab5ImageCodec::ImportInfo info;CHECK(Tab5ImageCodec::loadJpeg(&d,Reader::read,&r,{nullptr,nullptr,&info}));CHECK(info.sourceWidth==f.sw&&info.sourceHeight==f.sh&&info.width==f.w&&info.height==f.h&&info.reduction==f.reduction);CHECK(d.current.width==int(f.w)&&d.current.height==int(f.h)&&d.current.count==1&&d.current.active==0);CHECK(d.undo_count==0&&d.redo_count==0&&!d.dirty);CHECK(d.yield==cooperate);for(size_t i=0;i<size_t(f.w)*f.h;i+=31)CHECK((d.current.layers[0].pixels[i]>>24)==255);ie_destroy(&d);CHECK(heap.bytes==0);if(argc>2)break;}
 if(argc>2)return 0;
 const auto good=file(root+"/gradient.jpg");
 for(const auto*name:{"progressive.jpg","overside.jpg","not.jpg","quarter.jpg","eighth.jpg"}){IEDoc d=make(heap);Old old(d);Reader r{file(root+"/"+name)};CHECK(!Tab5ImageCodec::loadJpeg(&d,Reader::read,&r));old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
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

 // Every pixel dimension and alpha channel survives non-JPEG import.
 using Import=int(*)(IEDoc*,IERead,void*,Tab5ImageCodec::Options);
 struct Raster {const char*name;Import load;unsigned w,h;bool exact;};
 const Raster rasters[]={
  {"alpha.png",Tab5ImageCodec::loadPng,129,97,true},{"palette.png",Tab5ImageCodec::loadPng,129,97,true},
  {"interlace.png",Tab5ImageCodec::loadPng,129,97,true},{"large.png",Tab5ImageCodec::loadPng,1600,1200,false},
  {"thin.png",Tab5ImageCodec::loadPng,8192,16,false},{"mono.png",Tab5ImageCodec::loadPng,129,97,false},
  {"grey16.png",Tab5ImageCodec::loadPng,129,97,false},{"alpha.webp",Tab5ImageCodec::loadWebp,129,97,true},
  {"lossy.webp",Tab5ImageCodec::loadWebp,129,97,false},{"large.webp",Tab5ImageCodec::loadWebp,1600,1200,false},
  {"animated.webp",Tab5ImageCodec::loadWebp,129,97,false}};
 size_t faults=0;
 for(const auto& f:rasters){
  IEDoc d=make(heap);Reader r{file(root+"/"+f.name)};r.chunk=19;Tab5ImageCodec::ImportInfo info;
  const size_t first=heap.calls;
  CHECK(f.load(&d,Reader::read,&r,{nullptr,nullptr,&info}));
  const size_t allocations=heap.calls-first;
  CHECK(d.current.width==int(f.w)&&d.current.height==int(f.h));CHECK(info.width==f.w&&info.height==f.h&&info.reduction==1);
  CHECK(!d.dirty&&!d.undo_count&&!d.redo_count);
  if(f.exact){Reader ref{file(root+"/alpha.rgba")};CHECK(ref.bytes.size()==size_t(f.w)*f.h*4);CHECK(!std::memcmp(ref.bytes.data(),d.current.layers[0].pixels,ref.bytes.size()));}
  ie_destroy(&d);CHECK(heap.bytes==0);
  // Every allocation failure leaves prior image/history byte-for-byte intact.
  for(size_t fault=0;fault<allocations;++fault){IEDoc q=make(heap);Old old(q);Reader input{r.bytes};heap.failAt=heap.calls+fault;CHECK(!f.load(&q,Reader::read,&input,{}));heap.failAt=SIZE_MAX;old.check(q);ie_destroy(&q);CHECK(heap.bytes==0);++faults;}
  for(size_t point:{size_t(1),size_t(2),size_t(8),size_t(30)}){IEDoc q=make(heap);Old old(q);Reader input{r.bytes};Cancel cancel{0,point};const bool ok=f.load(&q,Reader::read,&input,{Cancel::test,&cancel,nullptr});CHECK(!ok||cancel.calls<point);if(!ok)old.check(q);else CHECK(q.current.width==int(f.w)&&q.current.height==int(f.h));ie_destroy(&q);CHECK(heap.bytes==0);}
  for(size_t n:{size_t(0),size_t(12),r.bytes.size()/2,r.bytes.size()-1}){IEDoc q=make(heap);Old old(q);Reader input{r.bytes};input.stopAt=n;CHECK(!f.load(&q,Reader::read,&input,{}));old.check(q);ie_destroy(&q);CHECK(heap.bytes==0);}
 }
 for(auto importer:{Tab5ImageCodec::loadPng,Tab5ImageCodec::loadWebp}){IEDoc d=make(heap);Old old(d);Reader r{file(root+(importer==Tab5ImageCodec::loadPng?"/too_large.png":"/too_large.webp"))};CHECK(!importer(&d,Reader::read,&r,{}));old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
 // Native BMP size is never clamped to 1024, even on tall/wide canvases.
 for(const auto* name:{"large.bmp","thin.bmp"}){
  IEDoc d=make(heap);Reader r{file(root+"/"+name)};CHECK(ie_load_bmp(&d,Reader::read,&r));const bool thin=std::string(name)=="thin.bmp";
  CHECK(d.current.width==(thin?8192:1600)&&d.current.height==(thin?16:1200));
  for(size_t i=0;i<size_t(d.current.width)*d.current.height;i+=17)CHECK(d.current.layers[0].pixels[i]==IE_RGBA(37,160,219,255));
  Writer out;out.chunk=13;CHECK(ie_save_bmp(&d,Writer::write,&out));Reader again{out.bytes};CHECK(ie_load_bmp(&d,Reader::read,&again));CHECK(d.current.width==(thin?8192:1600)&&d.current.height==(thin?16:1200));
  for(size_t i=0;i<size_t(d.current.width)*d.current.height;i+=17)CHECK(d.current.layers[0].pixels[i]==IE_RGBA(37,160,219,255));
  if(thin){std::ofstream bmp(root+"/export_wide.bmp",std::ios::binary);bmp.write(reinterpret_cast<const char*>(out.bytes.data()),out.bytes.size());}
  ie_destroy(&d);CHECK(heap.bytes==0);
  for(size_t fault=0;fault<2;++fault){IEDoc q=make(heap);Old old(q);Reader input{r.bytes};heap.failAt=heap.calls+fault;CHECK(!ie_load_bmp(&q,Reader::read,&input));heap.failAt=SIZE_MAX;old.check(q);ie_destroy(&q);CHECK(heap.bytes==0);++faults;}
  for(size_t n:{size_t(54),r.bytes.size()/2,r.bytes.size()-1}){IEDoc q=make(heap);Old old(q);Reader input{r.bytes};input.stopAt=n;CHECK(!ie_load_bmp(&q,Reader::read,&input));old.check(q);ie_destroy(&q);CHECK(heap.bytes==0);}
 }

 // Import refusal with a large existing undo snapshot must not reclaim it.
 for(int format=0;format<5;++format){
  IEDoc d=make(heap);Reader seed{file(root+"/large.bmp")};CHECK(ie_load_bmp(&d,Reader::read,&seed));CHECK(ie_begin(&d));d.current.layers[0].pixels[100]=IE_RGBA(7,31,55,255);
  const auto* pixels=d.current.layers[0].pixels;const auto* undo=d.undo[0];const size_t retained=heap.bytes;const unsigned undoCount=d.undo_count;const int dirty=d.dirty;
  Reader r{file(root+(format==0?"/half.jpg":format==1?"/large.png":format==2?"/large.webp":"/large.bmp"))};
  if(format==4){Writer project;CHECK(ie_save_project(&d,Writer::write,&project));r.bytes=std::move(project.bytes);}
  const int ok=format==0?Tab5ImageCodec::loadJpeg(&d,Reader::read,&r):format==1?Tab5ImageCodec::loadPng(&d,Reader::read,&r):format==2?Tab5ImageCodec::loadWebp(&d,Reader::read,&r):format==3?ie_load_bmp(&d,Reader::read,&r):ie_load_project(&d,Reader::read,&r);
  CHECK(!ok&&heap.bytes==retained);CHECK(d.current.layers[0].pixels==pixels&&d.undo_count==undoCount&&d.undo[0]==undo&&d.dirty==dirty);CHECK(pixels[100]==IE_RGBA(7,31,55,255));CHECK(ie_undo(&d));CHECK(d.current.layers[0].pixels[100]==IE_RGBA(37,160,219,255));ie_destroy(&d);CHECK(heap.bytes==0);
 }
 // T5I allocation and truncated input preserve the previous document/history.
 {IEDoc source=make(heap);CHECK(ie_layer_add(&source,"Alpha"));Writer w;CHECK(ie_save_project(&source,Writer::write,&w));ie_destroy(&source);
  for(size_t fault=0;fault<2;++fault){IEDoc d=make(heap);Old old(d);Reader r{w.bytes};heap.failAt=heap.calls+fault;CHECK(!ie_load_project(&d,Reader::read,&r));heap.failAt=SIZE_MAX;old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);++faults;}
  for(size_t n:{size_t(32),w.bytes.size()/2,w.bytes.size()-1}){IEDoc d=make(heap);Old old(d);Reader r{w.bytes};r.stopAt=n;CHECK(!ie_load_project(&d,Reader::read,&r));old.check(d);ie_destroy(&d);CHECK(heap.bytes==0);}
 }
 // BMP chunk/padding IO errors stop cleanly without changing source pixels.
 {IEDoc d=make(heap);CHECK(ie_new(&d,8191,7,IE_RGBA(17,99,163,255)));Writer whole;CHECK(ie_save_bmp(&d,Writer::write,&whole));const auto*pixels=d.current.layers[0].pixels;
  for(size_t n:{size_t(0),size_t(54),size_t(822),whole.bytes.size()-1}){Writer out;out.stopAt=n;CHECK(!ie_save_bmp(&d,Writer::write,&out));CHECK(d.current.layers[0].pixels==pixels&&pixels[8190]==IE_RGBA(17,99,163,255));}
  Reader r{whole.bytes};CHECK(ie_load_bmp(&d,Reader::read,&r));CHECK(d.current.width==8191&&d.current.height==7&&d.current.layers[0].pixels[8190]==IE_RGBA(17,99,163,255));ie_destroy(&d);CHECK(heap.bytes==0);
 }
 // A real 18 MiB owner envelope includes the preview; a refused brush leaves
 // the full-resolution image and transaction untouched and releases scratch.
 {IEDoc d=make(heap);Reader r{file(root+"/fullhd.jpg")};CHECK(Tab5ImageCodec::loadJpeg(&d,Reader::read,&r));void* view=Heap::alloc(&heap,1280u*600u*2u);CHECK(view);const size_t before=heap.bytes;const auto* pixels=d.current.layers[0].pixels;const uint32_t sample=pixels[100];CHECK(!ie_brush_begin(&d));CHECK(heap.bytes==before&&d.current.layers[0].pixels==pixels&&pixels[100]==sample&&!d.gesture&&!d.brush_coverage&&!d.undo_count);Heap::free(&heap,view);ie_destroy(&d);CHECK(heap.bytes==0);}
 CHECK(heap.bytes==0&&heap.blocks.empty());
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"faults\":"<<faults<<",\"peakBytes\":"<<heap.peak<<",\"remainingBytes\":"<<heap.bytes<<",\"cooperations\":"<<yields<<"}\n";
}
