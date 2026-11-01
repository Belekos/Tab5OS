#include "Tab5CryptoRandom.h"
#include <mbedtls/ctr_drbg.h>
#include <bootloader_random.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
namespace Tab5CryptoRandom {
namespace {
mbedtls_ctr_drbg_context generator;bool initialized=false,seeding=false,ready=false;
StaticSemaphore_t storage;
SemaphoreHandle_t mutex(){static auto m=xSemaphoreCreateMutexStatic(&storage);return m;}
int entropy(void*,unsigned char* out,size_t size){if(!seeding)return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;esp_fill_random(out,size);return 0;}
}
bool begin(){
 if(initialized)return ready;initialized=true;mbedtls_ctr_drbg_init(&generator);
 bootloader_random_enable();seeding=true;
 const unsigned char label[]="Tab5OS vault RNG v1";
 int result=mbedtls_ctr_drbg_seed(&generator,entropy,nullptr,label,sizeof(label)-1);
 seeding=false;bootloader_random_disable();
 if(result){mbedtls_ctr_drbg_free(&generator);return false;}
 // No later ADC takeover. The boot seed feeds a cryptographic generator;
 // if its reseed limit is reached, fail closed until the next reboot.
 mbedtls_ctr_drbg_set_reseed_interval(&generator,1000000);ready=true;return true;
}
bool fill(void* destination,size_t size){
 if(!ready||!destination||!size||size>MBEDTLS_CTR_DRBG_MAX_REQUEST)return false;
 xSemaphoreTake(mutex(),portMAX_DELAY);
 int result=mbedtls_ctr_drbg_random(&generator,static_cast<unsigned char*>(destination),size);
 xSemaphoreGive(mutex());return result==0;
}
}
