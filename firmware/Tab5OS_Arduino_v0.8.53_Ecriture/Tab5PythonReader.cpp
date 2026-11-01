#include "Tab5VolumeFS.h"
#include "Tab5PythonReader.h"
#include "Tab5PythonUI.h"
#include "Tab5Storage.h"
#include "Tab5FilePolicy.h"
#include "Tab5Focus.h"
#include "Tab5ImageDecode.h"
#include "Tab5ReaderLines.h"
#include "Tab5WikiTable.h"
#include "Tab5Memory.h"
#include "Tab5UiStyle.h"
#include <vector>
#include <string>
#include <misc/cache/instance/lv_image_cache.h>
#include <misc/cache/instance/lv_image_header_cache.h>
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <new>
#include <cstdlib>

namespace Tab5PythonReader {
namespace {
constexpr size_t WindowSize=32768;
constexpr unsigned HistorySize=128;
constexpr unsigned FontSizes[]={20,22,24,28,32,36};
Hooks hooks;
template<class T, size_t Limit> struct ReaderArray {
 Tab5PsramTable<T,Limit> data; size_t count=0;
 bool push_back(const T& value){if(!data.ensure(count+1))return false;data[count++]=value;return true;}
 void clear(){count=0;data.clear();}
 size_t size()const{return count;} bool empty()const{return !count;}
 T& back(){return data[count-1];} T& operator[](size_t i){return data[i];}
 T* begin(){return count?&data[0]:nullptr;} T* end(){return count?&data[0]+count:nullptr;}
};
struct State {
 Tab5PythonUI::ReaderRequest request;
 lv_obj_t *screen=nullptr,*text=nullptr,*progress=nullptr,*popup=nullptr,*values=nullptr,*picture=nullptr,*linkLayer=nullptr,*imageNote=nullptr,*tablePanel=nullptr,*pageLayer=nullptr,*previousButton=nullptr,*nextButton=nullptr;
 struct Table {uint32_t begin,end,id,columns,records;};ReaderArray<Table,2048> tables;
 struct CellView {uint32_t begin,end;lv_obj_t* box;lv_obj_t* label;int left,top;};ReaderArray<CellView,192> cellViews;
 struct Media {uint32_t offset;char path[288];};Media media[8]{};unsigned mediaCount=0;int pictureIndex=-2;
 struct Link {char target[768];};Link links[40]{};unsigned linkCount=0;
 Tab5ImageDecode::Image illustration;lv_image_dsc_t illustrationDescriptor{};
 uint32_t firstTextOffset=0;bool imageAttempted=false;int imageSpace=0;
 const lv_font_t* font=nullptr;
 char* window=nullptr;
 uint32_t size=0,offset=0,next=0,fingerprint=0,dirtyAt=0;
 uint32_t linkFileOffset=0,linkArticleOffset=0,linkFileSize=0;
 uint32_t history[HistorySize]{};
 unsigned historyCount=0,fontIndex=2,spacing=4;
 int width=680,height=1168,fullHeight=1168;
 bool dirty=false;
 char savePath[100]{};
};
State* state=nullptr;
const char* tr(const char* text){return hooks.translate?hooks.translate(text):text;}
const lv_font_t* uiFont(){return hooks.uiFont?hooks.uiFont():&lv_font_montserrat_20;}
lv_color_t color(unsigned i){return lv_color_hex(hooks.color?hooks.color(i):i==2?0xf1f3f5:i==3?0xb7bec7:i==1?0x222830:0x11151a);}
uint32_t hash(const void* bytes,size_t size,uint32_t seed=2166136261u){const auto* p=static_cast<const uint8_t*>(bytes);while(size--)seed=(seed^*p++)*16777619u;return seed;}
void dirty(){state->dirty=true;state->dirtyAt=millis();}
void save(){
 if(!state||!state->dirty)return;
 Tab5StorageUse lease;if(!lease)return;
 if(!tab5FS.exists("/System")&&!tab5FS.mkdir("/System"))return;
 if(!tab5FS.exists("/System/python-reader")&&!tab5FS.mkdir("/System/python-reader"))return;
 const uint32_t values[]={0x31525054,state->size,state->fingerprint,state->offset,state->fontIndex,state->spacing};
 char temporary[112];snprintf(temporary,sizeof(temporary),"%s.tmp",state->savePath);
 tab5FS.remove(temporary);File file=tab5FS.open(temporary,FILE_WRITE);
 const bool ok=file&&file.write(reinterpret_cast<const uint8_t*>(values),sizeof(values))==sizeof(values);
 if(file){file.flush();file.close();}
 if(!ok){tab5FS.remove(temporary);return;}
 // Position loss on a power failure is harmless; never alter the article itself.
 tab5FS.remove(state->savePath);
 if(!tab5FS.rename(temporary,state->savePath))return;
 state->dirty=false;
}
void loadPosition(){
 File file=tab5FS.open(state->savePath);uint32_t values[6]{};
 if(file&&file.size()==sizeof(values)&&file.read(reinterpret_cast<uint8_t*>(values),sizeof(values))==sizeof(values)&&
    values[0]==0x31525054&&values[1]==state->size&&values[2]==state->fingerprint){
  state->offset=std::min(values[3],state->size?state->size-1:0);
  state->fontIndex=std::min<unsigned>(values[4],5);state->spacing=std::min<unsigned>(values[5],12);
 }
}
size_t boundary(const char* text,size_t position,size_t length){
 while(position&&position<length&&(static_cast<unsigned char>(text[position])&0xc0)==0x80)--position;
 return position;
}
bool white(char c){return c==' '||c=='\n'||c=='\r'||c=='\t';}
int measured(size_t begin,size_t end){
 char saved=state->window[end];state->window[end]=0;
 // Measure the real label after shaping (Arabic/Persian included), rather
 // than measuring unshaped input and rendering a differently shaped string.
 lv_obj_set_style_text_font(state->text,state->font,0);lv_obj_set_style_text_line_space(state->text,state->spacing,0);
 lv_obj_set_size(state->text,state->width,LV_SIZE_CONTENT);lv_obj_set_pos(state->text,-4000,-4000);
 lv_label_set_text(state->text,state->window+begin);lv_obj_update_layout(state->text);
 const int height=lv_obj_get_height(state->text);state->window[end]=saved;return height;
}
size_t fitPrefix(size_t begin,size_t length,int available){
 size_t lo=begin,hi=std::min<size_t>(begin+1024,length);
 while(hi<length&&measured(begin,boundary(state->window,hi,length))<=available){lo=hi;hi=std::min(length,begin+(hi-begin)*2);}
 while(lo<hi){size_t middle=lo+(hi-lo+1)/2;size_t end=boundary(state->window,middle,length);
  if(end<=begin){lo=middle;continue;}
  if(measured(begin,end)<=available)lo=middle;else hi=middle-1;
 }
 size_t fit=boundary(state->window,lo,length);
 if(fit<length){size_t word=fit;while(word>begin&&!white(state->window[word]))--word;if(word>begin+(fit-begin)/2)fit=word;}
 return fit;
}
size_t fitSuffix(size_t length){
 size_t probe=std::min<size_t>(1024,length),lo=length-probe,hi=length;
 while(lo&&measured(boundary(state->window,lo,length),length)<=state->height){hi=lo;probe=std::min(length,probe*2);lo=length-probe;}
 while(lo<hi){size_t middle=lo+(hi-lo)/2;size_t start=boundary(state->window,middle,length);
  if(measured(start,length)<=state->height)hi=middle;else lo=middle+1;
 }
 size_t start=boundary(state->window,lo,length);
 while(start<length&&measured(start,length)>state->height){++start;while(start<length&&(static_cast<unsigned char>(state->window[start])&0xc0)==0x80)++start;}
 if(start&&start<length&&!white(state->window[start-1])){
  size_t word=start;while(word<length&&!white(state->window[word]))++word;
  if(word<length)start=word;
 }
 while(start<length&&white(state->window[start]))++start;
 return start;
}
int readWindow(uint32_t offset,size_t wanted,size_t& received){
 received=0;Tab5StorageUse lease;if(!lease)return -EBUSY;
 File file=tab5FS.open(state->request.path);
 if(!file||file.isDirectory())return -ENOENT;
 if(file.size()!=state->size)return -ESTALE;
 if(!file.seek(offset))return -EIO;
 uint32_t yielded=millis();
 while(received<wanted){size_t count=file.read(reinterpret_cast<uint8_t*>(state->window+received),std::min<size_t>(4096,wanted-received));
  if(!count){tab5StorageFault("Lecture document Python",state->request.path,EIO);return -EIO;}
  received+=count;if(millis()-yielded>=4){delay(1);yielded=millis();}
 }
 state->window[received]=0;
 for(size_t i=0;i<received;++i)if(!state->window[i]||state->window[i]=='\r')state->window[i]=' ';
 return 0;
}
void status(){
 if(!state||!state->progress)return;
 const unsigned percent=!state->size?100:unsigned(uint64_t(state->next>=state->size?state->size:state->offset)*100/state->size);
 char text[60];snprintf(text,sizeof(text),"%u %%",percent);lv_label_set_text(state->progress,text);
 if(state->previousButton){if(state->offset)lv_obj_remove_flag(state->previousButton,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(state->previousButton,LV_OBJ_FLAG_HIDDEN);}
 if(state->nextButton){if(state->next<state->size)lv_obj_remove_flag(state->nextButton,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(state->nextButton,LV_OBJ_FLAG_HIDDEN);}
 if(state->values){snprintf(text,sizeof(text),"%u px  ·  %u",FontSizes[state->fontIndex],state->spacing);lv_label_set_text(state->values,text);}
}
void clearPicture(){
 if(!state)return;
 if(state->picture){lv_obj_delete(state->picture);state->picture=nullptr;}
 if(state->imageNote){lv_obj_delete(state->imageNote);state->imageNote=nullptr;}
 lv_image_cache_drop(&state->illustrationDescriptor);lv_image_header_cache_drop(&state->illustrationDescriptor);state->illustration.clear();state->imageSpace=0;
}
bool thumbnailHeader(File& file){
 uint8_t b[24];if(file.read(b,sizeof(b))!=sizeof(b)||!file.seek(0))return false;
 auto dimensions=[](uint32_t w,uint32_t h){return w&&h&&w<=1024&&h<=1024&&uint64_t(w)*h<=262144;};
 if(!memcmp(b,"\x89PNG\r\n\x1a\n",8)){
  auto big=[](const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];};return dimensions(big(b+16),big(b+20));
 }
 // The bounded WebP decoder validates canvas dimensions before allocating pixels.
 if(!memcmp(b,"RIFF",4)&&!memcmp(b+8,"WEBP",4))return file.size()<=131072&&file.seek(0);
 if(b[0]!=0xff||b[1]!=0xd8||!file.seek(2))return false;
 size_t position=2;
 while(position+4<=file.size()){
  if(file.read(b,2)!=2||b[0]!=0xff)return false;position+=2;
  while(b[1]==0xff){if(file.read(b+1,1)!=1)return false;++position;}
  const unsigned marker=b[1];if(marker==0xda||marker==0xd9||marker==0||marker==0xd8)return false;
  if(file.read(b,2)!=2)return false;position+=2;const size_t length=unsigned(b[0])*256+b[1];
  if(length<2||length-2>file.size()-position)return false;
  if(marker>=0xc0&&marker<=0xc3){if(length<8||file.read(b,5)!=5)return false;const bool ok=dimensions(unsigned(b[3])*256+b[4],unsigned(b[1])*256+b[2]);return file.seek(0)&&ok;}
  position+=length-2;if(!file.seek(position))return false;
 }
 return false;
}
void end(int result,const char* target=nullptr);
size_t sidecarRead(void* context,uint32_t offset,uint8_t* data,size_t count){
 File& file=*static_cast<File*>(context);
 if(!file.seek(offset))return 0;
 return file.read(data,count);
}
void sidecarYield(){static uint32_t last=0;const uint32_t now=millis();if(uint32_t(now-last)>=4){last=now;delay(1);}}
void loadTables(){
 if(!state->request.rich)return;char filename[288];snprintf(filename,sizeof(filename),"%s.links",state->request.path);File file=tab5FS.open(filename);
 if(!file||file.isDirectory()||file.size()>Tab5Reader::Lines::MaxBytes)return;
 Tab5Reader::Lines lines(&file,sidecarRead,file.size(),0,sidecarYield);char row[832];State::Table pending{};bool firstCell=false;
 while(lines.next(row,sizeof(row))){unsigned at=0,id=0,columns=0;
  if(sscanf(row,"B\t%u\t%u",&at,&id)==2){pending={at,0,id,0,lines.position()};firstCell=true;}
  else if(row[0]=='C'&&firstCell){Tab5WikiTable::Cell cell;if(Tab5WikiTable::cell(row,cell)&&cell.table==pending.id){pending.begin=cell.begin;firstCell=false;}}
  else if(sscanf(row,"E\t%u\t%u\t%u",&at,&id,&columns)==3&&id==pending.id&&at>pending.begin&&at<=state->size&&columns&&columns<=64){
   if(state->tables.empty()||pending.begin>=state->tables.back().end){pending.end=at;pending.columns=columns;state->tables.push_back(pending);}
  }
 }
}
void clearTable(){
 if((state->pageLayer&&Tab5Focus.contains(state->pageLayer,Tab5Focus.focused()))||
    (state->linkLayer&&Tab5Focus.contains(state->linkLayer,Tab5Focus.focused())))Tab5Focus.clear();
 if(state->linkLayer){lv_obj_delete(state->linkLayer);state->linkLayer=nullptr;}
 state->cellViews.clear();state->tablePanel=nullptr;
 if(state->pageLayer){lv_obj_delete(state->pageLayer);state->pageLayer=nullptr;}
}
// At most 128 cells and 32 KiB of article text are resident. Oversized tables
// remain scrollable, while short tables share their page with adjacent text.
int renderTable(const State::Table& table,uint32_t cursor,uint32_t limit,int top,int available,uint32_t& finish){
 Tab5StorageUse lease;if(!lease)return 0;char filename[288];snprintf(filename,sizeof(filename),"%s.links",state->request.path);File file=tab5FS.open(filename);if(!file)return 0;
 Tab5Reader::Lines lines(&file,sidecarRead,file.size(),table.records,sidecarYield);char row[832];ReaderArray<Tab5WikiTable::Cell,128> cells;
 uint32_t firstRow=UINT32_MAX,lastRow=0;limit=std::min(limit,table.end);finish=limit;
 while(lines.next(row,sizeof(row))){if(row[0]=='E')break;Tab5WikiTable::Cell cell;if(!Tab5WikiTable::cell(row,cell)||cell.table!=table.id||cell.end<=cursor||cell.end>table.end)continue;
  if(firstRow==UINT32_MAX)firstRow=cell.row;if(cell.row<firstRow||cell.row-firstRow>1024)continue;
  // Stop BEFORE the next row, never unconditionally after the first cell.
  if(cell.begin>=limit||(cells.size()>=64&&cell.row!=lastRow)){finish=std::min(finish,cell.begin);break;}
  cell.begin=std::max(cell.begin,cursor);const bool clipped=cell.end>limit;
  cell.end=std::min(cell.end,limit);
  cell.end=state->offset+boundary(state->window,cell.end-state->offset,limit-state->offset);
  if(!cells.push_back(cell))return 0;lastRow=cell.row;
  if(clipped){finish=cell.end;break;}
 }
 if(cells.empty()||finish<=cursor)return 0;
 if(state->cellViews.size()+cells.size()>192)return 0;
 const int columnWidth=std::max(180,state->width/int(table.columns));
 unsigned rowCount=0;for(const auto& c:cells)rowCount=std::max(rowCount,unsigned(c.row-firstRow+c.rows));
 Tab5PsramTable<int,2049> heights,tops;if(!heights.ensure(rowCount)||!tops.ensure(rowCount+1))return 0;
 for(unsigned r=0;r<rowCount;++r)heights[r]=int(state->font->line_height)+20;
 auto height=[&](const Tab5WikiTable::Cell& c){const int width=state->width;state->width=columnWidth*c.columns-20;
  const int result=measured(c.begin-state->offset,c.end-state->offset)+20;state->width=width;return result;};
 for(const auto& c:cells)if(c.rows==1)heights[c.row-firstRow]=std::max(heights[c.row-firstRow],height(c));
 for(const auto& c:cells)if(c.rows>1){int used=0;unsigned r=c.row-firstRow;for(unsigned j=0;j<c.rows;++j)used+=heights[r+j];const int needed=height(c);if(used<needed)heights[r+c.rows-1]+=needed-used;}
 tops[0]=0;for(unsigned r=0;r<rowCount;++r)tops[r+1]=tops[r]+heights[r];
 const int used=std::min(available,tops[rowCount]);
 // Keep a useful first row visible; defer a tall table from a nearly full page.
 if(available<100&&tops[rowCount]>available)return 0;
 auto* panel=lv_obj_create(state->pageLayer);lv_obj_remove_style_all(panel);lv_obj_set_pos(panel,24,top);lv_obj_set_size(panel,state->width,used);
 lv_obj_add_flag(panel,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(panel,LV_DIR_ALL);lv_obj_set_scrollbar_mode(panel,LV_SCROLLBAR_MODE_AUTO);
 if(!state->tablePanel)state->tablePanel=panel;
 for(const auto& c:cells){const unsigned r=c.row-firstRow;auto* box=lv_obj_create(panel);lv_obj_remove_style_all(box);lv_obj_remove_flag(box,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(box,c.column*columnWidth,tops[r]);lv_obj_set_size(box,c.columns*columnWidth,tops[r+c.rows]-tops[r]);lv_obj_set_style_bg_color(box,color(c.heading?1:0),0);lv_obj_set_style_bg_opa(box,LV_OPA_COVER,0);lv_obj_set_style_border_width(box,1,0);lv_obj_set_style_border_color(box,color(3),0);lv_obj_set_style_border_opa(box,LV_OPA_40,0);
  auto* label=lv_label_create(box);lv_obj_set_pos(label,10,10);lv_obj_set_width(label,c.columns*columnWidth-20);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_font(label,state->font,0);lv_obj_set_style_text_color(label,color(2),0);lv_obj_set_style_text_line_space(label,state->spacing,0);
  char saved=state->window[c.end-state->offset];state->window[c.end-state->offset]=0;lv_label_set_text(label,state->window+c.begin-state->offset);state->window[c.end-state->offset]=saved;
  if(!state->cellViews.push_back({c.begin,c.end,box,label,10,10})){lv_obj_delete(panel);state->tablePanel=nullptr;return -ENOMEM;}
 }
 lv_obj_update_layout(panel);return used;
}
void loadMedia(){
 if(!state->request.rich||!state->request.illustrated)return;
 char filename[288];snprintf(filename,sizeof(filename),"%s.media",state->request.path);File file=tab5FS.open(filename);
 if(!file||file.isDirectory()||file.size()>Tab5Reader::Lines::MaxBytes)return;
 Tab5Reader::Lines lines(&file,sidecarRead,file.size(),0,sidecarYield);
 char row[352];unsigned scanned=0;
 while(state->mediaCount<8&&scanned++<32&&lines.next(row,sizeof(row))){
  if(!*row)continue;char* tail=nullptr;unsigned long at=strtoul(row,&tail,10);
  if(tail==row||*tail!='\t'||at>state->size)continue;const char* path=tail+1;std::string canonical;
  if(strlen(path)>=sizeof(state->media[0].path)||!Tab5Files::canonical(path,canonical))continue;
  if(state->mediaCount&&at<state->media[state->mediaCount-1].offset)continue;
  auto& media=state->media[state->mediaCount++];media.offset=at;strcpy(media.path,path);
 }
}
void pictureLayout(uint32_t at,int top,int available){
 int selected=-1;
 if(state->request.illustrated){
  if(state->request.rich){for(unsigned i=0;i<state->mediaCount;++i)if(state->media[i].offset<=at&&at-state->media[i].offset<=4)selected=int(i);}
  else if(at<=state->firstTextOffset)selected=0;
 }
 if(selected!=state->pictureIndex){clearPicture();state->imageAttempted=false;state->pictureIndex=selected;}
 if(selected>=0&&!state->imageAttempted){
  state->imageAttempted=true;Tab5StorageUse lease;
  if(lease){char name[288];if(state->request.rich)strcpy(name,state->media[selected].path);else snprintf(name,sizeof(name),"%s.image",state->request.path);File file=tab5FS.open(name);
   if(file&&!file.isDirectory()&&file.size()<=131072&&thumbnailHeader(file)){const char* error=nullptr;
    Tab5ImageDecode::Source source{&file,file.size(),[](void* p,uint8_t* b,size_t n){return static_cast<File*>(p)->read(b,n);},[](void* p,size_t at){return static_cast<File*>(p)->seek(at);}};
    if(Tab5ImageDecode::load(source,state->illustration,error,{},262144)){
     auto& image=state->illustration;unsigned scale=std::min(256U,std::min(unsigned(state->width)*256/image.width,unsigned(std::max(32,std::min(280,available-40)))*256/image.height));
     if(scale){auto& d=state->illustrationDescriptor;d={};d.header.magic=LV_IMAGE_HEADER_MAGIC;d.header.cf=LV_COLOR_FORMAT_RGB565;
      d.header.w=image.width;d.header.h=image.height;d.header.stride=image.width*2;d.data_size=image.width*image.height*2;d.data=reinterpret_cast<const uint8_t*>(image.pixels);
      state->picture=lv_image_create(state->screen);lv_image_set_src(state->picture,&d);lv_image_set_scale(state->picture,scale);
      lv_image_set_pivot(state->picture,0,0);lv_obj_set_pos(state->picture,(lv_display_get_horizontal_resolution(nullptr)-int(image.width*scale/256))/2,top);
      lv_obj_add_flag(state->picture,LV_OBJ_FLAG_CLICKABLE);
      Tab5Focus.describe(state->picture,tr("Ouvrir l’image"));
      lv_obj_add_event_cb(state->picture,[](lv_event_t*){if(!state)return;char target[48];
       const char* path=state->request.rich?state->media[state->pictureIndex].path:"";const char* suffix=strrchr(path,'.');
       unsigned index=unsigned(std::max(0,state->pictureIndex));if(suffix)sscanf(suffix,".img%u",&index);
       snprintf(target,sizeof(target),"@image:%u",index);end(0,target);},LV_EVENT_CLICKED,nullptr);
      state->imageSpace=int(image.height*scale/256)+16;
     }else image.clear();
    }
   }
  }
  if(!state->picture){state->imageNote=lv_label_create(state->screen);lv_label_set_text(state->imageNote,tr("Illustration indisponible"));lv_obj_set_pos(state->imageNote,24,top);lv_obj_set_style_text_color(state->imageNote,color(3),0);state->imageSpace=36;}
 }

}
unsigned charIndex(size_t byte){unsigned count=0;for(size_t i=0;i<byte;++i)if((uint8_t(state->window[i])&0xc0)!=0x80)++count;return count;}
size_t nextChar(size_t byte,size_t end){++byte;while(byte<end&&(uint8_t(state->window[byte])&0xc0)==0x80)++byte;return byte;}
void linkSegment(size_t begin,size_t finish,const lv_point_t& position,unsigned index,lv_obj_t* parent=nullptr,int left=24,int top=-1){
 if(begin>=finish)return;char saved=state->window[finish];state->window[finish]=0;lv_point_t dimensions{};
 lv_text_get_size(&dimensions,state->window+begin,state->font,0,0,32767,LV_TEXT_FLAG_NONE);
 auto* box=lv_obj_create(parent?parent:state->linkLayer);lv_obj_remove_style_all(box);lv_obj_remove_flag(box,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(box,LV_OBJ_FLAG_CLICKABLE);
 lv_obj_set_pos(box,left+position.x,(top<0?66+state->imageSpace:top)+position.y);lv_obj_set_size(box,std::max(8,int(dimensions.x)),state->font->line_height+state->spacing);
 auto* label=lv_label_create(box);lv_obj_remove_style_all(label);lv_obj_set_style_text_font(label,state->font,0);lv_obj_set_style_text_color(label,lv_color_hex(Tab5UiStyle::color(6)),0);lv_obj_set_style_text_decor(label,LV_TEXT_DECOR_UNDERLINE,0);
 lv_label_set_text(label,state->window+begin);lv_obj_set_pos(label,0,0);state->window[finish]=saved;
 Tab5Focus.describe(box,state->links[index].target);
 lv_obj_add_event_cb(box,[](lv_event_t* event){unsigned index=uintptr_t(lv_event_get_user_data(event));if(state&&index<state->linkCount){char target[768];strcpy(target,state->links[index].target);end(0,target);}},LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(index)));
}
void renderLinks(){
 if(state->linkLayer){Tab5Focus.clear();lv_obj_delete(state->linkLayer);state->linkLayer=nullptr;}state->linkCount=0;
 if(!state->request.rich||state->next<=state->offset)return;
 Tab5StorageUse lease;if(!lease)return;char filename[288];snprintf(filename,sizeof(filename),"%s.links",state->request.path);File file=tab5FS.open(filename);if(!file)return;
 if(file.isDirectory()||file.size()>Tab5Reader::Lines::MaxBytes)return;
 const uint32_t start=(file.size()==state->linkFileSize&&state->offset>=state->linkArticleOffset)?state->linkFileOffset:0;
 Tab5Reader::Lines lines(&file,sidecarRead,file.size(),start,sidecarYield);
 uint32_t checkpoint=start;bool checkpointFound=false;
 state->linkLayer=lv_obj_create(state->screen);lv_obj_remove_style_all(state->linkLayer);lv_obj_set_size(state->linkLayer,lv_display_get_horizontal_resolution(nullptr),lv_display_get_vertical_resolution(nullptr));lv_obj_remove_flag(state->linkLayer,LV_OBJ_FLAG_SCROLLABLE);lv_obj_remove_flag(state->linkLayer,LV_OBJ_FLAG_CLICKABLE);
 lv_obj_update_layout(state->text);char row[832];unsigned scanned=0,segments=0;
 while(state->linkCount<40&&segments<96){
  const uint32_t recordAt=lines.position();if(!lines.next(row,sizeof(row)))break;
  if(!*row)continue;char* tail=nullptr;unsigned long begin=strtoul(row,&tail,10);if(tail==row||*tail!='\t')continue;
  char* second=tail+1;unsigned long finish=strtoul(second,&tail,10);if(tail==second||*tail!='\t'||finish<=begin||finish>state->size)continue;
  if(finish<=state->offset)continue;
  if(!checkpointFound){checkpoint=recordAt;checkpointFound=true;}
  if(begin>=state->next)break;const char* target=tail+1;size_t length=strlen(target);if(!length||length>=768||strchr(target,'\t'))continue;
  const unsigned index=state->linkCount++;strcpy(state->links[index].target,target);
  size_t start=std::max<uint32_t>(begin,state->offset)-state->offset,endByte=std::min<uint32_t>(finish,state->next)-state->offset;
  while(start<endByte&&(uint8_t(state->window[start])&0xc0)==0x80)++start;endByte=boundary(state->window,endByte,state->next-state->offset);
  lv_obj_t* label=nullptr;lv_obj_t* parent=nullptr;unsigned origin=0;int left=24,top=66;
  for(auto& cell:state->cellViews)if(state->offset+start>=cell.begin&&state->offset+start<cell.end){label=cell.label;parent=cell.box;origin=charIndex(cell.begin-state->offset);endByte=std::min<size_t>(endByte,cell.end-state->offset);left=cell.left;top=cell.top;break;}
  if(!label)continue;lv_obj_update_layout(label);
  unsigned character=charIndex(start)-origin;lv_point_t position{};lv_label_get_letter_pos(label,character,&position);size_t segment=start;
  for(size_t byte=start;byte<endByte;){const size_t next=nextChar(byte,endByte);lv_point_t nextPosition{};lv_label_get_letter_pos(label,++character,&nextPosition);
   if(nextPosition.y!=position.y||next==endByte){size_t stop=next;while(stop>segment&&(state->window[stop-1]=='\n'||state->window[stop-1]==' '))--stop;linkSegment(segment,stop,position,index,parent,left,top);++segments;segment=next;position=nextPosition;if(segments>=96)break;}byte=next;
  }
 }
 state->linkFileOffset=checkpointFound?checkpoint:lines.position();state->linkFileSize=file.size();state->linkArticleOffset=state->offset;
}

int render(){
 if(!state||!state->text)return -EINVAL;
 clearTable();clearPicture();state->pictureIndex=-2;state->imageAttempted=false;
 lv_obj_remove_flag(state->text,LV_OBJ_FLAG_HIDDEN);lv_obj_set_pos(state->text,-4000,-4000);
 if(!state->size){state->window[0]=0;state->offset=state->next=0;status();return 0;}
 state->offset=std::min(state->offset,state->size-1);size_t size=0;
 int error=readWindow(state->offset,std::min<uint32_t>(WindowSize,state->size-state->offset),size);if(error)return error;
 if(state->offset+size<state->size){size_t end=size;while(end&&(uint8_t(state->window[end-1])&0xc0)==0x80)--end;
  if(end){unsigned char lead=state->window[end-1];unsigned need=lead<0x80?1:lead<0xe0?2:lead<0xf0?3:4;if(size-end+1<need)size=end-1;}}
 state->window[size]=0;
 auto* layer=lv_obj_create(state->screen);state->pageLayer=layer;lv_obj_remove_style_all(layer);lv_obj_set_size(layer,lv_display_get_horizontal_resolution(nullptr),lv_display_get_vertical_resolution(nullptr));lv_obj_remove_flag(layer,LV_OBJ_FLAG_SCROLLABLE);lv_obj_remove_flag(layer,LV_OBJ_FLAG_CLICKABLE);
 uint32_t cursor=state->offset,limit=state->offset+size;int top=66,available=state->fullHeight;bool imageUsed=false;
 for(unsigned objects=0;cursor<limit&&available>=state->font->line_height&&objects<12;++objects){
  int image=-1;
  if(state->request.illustrated){if(state->request.rich){for(unsigned i=0;i<state->mediaCount;++i)if(state->media[i].offset>=cursor&&state->media[i].offset-cursor<=4){image=int(i);break;}}
   else if(cursor<=state->firstTextOffset)image=0;}
  if(image>=0){
   if(imageUsed)break;if(available<100&&top>66)break;
   pictureLayout(state->request.rich?state->media[image].offset:cursor,top,available);imageUsed=true;
   const int used=state->imageSpace;top+=used;available-=used;
  }
  const State::Table* table=nullptr;uint32_t stop=limit;
  for(const auto& candidate:state->tables){if(cursor>=candidate.begin&&cursor<candidate.end){table=&candidate;break;}if(candidate.begin>cursor){stop=std::min(stop,candidate.begin);break;}}
  if(table){uint32_t finish=cursor,tableLimit=limit;
   if(state->request.illustrated)for(unsigned i=0;i<state->mediaCount;++i)if(state->media[i].offset>cursor)tableLimit=std::min(tableLimit,state->media[i].offset);
   const int used=renderTable(*table,cursor,tableLimit,top,available,finish);
   if(used<0)return used;if(!used){if(top>66)break;stop=std::min(limit,table->end);table=nullptr;}else{cursor=finish;top+=used+12;available-=used+12;continue;}}
  if(state->request.illustrated)for(unsigned i=0;i<state->mediaCount;++i)if(state->media[i].offset>cursor)stop=std::min(stop,state->media[i].offset);
  size_t begin=cursor-state->offset,end=stop-state->offset;
  while(begin<end&&((uint8_t(state->window[begin])&0xc0)==0x80||white(state->window[begin])))++begin;
  if(begin==end){cursor=stop;continue;}
  const size_t fit=fitPrefix(begin,end,available);if(fit<=begin)break;
  const int used=measured(begin,fit);
  auto* label=lv_label_create(layer);lv_obj_set_pos(label,24,top);lv_obj_set_width(label,state->width);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_font(label,state->font,0);lv_obj_set_style_text_color(label,color(2),0);lv_obj_set_style_text_line_space(label,state->spacing,0);
  char saved=state->window[fit];state->window[fit]=0;lv_label_set_text(label,state->window+begin);state->window[fit]=saved;
  if(!state->cellViews.push_back({uint32_t(state->offset+begin),uint32_t(state->offset+fit),nullptr,label,24,top}))return -ENOMEM;
  cursor=state->offset+fit;top+=used+12;available-=used+12;if(fit<end)break;
 }
 state->next=cursor;
 // Degenerate whitespace/UTF-8 windows still advance, without forcing clipped text.
 if(state->next==state->offset&&size&&!state->cellViews.size()&&!imageUsed)state->next=limit;
 renderLinks();if(state->popup)lv_obj_move_to_index(state->popup,-1);status();return 0;
}
void end(int result,const char* target){
 if(!state)return;const uint32_t revision=state->request.revision;
 save();suspend();Tab5PythonUI::readerFinished(revision,result,target);
 state->~State();heap_caps_free(state);state=nullptr;
}
void redraw(){int error=render();if(error)end(error);else dirty();}
void next(){
 if(!state||state->next>=state->size||state->next<=state->offset)return;
 if(state->historyCount==HistorySize){memmove(state->history,state->history+1,(HistorySize-1)*sizeof(uint32_t));--state->historyCount;}
 state->history[state->historyCount++]=state->offset;state->offset=state->next;redraw();
}
void previous(){
 if(!state||!state->offset)return;
 if(state->historyCount){state->offset=state->history[--state->historyCount];redraw();return;}
 const uint32_t endOffset=state->offset;
 const uint32_t beginOffset=endOffset>WindowSize?endOffset-WindowSize:0;
 size_t size=0;int error=readWindow(beginOffset,endOffset-beginOffset,size);if(error){end(error);return;}
 state->offset=beginOffset+fitSuffix(size);if(state->offset>=endOffset)state->offset=beginOffset;
 redraw();
}
void last(){
 if(!state||!state->size)return;
 const uint32_t beginOffset=state->size>WindowSize?state->size-WindowSize:0;
 size_t size=0;int error=readWindow(beginOffset,state->size-beginOffset,size);if(error){end(error);return;}
 state->offset=beginOffset+fitSuffix(size);state->historyCount=0;redraw();
}
void typography(int fontDelta,int spacingDelta){
 if(!state)return;
 state->fontIndex=std::min(5,std::max(0,int(state->fontIndex)+fontDelta));
 state->spacing=std::min(12,std::max(0,int(state->spacing)+spacingDelta));
 state->font=hooks.font?hooks.font(FontSizes[state->fontIndex]):&lv_font_montserrat_24;
 state->historyCount=0;redraw();
}
void popup();
void action(unsigned id){
 switch(id){case 0:close();break;case 1:previous();break;case 2:next();break;case 3:typography(-1,0);break;case 4:typography(1,0);break;
  case 5:popup();break;case 6:suspend();if(hooks.background)hooks.background();break;case 7:typography(0,-1);break;case 8:typography(0,1);break;case 9:end(0,"@images");break;case 10:if(state->request.canSave)end(0,"@save");break;case 11:if(state->request.canUpdate)end(0,"@update");break;}
}
lv_obj_t* button(lv_obj_t* parent,const char* title,const char* hint,int x,int y,int width,unsigned id){
 auto* object=lv_button_create(parent);lv_obj_set_pos(object,x,y);lv_obj_set_size(object,width,46);
 Tab5UiStyle::control(object);
 lv_obj_set_style_bg_color(object,color(1),0);lv_obj_set_style_text_color(object,color(2),0);lv_obj_set_style_pad_all(object,0,0);
 auto* label=lv_label_create(object);lv_label_set_text(label,tr(title));lv_obj_set_style_text_font(label,uiFont(),0);lv_obj_center(label);
 Tab5Focus.describe(object,tr(hint));
 lv_obj_add_event_cb(object,[](lv_event_t* event){action(uintptr_t(lv_event_get_user_data(event)));},LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(id)));
 return object;
}
void popup(){
 if(!state||!state->screen)return;Tab5Focus.clear();
 if(state->popup){lv_obj_delete(state->popup);state->popup=state->values=nullptr;return;}
 auto* panel=lv_obj_create(state->screen);state->popup=panel;
 lv_obj_set_size(panel,660,644);lv_obj_center(panel);lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_set_style_bg_color(panel,color(1),0);lv_obj_set_style_text_color(panel,color(2),0);lv_obj_set_style_pad_all(panel,18,0);
 auto* title=lv_label_create(panel);lv_label_set_text(title,tr("Lecture"));lv_obj_set_pos(title,2,4);
 button(panel,LV_SYMBOL_CLOSE,"Fermer",568,0,46,5);
 auto* fontLabel=lv_label_create(panel);lv_label_set_text(fontLabel,tr("Taille du texte"));lv_obj_set_pos(fontLabel,2,70);
 button(panel,"A-","Réduire le texte",384,58,74,3);button(panel,"A+","Agrandir le texte",476,58,74,4);
 auto* lineLabel=lv_label_create(panel);lv_label_set_text(lineLabel,tr("Interligne"));lv_obj_set_pos(lineLabel,2,134);
 button(panel,LV_SYMBOL_MINUS,"Réduire l’interligne",384,122,74,7);button(panel,LV_SYMBOL_PLUS,"Augmenter l’interligne",476,122,74,8);
 state->values=lv_label_create(panel);lv_obj_set_pos(state->values,2,196);status();
 button(panel,state->request.illustrated?"Masquer les images":"Afficher les images","Illustrations",2,246,606,9);
 if(state->request.canSave)button(panel,"Enregistrer cette page","Enregistrer cette page",2,302,606,10);
 if(state->request.canUpdate)button(panel,"Vérifier la mise à jour","Vérifier la mise à jour",2,state->request.canSave?358:302,606,11);
 auto* help=lv_label_create(panel);lv_obj_set_pos(help,2,430);lv_obj_set_width(help,608);lv_obj_set_style_text_line_space(help,5,0);
 lv_label_set_text(help,tr("Flèches / Page : naviguer\nEspace : suite · Maj+Espace : précédente\nDébut / Fin : début ou fin du document\nCtrl + / - : texte · Ctrl I : images\nRetour arrière / Échap : retour · F1 : aide"));
}
int show(){
 if(!state)return -EINVAL;
 if(state->screen)return 0;
 state->window=static_cast<char*>(heap_caps_malloc(WindowSize+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!state->window)return -ENOMEM;
 Tab5Focus.clear();if(hooks.enterPortrait)hooks.enterPortrait();
 const int width=lv_display_get_horizontal_resolution(nullptr),height=lv_display_get_vertical_resolution(nullptr);
 state->width=width-48;state->height=state->fullHeight=height-126;
 state->font=hooks.font?hooks.font(FontSizes[state->fontIndex]):&lv_font_montserrat_24;
 state->screen=lv_obj_create(nullptr);lv_obj_remove_style_all(state->screen);lv_obj_set_size(state->screen,width,height);
 lv_obj_set_style_bg_opa(state->screen,LV_OPA_COVER,0);lv_obj_set_style_bg_color(state->screen,color(0),0);
 lv_obj_set_style_text_color(state->screen,color(2),0);lv_obj_set_style_text_font(state->screen,uiFont(),0);
 lv_obj_remove_flag(state->screen,LV_OBJ_FLAG_SCROLLABLE);
 button(state->screen,LV_SYMBOL_LEFT,"Retour · Retour arrière / Échap",8,5,48,0);
 auto* title=lv_label_create(state->screen);lv_obj_set_pos(title,70,16);lv_obj_set_width(title,width-270);
 lv_label_set_long_mode(title,LV_LABEL_LONG_DOT);lv_label_set_text(title,state->request.title);
 auto* imageToggle=button(state->screen,LV_SYMBOL_IMAGE,state->request.illustrated?"Masquer les images · Ctrl I":"Afficher les images · Ctrl I",width-186,5,48,9);
 lv_obj_set_style_bg_color(imageToggle,state->request.illustrated?lv_color_hex(Tab5UiStyle::color(6)):color(1),0);
 lv_obj_set_style_text_color(imageToggle,state->request.illustrated?lv_color_hex(Tab5UiStyle::color(7)):color(2),0);
 button(state->screen,LV_SYMBOL_SETTINGS,"Lecture · F1",width-128,5,48,5);
 button(state->screen,LV_SYMBOL_HOME,"Accueil · F6",width-62,5,54,6);
 state->text=lv_label_create(state->screen);lv_obj_set_pos(state->text,24,66);lv_obj_set_size(state->text,state->width,state->height);
 lv_label_set_long_mode(state->text,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_color(state->text,color(2),0);
 lv_obj_set_style_text_font(state->text,state->font,0);lv_obj_set_style_text_line_space(state->text,state->spacing,0);
 auto* touch=lv_obj_create(state->screen);lv_obj_remove_style_all(touch);lv_obj_set_pos(touch,0,58);lv_obj_set_size(touch,width,height-116);
 lv_obj_remove_flag(touch,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(touch,LV_OBJ_FLAG_CLICKABLE);
 lv_obj_add_event_cb(touch,[](lv_event_t*){lv_indev_t* input=lv_indev_active();if(!input)return;lv_point_t point;lv_indev_get_point(input,&point);
  if(point.x<lv_display_get_horizontal_resolution(nullptr)/3)previous();else next();},LV_EVENT_CLICKED,nullptr);
 state->previousButton=button(state->screen,LV_SYMBOL_LEFT,"Page précédente",8,height-52,60,1);
 state->nextButton=button(state->screen,LV_SYMBOL_RIGHT,"Page suivante",width-68,height-52,60,2);
 state->progress=lv_label_create(state->screen);lv_obj_set_pos(state->progress,92,height-38);lv_obj_set_width(state->progress,width-184);
 lv_obj_set_style_text_align(state->progress,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_color(state->progress,color(3),0);
 lv_screen_load(state->screen);lv_obj_update_layout(state->screen);
 return render();
}
int open(const Tab5PythonUI::ReaderRequest& request){
 std::string canonical;if(!Tab5Files::canonical(request.path,canonical)||canonical=="/")return -EINVAL;
 auto* memory=heap_caps_malloc(sizeof(State),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!memory)return -ENOMEM;
 state=new(memory)State;state->request=request;
 {
  Tab5StorageUse lease;if(!lease)return -EBUSY;
  File file=tab5FS.open(request.path);if(!file||file.isDirectory())return -ENOENT;
  if(uint64_t(file.size())>UINT32_MAX)return -EFBIG;
  state->size=file.size();uint8_t prefix[256];const size_t count=file.read(prefix,std::min<size_t>(sizeof(prefix),state->size));
  if(count!=std::min<size_t>(sizeof(prefix),state->size))return -EIO;
  state->fingerprint=hash(prefix,count);
  while(state->firstTextOffset<count&&(white(char(prefix[state->firstTextOffset]))||(prefix[state->firstTextOffset]&0xc0)==0x80))++state->firstTextOffset;
  const uint32_t key=hash(request.title,strlen(request.title),hash(canonical.data(),canonical.size()));
  const uint32_t key2=hash(canonical.data(),canonical.size(),hash(request.title,strlen(request.title),0x91fe761du));
  snprintf(state->savePath,sizeof(state->savePath),"/System/python-reader/%08lx%08lx.pos",static_cast<unsigned long>(key),static_cast<unsigned long>(key2));
  loadMedia();loadTables();loadPosition();
 }
 return show();
}
}
void begin(Hooks configured){hooks=configured;}
bool active(){return state!=nullptr;}
bool visible(){return state&&state->screen;}
void close(){end(0);}
void suspend(){
 if(!state)return;save();clearPicture();state->imageAttempted=false;state->pictureIndex=-2;
 if(state->screen){Tab5Focus.clear();if(hooks.leavePortrait)hooks.leavePortrait();auto* old=state->screen;state->screen=state->text=state->progress=state->popup=state->values=state->linkLayer=state->tablePanel=state->pageLayer=state->previousButton=state->nextButton=nullptr;state->cellViews.clear();lv_obj_delete(old);}
 heap_caps_free(state->window);state->window=nullptr;
}
bool resume(){if(!state)return false;int result=show();if(result){end(result);return false;}return true;}
void service(bool foreground){
 Tab5PythonUI::ReaderRequest request;const bool requested=Tab5PythonUI::readerRequest(request);
 if(state&&(!requested||request.revision!=state->request.revision))end(0);
 if(!state&&requested&&foreground){int result=open(request);if(result){if(state)end(result);else Tab5PythonUI::readerFinished(request.revision,result);}}
 if(state&&state->screen&&!foreground)suspend();
 if(state&&state->dirty&&millis()-state->dirtyAt>=1500)save();
}
bool key(const writing::KeyEvent& event){
 if(!visible())return false;using K=writing::Key;
 if(event.ctrl&&event.key==K::Text&&(event.text[0]=='i'||event.text[0]=='I'||event.text[0]=='s'||event.text[0]=='S')){if(event.text[0]=='i'||event.text[0]=='I')action(9);else action(10);return true;}
 if(event.key==K::F1){popup();return true;}
 if(event.key==K::Escape||event.key==K::Backspace||(event.alt&&event.key==K::Left)){if(state->popup)popup();else close();return true;}
 if(event.key==K::F6||event.key==K::Command){action(6);return true;}
 if(event.ctrl&&event.key==K::Text){if(event.text[0]=='+'||event.text[0]=='='){typography(1,0);return true;}if(event.text[0]=='-'){typography(-1,0);return true;}}
 if(event.ctrl&&(event.key==K::Up||event.key==K::Down)){typography(0,event.key==K::Up?1:-1);return true;}
 if(state->popup){Tab5Focus.key(event,state->popup);return true;}
 if(state->tablePanel){
  const bool down=event.key==K::Down||event.key==K::PageDown||(event.key==K::Text&&event.text[0]==' '&&!event.shift);
  const bool up=event.key==K::Up||event.key==K::PageUp||(event.key==K::Text&&event.text[0]==' '&&event.shift);
  if((down&&lv_obj_get_scroll_bottom(state->tablePanel)>0)||(up&&lv_obj_get_scroll_top(state->tablePanel)>0)){
   int step=(event.key==K::Up||event.key==K::Down)?80:state->fullHeight-40;lv_obj_scroll_by(state->tablePanel,0,up?step:-step,LV_ANIM_OFF);return true;
  }
  if((event.key==K::Right&&lv_obj_get_scroll_right(state->tablePanel)>0)||(event.key==K::Left&&lv_obj_get_scroll_left(state->tablePanel)>0)){
   lv_obj_scroll_by(state->tablePanel,event.key==K::Left?180:-180,0,LV_ANIM_OFF);return true;
  }
 }
 if(event.key==K::PageDown||event.key==K::Right||event.key==K::Down||(event.key==K::Text&&event.text[0]==' '&&!event.shift)){next();return true;}
 if(event.key==K::PageUp||event.key==K::Left||event.key==K::Up||(event.key==K::Text&&event.text[0]==' '&&event.shift)){previous();return true;}
 if(event.key==K::Home){state->offset=0;state->historyCount=0;redraw();return true;}
 if(event.key==K::End){last();return true;}
 Tab5Focus.key(event,state->screen);return true;
}
}
