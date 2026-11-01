#pragma once
#include <cstddef>
#include <cstdint>
namespace Tab5PythonUI {
constexpr unsigned MaxWidgets=24;
constexpr unsigned TextCapacity=512;
struct Widget {uint32_t revision=0;int16_t x=0,y=0,w=0,h=0;uint16_t maximum=100,value=0;uint8_t type=0,shortcut=0;uint32_t background=0,foreground=0;uint32_t palette[9]{};char text[TextCapacity]{};};
struct ReaderRequest {uint32_t revision=0;bool illustrated=false,rich=false,canSave=true,canUpdate=true;char path[272]{};char title[192]{};};
bool imageRequest(char path[272]);
void imageFinished(int result=0);
bool readerRequest(ReaderRequest& request);
void readerFinished(uint32_t revision,int result=0,const char* target=nullptr);
void reset();void close();void releaseStorage();void setLanguage(const char* language);
bool opened(uint32_t& generation,char title[192]);
bool read(unsigned id,Widget& widget,uint32_t expectedGeneration=0);
void event(unsigned id,unsigned value,uint32_t expectedGeneration=0);
bool consumeEvent(unsigned id);
void input(unsigned id,const char* text,uint32_t expectedGeneration=0);
int call(int op,int a,int b,int c,const void* data,size_t n,void* out,size_t capacity);
}
