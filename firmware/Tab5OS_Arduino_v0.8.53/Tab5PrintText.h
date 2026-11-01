#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace tab5print {
class MarkdownFence {
 char marker_=0;unsigned length_=0;
 public:
 bool inside()const{return marker_!=0;}
 bool line(const char* text,size_t size,bool complete=true){
  size_t p=0;while(p<size&&text[p]==' '&&p<4)++p;
  if(p>=4||p==size||(text[p]!='`'&&text[p]!='~'))return false;
  const char marker=text[p];size_t end=p;while(end<size&&text[end]==marker)++end;
  if(marker_){
   size_t tail=end;while(tail<size&&(text[tail]==' '||text[tail]=='\t'||text[tail]=='\r'))++tail;
   if(marker==marker_&&end-p>=length_&&tail==size&&complete){marker_=0;length_=0;return true;}
   return false;
  }
  if(end-p<3)return false;
  if(marker=='`')for(size_t i=end;i<size;++i)if(text[i]=='`')return false;
  marker_=marker;length_=end-p;return true;
 }
};
// Streaming outline of the actual file, independent of the editor window.
// Only a bounded line prefix is retained; a paragraph may span any number
// of read buffers. Offsets are bytes, including CRLF and UTF-8 bytes.
struct TextSection {
 uint32_t begin=0,end=0;
 uint8_t level=0;
 char title[161]{};
};
class MarkdownOutline {
 char prefix_[256]{};
 uint32_t position_=0,lineStart_=0,lineLength_=0;
 unsigned prefixLength_=0;
 MarkdownFence fence_;
 static bool space(char c){return c==' '||c=='\t'||c=='\r';}
 template<class Emit> void line(Emit emit){
  if(fence_.line(prefix_,prefixLength_,lineLength_<=prefixLength_)||fence_.inside())return;
  unsigned p=0;while(p<prefixLength_&&prefix_[p]==' '&&p<4)++p;
  if(p>=4||p==prefixLength_)return;
  const char first=prefix_[p];unsigned run=p;
  while(run<prefixLength_&&prefix_[run]==first)++run;
  if(first!='#'||run-p>6||
     (run<prefixLength_&&!space(prefix_[run])))return;
  TextSection section;section.begin=lineStart_;section.level=uint8_t(run-p);
  unsigned begin=run;while(begin<prefixLength_&&space(prefix_[begin]))++begin;
  unsigned end=prefixLength_;while(end>begin&&space(prefix_[end-1]))--end;
  // Closing hashes are syntax only if separated from the title by whitespace.
  if(lineLength_<=prefixLength_){unsigned hashes=end;while(hashes>begin&&prefix_[hashes-1]=='#')--hashes;
   if(hashes<end&&(hashes==begin||space(prefix_[hashes-1]))){end=hashes;while(end>begin&&space(prefix_[end-1]))--end;}}
  unsigned length=end-begin;if(length>160)length=160;
  // Do not cut a UTF-8 character in the menu label.
  if(begin+length<end)while(length&&(uint8_t(prefix_[begin+length])&0xc0)==0x80)--length;
  if(length)memcpy(section.title,prefix_+begin,length);
  section.title[length]=0;emit(section);
 }
 public:
 void reset(){*this=MarkdownOutline();}
 template<class Emit> void feed(const char* data,size_t n,Emit emit){
  for(size_t i=0;i<n;++i){char c=data[i];++position_;
   if(c=='\n'){line(emit);lineStart_=position_;lineLength_=prefixLength_=0;}
   else {++lineLength_;if(prefixLength_<sizeof(prefix_))prefix_[prefixLength_++]=c;}
  }
 }
 template<class Emit> void finish(Emit emit){if(lineLength_)line(emit);lineLength_=prefixLength_=0;}
 uint32_t position()const{return position_;}
};
// A selected heading includes all its children, stopping before the next
// heading at the same or a higher level. Pages contain only the menu entries,
// never the document text; even headings outside that page close ranges.
inline void closeSections(TextSection* sections,unsigned count,const TextSection& next){
 for(unsigned i=0;i<count;++i)if(!sections[i].end&&next.level<=sections[i].level)sections[i].end=next.begin;
}
} // namespace tab5print
