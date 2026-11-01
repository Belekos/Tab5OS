#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>
#include "Tab5JpegHeader.h"
#define JPEG_COMPONENT_NUMBER_MAX 4
#define JPEG_QUANTIZATION_TABLE_LEN 64
#define JPEG_HUFFMAN_BITS_LEN_TABLE_LEN 16
#define JPEG_HUFFMAN_AC_VALUE_TABLE_LEN 256
#define JPEG_M_SOF0 0xffc0
#define JPEG_MEM_ALLOC_CAPS 4096
#define ESP_LOGE(...) ((void)0)
#define ESP_RETURN_ON_FALSE(test,code,...) do{if(!(test))return code;}while(0)
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_INVALID_ARG=1,ESP_ERR_NO_MEM=2,ESP_ERR_INVALID_STATE=3;
enum jpeg_down_sampling_type_t{JPEG_DOWN_SAMPLING_GRAY,JPEG_DOWN_SAMPLING_YUV444,JPEG_DOWN_SAMPLING_YUV422,JPEG_DOWN_SAMPLING_YUV420};
struct jpeg_decode_picture_info_t{uint32_t width,height;jpeg_down_sampling_type_t sample_method;};
static constexpr unsigned zigzag_arr[64]={0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};
std::unordered_set<void*> allocations;unsigned allocationCount=0;
void* heap_caps_calloc(size_t n,size_t size,unsigned){void* p=calloc(n,size);if(p){allocations.insert(p);++allocationCount;}return p;}
void trackedFree(void* p){allocations.erase(p);free(p);}
#define free trackedFree
// NATIVE_PARSERS
#undef free
using Bytes=std::vector<uint8_t>;
unsigned checks=0;
void check(bool value){++checks;if(!value){std::cerr<<"check "<<checks<<" failed\n";std::abort();}}
void segment(Bytes& b,unsigned marker,const Bytes& body){b.push_back(0xff);b.push_back(marker);unsigned n=body.size()+2;b.push_back(n>>8);b.push_back(n);b.insert(b.end(),body.begin(),body.end());}
Bytes header(unsigned sample=0x22,unsigned components=3){Bytes b={0xff,0xd8};Bytes sof={8,0,16,0,16,static_cast<uint8_t>(components),1,static_cast<uint8_t>(sample),0};if(components==3)sof.insert(sof.end(),{2,0x11,1,3,0x11,1});segment(b,0xc0,sof);segment(b,0xda,components==3?Bytes{3,1,0,2,0x11,3,0x11,0,63,0}:Bytes{1,1,0,0,63,0});b.insert(b.end(),{0,0xff,0xd9});return b;}
bool probe(const Bytes& b){Tab5JpegHeader out;return tab5ProbeJpegHeader(b.data(),b.size(),out);}
unsigned nativeParses=0;
void guardedNative(const Bytes& b){
 Tab5JpegHeader result;if(!tab5ProbeJpegHeader(b.data(),b.size(),result)||result.width>1280||result.height>720)return;
 ++nativeParses;
 jpeg_dec_header_info_t info={};size_t p=2;
 while(p<b.size()){
  assert(b[p++]==0xff);while(b[p]==0xff)++p;unsigned marker=b[p++];size_t n=(b[p]<<8)|b[p+1];
  info.buffer_offset=const_cast<uint8_t*>(b.data()+p);info.buffer_left=b.size()-p;info.header_size=p;
  esp_err_t rc=ESP_OK;
  if(marker==0xdb)rc=jpeg_parse_dqt_marker(&info);
  if(marker==0xc0)rc=jpeg_parse_sof_marker(&info);
  if(marker==0xc4)rc=jpeg_parse_dht_marker(&info);
  if(marker==0xda)return;
  if(rc!=ESP_OK)return;
  p+=n;
 }
}
int main(int argc,char** argv){
 if(argc<2)return 2;
 if(argc==3){
  jpeg_dec_header_info_t info={};Bytes b;const std::string mode=argv[2];
  if(mode=="unsafe-dqt"){b={0,67,15};b.resize(67,1);info.buffer_offset=b.data();info.buffer_left=b.size();return jpeg_parse_dqt_marker(&info);}
  if(mode=="unsafe-dht"){b={0,20,0xff};b.resize(20,0);info.buffer_offset=b.data();info.buffer_left=b.size();return jpeg_parse_dht_marker(&info);}
  if(mode=="unsafe-gray"){b={0,11,8,0,16,0,16,1,1,0,0};info.buffer_offset=b.data();info.buffer_left=b.size();return jpeg_parse_sof_marker(&info);}
  return 3;
 }
 for(unsigned i=0;i<16;++i){
  std::ifstream input(std::string(argv[1])+"/"+std::to_string(i)+".jpg",std::ios::binary);
  Bytes b{std::istreambuf_iterator<char>(input),{}};check(!b.empty());
  Tab5JpegHeader current;check(tab5ProbeJpegHeader(b.data(),b.size(),current));
  jpeg_decode_picture_info_t old={};check(jpeg_decoder_get_info(b.data(),b.size(),&old)==ESP_OK);
  check(old.width==current.width&&old.height==current.height);guardedNative(b);
  for(size_t n=0;n<b.size();++n){Tab5JpegHeader truncated;tab5ProbeJpegHeader(b.data(),n,truncated);++checks;}
 }
 check(allocations.empty());
 Bytes embedded={0xff,0xd8};Bytes fake={0xff,0xc0,0,11,8,0,8,0,8,1,1,0x11,0};segment(embedded,0xe1,fake);
 Bytes ordinary=header();embedded.insert(embedded.end(),ordinary.begin()+2,ordinary.end());
 Tab5JpegHeader current;check(tab5ProbeJpegHeader(embedded.data(),embedded.size(),current));check(current.width==16);
 jpeg_decode_picture_info_t old={};check(jpeg_decoder_get_info(embedded.data(),embedded.size(),&old)==ESP_OK);check(old.width==8);
 const Bytes unsupported=header(0x33);const unsigned before=allocationCount;
 for(unsigned i=0;i<100;++i){check(jpeg_decoder_get_info(unsupported.data(),unsupported.size(),&old)==ESP_ERR_INVALID_STATE);check(!probe(unsupported));}
 const size_t leaked=allocations.size();check(leaked==100);for(void* p:allocations)free(p);allocations.clear();
 const unsigned oldAllocations=allocationCount-before;const unsigned currentBefore=allocationCount;
 for(unsigned i=0;i<100;++i){check(!probe(unsupported));}check(allocationCount==currentBefore);
 check(!probe(header(0,1)));check(probe(header(0x22,1)));
 // Preserve legal chroma factors; the native DMA footprint uses component 0.
 for(unsigned h=1;h<=4;++h)for(unsigned v=1;v<=4;++v){
  Bytes b=header(0x22);b[16]=(h<<4)|v;b[19]=(h<<4)|v;
  check(probe(b));guardedNative(b);
  jpeg_dec_header_info_t native={};native.buffer_offset=b.data()+4;native.buffer_left=b.size()-4;
  check(jpeg_parse_sof_marker(&native)==ESP_OK);check(native.process_h==16&&native.process_v==16);
 }

 for(unsigned bad=4;bad<16;++bad){Bytes b={0xff,0xd8};Bytes q(65,1);q[0]=bad;segment(b,0xdb,q);b.insert(b.end(),ordinary.begin()+2,ordinary.end());check(!probe(b));}
 for(unsigned descriptor:{2,15,0x12,0x20,0xff}){Bytes b={0xff,0xd8};Bytes table(17,0);table[0]=descriptor;segment(b,0xc4,table);b.insert(b.end(),ordinary.begin()+2,ordinary.end());check(!probe(b));}
 Bytes five={0xff,0xd8};for(unsigned i=0;i<5;++i){Bytes q(65,1);q[0]=0;segment(five,0xdb,q);}five.insert(five.end(),ordinary.begin()+2,ordinary.end());check(!probe(five));
 Bytes excessive={0xff,0xd8};Bytes ht(17+257,0);ht[1]=255;ht[2]=2;segment(excessive,0xc4,ht);excessive.insert(excessive.end(),ordinary.begin()+2,ordinary.end());check(!probe(excessive));
 check(probe(ordinary));guardedNative(ordinary);
 std::mt19937 rng(0x0848);unsigned accepted=0;
 for(unsigned i=0;i<100000;++i){Bytes b=ordinary;
  if(i&1){b.resize(rng()%150);for(auto& byte:b)byte=rng();}
  else{for(unsigned j=0;j<1+rng()%5;++j)b[rng()%b.size()]=rng();}
  if(probe(b)){++accepted;guardedNative(b);}++checks;
 }
 check(allocations.empty());
 std::cout<<"{\"passed\":true,\"assertions\":"<<checks<<",\"validJpegs\":16,\"fuzzInputs\":100000,\"guardedNativeParses\":"<<nativeParses<<",\"acceptedFuzzHeaders\":"<<accepted<<",\"oldProbeAllocationsFor100BadFrames\":"<<oldAllocations<<",\"oldProbeLeakedHeaders\":"<<leaked<<",\"newProbeAllocations\":0,\"nativeHeaderBytesHost\":"<<sizeof(jpeg_dec_header_info_t)<<"}\n";
}
