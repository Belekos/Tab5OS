#pragma once
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class String : public std::string {
 public:
 using std::string::string;
 String()=default;
 String(const std::string& value):std::string(value){}
 String(size_t value):std::string(std::to_string(value)){}
 friend String operator+(const String& a,const char* b){return String(static_cast<const std::string&>(a)+b);}
 friend String operator+(const String& a,const String& b){return String(static_cast<const std::string&>(a)+static_cast<const std::string&>(b));}
};
constexpr int FILE_READ=0,pdPASS=1,ESP_OK=0;
constexpr uint8_t ESP_IMAGE_HEADER_MAGIC=0xe9;
constexpr uint16_t ESP_CHIP_ID_ESP32P4=0x12;
constexpr uint32_t ESP_APP_DESC_MAGIC_WORD=0xabcd5432;
struct esp_image_header_t {uint8_t magic=0,segment_count=0;uint16_t chip_id=0,min_chip_rev_full=0,max_chip_rev_full=0;};
struct esp_image_segment_header_t {uint32_t load_addr=0,data_len=0;};
struct esp_app_desc_t {uint32_t magic_word=0;char project_name[32]{},version[32]{};};
struct esp_chip_info_t {uint16_t revision=300;};
struct esp_partition_t {size_t size=1024*1024;};
using esp_err_t=int;
using esp_ota_handle_t=unsigned;
namespace Tab5Firmware {void cancel();}
namespace mock {
inline std::map<std::string,std::shared_ptr<std::vector<uint8_t>>> files;
inline std::function<void()> job;
inline esp_partition_t target,running;
inline std::string lastOpen;
inline bool leaseAvailable=true,taskAvailable=true,failBegin=false,failWrite=false,failEnd=false,failBoot=false,cancelWrite=false;
inline unsigned begins=0,writes=0,ends=0,aborts=0,boots=0,restarts=0;
inline std::string key(std::string path){for(auto& c:path)c=char(std::tolower(static_cast<unsigned char>(c)));return path;}
inline void execute(){assert(job);auto run=std::move(job);job={};run();}
inline void file(const std::string& path,size_t payload=8192){
 auto bytes=std::make_shared<std::vector<uint8_t>>();
 esp_image_header_t header{};header.magic=ESP_IMAGE_HEADER_MAGIC;header.segment_count=1;header.chip_id=ESP_CHIP_ID_ESP32P4;
 esp_image_segment_header_t segment{};segment.data_len=sizeof(esp_app_desc_t)+uint32_t(payload);
 esp_app_desc_t desc{};desc.magic_word=ESP_APP_DESC_MAGIC_WORD;std::memcpy(desc.project_name,"Test",5);std::memcpy(desc.version,"1",2);
 auto add=[&](const void* p,size_t count){const auto* first=static_cast<const uint8_t*>(p);bytes->insert(bytes->end(),first,first+count);};
 add(&header,sizeof(header));add(&segment,sizeof(segment));add(&desc,sizeof(desc));bytes->resize(bytes->size()+payload,0x57);files[key(path)]=bytes;
}
}
class File {
 std::shared_ptr<std::vector<uint8_t>> bytes;size_t offset=0;
 public:
 File()=default;explicit File(std::shared_ptr<std::vector<uint8_t>> value):bytes(std::move(value)){}
 explicit operator bool()const{return bool(bytes);}
 bool isDirectory()const{return false;}
 size_t size()const{return bytes?bytes->size():0;}
 size_t read(uint8_t* output,size_t count){if(!bytes)return 0;count=std::min(count,bytes->size()-offset);std::memcpy(output,bytes->data()+offset,count);offset+=count;return count;}
 bool seek(size_t pos){if(!bytes||pos>bytes->size())return false;offset=pos;return true;}
};
struct MockFS {File open(const String& path,int){mock::lastOpen=path;auto i=mock::files.find(mock::key(path));return i==mock::files.end()?File{}:File(i->second);}};
inline MockFS tab5FS;
struct Tab5StorageUse {explicit Tab5StorageUse(bool){}explicit operator bool()const{return mock::leaseAvailable;}};
inline void tab5TaskCheckpoint(const char*){}
inline void vTaskDelay(unsigned){}
inline void vTaskDelete(void*){}
template<class Function>int tab5CreateTask(Function function,const char*,unsigned,void* argument,unsigned,void*,int){if(!mock::taskAvailable)return 0;mock::job=[=](){function(argument);};return pdPASS;}
inline const esp_partition_t* esp_ota_get_next_update_partition(void*){return &mock::target;}
inline const esp_partition_t* esp_ota_get_running_partition(){return &mock::running;}
inline void esp_chip_info(esp_chip_info_t* chip){chip->revision=300;}
inline int esp_ota_begin(const esp_partition_t*,size_t,esp_ota_handle_t* handle){++mock::begins;*handle=1;return mock::failBegin?-1:ESP_OK;}
inline int esp_ota_write(esp_ota_handle_t,const void*,size_t){++mock::writes;if(mock::cancelWrite)Tab5Firmware::cancel();return mock::failWrite?-1:ESP_OK;}
inline int esp_ota_abort(esp_ota_handle_t){++mock::aborts;return ESP_OK;}
inline int esp_ota_end(esp_ota_handle_t){++mock::ends;return mock::failEnd?-1:ESP_OK;}
inline int esp_ota_set_boot_partition(const esp_partition_t*){++mock::boots;return mock::failBoot?-1:ESP_OK;}
inline void esp_restart(){++mock::restarts;}
// Deterministic digest substitute: tests exercise review/OTA control flow, not
// SHA-256 correctness. Target compilation uses the real mbedTLS implementation.
struct mbedtls_sha256_context {uint32_t state=2166136261U;};
inline void mbedtls_sha256_init(mbedtls_sha256_context* ctx){ctx->state=2166136261U;}
inline int mbedtls_sha256_starts(mbedtls_sha256_context*,int){return 0;}
inline int mbedtls_sha256_update(mbedtls_sha256_context* ctx,const uint8_t* data,size_t size){while(size--)ctx->state=(ctx->state^*data++)*16777619U;return 0;}
inline int mbedtls_sha256_finish(mbedtls_sha256_context* ctx,uint8_t* digest){for(unsigned i=0;i<32;++i)digest[i]=uint8_t(ctx->state>>((i%4)*8));return 0;}
inline void mbedtls_sha256_free(mbedtls_sha256_context*){}
