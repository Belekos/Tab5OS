#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>

// Table records share the link sidecar. Plain link readers ignore their prefix.
// Text remains ordinary UTF-8; offsets also preserve links inside table cells.
namespace Tab5WikiTable {
using Sink = bool (*)(void*, const char*, size_t);
struct Cell {
 uint32_t begin=0,end=0,table=0,row=0,column=0,rows=1,columns=1,heading=0;
};
inline bool cell(const char* text,Cell& c){
 unsigned a[9]{};int used=0;
 if(std::sscanf(text,"C\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u%n",a,a+1,a+2,a+3,a+4,a+5,a+6,a+7,a+8,&used)!=9||text[used])return false;
 c={a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]};
 // Last field is a format version; reject stale/unknown layouts.
 return a[8]==1&&c.end>=c.begin&&c.columns&&c.columns<=64&&c.rows&&c.column<64&&c.column+c.columns<=64&&c.rows<=1024&&c.row<=1000000&&c.heading<=1;
}
class Recorder {
 Sink sink_;void* context_;unsigned depth_=0,id_=0,row_=0,column_=0,columns_=0;
 uint32_t occupied_[64]{};Cell active_{};bool inCell_=false,hasRow_=false,valid_=true,good_=true;
 bool send(const char* data,size_t n){if(sink_&&!sink_(context_,data,n))good_=false;return good_;}
 public:
 Recorder(Sink sink,void* context):sink_(sink),context_(context){}
 bool good()const{return good_;}
 unsigned depth()const{return depth_;}
 bool inCell()const{return inCell_;}
 void closeCell(uint32_t offset){
  if(!inCell_)return;active_.end=offset;char line[160];
  int n=std::snprintf(line,sizeof(line),"C\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t1\n",active_.begin,active_.end,active_.table,active_.row,active_.column,active_.rows,active_.columns,active_.heading);
  if(valid_)send(line,size_t(n));inCell_=false;
 }
 void begin(uint32_t offset){
  if(depth_++)return;++id_;row_=column_=columns_=0;hasRow_=false;valid_=true;inCell_=false;std::memset(occupied_,0,sizeof(occupied_));
  char line[64];int n=std::snprintf(line,sizeof(line),"B\t%u\t%u\n",offset,id_);send(line,size_t(n));
 }
 void row(uint32_t offset){
  if(depth_!=1)return;closeCell(offset);if(hasRow_)++row_;hasRow_=true;column_=0;
 }
 void openCell(uint32_t offset,bool heading,unsigned colspan=1,unsigned rowspan=1){
  if(depth_!=1)return;closeCell(offset);if(!hasRow_){hasRow_=true;row_=0;}
  colspan=std::max(1u,colspan);rowspan=std::max(1u,rowspan);
  if(colspan>64||rowspan>1024){valid_=false;return;}
  while(column_+colspan<=64){bool free=true;for(unsigned i=0;i<colspan;++i)if(occupied_[column_+i]>row_){free=false;break;}if(free)break;++column_;}
  if(column_+colspan>64){valid_=false;return;}
  active_={offset,offset,id_,row_,column_,rowspan,colspan,unsigned(heading)};inCell_=true;
  for(unsigned i=0;i<colspan;++i)occupied_[column_+i]=row_+rowspan;
  column_+=colspan;columns_=std::max(columns_,column_);
 }
 void end(uint32_t offset){
  if(!depth_)return;if(--depth_)return;closeCell(offset);char line[64];int n=std::snprintf(line,sizeof(line),"E\t%u\t%u\t%u\n",offset,id_,valid_?columns_:0);send(line,size_t(n));
 }
 void finish(uint32_t offset){while(depth_)end(offset);}
};
}
