#include "jpge.h"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vector>
int main(int argc,char** argv) {
 assert(argc==2);FILE* file=std::fopen(argv[1],"wb");assert(file);
 unsigned cases=0;std::vector<uint8_t> output(256*1024);
 for(int dims=0;dims<3;++dims)for(int channels:{1,3,4})
 for(int quality:{1,25,50,85,100})for(int sub=0;sub<4;++sub)
 for(int standard=0;standard<2;++standard)for(int equal=0;equal<2;++equal)for(int two=0;two<2;++two){
  int width=dims==0?1:dims==1?17:31,height=dims==0?1:dims==1?9:23;
  std::vector<uint8_t> pixels(width*height*channels);
  for(size_t n=0;n<pixels.size();++n)pixels[n]=uint8_t(n*37+(n/width)*101+(n%7)*13);
  jpge::params p;p.m_quality=quality;p.m_subsampling=jpge::subsampling_t(sub);
  p.m_use_std_tables=standard;p.m_no_chroma_discrim_flag=equal;p.m_two_pass_flag=two;
  int bytes=output.size();assert(jpge::compress_image_to_jpeg_file_in_memory(output.data(),bytes,width,height,channels,pixels.data(),p));
  assert(bytes>4&&output[0]==0xff&&output[1]==0xd8&&output[bytes-2]==0xff&&output[bytes-1]==0xd9);
  const uint32_t len=bytes;assert(std::fwrite(&len,sizeof(len),1,file)==1);assert(std::fwrite(output.data(),1,bytes,file)==size_t(bytes));++cases;
 }
 assert(!std::fclose(file));std::printf("{\"passed\":true,\"cases\":%u}\n",cases);
}
