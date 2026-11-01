#include "esp_intr_alloc.h"
intr_handler_t sdkHandler=nullptr;
void* sdkArgument=nullptr;
int sdkSource=0,sdkFlags=0,sdkError=0,sdkCalls=0;
intr_handle_t* sdkResult=nullptr;
extern "C" int esp_intr_alloc(int source,int flags,intr_handler_t handler,void* arg,intr_handle_t* result){
 ++sdkCalls;sdkSource=source;sdkFlags=flags;sdkResult=result;
 if(sdkError)return sdkError;
 sdkHandler=handler;sdkArgument=arg;if(result)*result=reinterpret_cast<void*>(123);
 return ESP_OK;
}
