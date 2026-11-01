#include <cerrno>
#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <iostream>
#include "Tab5ImageWrite.h"
#include "Tab5AppInputQueue.h"
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
#define CHECK(x) do{++checks;assert(x);}while(0)
int main(){
 using Writer=Tab5ImageWrite<Fs,File>;
 for(const char*p:{"/Images/a.bmp","/Images/test long.t5i","/Images/été.bmp","/Images/A.BMP","/Images/A.T5I"})CHECK(Writer::valid(p));
 for(const char*p:{"/a.bmp","/Images/.hidden.bmp","/Images/../b.bmp","/Images/sub/b.bmp","/Images/b.exe","/Images/a.bmp/evil","/Images/a?b.bmp","/Images/a\nb.bmp","/Images/a.bmp.tab5tmp"})CHECK(!Writer::valid(p));
 for(unsigned cycle=0;cycle<200;++cycle){
 Fs fs;Writer w;fs.put("/Images/a.t5i","old");int h=w.begin(fs,"/Images/a.t5i","w");CHECK(h>0);CHECK(w.begin(fs,"/Images/b.bmp","w")<0);CHECK(w.chunk(h,"new",3)==3);CHECK(fs.text("/Images/a.t5i")=="old");CHECK(w.finish(fs,h,1)==0);CHECK(fs.text("/Images/a.t5i")=="new");CHECK(!fs.exists("/Images/a.t5i.tab5old"));CHECK(!fs.exists("/Images/a.t5i.tab5tmp"));CHECK(w.chunk(h,"x",1)<0);CHECK(w.finish(fs,h,1)<0);
 int next=w.begin(fs,"/Images/a.t5i","w");CHECK(next>0&&next!=h);CHECK(w.chunk(h,"x",1)<0);CHECK(w.chunk(next,"lost",4)==4);w.abort(fs);CHECK(fs.text("/Images/a.t5i")=="new");CHECK(!fs.exists("/Images/a.t5i.tab5tmp"));
 }
 for(unsigned failure=0;failure<6;++failure){
 Fs fs;Writer w;fs.put("/Images/a.bmp","original");int h=w.begin(fs,"/Images/a.bmp","w");CHECK(h>0);
 if(failure==0)fs.shortWrite=1;if(failure==1)fs.failFlush=true;if(failure==2)fs.failRename=1;if(failure==3)fs.failRename=2;if(failure==4){fs.failRename=2;fs.failRename2=3;}
 int r=w.chunk(h,"complete",8);CHECK(failure==0?r<0:r==8);CHECK(w.finish(fs,h,failure!=5)<=(failure==5?0:-1));
 if(failure==4){CHECK(fs.text("/Images/a.bmp.tab5old")=="original");fs.failRename=fs.failRename2=0;CHECK(Writer::recover(fs,"/Images/a.bmp"));CHECK(fs.text("/Images/a.bmp")=="original");int recovered=w.begin(fs,"/Images/a.bmp","w");CHECK(recovered>0);CHECK(fs.text("/Images/a.bmp")=="original");w.abort(fs);}else CHECK(fs.text("/Images/a.bmp")=="original");
 CHECK(!fs.exists("/Images/a.bmp.tab5tmp"));
 }
 {Fs fs;Writer w;fs.failMkdir=true;CHECK(w.begin(fs,"/Images/a.bmp","w")<0);fs.failMkdir=false;fs.failOpen=true;CHECK(w.begin(fs,"/Images/a.bmp","w")<0);}
 {Fs fs;Writer w;int h=w.begin(fs,"/Images/a.bmp","w");CHECK(h>0);CHECK(w.chunk(h,"x",4097)<0);CHECK(w.finish(fs,h,1)<0);CHECK(!fs.exists("/Images/a.bmp"));}
 {Fs fs;Writer w;int h=w.begin(fs,"/Images/a.bmp","w");std::vector<char> chunk(4096,'x');for(size_t n=0;n<Writer::MaxBytes;n+=chunk.size())CHECK(w.chunk(h,chunk.data(),chunk.size())==4096);CHECK(w.chunk(h,"x",1)<0);CHECK(w.finish(fs,h,1)<0);CHECK(!fs.exists("/Images/a.bmp"));}
 {Tab5AppInputQueue q;tab5_app_input_event out{};CHECK(!q.pop(out));tab5_app_input_event e{};e.size=sizeof(e);e.type=TAB5_INPUT_DOWN;q.push(e);e.type=TAB5_INPUT_MOVE;for(int i=0;i<10000;++i){e.x=i;q.push(e);}e.type=TAB5_INPUT_UP;q.push(e);CHECK(q.pop(out)&&out.type==TAB5_INPUT_DOWN);CHECK(q.pop(out)&&out.type==TAB5_INPUT_MOVE&&out.x==9999);CHECK(q.pop(out)&&out.type==TAB5_INPUT_UP);CHECK(!q.pop(out));
 e.type=TAB5_INPUT_KEY;for(unsigned i=0;i<33;++i){e.key=i;q.push(e);}CHECK(q.pop(out)&&out.type==TAB5_INPUT_CANCEL);CHECK(q.pop(out)&&out.type==TAB5_INPUT_KEY&&out.key==32);CHECK(!q.pop(out));
 for(unsigned i=0;i<32;++i)q.push(e);e.type=TAB5_INPUT_UP;q.push(e);CHECK(q.pop(out)&&out.type==TAB5_INPUT_CANCEL);CHECK(!q.pop(out));e.type=TAB5_INPUT_CLOSE;q.push(e);CHECK(q.pop(out)&&out.type==TAB5_INPUT_CLOSE);
 }
 std::cout<<"checks="<<checks<<"\n";
}
