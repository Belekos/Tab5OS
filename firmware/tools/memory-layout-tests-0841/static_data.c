#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "sha1.h"
#include "xz_config.h"
#include "xz_crc32_table.h"
#include "xz_crc64_table.h"

static void check_crc(void) {
  // Independent bit-by-bit generation verifies every entry of both tables.
  for(unsigned i=0;i<256;++i){
    uint32_t a=i;uint64_t b=i;
    for(unsigned j=0;j<8;++j){a=(a>>1)^((a&1)?UINT32_C(0xEDB88320):0);b=(b>>1)^((b&1)?UINT64_C(0xC96C5795D7870F42):0);}
    assert(a==xz_crc32_table[i]&&b==xz_crc64_table[i]);
  }
  xz_crc32_init();xz_crc64_init();
  const uint8_t *s=(const uint8_t *)"123456789";
  assert(xz_crc32(s,9,0)==UINT32_C(0xCBF43926));
  assert(xz_crc64(s,9,0)==UINT64_C(0x995DC9BBDF1939FA));
  assert(xz_crc32(s+4,5,xz_crc32(s,4,0))==UINT32_C(0xCBF43926));
  assert(xz_crc64(s+4,5,xz_crc64(s,4,0))==UINT64_C(0x995DC9BBDF1939FA));
  xz_crc32_init();xz_crc64_init();
  assert(xz_crc32(s,9,0)==UINT32_C(0xCBF43926));
}
int main(int argc,char **argv) {
  check_crc();
  if(argc==1){puts("CRC tables and standard vectors: pass");return 0;}
  assert(argc==5);
  FILE *file=fopen(argv[2],"rb");assert(file);
  if(!strcmp(argv[1],"sha1")){
    unsigned char data[4096],hash[20];SHA1_CTX ctx;
    size_t chunk=strtoul(argv[3],NULL,10);assert(chunk>0&&chunk<=sizeof(data));
    SHA1DCInit(&ctx);SHA1DCSetSafeHash(&ctx,atoi(argv[4]));
    size_t bytes;while((bytes=fread(data,1,chunk,file)))SHA1DCUpdate(&ctx,(const char*)data,bytes);
    assert(!ferror(file));int collision=SHA1DCFinal(hash,&ctx);
    printf("%d ",collision);for(unsigned i=0;i<20;++i)printf("%02x",hash[i]);puts("");
  }else{
    assert(!strcmp(argv[1],"xz"));
    assert(!fseek(file,0,SEEK_END));long bytes=ftell(file);assert(bytes>0);rewind(file);
    uint8_t *input=malloc((size_t)bytes);assert(input);
    assert(fread(input,1,(size_t)bytes,file)==(size_t)bytes);
    size_t capacity=strtoul(argv[3],NULL,10);uint8_t *output=malloc(capacity);assert(output);
    struct xz_dec *decoder=xz_dec_init(XZ_DYNALLOC,32U*1024*1024);assert(decoder);
    struct xz_buf b={input,0,(size_t)bytes,output,0,capacity};
    enum xz_ret result=xz_dec_run(decoder,&b);assert(result==XZ_STREAM_END&&b.in_pos==(size_t)bytes);
    FILE *out=fopen(argv[4],"wb");assert(out);assert(fwrite(output,1,b.out_pos,out)==b.out_pos);assert(!fclose(out));
    xz_dec_end(decoder);free(input);free(output);
  }
  assert(!fclose(file));return 0;
}
