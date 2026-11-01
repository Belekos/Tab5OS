#include "Tab5FirmwareFootprint.h"
#include <esp_ota_ops.h>
#include <esp_image_format.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

static esp_partition_t partition{0x10000,6553600};
static esp_image_header_t header{ESP_IMAGE_HEADER_MAGIC,6,{}};
static uint32_t imageBytes=6294496,startAddress=0x10000;
static esp_err_t headerError=ESP_OK,metadataError=ESP_OK;
static bool absent=false;
static unsigned getCalls=0,readCalls=0,metadataCalls=0;
static size_t readBytes=0;
const esp_partition_t* esp_ota_get_running_partition(){++getCalls;return absent?nullptr:&partition;}
esp_err_t esp_partition_read(const esp_partition_t* p,size_t offset,void* out,size_t length){
  ++readCalls;readBytes+=length;
  assert(p==&partition&&offset==0&&length==sizeof(header));
  if(headerError!=ESP_OK)return headerError;
  std::memcpy(out,&header,length);return ESP_OK;
}
esp_err_t esp_image_get_metadata(const esp_partition_pos_t* p,esp_image_metadata_t* out){
  ++metadataCalls;
  assert(p->offset==partition.address&&p->size==partition.size);
  if(metadataError!=ESP_OK)return metadataError;
  out->start_addr=startAddress;out->image_len=imageBytes;return ESP_OK;
}
int main(int argc,char** argv){
  assert(argc==2);const int test=std::atoi(argv[1]);
  esp_err_t expectedError=ESP_OK;bool expectedValid=true;
  unsigned expectedRead=1,expectedMetadata=1;
  switch(test){
    case 0: break;
    case 1: absent=true;expectedError=ESP_ERR_NOT_FOUND;expectedRead=expectedMetadata=0;break;
    case 2: partition.size=sizeof(header)-1;expectedError=ESP_ERR_INVALID_SIZE;expectedRead=expectedMetadata=0;break;
    case 3: partition.address=UINT32_MAX-10;expectedError=ESP_ERR_INVALID_SIZE;expectedRead=expectedMetadata=0;break;
    case 4: headerError=ESP_FAIL;expectedError=ESP_FAIL;expectedMetadata=0;break;
    case 5: header.magic=0;expectedError=ESP_ERR_IMAGE_INVALID;expectedMetadata=0;break;
    case 6: header.segment_count=0;expectedError=ESP_ERR_IMAGE_INVALID;expectedMetadata=0;break;
    case 7: header.segment_count=ESP_IMAGE_MAX_SEGMENTS+1;expectedError=ESP_ERR_IMAGE_INVALID;expectedMetadata=0;break;
    case 8: header.segment_count=ESP_IMAGE_MAX_SEGMENTS;break;
    case 9: metadataError=ESP_FAIL;expectedError=ESP_FAIL;break;
    case 10: startAddress=partition.address+4;expectedError=ESP_ERR_IMAGE_INVALID;break;
    case 11: imageBytes=0;expectedError=ESP_ERR_IMAGE_INVALID;break;
    case 12: imageBytes=partition.size+1;expectedError=ESP_ERR_IMAGE_INVALID;break;
    case 13: imageBytes=partition.size;break;
    case 14: partition.size=sizeof(header);imageBytes=sizeof(header);break;
    default: assert(false);
  }
  expectedValid=expectedError==ESP_OK;
  const auto& initial=Tab5FirmwareFootprint::snapshot();
  assert(initial.valid==expectedValid&&initial.error==expectedError);
  assert(initial.imageBytes==(expectedValid?imageBytes:0));
  assert(initial.partitionBytes==(absent?0:partition.size));
  assert(initial.partitionAddress==(absent?0:partition.address));
  // A failed first read must also be cached. Even changing the mocked source
  // after initial access cannot cause an expensive retry on each UI refresh.
  headerError=ESP_OK;metadataError=ESP_OK;absent=false;
  partition={0x20000,1};imageBytes=1;startAddress=0x20000;
  for(unsigned i=0;i<100000;++i){
    const auto& next=Tab5FirmwareFootprint::snapshot();
    assert(&next==&initial&&next.valid==expectedValid&&next.error==expectedError);
  }
  assert(getCalls==1&&readCalls==expectedRead&&metadataCalls==expectedMetadata);
  assert(readBytes==expectedRead*sizeof(header));
  std::printf("scenario %d: valid=%d error=%d, running lookup=%u header reads=%u SDK metadata calls=%u; 100000 cached accesses: zero I/O\n",test,initial.valid,initial.error,getCalls,readCalls,metadataCalls);
}
