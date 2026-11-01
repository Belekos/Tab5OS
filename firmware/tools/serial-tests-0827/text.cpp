#include "Tab5SerialText.h"
#include "WriterDocument.h"
#include <cstdio>
#include <cstdlib>
#include <string>
static unsigned checks=0;
#define CHECK(value) do{++checks;if(!(value)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#value);exit(1);}}while(0)
int main(){
 using Tab5Serial::TextCapture;TextCapture text;
 const std::string expected=u8"été · € · 中文 · 🛩\nnext\nline\tend";
 const std::string wire=u8"été · € · 中文 · 🛩\r\nnext\rline\tend";
 for(size_t chunk=1;chunk<=wire.size();++chunk){text.clear();for(size_t at=0;at<wire.size();at+=chunk)text.feed(wire.data()+at,std::min(chunk,wire.size()-at));CHECK(std::string(text.data,text.size)==expected);CHECK(writing::validUtf8(text.data,text.size));CHECK(text.invalid==0);}
 text.clear();text.feed("\xe2\x82",2);CHECK(text.size==0);text.feed("\xac",1);CHECK(std::string(text.data)==u8"€");
 text.clear();text.feed("\xf0\x9f",2);text.finish();CHECK(text.invalid==1);CHECK(writing::validUtf8(text.data,text.size));
 for(const auto& bad: {std::string("\0\x01\x7f",3),std::string("\xc0\xaf"),std::string("\xe0\x80\x80"),std::string("\xed\xa0\x80"),std::string("\xf4\x90\x80\x80"),std::string("\xe2" "ABC")}){text.clear();for(char c:bad)text.feed(&c,1);text.finish();CHECK(text.invalid>0);CHECK(writing::validUtf8(text.data,text.size));}
 std::string lot=u8"€中文🛩\n";text.clear();for(unsigned i=0;i<20000;++i){text.feed(lot.data(),lot.size());CHECK(text.size<=TextCapture::Capacity);CHECK(writing::validUtf8(text.data,text.size));CHECK(text.data[text.size]==0);}CHECK(text.trimmed>0&&text.invalid==0);
 text.clear();unsigned seed=82627;for(unsigned i=0;i<50000;++i){seed=seed*1664525+1013904223;uint8_t c=uint8_t(seed>>24);text.feed(&c,1);CHECK(text.size<=TextCapture::Capacity);CHECK(writing::validUtf8(text.data,text.size));}text.finish();CHECK(writing::validUtf8(text.data,text.size));
 text.clear();CHECK(text.size==0&&text.invalid==0&&text.trimmed==0&&text.data[0]==0);
 printf("PASS serial text %u checks\n",checks);
}
