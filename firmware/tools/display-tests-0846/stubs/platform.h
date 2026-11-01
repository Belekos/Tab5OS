#pragma once
#include <cstdint>
#include <cstddef>
using TaskHandle_t=void*;
using esp_lcd_panel_handle_t=void*;
struct esp_lcd_dpi_panel_event_data_t {};
struct esp_lcd_dpi_panel_event_callbacks_t { bool (*on_refresh_done)(esp_lcd_panel_handle_t,esp_lcd_dpi_panel_event_data_t*,void*)=nullptr; };
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_TIMEOUT=1,ESP_ERR_NOT_SUPPORTED=2,ESP_ERR_INVALID_STATE=3,ESP_ERR_NO_MEM=4,ESP_ERR_INVALID_ARG=5;
constexpr int ESP_CACHE_MSYNC_FLAG_DIR_C2M=1, ESP_CACHE_MSYNC_FLAG_TYPE_DATA=2;
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
#define IRAM_ATTR
#define DRAM_ATTR
#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,5,4)
#define CONFIG_IDF_TARGET_ESP32P4 1
namespace lgfx {
enum class bus_type_t { bus_dsi };
struct Bus { bus_type_t busType(){return bus_type_t::bus_dsi;} };
struct Panel_DSI {
  struct Config {int panel_width=720,panel_height=1280,memory_width=720,memory_height=1280,offset_x=0,offset_y=0;uint8_t offset_rotation=0;};
  struct Detail {void* buffer=nullptr;uint16_t dpi_freq_mhz=80,hsync_back_porch=140,hsync_pulse_width=40,hsync_front_porch=40,vsync_back_porch=20,vsync_pulse_width=4,vsync_front_porch=20;};
  Config cfg; Detail detail; Bus bus;int count=0;uint8_t rotation=0;bool autoDisplay=true;
  int getStartCount()const{return count;} bool getAutoDisplay()const{return autoDisplay;}
  const Config& config()const{return cfg;} Detail config_detail()const{return detail;}
  void config_detail(const Detail& d){detail=d;} uint8_t getRotation()const{return rotation;}
  Bus* getBus(){return &bus;}
  void init(void* b,uint8_t** l){detail.buffer=b;_lines_buffer=l;_disp_panel_handle=this;}
  uint8_t* row(unsigned i){return _lines_buffer[i];}
protected:
  esp_lcd_panel_handle_t _disp_panel_handle=nullptr;uint8_t** _lines_buffer=nullptr;
};
}
namespace m5 {enum board_t {board_M5Tab5};}
struct DisplayMock {lgfx::Panel_DSI panel;lgfx::Panel_DSI* getPanel(){return &panel;}int getColorDepth(){return 16;}};
struct M5Mock {DisplayMock Display; m5::board_t getBoard(){return m5::board_M5Tab5;}};
extern M5Mock M5;
TaskHandle_t xTaskGetCurrentTaskHandle();void vTaskDelay(unsigned);
int64_t esp_timer_get_time();
bool esp_ptr_external_ram(const void*);
void* heap_caps_malloc(size_t,int);void heap_caps_free(void*);
int esp_cache_msync(void*,size_t,int);
int esp_lcd_dpi_panel_get_frame_buffer(esp_lcd_panel_handle_t,unsigned,void**,void**);
int esp_lcd_dpi_panel_register_event_callbacks(esp_lcd_panel_handle_t,const esp_lcd_dpi_panel_event_callbacks_t*,void*);
int esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t,int,int,int,int,const void*);
