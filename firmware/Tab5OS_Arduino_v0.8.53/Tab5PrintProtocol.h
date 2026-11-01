#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
namespace tab5print {
struct Endpoint {std::string host,path,uri;uint16_t port=631;bool tls=false;};
bool endpoint(const std::string& uri,Endpoint& result);
struct Attribute {uint8_t tag;std::string name;std::vector<uint8_t> value;};
struct Response {uint16_t status=0xffff;uint32_t request=0;std::vector<Attribute> attributes;bool has(const char* name,const char* value) const;int integer(const char* name,int fallback=0) const;};
std::vector<uint8_t> request(uint16_t operation,uint32_t id,const std::string& uri,const char* format=nullptr,const char* media=nullptr,int dpi=0,int jobId=0,int orientation=3);
bool parse(const uint8_t* data,size_t size,uint32_t expected,Response& response);
struct Layout {unsigned width,height,margin,dpi;};
Layout page(double widthMm,double heightMm,unsigned dpi,double marginMm=8);
struct ImagePlacement {unsigned width=0,height=0,columns=0,rows=0;unsigned pages()const{return columns*rows;}};
ImagePlacement imagePlacement(const Layout&,unsigned sourceWidth,unsigned sourceHeight,unsigned percent);
// Network byte order PWG Raster header. RGB/sRGB, 8 bits per component.
std::vector<uint8_t> rasterHeader(const Layout& page,unsigned pageCount,const char* media);
size_t rasterRowCapacity(unsigned pixels);
size_t encodeRasterRow(const uint8_t* rgb,unsigned pixels,uint8_t* out,size_t capacity);
std::vector<uint8_t> rasterRow(const uint8_t* rgb,unsigned pixels);
}
