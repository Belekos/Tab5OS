#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#define CONFIG_IDF_TARGET_ESP32P4 1
#define CONFIG_LCD_DSI_OBJ_FORCE_INTERNAL 1
#define CONFIG_LCD_DSI_ISR_HANDLER_IN_IRAM 1
#define CONFIG_DW_GDMA_ENABLE_DEBUG_LOG 0
#define SOC_RCC_IS_INDEPENDENT 1
#define SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE 0
#define DRAM_ATTR
#define IRAM_ATTR
#define DW_GDMA_LL_GROUPS 1
#define DW_GDMA_LL_CHANNELS_PER_GROUP 4
#define DW_GDMA_LL_MAX_OUTSTANDING_REQUESTS 16
#define DW_GDMA_LL_LINK_LIST_ALIGNMENT 64
#define DW_GDMA_LL_MASTER_PORT_MEMORY 1
#define DW_GDMA_LL_CHANNEL_EVENT_BLOCK_TFR_DONE 1
#define DW_GDMA_LL_CHANNEL_EVENT_DMA_TFR_DONE 2
#define DW_GDMA_LL_CHANNEL_EVENT_SHADOWREG_OR_LLI_INVALID_ERR 8192
#define DW_GDMA_LL_CHANNEL_EVENT_MASK(x) (1u<<(x))
#define MALLOC_CAP_8BIT 4
#define MALLOC_CAP_DMA 8
#define MALLOC_CAP_INTERNAL 2048
#define MALLOC_CAP_DEFAULT 4096
#define ESP_INTR_FLAG_IRAM 1024
#define ESP_INTR_FLAG_SHARED 256
#define ESP_INTR_FLAG_LOWMED 14
#define ETS_DW_GDMA_INTR_SOURCE 17
#define CACHE_LL_LEVEL_INT_MEM 0
#define CACHE_TYPE_DATA 0
#define ESP_CACHE_MSYNC_FLAG_DIR_C2M 1
#define ESP_CACHE_MSYNC_FLAG_INVALIDATE 2
#define ESP_CACHE_MSYNC_FLAG_UNALIGNED 4
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_LOG_DEBUG 0
#define ESP_LOGD(...) ((void)0)
#define ESP_DRAM_LOGE(...) ((void)0)
#define ESP_RETURN_ON_FALSE(cond,err,...) do {if(!(cond))return (err);}while(0)
#define ESP_RETURN_ON_FALSE_ISR ESP_RETURN_ON_FALSE
#define ESP_RETURN_ON_ERROR(expr,...) do {int e=(expr);if(e)return e;}while(0)
#define ESP_GOTO_ON_FALSE(cond,err,label,...) do {if(!(cond)){ret=(err);goto label;}}while(0)
#define ESP_GOTO_ON_ERROR(expr,label,...) do {int e=(expr);if(e){ret=e;goto label;}}while(0)
typedef int esp_err_t;
typedef int _lock_t;
typedef int portMUX_TYPE;
#define _lock_acquire(x) ((void)(x))
#define _lock_release(x) ((void)(x))
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portMUX_INITIALIZE(x) (*(x)=0)
#define portYIELD_FROM_ISR() (++yield_count)
typedef struct dw_gdma_link_list_item_t { uint64_t words[8]; } dw_gdma_link_list_item_t;
typedef void (*intr_handler_t)(void*);
typedef struct HostIrq *intr_handle_t;
extern unsigned yield_count;
extern uint32_t pending_status;
void *heap_caps_calloc(size_t,size_t,unsigned);
void *heap_caps_aligned_calloc(size_t,size_t,size_t,unsigned);
void tracked_free(void *);
#define free tracked_free
bool esp_ptr_internal(const void *);
bool esp_ptr_in_iram(const void *);
int esp_intr_alloc_intrstatus(int,int,uint32_t,uint32_t,intr_handler_t,void*,intr_handle_t*);
int esp_intr_free(intr_handle_t);
#define esp_cache_msync(...) 0
#define cache_hal_get_cache_line_size(...) 0
#define dw_gdma_ll_channel_get_intr_status(...) pending_status
#define dw_gdma_ll_channel_clear_intr(...) (pending_status=0)
#define dw_gdma_ll_get_intr_status_reg(...) ((void*)(uintptr_t)0x50000000u)
#define dw_gdma_ll_channel_get_current_link_list_item_addr(...) 0
