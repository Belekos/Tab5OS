#include "Tab5PrintProtocol.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cctype>
namespace tab5print {
namespace {
void u16(std::vector<uint8_t>& v,unsigned n){v.push_back(n>>8);v.push_back(n);}
void u32(std::vector<uint8_t>& v,uint32_t n){u16(v,n>>16);u16(v,n);}
uint16_t get16(const uint8_t* p){return unsigned(p[0])*256+p[1];}
uint32_t get32(const uint8_t* p){return uint32_t(get16(p))<<16|get16(p+2);}
void attr(std::vector<uint8_t>& v,uint8_t tag,const char* name,const char* value){v.push_back(tag);u16(v,strlen(name));v.insert(v.end(),name,name+strlen(name));u16(v,strlen(value));v.insert(v.end(),value,value+strlen(value));}
void integer(std::vector<uint8_t>& v,const char* name,int value){v.push_back(0x21);u16(v,strlen(name));v.insert(v.end(),name,name+strlen(name));u16(v,4);u32(v,value);}
}
bool endpoint(const std::string& uri,Endpoint& result){
 if(uri.size()>256||uri.find_first_of("\r\n\t ")!=std::string::npos)return false;
 bool tls=uri.rfind("ipps://",0)==0;if(!tls&&uri.rfind("ipp://",0)!=0)return false;
 size_t start=tls?7:6,slash=uri.find('/',start);if(slash==std::string::npos)return false;
 std::string authority=uri.substr(start,slash-start);if(authority.empty()||authority.find('@')!=std::string::npos||authority.find('[')!=std::string::npos)return false;
 uint32_t port=631;size_t colon=authority.find(':');if(colon!=std::string::npos){std::string p=authority.substr(colon+1);authority.resize(colon);port=0;if(p.empty()||p.size()>5)return false;for(char c:p){if(c<'0'||c>'9')return false;port=port*10+c-'0';}if(!port||port>65535)return false;}
 if(authority.empty())return false;for(char c:authority)if(!(std::isalnum(static_cast<unsigned char>(c))||c=='.'||c=='-'))return false;
 std::string path=uri.substr(slash);if(path.find('#')!=std::string::npos)return false;
 result={authority,path,uri,uint16_t(port),tls};return true;
}
std::vector<uint8_t> request(uint16_t op,uint32_t id,const std::string& uri,const char* format,const char* media,int dpi,int jobId,int orientation){
 std::vector<uint8_t> v{2,0};u16(v,op);u32(v,id);v.push_back(1);
 attr(v,0x47,"attributes-charset","utf-8");attr(v,0x48,"attributes-natural-language","fr");attr(v,0x45,"printer-uri",uri.c_str());
 if(jobId)integer(v,"job-id",jobId);
 attr(v,0x42,"requesting-user-name","Tab5OS");
 if(format){attr(v,0x42,"job-name","Tab5OS");attr(v,0x49,"document-format",format);}
 if(op==0x000b){bool first=true;for(auto* name:{"document-format-supported","printer-resolution-supported","media-supported","printer-state","printer-is-accepting-jobs","pwg-raster-document-type-supported","pwg-raster-document-resolution-supported"}){attr(v,0x44,first?"requested-attributes":"",name);first=false;}}
 if(media||dpi){v.push_back(2);if(media)attr(v,0x44,"media",media);size_t tagPos=v.size();integer(v,"orientation-requested",orientation==4?4:3);v[tagPos]=0x23;attr(v,0x44,"sides","one-sided");if(dpi){v.push_back(0x32);u16(v,18);const char* n="printer-resolution";v.insert(v.end(),n,n+18);u16(v,9);u32(v,dpi);u32(v,dpi);v.push_back(3);}}
 v.push_back(3);return v;
}
bool parse(const uint8_t* p,size_t size,uint32_t expected,Response& out){
 if(!p||size<9||size>65536||p[0]<1||p[0]>2||get32(p+4)!=expected)return false;
 Response r;r.status=get16(p+2);r.request=expected;size_t pos=8;std::string name;bool group=false;unsigned collectionDepth=0;
 while(pos<size){uint8_t tag=p[pos++];if(tag==3){if(collectionDepth)return false;out=std::move(r);return true;}
 if(tag<0x10){group=true;name.clear();continue;}
 if(!group||size-pos<2)return false;unsigned nl=get16(p+pos);pos+=2;if(nl>size-pos||nl>255)return false;
 if(nl){name.assign((const char*)p+pos,nl);pos+=nl;}else if(name.empty())return false;
 if(size-pos<2)return false;unsigned vl=get16(p+pos);pos+=2;if(vl>size-pos)return false;
 // Collections are length-framed attributes too; retain only top-level values.
 if(tag==0x34)++collectionDepth;
 else if(tag==0x37){if(!collectionDepth)return false;--collectionDepth;}
 else if(!collectionDepth){if(r.attributes.size()>=512)return false;r.attributes.push_back({tag,name,std::vector<uint8_t>(p+pos,p+pos+vl)});}
 pos+=vl;
 }return false;
}
bool Response::has(const char* n,const char* v)const{for(auto& a:attributes)if(a.name==n&&a.value.size()==strlen(v)&&!memcmp(a.value.data(),v,a.value.size()))return true;return false;}
int Response::integer(const char* name,int fallback)const{for(auto& a:attributes)if(a.name==name&&a.value.size()==4&&(a.tag==0x21||a.tag==0x23))return int(get32(a.value.data()));return fallback;}
Layout page(double w,double h,unsigned dpi,double margin){if(!std::isfinite(w)||!std::isfinite(h)||w<50||h<50||w>500||h>500||dpi<72||dpi>600||!std::isfinite(margin)||margin<0||margin>20)return{};
 return{unsigned(std::lround(w*dpi/25.4)),unsigned(std::lround(h*dpi/25.4)),unsigned(std::ceil(margin*dpi/25.4)),dpi};}
std::vector<uint8_t> rasterHeader(const Layout& p,unsigned pages,const char* media){
 std::vector<uint8_t> h(1796);auto put=[&](size_t offset,uint32_t v){h[offset]=v>>24;h[offset+1]=v>>16;h[offset+2]=v>>8;h[offset+3]=v;};
 memcpy(h.data(),"PwgRaster",9);put(276,p.dpi);put(280,p.dpi);put(340,1);
 put(352,unsigned(std::lround(p.width*72.0/p.dpi)));put(356,unsigned(std::lround(p.height*72.0/p.dpi)));
 put(372,p.width);put(376,p.height);put(384,8);put(388,24);put(392,p.width*3);put(400,19);put(420,3);
 put(452,pages);put(456,1);put(460,1);put(464,0);put(468,0);put(472,p.width);put(476,p.height);
 if(media)memcpy(h.data()+1732,media,std::min<size_t>(63,strlen(media)));return h;
}
size_t rasterRowCapacity(unsigned pixels){
 if(size_t(pixels)>(SIZE_MAX-2)/4)return 0;
 return size_t(pixels)*3+pixels/128+2;
}
size_t encodeRasterRow(const uint8_t* rgb,unsigned pixels,uint8_t* out,size_t capacity){
 const size_t required=rasterRowCapacity(pixels);
 if(!rgb||!out||!required||capacity<required)return 0;
 size_t used=0;out[used++]=0; // no row repetitions
 for(unsigned i=0;i<pixels;){unsigned repeat=1;while(repeat<128&&i+repeat<pixels&&!memcmp(rgb+i*3,rgb+(i+repeat)*3,3))++repeat;
  if(repeat>1){out[used++]=repeat-1;memcpy(out+used,rgb+i*3,3);used+=3;i+=repeat;}
  else{unsigned n=1;while(n<128&&i+n<pixels){if(i+n+1<pixels&&!memcmp(rgb+(i+n)*3,rgb+(i+n+1)*3,3))break;++n;}
    out[used++]=n==1?0:257-n;memcpy(out+used,rgb+i*3,n*3);used+=n*3;i+=n;}
 }
 return used;
}
std::vector<uint8_t> rasterRow(const uint8_t* rgb,unsigned pixels){
 std::vector<uint8_t> out(rasterRowCapacity(pixels));
 out.resize(encodeRasterRow(rgb,pixels,out.data(),out.size()));return out;
}
}

namespace tab5print {
ImagePlacement imagePlacement(const Layout& p,unsigned w,unsigned h,unsigned percent){
 ImagePlacement image;if(!w||!h||p.margin*2>=p.width||p.margin*2>=p.height||percent<25||percent>200)return image;
 unsigned cw=p.width-p.margin*2,ch=p.height-p.margin*2;
 double factor=std::min(double(cw)/w,double(ch)/h)*percent/100.0;
 image.width=std::max(1u,unsigned(w*factor));image.height=std::max(1u,unsigned(h*factor));
 image.columns=(image.width+cw-1)/cw;image.rows=(image.height+ch-1)/ch;return image;
}
}
