#include "platform.h"
#include "esp_private/dw_gdma.h"
#include "hal/dw_gdma_hal.h"
#undef free
struct HostIrq { int flags; intr_handler_t handler; void *argument; bool used; } irq[8];
struct Block {void *p;size_t n;unsigned caps;} blocks[32];
static int fault_after=-1, allocation_calls, irq_fail;
static unsigned checks, delivered, masked, callbacks, peak, allocations;
unsigned yield_count;
uint32_t pending_status;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"DW check %d: %s\n",__LINE__,#x);abort();}}while(0)
static unsigned live(void) {unsigned n=0;for(unsigned i=0;i<32;++i)n+=blocks[i].p!=NULL;return n;}
void *heap_caps_calloc(size_t n,size_t s,unsigned caps) {
 if(fault_after>=0&&allocation_calls++==fault_after)return NULL;
 void *p=calloc(n,s);CHECK(p);++allocations;
 for(unsigned i=0;i<32;++i)if(!blocks[i].p){blocks[i]=(struct Block){p,n*s,caps};if(live()>peak)peak=live();return p;}
 abort();
}
void *heap_caps_aligned_calloc(size_t a,size_t n,size_t s,unsigned caps){(void)a;return heap_caps_calloc(n,s,caps);}
void tracked_free(void *p){if(!p)return;for(unsigned i=0;i<32;++i)if(blocks[i].p==p){blocks[i].p=NULL;free(p);return;}abort();}
bool esp_ptr_internal(const void *p){for(unsigned i=0;i<32;++i)if(blocks[i].p==p)return (blocks[i].caps&MALLOC_CAP_INTERNAL)!=0;return p==(void*)(uintptr_t)0x1234;}
bool esp_ptr_in_iram(const void *p){return p!=(void*)(uintptr_t)0x40000000;}
int esp_intr_alloc_intrstatus(int src,int flags,uint32_t reg,uint32_t mask,intr_handler_t h,void *arg,intr_handle_t *out){
 CHECK(src==ETS_DW_GDMA_INTR_SOURCE&&reg==0x50000000u&&mask);if(irq_fail)return ESP_FAIL;
 for(unsigned i=0;i<8;++i)if(!irq[i].used){irq[i]=(struct HostIrq){flags,h,arg,true};*out=&irq[i];return ESP_OK;}return ESP_FAIL;
}
int esp_intr_free(intr_handle_t h){CHECK(h->used);h->used=false;return ESP_OK;}
void dw_gdma_hal_init(dw_gdma_hal_context_t*h,const dw_gdma_hal_config_t*c){(void)c;h->dev=(void*)(uintptr_t)0x50000000u;}
void dw_gdma_hal_deinit(dw_gdma_hal_context_t*h){h->dev=NULL;}
static bool eof(dw_gdma_channel_handle_t ch,const dw_gdma_trans_done_event_data_t*e,void*ctx){CHECK(ch&&e&&ctx==(void*)(uintptr_t)0x1234);++callbacks;return true;}
static void dispatch(bool cache_off){for(unsigned i=0;i<8;++i)if(irq[i].used){if(cache_off&&!(irq[i].flags&ESP_INTR_FLAG_IRAM)){++masked;continue;}pending_status=2;irq[i].handler(irq[i].argument);CHECK(!pending_status);++delivered;}}
static void no_handles(void){CHECK(!live());for(unsigned i=0;i<8;++i)CHECK(!irq[i].used);}
int main(void){
 dw_gdma_channel_alloc_config_t conf={.src={.block_transfer_type=DW_GDMA_BLOCK_TRANSFER_LIST,.role=DW_GDMA_ROLE_MEM,.num_outstanding_requests=5},.dst={.block_transfer_type=DW_GDMA_BLOCK_TRANSFER_LIST,.role=DW_GDMA_ROLE_PERIPH_DSI,.num_outstanding_requests=2}};
 dw_gdma_link_list_config_t lc={.num_items=1,.link_type=DW_GDMA_LINKED_LIST_TYPE_SINGLY};dw_gdma_event_callbacks_t cb={.on_full_trans_done=eof};
 for(unsigned cycle=0;cycle<200;++cycle){
  dw_gdma_channel_handle_t ch=NULL;dw_gdma_link_list_handle_t list=NULL;
  CHECK(dw_gdma_new_channel(&conf,&ch)==ESP_OK);CHECK(dw_gdma_new_link_list(&lc,&list)==ESP_OK);
  CHECK(live()==4);
  for(unsigned i=0;i<32;++i)if(blocks[i].p){if(EXPECT_SAFE)CHECK(blocks[i].caps&MALLOC_CAP_INTERNAL);else CHECK(blocks[i].caps==(MALLOC_CAP_DEFAULT)||(blocks[i].caps&MALLOC_CAP_DMA));}
  CHECK(dw_gdma_channel_register_event_callbacks(ch,&cb,(void*)(uintptr_t)0x1234)==ESP_OK);
  CHECK(irq[0].flags==(EXPECT_SAFE?1294:270));
  CHECK(dw_gdma_link_list_get_item(list,0));
  CHECK(dw_gdma_channel_use_link_list(ch,list)==ESP_OK);CHECK(dw_gdma_channel_enable_ctrl(ch,true)==ESP_OK);
  for(unsigned i=0;i<60;++i)dispatch(true);
  dispatch(false);
  CHECK(dw_gdma_del_link_list(list)==ESP_OK);CHECK(dw_gdma_del_channel(ch)==ESP_OK);no_handles();
 }
 CHECK(callbacks==(EXPECT_SAFE?12200u:200u));CHECK(masked==(EXPECT_SAFE?0u:12000u));CHECK(yield_count==callbacks);
 // Real native cleanup paths on each allocation failure.
 for(int k=0;k<2;++k){fault_after=k;allocation_calls=0;dw_gdma_channel_handle_t ch=NULL;CHECK(dw_gdma_new_channel(&conf,&ch)==ESP_ERR_NO_MEM);CHECK(!ch);no_handles();}
 fault_after=-1;
 for(int k=0;k<2;++k){dw_gdma_channel_handle_t ch=NULL;CHECK(dw_gdma_new_channel(&conf,&ch)==ESP_OK);fault_after=k;allocation_calls=0;dw_gdma_link_list_handle_t list=NULL;CHECK(dw_gdma_new_link_list(&lc,&list)==ESP_ERR_NO_MEM);CHECK(!list);fault_after=-1;CHECK(dw_gdma_del_channel(ch)==ESP_OK);no_handles();}
 dw_gdma_channel_handle_t ch=NULL;CHECK(dw_gdma_new_channel(&conf,&ch)==ESP_OK);irq_fail=1;CHECK(dw_gdma_channel_register_event_callbacks(ch,&cb,(void*)(uintptr_t)0x1234)==ESP_FAIL);irq_fail=0;
 if(EXPECT_SAFE){CHECK(dw_gdma_channel_register_event_callbacks(ch,&cb,(void*)(uintptr_t)0x40000000)==ESP_ERR_INVALID_ARG);cb.on_full_trans_done=(void*)(uintptr_t)0x40000000;CHECK(dw_gdma_channel_register_event_callbacks(ch,&cb,(void*)(uintptr_t)0x1234)==ESP_ERR_INVALID_ARG);}
 CHECK(dw_gdma_del_channel(ch)==ESP_OK);no_handles();
 printf("{\"checks\":%u,\"cycles\":200,\"cacheOffCallbacks\":%u,\"maskedCacheOffEvents\":%u,\"liveBlocks\":%u,\"peakBlocks\":%u,\"allocations\":%u}\n",checks,callbacks-200,masked,live(),peak,allocations);
}
