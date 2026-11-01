#pragma once
// Test-only ESP/NimBLE boundary. Production callback bodies are compiled unchanged.
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <vector>
#include <ctime>
#include <new>
#include "Tab5MeshStore.h"
#define CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU 256
#define CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT 12
#define CONFIG_BT_NIMBLE_MSYS_1_BLOCK_SIZE 256
#define CONFIG_BT_NIMBLE_MSYS_2_BLOCK_COUNT 24
#define CONFIG_BT_NIMBLE_MSYS_2_BLOCK_SIZE 320
#define CONFIG_BT_NIMBLE_TRANSPORT_ACL_FROM_LL_COUNT 24
#define CONFIG_BT_NIMBLE_TRANSPORT_ACL_SIZE 255
#define CONFIG_BT_NIMBLE_TRANSPORT_EVT_COUNT 30
#define CONFIG_BT_NIMBLE_TRANSPORT_EVT_DISCARD_COUNT 8
#define CONFIG_BT_NIMBLE_TRANSPORT_EVT_SIZE 70
#define CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE 5120
#define TAB5_MESH_EXTERNAL_POOLS 1
#include "Tab5MeshMemory.h"
static bool loadHistoryFails=false;static bool storageReady=false;static bool tab5SdAvailable(){return storageReady;}
static Tab5MeshModel::History storedHistory;static Tab5MeshModel::Preferences storedPreferences;static unsigned stores=0,prefStores=0;
namespace Tab5MeshStore {bool loadPreferences(Tab5MeshModel::Preferences& p){p=storedPreferences;return true;} bool savePreferences(const Tab5MeshModel::Preferences&p){storedPreferences=p;++prefStores;return true;} bool loadHistory(Tab5MeshModel::History&h){if(loadHistoryFails)return false;h=storedHistory;return true;} bool saveHistory(const Tab5MeshModel::History&h){storedHistory=h;++stores;return true;}}
#include <lvgl.h>
#include "Tab5Meshtastic.h"
#include "Tab5MeshtasticProtocol.h"
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5UiStyle.h"
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static bool heapFail=false,admit=true,denyBlock=false,denyExternal=false,scheduleIo=false,scheduleBle=false;
static void(*pendingIo)(void*)=nullptr;static void* pendingIoArg=nullptr;
static unsigned heapAllocations=0,heapFrees=0,taskRequests=0;
static void* heap_caps_malloc(size_t n,int){if(heapFail)return nullptr;++heapAllocations;return malloc(n);}
static void* heap_caps_calloc(size_t a,size_t b,int){if(heapFail)return nullptr;++heapAllocations;return calloc(a,b);}
static void heap_caps_free(void*p){if(p){++heapFrees;free(p);}}
static uint32_t now=100;
static uint32_t millis(){return now;}
static uint32_t esp_random(){return 0x12345678;}
static void tab5TaskStarted(const char*){}static void tab5TaskCheckpoint(const char*){}
static void vTaskDelay(unsigned n){now+=n;}
static void vTaskDelete(void*){}
#define pdMS_TO_TICKS(x) (x)
#define pdPASS 1
namespace Tab5Resources {enum class Owner{Tools};struct Budget{size_t internal,internalBlock,c,psram,psramBlock;};enum class Denial{Internal,InternalBlock,Psram,PsramBlock};struct AdmissionReport{struct {size_t internal=100000,internalBlock=40000,psram=2000000,psramBlock=1000000;}memory;Budget requested{};Denial reason=Denial::Internal;};struct Reservation{operator bool()const{return admit;}void release(){}};}
static Tab5Resources::Reservation tab5ReserveMemory(Tab5Resources::Owner,Tab5Resources::Budget b,const char*,Tab5Resources::AdmissionReport* r=nullptr){if(r){r->requested=b;r->reason=denyExternal?(denyBlock?Tab5Resources::Denial::PsramBlock:Tab5Resources::Denial::Psram):(denyBlock?Tab5Resources::Denial::InternalBlock:Tab5Resources::Denial::Internal);}return {};}
static int xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,unsigned,void*,int){++taskRequests;return 0;}
static int tab5CreateTask(void(*f)(void*),const char*n,unsigned s,void*a,unsigned p,void*h,int c){if(scheduleIo&&!strcmp(n,"mesh_store")){pendingIo=f;pendingIoArg=a;++taskRequests;return pdPASS;}if(scheduleBle&&!strcmp(n,"tab5_meshtastic")){++taskRequests;return pdPASS;}return xTaskCreatePinnedToCore(f,n,s,a,p,h,c);}
#define ESP_OK 0
static unsigned hostedStarts=0,hostedStops=0,nimbleStarts=0,nimbleStops=0;static int nimbleResult=0;
static bool hostedIsBLEActive(){return false;}static bool hostedInitBLE(){++hostedStarts;return true;}static bool hostedDeinitBLE(){++hostedStops;return true;}
static int nimble_port_init(){++nimbleStarts;return nimbleResult;}static int nimble_port_stop(){return 0;}static int nimble_port_deinit(){++nimbleStops;return 0;}static void nimble_port_run(){}
static void ble_store_config_init(){}static int ble_store_util_status_rr(int,void*){return 0;}
struct ble_uuid_t{uint8_t type;};struct ble_uuid128_t{ble_uuid_t u;uint8_t value[16];};struct ble_uuid16_t{ble_uuid_t u;uint16_t value;};union ble_uuid_any_t{ble_uuid_t u;ble_uuid128_t u128;ble_uuid16_t u16;};
#define BLE_UUID128_INIT(...) {{128},{__VA_ARGS__}}
static int ble_uuid_cmp(const ble_uuid_t*a,const ble_uuid_t*b){return a->type!=b->type?1:memcmp(a,b,a->type==128?sizeof(ble_uuid128_t):sizeof(ble_uuid16_t));}
static uint16_t ble_uuid_u16(const ble_uuid_t*a){return a->type==16?reinterpret_cast<const ble_uuid16_t*>(a)->value:0;}
struct ble_addr_t{uint8_t type;uint8_t val[6];};
#define BLE_HS_CONN_HANDLE_NONE 65535
#define BLE_HS_EDONE 14
#define BLE_HS_EAPP 10
#define BLE_HS_EINVAL 3
#define BLE_HS_ENOMEM 6
#define BLE_HS_EALREADY 2
#define BLE_HS_IO_KEYBOARD_DISPLAY 4
#define BLE_SM_PAIR_KEY_DIST_ENC 1
#define BLE_SM_IOACT_INPUT 2
#define BLE_SM_IOACT_NUMCMP 4
#define BLE_ERR_REM_USER_CONN_TERM 19
#define BLE_GATT_CHR_PROP_READ 2
#define BLE_GATT_CHR_PROP_WRITE 8
#define BLE_GATT_CHR_PROP_NOTIFY 16
#define BLE_GAP_REPEAT_PAIRING_IGNORE 0
#define BLE_GAP_EVENT_DISC 1
#define BLE_GAP_EVENT_DISC_COMPLETE 2
#define BLE_GAP_EVENT_CONNECT 3
#define BLE_GAP_EVENT_DISCONNECT 4
#define BLE_GAP_EVENT_ENC_CHANGE 5
#define BLE_GAP_EVENT_PASSKEY_ACTION 6
#define BLE_GAP_EVENT_NOTIFY_RX 7
#define BLE_GAP_EVENT_REPEAT_PAIRING 8
struct os_mbuf{std::vector<uint8_t> bytes;};
#define OS_MBUF_PKTLEN(p) ((p)->bytes.size())
static int os_mbuf_copydata(os_mbuf*m,unsigned offset,unsigned n,void*out){if(offset+n>m->bytes.size())return 1;memcpy(out,m->bytes.data()+offset,n);return 0;}
struct ble_gatt_error{uint16_t status=0,att_handle=0;};struct ble_gatt_attr{uint16_t handle=0,offset=0;os_mbuf* om=nullptr;};
struct ble_gatt_dsc{uint16_t handle;ble_uuid_any_t uuid;};struct ble_gatt_chr{uint16_t def_handle,val_handle;uint8_t properties;ble_uuid_any_t uuid;};struct ble_gatt_svc{uint16_t start_handle,end_handle;ble_uuid_any_t uuid;};
struct ble_gap_conn_desc{struct{bool encrypted=true,authenticated=true;}sec_state;};
struct ble_hs_adv_fields{const ble_uuid128_t*uuids128=nullptr;unsigned num_uuids128=0;const uint8_t*name=nullptr;unsigned name_len=0;};
struct ble_gap_disc_params{unsigned passive=0,filter_duplicates=0,itvl=0,window=0;};
struct ble_sm_io{uint8_t action=0;uint32_t passkey=0;uint8_t numcmp_accept=0;};
struct ble_gap_event{
 unsigned type;
 struct{const uint8_t*data;unsigned length_data;ble_addr_t addr;int8_t rssi;}disc;
 struct{int status;uint16_t conn_handle;}connect,enc_change;
 struct{struct{uint8_t action;uint32_t numcmp;}params;}passkey;
 struct{uint16_t conn_handle,attr_handle;}notify_rx;
};
static struct{void(*reset_cb)(int);void(*sync_cb)();int(*store_status_cb)(int,void*);int sm_io_cap,sm_bonding,sm_mitm,sm_sc,sm_our_key_dist,sm_their_key_dist;}ble_hs_cfg;
using AttrCB=int(*)(uint16_t,const ble_gatt_error*,ble_gatt_attr*,void*);
static unsigned discovers=0,securityRequests=0,writes=0,terminations=0,injections=0;
static uint16_t writtenHandle=0;static std::vector<uint8_t> writtenBytes;static ble_hs_adv_fields advertisement;
static bool authenticated=true;
static int addressResult=0;static unsigned addressRequests=0;
static int ble_hs_util_ensure_addr(int){++addressRequests;return addressResult;}
static int ble_hs_id_infer_auto(int,uint8_t*p){*p=0;return 0;}
static int ble_att_set_preferred_mtu(int){return 0;}static unsigned ble_att_mtu(uint16_t){return 23;}
static int ble_gattc_write_flat(uint16_t,uint16_t h,const void*p,uint16_t n,AttrCB,void*){++writes;writtenHandle=h;writtenBytes.assign((const uint8_t*)p,(const uint8_t*)p+n);return 0;}
static int ble_gattc_write_long(uint16_t,uint16_t h,uint16_t,os_mbuf*p,AttrCB cb,void*arg){int r=ble_gattc_write_flat(0,h,p->bytes.data(),p->bytes.size(),cb,arg);delete p;return r;}
static os_mbuf* ble_hs_mbuf_from_flat(void*p,unsigned n){return new os_mbuf{{(uint8_t*)p,(uint8_t*)p+n}};}
static int ble_gattc_read_long(uint16_t,uint16_t,uint16_t,AttrCB,void*){return 0;}
static int ble_gattc_disc_all_dscs(uint16_t,uint16_t,uint16_t,int(*)(uint16_t,const ble_gatt_error*,uint16_t,const ble_gatt_dsc*,void*),void*){return 0;}
static int ble_gattc_disc_all_chrs(uint16_t,uint16_t,uint16_t,int(*)(uint16_t,const ble_gatt_error*,const ble_gatt_chr*,void*),void*){return 0;}
static int ble_gattc_disc_svc_by_uuid(uint16_t,const ble_uuid_t*,int(*)(uint16_t,const ble_gatt_error*,const ble_gatt_svc*,void*),void*){++discovers;return 0;}
static int ble_gattc_exchange_mtu(uint16_t,int(*)(uint16_t,const ble_gatt_error*,uint16_t,void*),void*){return 0;}
static int ble_gap_security_initiate(uint16_t){++securityRequests;return 0;}
static int ble_gap_conn_find(uint16_t,ble_gap_conn_desc*p){p->sec_state.authenticated=authenticated;return 0;}
static int ble_gap_terminate(uint16_t,unsigned){++terminations;return 0;}
static int ble_hs_adv_parse_fields(ble_hs_adv_fields*f,const uint8_t*,unsigned){*f=advertisement;return 0;}
static int ble_gap_disc_cancel(){return 0;}static int ble_gap_conn_cancel(){return 0;}
static int ble_gap_disc(uint8_t,unsigned,ble_gap_disc_params*,int(*)(ble_gap_event*,void*),void*){return 0;}
static int ble_gap_connect(uint8_t,ble_addr_t*,unsigned,void*,int(*)(ble_gap_event*,void*),void*){return 0;}
static int ble_sm_inject_io(uint16_t,ble_sm_io*){++injections;return 0;}
