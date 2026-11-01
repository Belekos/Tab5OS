#include <cerrno>
#include "Tab5ImageWrite.h"
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <iostream>
struct Data{std::vector<uint8_t> bytes;bool error=false;};
struct Fs;
struct File{
 Fs* fs=nullptr;std::shared_ptr<Data> data;bool opened=false;
 explicit operator bool()const{return opened;}
 size_t write(const uint8_t*,size_t);void flush();
 bool getWriteError()const{return data&&data->error;}
 size_t size()const{return data?data->bytes.size():0;}
 void close(){opened=false;}
};
struct Fs{
 std::map<std::string,std::shared_ptr<Data>> entries;
 bool failMkdir=false,failOpen=false,failRemove=false,failFlush=false;int shortWrite=-1;unsigned renames=0,failRename=0,failRename2=0;
 bool exists(const char* p){if(entries.count(p))return true;errno=ENOENT;return false;}
 bool mkdir(const char* p){if(failMkdir)return false;entries[p]=std::make_shared<Data>();return true;}
 bool remove(const char* p){if(failRemove)return false;return entries.erase(p);}
 bool rename(const char*a,const char*b){++renames;if(renames==failRename||renames==failRename2||!entries.count(a)||entries.count(b))return false;entries[b]=entries[a];entries.erase(a);return true;}
 File open(const char*p,const char*){if(failOpen)return {};auto& d=entries[p];if(!d)d=std::make_shared<Data>();return {this,d,true};}
 std::string text(const char*p){auto i=entries.find(p);if(i==entries.end())return "<missing>";return std::string(i->second->bytes.begin(),i->second->bytes.end());}
 void put(const char*p,const char*v){auto d=std::make_shared<Data>();d->bytes.assign(v,v+strlen(v));entries[p]=d;}
};
size_t File::write(const uint8_t*p,size_t n){if(fs->shortWrite>=0)n=std::min(n,size_t(fs->shortWrite));data->bytes.insert(data->bytes.end(),p,p+n);return n;}
void File::flush(){if(fs->failFlush)errno=EIO;}
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"check %u line %d: %s\n",checks,__LINE__,#x);std::abort();}}while(0)
int main(){
 using Writer=Tab5ImageWrite<Fs,File>;using Jpeg=Tab5ImageWrite<Fs,File,true>;
 CHECK(!Writer::valid(nullptr));
 for(const char* root:{"/Picture","/Images"}){
  std::string prefix=std::string(root)+"/";auto name=[&](const char*n){return prefix+n;};
  for(const char* suffix:{"a.bmp","test long.t5i","été.bmp","A.BMP","A.T5I"}){CHECK(Writer::valid(name(suffix).c_str()));CHECK(Jpeg::valid(name(suffix).c_str()));}
  for(const char* suffix:{"a.jpg","a.jpeg","a.JPG","a.JPEG"}){CHECK(!Writer::valid(name(suffix).c_str()));CHECK(Jpeg::valid(name(suffix).c_str()));}
  for(const char* suffix:{".hidden.bmp","../b.bmp","sub/b.bmp","b.exe","a.bmp/evil","a?b.bmp","a\nb.bmp","a.bmp.tab5tmp","a~b.bmp","a\\b.bmp","a|b.bmp","a:b.bmp","a\x7f.bmp","a"}){CHECK(!Writer::valid(name(suffix).c_str()));CHECK(!Jpeg::valid(name(suffix).c_str()));}
  auto longest=prefix+std::string(260-prefix.size()-4,'a')+".bmp";CHECK(Writer::valid(longest.c_str()));longest.insert(prefix.size(),"a");CHECK(!Writer::valid(longest.c_str()));
  for(unsigned cycle=0;cycle<120;++cycle){
   const auto p=name("a.t5i"),q=name("b.bmp");Fs fs;Writer w;fs.put(p.c_str(),"old");fs.put("/Documents/keep.txt","keep");
   int h=w.begin(fs,p.c_str(),"w");CHECK(h>0);CHECK(fs.exists(root));CHECK(!fs.exists(!strcmp(root,"/Picture")?"/Images":"/Picture"));CHECK(w.begin(fs,q.c_str(),"w")<0);CHECK(w.chunk(h,"new",3)==3);CHECK(fs.text(p.c_str())=="old");CHECK(w.finish(fs,h,true)==0);CHECK(fs.text(p.c_str())=="new");CHECK(fs.text("/Documents/keep.txt")=="keep");CHECK(!fs.exists((p+".tab5old").c_str()));CHECK(!fs.exists((p+".tab5tmp").c_str()));CHECK(w.chunk(h,"x",1)<0);CHECK(w.finish(fs,h,true)<0);
   int next=w.begin(fs,p.c_str(),"w");CHECK(next>0&&next!=h);CHECK(w.chunk(h,"x",1)<0);CHECK(w.chunk(next,"lost",4)==4);w.abort(fs);CHECK(fs.text(p.c_str())=="new");CHECK(!fs.exists((p+".tab5tmp").c_str()));
  }
  for(unsigned failure=0;failure<6;++failure){
   Fs fs;Writer w;auto p=name("a.bmp");fs.put(p.c_str(),"original");int h=w.begin(fs,p.c_str(),"w");CHECK(h>0);
   if(failure==0)fs.shortWrite=1;if(failure==1)fs.failFlush=true;if(failure==2)fs.failRename=1;if(failure==3)fs.failRename=2;if(failure==4){fs.failRename=2;fs.failRename2=3;}
   int r=w.chunk(h,"complete",8);CHECK(failure==0?r<0:r==8);CHECK(w.finish(fs,h,failure!=5)<=(failure==5?0:-1));
   if(failure==4){CHECK(fs.text((p+".tab5old").c_str())=="original");fs.failRename=fs.failRename2=0;CHECK(Writer::recover(fs,p.c_str()));CHECK(fs.text(p.c_str())=="original");int recovered=w.begin(fs,p.c_str(),"w");CHECK(recovered>0);CHECK(fs.text(p.c_str())=="original");w.abort(fs);}else CHECK(fs.text(p.c_str())=="original");
   CHECK(!fs.exists((p+".tab5tmp").c_str()));
  }
  {Fs fs;Writer w;auto p=name("a.bmp");fs.failMkdir=true;CHECK(w.begin(fs,p.c_str(),"w")<0);fs.failMkdir=false;fs.failOpen=true;CHECK(w.begin(fs,p.c_str(),"w")<0);}
  {Fs fs;Writer w;auto p=name("a.bmp");int h=w.begin(fs,p.c_str(),"w");CHECK(h>0);CHECK(w.chunk(h,"x",4097)<0);CHECK(w.finish(fs,h,true)<0);CHECK(!fs.exists(p.c_str()));}
  {Fs fs;Writer w;auto p=name("a.bmp");int h=w.begin(fs,p.c_str(),"w");std::vector<char> chunk(4096,'x');for(size_t n=0;n<Writer::MaxBytes;n+=chunk.size())CHECK(w.chunk(h,chunk.data(),chunk.size())==4096);CHECK(w.chunk(h,"x",1)<0);CHECK(w.finish(fs,h,true)<0);CHECK(!fs.exists(p.c_str()));}
 }
 for(const char* path:{"/a.bmp","/PictureOther/a.bmp","/Pictures/a.bmp","/ImagesOther/a.bmp","/Picture/","/Images/","picture/a.bmp","/USB/Picture/a.bmp"})CHECK(!Writer::valid(path));
 std::cout<<"{\"checks\":"<<checks<<"}\n";
}
