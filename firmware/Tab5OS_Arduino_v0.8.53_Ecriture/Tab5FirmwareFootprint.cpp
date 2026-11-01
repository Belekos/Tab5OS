#include "Tab5FirmwareFootprint.h"
#include <esp_ota_ops.h>
#include <esp_image_format.h>
#include <esp_partition.h>
#include <stdint.h>

namespace Tab5FirmwareFootprint {
namespace {
Snapshot read() {
  Snapshot s;
  const auto* partition=esp_ota_get_running_partition();
  if(!partition){s.error=ESP_ERR_NOT_FOUND;return s;}
  s.partitionBytes=partition->size;
  s.partitionAddress=partition->address;
  if(partition->size<sizeof(esp_image_header_t)||
      partition->address>UINT32_MAX-partition->size){
    s.error=ESP_ERR_INVALID_SIZE;return s;
  }
  // get_metadata deliberately skips verify_image_header in ESP-IDF 5.5.4.
  // Bound its segment array before calling it, using the official header type.
  esp_image_header_t header{};
  s.error=esp_partition_read(partition,0,&header,sizeof(header));
  if(s.error!=ESP_OK)return s;
  if(header.magic!=ESP_IMAGE_HEADER_MAGIC||header.segment_count==0||
      header.segment_count>ESP_IMAGE_MAX_SEGMENTS){
    s.error=ESP_ERR_IMAGE_INVALID;return s;
  }
  const esp_partition_pos_t position={partition->address,partition->size};
  esp_image_metadata_t metadata{};
  // Unlike ESP.getSketchSize()/esp_image_verify, this API skips segment
  // payloads and checksum/SHA processing. It reads segment headers and the
  // appended 32-byte digest, if present. Boot already verified the image.
  s.error=esp_image_get_metadata(&position,&metadata);
  if(s.error!=ESP_OK)return s;
  if(metadata.start_addr!=partition->address||metadata.image_len==0||
      metadata.image_len>partition->size){
    s.error=ESP_ERR_IMAGE_INVALID;return s;
  }
  s.imageBytes=metadata.image_len;
  s.valid=true;
  return s;
}
}
const Snapshot& snapshot(){static const Snapshot value=read();return value;}
}
