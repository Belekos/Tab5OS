#include "Tab5Meshtastic.h"
#include "Tab5MeshtasticProtocol.h"
#include "Tab5MeshStore.h"
#include "Tab5VolumeFS.h"
#include <time.h>
#include <new>
#include "Tab5UiStyle.h"
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5Task.h"
#include <esp32-hal-hosted.h>
#include <esp_random.h>
#include <esp_heap_caps.h>
#include <atomic>
#include "Tab5MeshMemory.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>
extern "C" {
#include <nimble/nimble_port.h>
#include <host/ble_hs.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_sm.h>
#include <host/ble_hs_mbuf.h>
#include <host/util/util.h>
#include <host/ble_store.h>
#include <store/config/ble_store_config.h>
#include <services/gap/ble_svc_gap.h>
void ble_store_config_init(void);
}
namespace Tab5Meshtastic {
namespace {
namespace P=Tab5MeshtasticProtocol;
namespace M=Tab5MeshModel;
constexpr unsigned MaxPeers=12,MaxNodes=M::MaxNodes,MaxMessages=M::MaxMessages;
using Node=M::Node;using Message=M::Message;
constexpr uint16_t NoConnection=BLE_HS_CONN_HANDLE_NONE;
const ble_uuid128_t serviceUuid=BLE_UUID128_INIT(0xfd,0xea,0x73,0xe2,0xca,0x5d,0xa8,0x9f,0x1f,0x46,0xa8,0x15,0x18,0xb2,0xa1,0x6b);
const ble_uuid128_t fromUuid=BLE_UUID128_INIT(0x02,0x00,0x12,0xac,0x42,0x02,0x78,0xb8,0xed,0x11,0x93,0x49,0x9e,0xe6,0x55,0x2c);
const ble_uuid128_t toUuid=BLE_UUID128_INIT(0xe7,0x01,0x44,0x12,0x66,0x78,0xdd,0xa1,0xad,0x4d,0x9e,0x12,0xd2,0x76,0x5c,0xf7);
const ble_uuid128_t numUuid=BLE_UUID128_INIT(0x53,0x44,0xe3,0x47,0x75,0xaa,0x70,0xa6,0x66,0x4f,0x00,0xa8,0x8c,0xa1,0x9d,0xed);
enum class Status:uint8_t {Idle,Starting,Scanning,Connecting,Pairing,Discovering,Configuring,Ready,Stopping,Failed};
enum class Notice:uint8_t {None,Memory,Start,Sync,Connect,Security,Service,Protocol,Write,Read,Timeout,Queue,Stopped,Overflow,TooLong,Storage,Preferences,Internal,InternalBlock,External,ExternalBlock,WorkerStack,HistoryRead};
struct Peer {ble_addr_t address{};char name[40]{};int8_t rssi=0;};
// Shared bounded model; only the UI thread accesses LVGL. BLE callbacks never do.
struct Context {
 Status status=Status::Idle;Notice notice=Notice::None;int error=0;
 bool synced=false,stop=false,scan=false,connect=false,disconnect=false,send=false,allowUnpaired=false;
 bool ready=false,readWanted=false,readActive=false,readReady=false,writeActive=false,configWanted=false;
 uint8_t peerCount=0,connectIndex=0,pairAction=0,submitPair=0;
 uint8_t sendChannel=0;uint16_t connection=NoConnection,start=0,end=0,from=0,to=0,num=0,numEnd=0,ccc=0;
 uint32_t revision=1,peersRevision=1,nodesRevision=1,messagesRevision=1,generation=0,nonce=0,localNode=0;
 uint32_t pairNumber=0,pairInput=0,sendTo=P::Broadcast,sendId=0,deadline=0,lastRead=0;
 size_t readSize=0;uint8_t readBuffer[P::MaxPacket]{};char sendText[P::MaxText+1]{};
 Peer peers[MaxPeers]{};M::History* history=nullptr;
 uint32_t dataRevision=1,savedDataRevision=1,positionRevision=1,nextReconnect=0;
 uint32_t memoryFree=0,memoryBlock=0,memoryNeed=0;
 bool historyLoaded=false,historyLoadFailed=false,manualDisconnect=false,autoconnect=false,launchBlocked=false;
 uint8_t reconnectAttempt=0;
};
Hooks hooks;Context* state=nullptr;M::Preferences preferences;
uint32_t preferencesRevision=1,savedPreferencesRevision=1,nextLaunch=0,lastPersist=0,diskSequence=0,nextIoAttempt=0;
bool paused=false;portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
std::atomic<bool> running{false},hostExited{true};
struct Guard {Guard(){portENTER_CRITICAL(&mux);}~Guard(){portEXIT_CRITICAL(&mux);}};
lv_obj_t *overlay=nullptr,*panel=nullptr,*statusLabel=nullptr,*noticeLabel=nullptr,*peerList=nullptr,*recipient=nullptr,*channel=nullptr,*input=nullptr,*messages=nullptr,*pairRow=nullptr,*pairLabel=nullptr,*pin=nullptr,*pairAccept=nullptr,*unpaired=nullptr,*scanButton=nullptr,*disconnectButton=nullptr,*sendButton=nullptr,*toolbarRow=nullptr,*bodyRow=nullptr,*helpLabel=nullptr;
uint32_t drawn=0,drawnPeers=0,drawnNodes=0,drawnMessages=0,lastUi=0;
uint32_t recipientIds[MaxNodes+1]{};uint8_t recipientCount=1;bool keyboardOpen=false;
const char* tr(const char* s){return hooks.translate?hooks.translate(s):s;}
uint32_t color(unsigned n){return hooks.color?hooks.color(n):(n==2?0xffffff:0x202428);}
const char* statusKey(Status value){switch(value){case Status::Idle:return "Bluetooth arrêté";case Status::Starting:return "Démarrage Bluetooth…";case Status::Scanning:return "Recherche d’appareils Meshtastic…";case Status::Connecting:return "Connexion Bluetooth…";case Status::Pairing:return "Appairage Bluetooth…";case Status::Discovering:return "Lecture du service Meshtastic…";case Status::Configuring:return "Lecture des nœuds Meshtastic…";case Status::Ready:return "Meshtastic connecté";case Status::Stopping:return "Arrêt Bluetooth…";default:return "Meshtastic indisponible";}}
const char* noticeKey(Notice value){switch(value){case Notice::Memory:return "Mémoire insuffisante pour Bluetooth";case Notice::Start:return "Démarrage Bluetooth impossible";case Notice::Sync:return "Le contrôleur Bluetooth ne répond pas";case Notice::Connect:return "Connexion Meshtastic impossible";case Notice::Security:return "Appairage refusé ou code incorrect";case Notice::Service:return "Service Meshtastic incomplet";case Notice::Protocol:return "Paquet Meshtastic invalide";case Notice::Write:return "Envoi à la radio impossible";case Notice::Read:return "Lecture de la radio impossible";case Notice::Timeout:return "Délai Bluetooth dépassé";case Notice::Queue:return "Message refusé par la radio";case Notice::Stopped:return "Connexion interrompue";case Notice::Overflow:return "Liste de nœuds limitée à 32";case Notice::TooLong:return "Message limité à 233 octets UTF-8";case Notice::HistoryRead:return "Lecture de l’historique Meshtastic impossible";case Notice::Storage:return "Historique non enregistré : carte SD indisponible";case Notice::Preferences:return "Enregistrement des réglages Mesh impossible";case Notice::Internal:return "RAM interne insuffisante pour Bluetooth";case Notice::InternalBlock:return "RAM interne fragmentée : bloc Bluetooth indisponible";case Notice::External:return "PSRAM insuffisante pour Bluetooth";case Notice::ExternalBlock:return "PSRAM fragmentée : bloc Bluetooth indisponible";case Notice::WorkerStack:return "Mémoire insuffisante pour la tâche Bluetooth";default:return "";}}
void changed(){if(state)++state->revision;}
// Caller holds Guard. A stopped/disconnected radio must never consume an old
// send request or a previously entered pairing code on its next connection.
void discardCommands(){if(!state)return;state->send=false;state->sendId=0;state->sendTo=P::Broadcast;state->sendChannel=0;state->submitPair=state->pairAction=0;state->pairInput=state->pairNumber=0;memset(state->sendText,0,sizeof(state->sendText));}
void fail(Notice note,int error=0){Guard lock;if(!state)return;state->notice=note;state->error=error;state->status=Status::Failed;discardCommands();if(note==Notice::Security)state->manualDisconnect=true;state->deadline=0;state->ready=false;state->disconnect=state->connection!=NoConnection;changed();}
bool current(uint16_t connection,void* token){Guard lock;return state&&!state->stop&&state->connection==connection&&state->generation==uint32_t(reinterpret_cast<uintptr_t>(token));}
void* token(){Guard lock;return reinterpret_cast<void*>(uintptr_t(state->generation));}
void hostReset(int reason){fail(Notice::Sync,reason);}
void hostSync(){int rc=ble_hs_util_ensure_addr(0);if(rc){fail(Notice::Sync,rc);return;}uint8_t type;rc=ble_hs_id_infer_auto(0,&type);if(rc){fail(Notice::Sync,rc);return;}Guard lock;if(state){state->synced=true;changed();}}
void hostTask(void*){tab5TaskStarted("tab5_mesh_ble");nimble_port_run();tab5TaskCheckpoint("tab5_mesh_ble");hostExited.store(true,std::memory_order_release);vTaskDelete(nullptr);}
uint32_t epoch(){const time_t value=time(nullptr);return value>=1577836800&&uint64_t(value)<=UINT32_MAX?uint32_t(value):0;}
void dataChanged(){if(state){++state->dataRevision;changed();}}
int upsertNode(uint32_t id,const char* name,bool& metadataChanged){metadataChanged=false;if(!id||!state||!state->history)return -1;auto& h=*state->history;int i=M::findNode(h,id);if(i<0){if(h.nodeCount==MaxNodes)return -1;i=h.nodeCount++;h.nodes[i]={};h.nodes[i].id=id;metadataChanged=true;}if(name&&*name&&strcmp(name,h.nodes[i].name)){snprintf(h.nodes[i].name,sizeof(h.nodes[i].name),"%s",name);metadataChanged=true;}return i;}
void addMessage(const P::Decoded& d,bool outgoing=false){
 if(!state||!state->history||M::duplicate(*state->history,d.id,d.from))return;
 outgoing=outgoing||(state->localNode&&d.from==state->localNode);
 auto& m=M::append(*state->history);m.from=d.from;m.to=d.to;m.id=d.id;m.channel=uint8_t(d.channel);m.outgoing=outgoing;m.positionOnly=d.kind==P::Kind::Position;m.time=d.time?d.time:epoch();m.unread=!outgoing&&!m.positionOnly;
 memcpy(m.text,d.text,sizeof(m.text));
 if(d.hasPosition){m.hasPosition=true;m.latitude=d.latitude;m.longitude=d.longitude;m.positionTime=d.positionTime;}
 else{int i=M::findNode(*state->history,outgoing?d.to:d.from);if(i>=0){auto& n=state->history->nodes[i];m.hasPosition=n.hasPosition;m.latitude=n.latitude;m.longitude=n.longitude;m.positionTime=n.positionTime;}}
 ++state->messagesRevision;dataChanged();
}
void rememberRadio(){if(!state||state->connectIndex>=state->peerCount)return;const auto&p=state->peers[state->connectIndex];unsigned at=0;while(at<preferences.knownCount&&!M::sameAddress(preferences.known[at],p.address.type,p.address.val))++at;
 if(at==M::MaxKnown){uint32_t oldest=UINT32_MAX;at=0;for(unsigned i=0;i<M::MaxKnown;++i)if(preferences.known[i].lastUsed<oldest){oldest=preferences.known[i].lastUsed;at=i;}}
 else if(at==preferences.knownCount)++preferences.knownCount;
 auto& k=preferences.known[at];k.type=p.address.type;memcpy(k.address,p.address.val,6);k.unpaired=state->allowUnpaired;snprintf(k.name,sizeof(k.name),"%s",p.name);k.lastUsed=epoch();preferences.preferred=at;++preferencesRevision;
}
void apply(const P::Decoded& d){Guard lock;if(!state)return;switch(d.kind){
 case P::Kind::Node:case P::Kind::Position:{const uint32_t id=d.kind==P::Kind::Node?d.number:d.from;bool metadataChanged=false,positionChanged=false;int i=upsertNode(id,d.name,metadataChanged);if(i<0){state->notice=Notice::Overflow;changed();break;}auto& n=state->history->nodes[i];if(d.hasPosition&&(!n.hasPosition||!d.positionTime||d.positionTime>=n.positionTime)){uint32_t stamp=d.positionTime?d.positionTime:(d.time?d.time:epoch());positionChanged=!n.hasPosition||n.latitude!=d.latitude||n.longitude!=d.longitude||n.positionTime!=stamp;n.hasPosition=true;n.latitude=d.latitude;n.longitude=d.longitude;n.positionTime=stamp;}
 if(metadataChanged)++state->nodesRevision;if(metadataChanged||positionChanged){++state->positionRevision;dataChanged();}if(d.kind==P::Kind::Position)addMessage(d);break;}
 case P::Kind::Text:addMessage(d);break;
 case P::Kind::MyNode:state->localNode=d.number;changed();break;
 case P::Kind::ConfigComplete:if(d.nonce==state->nonce){state->ready=true;state->status=Status::Ready;state->deadline=0;state->reconnectAttempt=0;state->nextReconnect=0;state->manualDisconnect=!preferences.options.autoReconnect;rememberRadio();changed();}break;
 case P::Kind::Queue:if(d.queueResult){state->notice=Notice::Queue;state->error=d.queueResult;changed();}break;
 case P::Kind::Reboot:state->ready=false;state->configWanted=true;state->status=Status::Configuring;state->deadline=millis()+20000;changed();break;
 default:break;
}}
int gapEvent(ble_gap_event*,void*);
void discover(uint16_t connection);
int readResult(uint16_t connection,const ble_gatt_error* error,ble_gatt_attr* attr,void* arg){
 if(!current(connection,arg))return 0;
 if(error->status==BLE_HS_EDONE||(!error->status&&!attr)){Guard lock;state->readActive=false;state->readReady=true;state->lastRead=millis();return 0;}
 if(error->status){fail(Notice::Read,error->status);return 0;}
 if(!attr||!attr->om)return 0;
 size_t n=OS_MBUF_PKTLEN(attr->om);bool invalid=false;
 {Guard lock;if(attr->offset!=state->readSize||n>P::MaxPacket-state->readSize)invalid=true;else if(os_mbuf_copydata(attr->om,0,n,state->readBuffer+state->readSize))invalid=true;else state->readSize+=n;}
 if(invalid){fail(Notice::Protocol);return BLE_HS_EAPP;}return 0;
}
int writeResult(uint16_t connection,const ble_gatt_error* error,ble_gatt_attr*,void* arg){
 if(!current(connection,arg))return 0;
 if(error->status&&error->status!=BLE_HS_EDONE){fail(Notice::Write,error->status);return 0;}
 Guard lock;state->writeActive=false;state->readWanted=true;
 if(state->sendId){P::Decoded d;d.kind=P::Kind::Text;d.from=state->localNode;d.to=state->sendTo;d.channel=state->sendChannel;d.id=state->sendId;memcpy(d.text,state->sendText,sizeof(d.text));addMessage(d,true);state->sendId=0;memset(state->sendText,0,sizeof(state->sendText));}
 return 0;
}
int subscribed(uint16_t connection,const ble_gatt_error* error,ble_gatt_attr*,void* arg){if(!current(connection,arg))return 0;if(error->status){fail(Notice::Service,error->status);return 0;}Guard lock;state->configWanted=true;state->status=Status::Configuring;state->deadline=millis()+20000;changed();return 0;}
int descriptor(uint16_t connection,const ble_gatt_error* error,uint16_t chr,const ble_gatt_dsc* d,void* arg){
 if(!current(connection,arg))return 0;
 if(!error->status&&d){Guard lock;if(chr==state->num&&ble_uuid_u16(&d->uuid.u)==0x2902)state->ccc=d->handle;return 0;}
 if(error->status!=BLE_HS_EDONE){fail(Notice::Service,error->status);return 0;}
 uint16_t handle;{Guard lock;handle=state->ccc;}if(!handle){fail(Notice::Service);return 0;}uint8_t on[]={1,0};int rc=ble_gattc_write_flat(connection,handle,on,2,subscribed,arg);if(rc)fail(Notice::Service,rc);return 0;
}
int characteristic(uint16_t connection,const ble_gatt_error* error,const ble_gatt_chr* c,void* arg){
 if(!current(connection,arg))return 0;
 if(!error->status&&c){Guard lock;if(state->num&&c->def_handle>state->num&&(!state->numEnd||c->def_handle-1<state->numEnd))state->numEnd=c->def_handle-1;
  if(!ble_uuid_cmp(&c->uuid.u,&fromUuid.u)&&(c->properties&BLE_GATT_CHR_PROP_READ))state->from=c->val_handle;
  if(!ble_uuid_cmp(&c->uuid.u,&toUuid.u)&&(c->properties&BLE_GATT_CHR_PROP_WRITE))state->to=c->val_handle;
  if(!ble_uuid_cmp(&c->uuid.u,&numUuid.u)&&(c->properties&BLE_GATT_CHR_PROP_NOTIFY))state->num=c->val_handle;
  return 0;
 }
 if(error->status!=BLE_HS_EDONE){fail(Notice::Service,error->status);return 0;}
 uint16_t start,end;bool valid;{Guard lock;valid=state->from&&state->to&&state->num;start=state->num;end=state->numEnd?state->numEnd:state->end;}
 if(!valid||start>=end){fail(Notice::Service);return 0;}int rc=ble_gattc_disc_all_dscs(connection,start,end,descriptor,arg);if(rc)fail(Notice::Service,rc);return 0;
}
int serviceResult(uint16_t connection,const ble_gatt_error* error,const ble_gatt_svc* s,void* arg){
 if(!current(connection,arg))return 0;
 if(!error->status&&s){Guard lock;state->start=s->start_handle;state->end=s->end_handle;return 0;}
 if(error->status!=BLE_HS_EDONE){fail(Notice::Service,error->status);return 0;}
 uint16_t start,end;{Guard lock;start=state->start;end=state->end;}if(!start||end<=start){fail(Notice::Service);return 0;}
 int rc=ble_gattc_disc_all_chrs(connection,start,end,characteristic,arg);if(rc)fail(Notice::Service,rc);return 0;
}
void discover(uint16_t connection){{Guard lock;if(!state||state->stop)return;state->status=Status::Discovering;state->pairAction=0;state->deadline=millis()+15000;changed();}int rc=ble_gattc_disc_svc_by_uuid(connection,&serviceUuid.u,serviceResult,token());if(rc)fail(Notice::Service,rc);}
int mtuResult(uint16_t connection,const ble_gatt_error*,uint16_t,void* arg){
 if(!current(connection,arg))return 0;bool unsecured;{Guard lock;unsecured=state->allowUnpaired;state->status=Status::Pairing;state->deadline=millis()+90000;changed();}
 if(unsecured)discover(connection);else{int rc=ble_gap_security_initiate(connection);if(rc==BLE_HS_EALREADY){ble_gap_conn_desc d{};if(!ble_gap_conn_find(connection,&d)&&d.sec_state.encrypted&&d.sec_state.authenticated)discover(connection);else fail(Notice::Security,rc);}else if(rc)fail(Notice::Security,rc);}return 0;
}
int gapEvent(ble_gap_event* e,void*){
 if(!state)return 0;
 switch(e->type){
 case BLE_GAP_EVENT_DISC:{ble_hs_adv_fields fields{};if(ble_hs_adv_parse_fields(&fields,e->disc.data,e->disc.length_data))break;bool found=false;for(unsigned i=0;i<fields.num_uuids128;++i)if(!ble_uuid_cmp(&fields.uuids128[i].u,&serviceUuid.u))found=true;Guard lock;if(state->stop||state->status!=Status::Scanning)break;unsigned i=0;while(i<state->peerCount&&memcmp(&state->peers[i].address,&e->disc.addr,sizeof(ble_addr_t)))++i;if(!found&&i==state->peerCount)break;if(i==MaxPeers)break;if(i==state->peerCount)++state->peerCount;auto& p=state->peers[i];p.address=e->disc.addr;p.rssi=e->disc.rssi;if(fields.name&&fields.name_len<sizeof(p.name)&&P::utf8(fields.name,fields.name_len)){memcpy(p.name,fields.name,fields.name_len);p.name[fields.name_len]=0;for(char* c=p.name;*c;++c)if(*c=='\n'||*c=='\r'||*c=='\t')*c=' ';}++state->peersRevision;changed();break;}
 case BLE_GAP_EVENT_DISC_COMPLETE:{Guard lock;if(state->status==Status::Scanning){state->status=Status::Idle;changed();}break;}
 case BLE_GAP_EVENT_CONNECT:if(e->connect.status){fail(Notice::Connect,e->connect.status);}else{bool stop;void* arg;{Guard lock;state->connection=e->connect.conn_handle;stop=state->stop;arg=reinterpret_cast<void*>(uintptr_t(state->generation));}if(stop)ble_gap_terminate(e->connect.conn_handle,BLE_ERR_REM_USER_CONN_TERM);else{int rc=ble_gattc_exchange_mtu(e->connect.conn_handle,mtuResult,arg);if(rc)mtuResult(e->connect.conn_handle,nullptr,0,arg);}}break;
 case BLE_GAP_EVENT_DISCONNECT:{Guard lock;discardCommands();state->connection=NoConnection;state->ready=false;state->readActive=state->writeActive=state->readReady=state->readWanted=state->configWanted=false;state->sendId=0;state->pairAction=0;state->deadline=0;memset(state->sendText,0,sizeof(state->sendText));if(!state->stop&&state->status!=Status::Failed){state->status=Status::Idle;state->notice=Notice::Stopped;}changed();break;}
 case BLE_GAP_EVENT_ENC_CHANGE:if(e->enc_change.conn_handle!=state->connection)break;if(e->enc_change.status){fail(Notice::Security,e->enc_change.status);}else{ble_gap_conn_desc d{};if(ble_gap_conn_find(e->enc_change.conn_handle,&d)||!d.sec_state.encrypted||!d.sec_state.authenticated)fail(Notice::Security);else discover(e->enc_change.conn_handle);}break;
 case BLE_GAP_EVENT_PASSKEY_ACTION:{if(e->passkey.params.action!=BLE_SM_IOACT_INPUT&&e->passkey.params.action!=BLE_SM_IOACT_NUMCMP){fail(Notice::Security);break;}Guard lock;if(state->stop)break;state->pairAction=e->passkey.params.action;state->pairNumber=e->passkey.params.numcmp;state->status=Status::Pairing;state->deadline=millis()+90000;changed();break;}
 case BLE_GAP_EVENT_NOTIFY_RX:{Guard lock;if(e->notify_rx.conn_handle==state->connection&&e->notify_rx.attr_handle==state->num)state->readWanted=true;break;}
 case BLE_GAP_EVENT_REPEAT_PAIRING:fail(Notice::Security);return BLE_GAP_REPEAT_PAIRING_IGNORE;
 default:break;
 }
 return 0;
}
// Called by the BLE worker; kept separate so retry/auth/stop policy is testable.
void queueReconnect(uint32_t now,bool pendingConnect){Guard lock;if(!state||state->stop||state->manualDisconnect||!preferences.options.autoReconnect||!preferences.knownCount||pendingConnect||state->connection!=NoConnection||!state->synced||state->connect||state->status==Status::Connecting||state->status==Status::Pairing||state->status==Status::Scanning||int32_t(now-state->nextReconnect)<0)return;const auto& k=preferences.known[preferences.preferred];state->peers[0]={};state->peers[0].address.type=k.type;memcpy(state->peers[0].address.val,k.address,6);snprintf(state->peers[0].name,sizeof(state->peers[0].name),"%s",k.name);state->peerCount=std::max<uint8_t>(1,state->peerCount);state->connectIndex=0;state->allowUnpaired=k.unpaired;state->connect=true;state->notice=Notice::None;++state->peersRevision;unsigned shift=std::min<unsigned>(state->reconnectAttempt,4);if(state->reconnectAttempt<4)++state->reconnectAttempt;state->nextReconnect=now+(5000u<<shift);changed();}
// The worker owns the host lifecycle. It cannot release state until hostTask exits.
void populateKnown();
void worker(void*){
 bool hosted=false,initialized=false,hostStarted=false;
 Tab5Resources::AdmissionReport memoryReport;
 auto admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,{Tab5MeshMemory::Internal,Tab5MeshMemory::LargestBlock,0,Tab5MeshMemory::External,Tab5MeshMemory::ExternalBlock},"meshtastic BLE",&memoryReport);
 if(!admission){const bool external=memoryReport.reason==Tab5Resources::Denial::Psram||memoryReport.reason==Tab5Resources::Denial::PsramBlock;{Guard lock;state->memoryFree=external?memoryReport.memory.psram:memoryReport.memory.internal;state->memoryBlock=external?memoryReport.memory.psramBlock:memoryReport.memory.internalBlock;state->memoryNeed=external?memoryReport.requested.psram:memoryReport.requested.internal;}fail(external?(memoryReport.reason==Tab5Resources::Denial::PsramBlock?Notice::ExternalBlock:Notice::External):(memoryReport.reason==Tab5Resources::Denial::InternalBlock?Notice::InternalBlock:Notice::Internal));goto finish;}
 if(hostedIsBLEActive()){fail(Notice::Start);goto finish;}
 hosted=hostedInitBLE();if(!hosted){fail(Notice::Start);goto finish;}
 {int rc=nimble_port_init();if(rc!=ESP_OK){fail(Notice::Start,rc);goto finish;}}initialized=true;
 ble_hs_cfg.reset_cb=hostReset;ble_hs_cfg.sync_cb=hostSync;ble_hs_cfg.store_status_cb=ble_store_util_status_rr;
 ble_hs_cfg.sm_io_cap=BLE_HS_IO_KEYBOARD_DISPLAY;ble_hs_cfg.sm_bonding=1;ble_hs_cfg.sm_mitm=1;ble_hs_cfg.sm_sc=1;
 ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ENC;ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ENC;
 ble_store_config_init();ble_att_set_preferred_mtu(CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU);
 hostExited=false;
 if(xTaskCreatePinnedToCore(hostTask,"tab5_mesh_ble",Tab5MeshMemory::HostStack,nullptr,5,nullptr,0)!=pdPASS){hostExited=true;fail(Notice::Memory);goto finish;}hostStarted=true;
 admission.release();
 {uint32_t start=millis();while(true){bool sync,stop;{Guard lock;sync=state->synced;stop=state->stop;}if(stop)goto finish;if(sync)break;if(millis()-start>12000){fail(Notice::Sync);goto finish;}vTaskDelay(pdMS_TO_TICKS(20));}}
 {Guard lock;state->status=Status::Idle;state->scan=!state->autoconnect&&!state->connect;state->nextReconnect=millis();changed();}
 while(true){
  bool stop,scan,connect,disconnect,read,config,send,rx;uint8_t peerIndex,pair,pairAction;uint16_t connection,from,to;uint32_t pairValue,deadline,generation;bool allow;
  {Guard lock;stop=state->stop;scan=state->scan;state->scan=false;connect=state->connect;state->connect=false;peerIndex=state->connectIndex;disconnect=state->disconnect;state->disconnect=false;connection=state->connection;pair=state->submitPair;state->submitPair=0;pairAction=state->pairAction;pairValue=state->pairInput;deadline=state->deadline;allow=state->allowUnpaired;generation=state->generation;from=state->from;to=state->to;rx=state->readReady;read=!state->readActive&&!state->writeActive&&!rx&&from&&(state->readWanted||(state->ready&&millis()-state->lastRead>2000));config=!state->readActive&&!state->writeActive&&state->configWanted;send=!state->readActive&&!state->writeActive&&state->send&&state->ready;}
  if(stop)break;
  queueReconnect(millis(),connect);

  if(deadline&&int32_t(millis()-deadline)>=0){fail(Notice::Timeout);{Guard lock;state->deadline=0;}continue;}
  if(disconnect&&connection!=NoConnection){ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);continue;}
  if(pair&&connection!=NoConnection){if(pair==2){ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);}else{ble_sm_io io{};io.action=pairAction;if(pairAction==BLE_SM_IOACT_INPUT)io.passkey=pairValue;else if(pairAction==BLE_SM_IOACT_NUMCMP)io.numcmp_accept=1;else{fail(Notice::Security);continue;}int rc=ble_sm_inject_io(connection,&io);if(rc)fail(Notice::Security,rc);{Guard lock;state->pairAction=0;changed();}}}
  if(scan&&connection==NoConnection){ble_gap_disc_cancel();ble_gap_disc_params p{};p.passive=0;p.filter_duplicates=1;p.itvl=160;p.window=80;uint8_t type=0;if(ble_hs_id_infer_auto(0,&type)){fail(Notice::Sync);continue;}{Guard lock;populateKnown();state->status=Status::Scanning;state->notice=Notice::None;changed();}int rc=ble_gap_disc(type,10000,&p,gapEvent,nullptr);if(rc)fail(Notice::Connect,rc);}
  if(connect&&connection==NoConnection){Peer p;{Guard lock;if(peerIndex>=state->peerCount)continue;p=state->peers[peerIndex];++state->generation;state->start=state->end=state->from=state->to=state->num=state->numEnd=state->ccc=0;state->localNode=0;state->readSize=0;state->ready=false;state->status=Status::Connecting;state->notice=Notice::None;state->deadline=millis()+20000;changed();}ble_gap_disc_cancel();ble_hs_cfg.sm_mitm=allow?0:1;uint8_t type=0;if(ble_hs_id_infer_auto(0,&type)){fail(Notice::Sync);continue;}int rc=ble_gap_connect(type,&p.address,15000,nullptr,gapEvent,nullptr);if(rc)fail(Notice::Connect,rc);}
  if(rx){uint8_t buffer[P::MaxPacket];size_t n;{Guard lock;n=state->readSize;memcpy(buffer,state->readBuffer,n);memset(state->readBuffer,0,n);state->readSize=0;state->readReady=false;state->readWanted=n!=0;}if(n){P::Decoded d;if(P::decode(buffer,n,d))apply(d);else{Guard lock;state->notice=Notice::Protocol;changed();}}memset(buffer,0,sizeof(buffer));}
  if(connection!=NoConnection&&(config||send)){
   uint8_t buffer[P::MaxPacket];size_t n=0;
   {Guard lock;if(config){state->configWanted=false;state->nonce=esp_random();if(!state->nonce)state->nonce=1;n=P::configuration(buffer,sizeof(buffer),state->nonce);state->sendId=0;}else{state->send=false;state->sendId=esp_random();if(!state->sendId)state->sendId=1;n=P::text(buffer,sizeof(buffer),state->sendText,strlen(state->sendText),state->sendTo,state->sendChannel,state->sendId);}state->writeActive=true;}
   int rc=BLE_HS_EINVAL;if(n){void* arg=reinterpret_cast<void*>(uintptr_t(generation));if(n<=size_t(ble_att_mtu(connection)-3))rc=ble_gattc_write_flat(connection,to,buffer,n,writeResult,arg);else{auto* mb=ble_hs_mbuf_from_flat(buffer,n);rc=mb?ble_gattc_write_long(connection,to,0,mb,writeResult,arg):BLE_HS_ENOMEM;}}memset(buffer,0,sizeof(buffer));if(rc)fail(Notice::Write,rc);
  }else if(connection!=NoConnection&&read){
   {Guard lock;state->readActive=true;state->readWanted=false;state->readSize=0;}
   int rc=ble_gattc_read_long(connection,from,0,readResult,reinterpret_cast<void*>(uintptr_t(generation)));if(rc)fail(Notice::Read,rc);
  }
  vTaskDelay(pdMS_TO_TICKS(20));
 }
 finish:
 admission.release();
 if(initialized){ble_gap_disc_cancel();ble_gap_conn_cancel();uint16_t connection;{Guard lock;connection=state->connection;state->stop=true;state->status=Status::Stopping;changed();}if(connection!=NoConnection)ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);
  if(hostStarted){int rc=nimble_port_stop();if(rc){fail(Notice::Start,rc);}while(!hostExited.load(std::memory_order_acquire))vTaskDelay(pdMS_TO_TICKS(20));}
  nimble_port_deinit();
 }
 if(hosted)hostedDeinitBLE();
 {Guard lock;discardCommands();state->connection=NoConnection;state->ready=false;state->synced=false;state->pairAction=0;state->status=state->notice==Notice::None?Status::Idle:Status::Failed;if(!hostStarted)state->launchBlocked=true;changed();}
 tab5TaskCheckpoint("meshtastic");running.store(false,std::memory_order_release);vTaskDelete(nullptr);
}
// Persistence is isolated from both NimBLE callbacks and LVGL. Snapshots are
// copied one bounded record at a time, then a revision check rejects a torn copy.
struct IoJob {M::History* history=nullptr;M::Preferences preferences{};uint32_t revision=0,prefRevision=0;bool load=false,saveHistory=false,savePreferences=false,historyOk=false,preferencesOk=false;};
IoJob* io=nullptr;std::atomic<bool> ioRunning{false};
bool storageAvailable(){return hooks.storageAvailable?hooks.storageAvailable():tab5SdAvailable();}
void ioTask(void* argument){auto* job=static_cast<IoJob*>(argument);tab5TaskStarted("mesh_store");
 job->preferencesOk=!job->savePreferences||Tab5MeshStore::savePreferences(job->preferences);
 if(job->load)job->historyOk=Tab5MeshStore::loadHistory(*job->history);
 else job->historyOk=!job->saveHistory||Tab5MeshStore::saveHistory(*job->history);
 tab5TaskCheckpoint("mesh_store");ioRunning.store(false,std::memory_order_release);vTaskDelete(nullptr);
}
bool copyHistory(M::History& out,uint32_t& revision){
 {Guard lock;if(!state||!state->history)return false;const auto& h=*state->history;revision=state->dataRevision;out.sequence=h.sequence;out.nodeCount=h.nodeCount;out.messageCount=h.messageCount;out.messageStart=h.messageStart;}
 for(unsigned i=0;i<out.nodeCount;++i){Guard lock;out.nodes[i]=state->history->nodes[i];}
 for(unsigned i=0;i<out.messageCount;++i){unsigned at=(out.messageStart+i)%MaxMessages;Guard lock;out.messages[at]=state->history->messages[at];}
 Guard lock;return state&&state->dataRevision==revision;
}
void populateKnown(){if(!state)return;state->peerCount=0;for(unsigned i=0;i<preferences.knownCount&&i<MaxPeers;++i){auto& p=state->peers[state->peerCount++];const auto& k=preferences.known[i];p={};p.address.type=k.type;memcpy(p.address.val,k.address,6);snprintf(p.name,sizeof(p.name),"%s",k.name);}++state->peersRevision;}
bool ensureState(){if(state)return true;auto* context=static_cast<Context*>(heap_caps_calloc(1,sizeof(Context),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!context)return false;auto* history=static_cast<M::History*>(heap_caps_calloc(1,sizeof(M::History),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!history){heap_caps_free(context);return false;}new(context) Context{};context->history=history;{Guard lock;state=context;populateKnown();}return true;}
void clearIo(){if(!io)return;heap_caps_free(io->history);heap_caps_free(io);io=nullptr;}
void deferIo(bool loading){nextIoAttempt=millis()+5000;if(loading){Guard lock;if(state&&!state->historyLoadFailed){state->historyLoadFailed=true;state->notice=Notice::HistoryRead;changed();}}}
void persistenceService(){
 if(io&&!ioRunning.load(std::memory_order_acquire)){
  if(io->savePreferences){Guard lock;if(io->preferencesOk)savedPreferencesRevision=io->prefRevision;else if(state){state->notice=Notice::Preferences;changed();}}
  if(io->load){
   if(io->historyOk&&state){
    // Merge any messages received while SD loading was in progress. The new
    // immutable snapshot replaces the old one with a pointer swap under lock.
    uint32_t revision;unsigned count,start,nodes;
    {Guard lock;revision=state->dataRevision;count=state->history->messageCount;start=state->history->messageStart;nodes=state->history->nodeCount;}
    for(unsigned i=0;i<nodes;++i){Node value;{Guard lock;value=state->history->nodes[i];}int n=M::findNode(*io->history,value.id);if(n<0&&io->history->nodeCount<MaxNodes)n=io->history->nodeCount++;if(n>=0&&(!io->history->nodes[n].hasPosition||value.positionTime>=io->history->nodes[n].positionTime))io->history->nodes[n]=value;}
    for(unsigned i=0;i<count;++i){Message value;{Guard lock;value=state->history->messages[(start+i)%MaxMessages];}if(!M::sameMessage(*io->history,value))M::append(*io->history)=value;}
    M::History* previous=nullptr;{Guard lock;if(revision==state->dataRevision){previous=state->history;state->history=io->history;io->history=nullptr;diskSequence=state->history->sequence;state->historyLoaded=true;state->historyLoadFailed=false;++state->messagesRevision;++state->nodesRevision;++state->positionRevision;dataChanged();state->savedDataRevision=(count||nodes)?0:state->dataRevision;}}
    if(!previous)return; // A callback changed the model: retry the merge later.
    heap_caps_free(previous);
   }else if(state){Guard lock;state->historyLoaded=false;state->historyLoadFailed=true;state->notice=Notice::HistoryRead;changed();}
  }else{
   Guard lock;
   if(io->historyOk&&io->saveHistory&&state){state->savedDataRevision=io->revision;diskSequence=io->history->sequence;}
   else if(!io->historyOk&&state){state->notice=Notice::Storage;changed();}
  }
  const bool loadFailed=io->load&&!io->historyOk;clearIo();lastPersist=millis();nextIoAttempt=millis()+(loadFailed?30000:5000);
 }
 if(io||!state||int32_t(millis()-nextIoAttempt)<0)return;
 bool needLoad,needHistory,needPreferences;{Guard lock;needLoad=!state->historyLoaded&&storageAvailable();needHistory=state->dataRevision!=state->savedDataRevision&&storageAvailable();needPreferences=preferencesRevision!=savedPreferencesRevision;}
 if(!needLoad&&!needHistory&&!needPreferences)return;
 if(!needLoad&&uint32_t(millis()-lastPersist)<5000)return;
 auto* job=static_cast<IoJob*>(heap_caps_calloc(1,sizeof(IoJob),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!job){deferIo(needLoad);return;}new(job) IoJob{};
 if(needLoad||needHistory){job->history=static_cast<M::History*>(heap_caps_calloc(1,sizeof(M::History),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!job->history){heap_caps_free(job);deferIo(needLoad);return;}}
 job->load=needLoad;job->saveHistory=!needLoad&&needHistory;job->savePreferences=needPreferences;
 {Guard lock;job->preferences=preferences;job->prefRevision=preferencesRevision;}
 if(job->saveHistory&&!copyHistory(*job->history,job->revision)){heap_caps_free(job->history);heap_caps_free(job);return;}
 if(job->saveHistory)job->history->sequence=diskSequence+1;
 io=job;ioRunning=true;
 if(tab5CreateTask(ioTask,"mesh_store",4096,job,1,nullptr,0)!=pdPASS){ioRunning=false;clearIo();lastPersist=millis();deferIo(needLoad);}
}
// Native controls opt into the actual OS palette; panels also need an explicit
// fill because the base LVGL theme is white, including when Tab5OS is dark.
char* displayBuffer=nullptr;
lv_obj_t* historyPane=nullptr,*historyRange=nullptr,*olderButton=nullptr,*newerButton=nullptr,*optionsPanel=nullptr,*optionsBackdrop=nullptr,*optionsReturnScope=nullptr;
lv_obj_t* peerRows[MaxPeers]{};lv_obj_t* peerEmpty=nullptr;
struct MapTarget {uint32_t node;int32_t latitude,longitude;};
MapTarget visiblePositions[20]{};
unsigned historyPage=0;bool startRequested=false,startAutomatic=false;
void setText(lv_obj_t* object,const char* value){if(object&&strcmp(lv_label_get_text(object),value))lv_label_set_text(object,value);}
void enable(lv_obj_t* object,bool value){if(!object)return;bool old=!lv_obj_has_state(object,LV_STATE_DISABLED);if(old==value)return;if(value)lv_obj_remove_state(object,LV_STATE_DISABLED);else lv_obj_add_state(object,LV_STATE_DISABLED);}
void surface(lv_obj_t* o,bool raised=false){lv_obj_set_style_bg_color(o,lv_color_hex(Tab5UiStyle::color(raised?2:1)),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_text_color(o,lv_color_hex(Tab5UiStyle::color(4)),0);lv_obj_set_style_border_color(o,lv_color_hex(Tab5UiStyle::color(3)),0);lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_radius(o,Tab5UiStyle::radius(),0);lv_obj_set_style_shadow_width(o,0,0);}
lv_obj_t* row(lv_obj_t* parent){auto* o=lv_obj_create(parent);lv_obj_remove_style_all(o);lv_obj_set_width(o,LV_PCT(100));lv_obj_set_height(o,LV_SIZE_CONTENT);lv_obj_set_flex_flow(o,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(o,10,0);lv_obj_set_flex_align(o,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);return o;}
lv_obj_t* label(lv_obj_t* parent,const char* value){auto* o=lv_label_create(parent);lv_label_set_text(o,tr(value));lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);return o;}
lv_obj_t* button(lv_obj_t* parent,const char* value,void(*action)(),int width=52){auto* o=lv_button_create(parent);lv_obj_set_size(o,width,48);Tab5UiStyle::button(o);auto* l=label(o,value);lv_obj_set_width(l,LV_PCT(100));lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(l);lv_obj_add_event_cb(o,[](lv_event_t* e){reinterpret_cast<void(*)()>(lv_event_get_user_data(e))();},LV_EVENT_CLICKED,reinterpret_cast<void*>(action));return o;}
void searchIcon(lv_obj_t* button){auto* ring=lv_obj_create(button);lv_obj_remove_style_all(ring);lv_obj_set_size(ring,20,20);lv_obj_align(ring,LV_ALIGN_CENTER,-4,-4);lv_obj_set_style_border_width(ring,2,0);lv_obj_set_style_border_color(ring,lv_color_hex(Tab5UiStyle::color(4)),0);lv_obj_set_style_radius(ring,10,0);lv_obj_remove_flag(ring,lv_obj_flag_t(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));static const lv_point_precise_t points[]={{0,0},{10,10}};auto* handle=lv_line_create(button);lv_line_set_points(handle,points,2);lv_obj_set_style_line_width(handle,2,0);lv_obj_set_style_line_color(handle,lv_color_hex(Tab5UiStyle::color(4)),0);lv_obj_align(handle,LV_ALIGN_CENTER,9,9);lv_obj_remove_flag(handle,LV_OBJ_FLAG_CLICKABLE);}
void focusComposer(){if(!input)return;Tab5Focus.editObject(input);if(hooks.keyboard){keyboardOpen=true;hooks.keyboard(input,false);}}
void showKeyboard(){if(!input)return;Tab5Focus.editObject(input);if(hooks.keyboard){keyboardOpen=true;hooks.keyboard(input,true);}}
void requestScan(){paused=false;if(!ensureState()){setText(noticeLabel,tr("Mémoire insuffisante pour Bluetooth"));return;}if(running.load()){Guard lock;if(state->connection==NoConnection&&!state->stop){state->manualDisconnect=true;state->scan=true;state->notice=Notice::None;}return;}startRequested=true;startAutomatic=false;}
void startRadio(){
 if(running.load()||!state)return;
 {Guard lock;discardCommands();state->synced=false;state->stop=false;state->scan=false;state->disconnect=false;state->ready=false;state->readActive=state->readReady=state->readWanted=state->writeActive=state->configWanted=false;state->connection=NoConnection;state->status=Status::Starting;state->launchBlocked=false;state->notice=Notice::None;state->error=0;state->autoconnect=startAutomatic;state->manualDisconnect=!startAutomatic;state->sendTo=P::Broadcast;changed();}
 running=true;startRequested=false;nextLaunch=millis()+30000;
 if(tab5CreateTask(worker,"tab5_meshtastic",8192,nullptr,2,nullptr,0)!=pdPASS){running=false;{Guard lock;state->launchBlocked=true;}fail(Notice::WorkerStack);}
}
void requestDisconnect(){Guard lock;if(state){discardCommands();state->manualDisconnect=true;state->connect=false;state->disconnect=true;state->ready=false;state->deadline=0;changed();}}
void requestSend(){if(!input||!state)return;const char* value=lv_textarea_get_text(input);size_t n=strlen(value);if(!n)return;if(n>P::MaxText||!P::utf8(reinterpret_cast<const uint8_t*>(value),n)){Guard lock;state->notice=Notice::TooLong;changed();return;}uint16_t selection=lv_dropdown_get_selected(recipient);if(selection>=recipientCount)return;
 {Guard lock;if(!state->ready||state->send||state->writeActive)return;memcpy(state->sendText,value,n+1);state->sendTo=recipientIds[selection];state->sendChannel=lv_dropdown_get_selected(channel);state->send=true;state->notice=Notice::None;changed();}
 lv_textarea_set_text(input,"");Tab5Focus.editObject(input);
}
void pairYes(){if(!state)return;uint32_t value=0;uint8_t action;{Guard lock;action=state->pairAction;}if(action==BLE_SM_IOACT_INPUT){const char* s=lv_textarea_get_text(pin);if(strlen(s)!=6)return;for(unsigned i=0;i<6;++i){if(s[i]<'0'||s[i]>'9')return;value=value*10+s[i]-'0';}}{Guard lock;state->pairInput=value;state->submitPair=1;}lv_textarea_set_text(pin,"");focusComposer();}
void pairNo(){Guard lock;if(state){state->manualDisconnect=true;state->submitPair=2;}}
void connectPeer(lv_event_t* e){unsigned i=uintptr_t(lv_event_get_user_data(e));bool unsecured=unpaired&&lv_obj_has_state(unpaired,LV_STATE_CHECKED);if(!state||i>=state->peerCount)return;
 if(!running.load()){startRequested=true;startAutomatic=false;paused=false;}
 Guard lock;if(state->connection!=NoConnection||state->status==Status::Connecting)return;state->connectIndex=i;state->allowUnpaired=unsecured;state->manualDisconnect=true;state->connect=true;state->stop=false;
}
void focusInput(lv_event_t* e){Tab5Focus.editObject(lv_event_get_target_obj(e));if(hooks.keyboard){keyboardOpen=true;hooks.keyboard(lv_event_get_target_obj(e),true);}}
void renderPeers(){
 if(!peerList||!state)return;unsigned count;{Guard lock;count=state->peerCount;drawnPeers=state->peersRevision;}if(count&&peerEmpty){lv_obj_delete(peerEmpty);peerEmpty=nullptr;}
 for(unsigned i=0;i<MaxPeers;++i){
  if(i>=count){if(peerRows[i]){if(Tab5Focus.contains(peerRows[i],Tab5Focus.focused()))Tab5Focus.editObject(input);lv_obj_delete(peerRows[i]);peerRows[i]=nullptr;}continue;}
  Peer p;bool known=false;{Guard lock;p=state->peers[i];for(unsigned k=0;k<preferences.knownCount;++k)known|=M::sameAddress(preferences.known[k],p.address.type,p.address.val);}auto* b=peerRows[i];lv_obj_t* l;
  if(!b){b=peerRows[i]=lv_button_create(peerList);lv_obj_set_width(b,LV_PCT(100));lv_obj_set_height(b,70);Tab5UiStyle::button(b);l=label(b,"");lv_obj_set_width(l,LV_PCT(100));lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);lv_obj_center(l);lv_obj_add_event_cb(b,connectPeer,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(i)));}else l=lv_obj_get_child(b,0);
  char text[140];snprintf(text,sizeof(text),"%s%s\n%02X:%02X:%02X:%02X:%02X:%02X%s",known?LV_SYMBOL_SAVE " ":"",p.name[0]?p.name:"Meshtastic",p.address.val[5],p.address.val[4],p.address.val[3],p.address.val[2],p.address.val[1],p.address.val[0],p.rssi?"  " LV_SYMBOL_BLUETOOTH:"");setText(l,text);
 }
 if(!count&&!peerEmpty){peerEmpty=label(peerList,"Aucun appareil connu");lv_obj_set_width(peerEmpty,LV_PCT(100));}
}
void renderNodes(){if(!recipient||!state||!displayBuffer)return;unsigned old=lv_dropdown_get_selected(recipient);uint32_t selected=old<recipientCount?recipientIds[old]:P::Broadcast;recipientCount=1;recipientIds[0]=P::Broadcast;size_t at=snprintf(displayBuffer,2048,"%s",tr("Tous les nœuds"));unsigned count;{Guard lock;count=state->history->nodeCount;drawnNodes=state->nodesRevision;}unsigned chosen=0;
 for(unsigned i=0;i<count;++i){Node n;{Guard lock;n=state->history->nodes[i];}recipientIds[recipientCount]=n.id;if(n.id==selected)chosen=recipientCount;++recipientCount;int written=snprintf(displayBuffer+at,2048-at,"\n%s !%08lx",n.name,static_cast<unsigned long>(n.id));if(written<0||size_t(written)>=2048-at)break;at+=written;}
 lv_dropdown_set_options(recipient,displayBuffer);lv_dropdown_set_selected(recipient,chosen);
}
void formatTime(uint32_t value,char* out,size_t cap){if(!value){snprintf(out,cap,"%s",tr("Sans date"));return;}time_t t=value;tm local{};if(!localtime_r(&t,&local)||!strftime(out,cap,"%Y-%m-%d %H:%M:%S",&local))snprintf(out,cap,"%lu",static_cast<unsigned long>(value));}
void mapMessage(lv_event_t* e){const auto* p=static_cast<const MapTarget*>(lv_event_get_user_data(e));if(p&&hooks.openMap)hooks.openMap(p->node,double(p->latitude)*1e-7,double(p->longitude)*1e-7);}
void renderMessages(){if(!historyPane||!state)return;const bool follow=historyPage==0&&lv_obj_get_scroll_bottom(historyPane)<=16;int32_t scroll=lv_obj_get_scroll_y(historyPane);unsigned count,start;{Guard lock;count=state->history->messageCount;start=state->history->messageStart;drawnMessages=state->messagesRevision;}
 if(Tab5Focus.contains(historyPane,Tab5Focus.focused()))Tab5Focus.editObject(input);lv_obj_clean(historyPane);messages=nullptr;
 const unsigned pages=(count+19)/20;historyPage=std::min(historyPage,pages?pages-1:0);unsigned end=count>historyPage*20?count-historyPage*20:0,begin=end>20?end-20:0;
 char range[48];snprintf(range,sizeof(range),"%u–%u / %u",count?begin+1:0,end,count);setText(historyRange,range);enable(olderButton,historyPage+1<pages);enable(newerButton,historyPage>0);
 for(unsigned i=begin;i<end;++i){Message m;char name[40]{};{Guard lock;m=state->history->messages[(start+i)%MaxMessages];int n=M::findNode(*state->history,m.outgoing?m.to:m.from);if(n>=0)snprintf(name,sizeof(name),"%s",state->history->nodes[n].name);}
  auto* card=lv_obj_create(historyPane);lv_obj_set_width(card,LV_PCT(100));lv_obj_set_height(card,LV_SIZE_CONTENT);lv_obj_set_flex_flow(card,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(card,10,0);lv_obj_set_style_pad_row(card,6,0);surface(card,m.outgoing);
  auto* header=row(card);char stamp[40],title[150];formatTime(m.time,stamp,sizeof(stamp));snprintf(title,sizeof(title),"%s %s !%08lx · %s",m.outgoing?LV_SYMBOL_UPLOAD:LV_SYMBOL_DOWNLOAD,name,static_cast<unsigned long>(m.outgoing?m.to:m.from),stamp);auto*l=label(header,"");lv_label_set_text(l,title);lv_obj_set_flex_grow(l,1);lv_obj_set_style_text_color(l,lv_color_hex(Tab5UiStyle::color(5)),0);
  if(m.hasPosition){auto* b=lv_button_create(header);lv_obj_set_size(b,48,40);Tab5UiStyle::button(b);auto*icon=label(b,LV_SYMBOL_GPS);lv_obj_center(icon);lv_obj_add_event_cb(b,mapMessage,LV_EVENT_CLICKED,&visiblePositions[i-begin]);visiblePositions[i-begin]={m.outgoing?m.to:m.from,m.latitude,m.longitude};}
  auto* body=label(card,m.positionOnly?"Position reçue":"");lv_obj_set_width(body,LV_PCT(100));if(!m.positionOnly)lv_label_set_text(body,m.text);Tab5TextInput::selectable(body);if(!messages)messages=body;
 }
 if(!count){messages=label(historyPane,"Aucun message");lv_obj_set_width(messages,LV_PCT(100));}
 lv_obj_update_layout(historyPane);lv_obj_scroll_to_y(historyPane,follow?LV_COORD_MAX:scroll,LV_ANIM_OFF);
}
void destroyOptions(bool restore){
 if(!optionsBackdrop)return;
 auto* previousScope=optionsReturnScope;auto* previous=optionsBackdrop;
 optionsPanel=optionsBackdrop=optionsReturnScope=nullptr;
 Tab5Focus.clear();lv_obj_delete(previous);
 if(restore){if(overlay)focusComposer();else if(previousScope&&lv_obj_is_valid(previousScope))Tab5Focus.restore(previousScope);}
}
void closeOptions(){destroyOptions(true);}
void openOptions(){openSettings();}

void render(){if(!state)return;Status status;Notice note;int error;uint8_t pairAction;uint32_t number,pr,nr,mr,free,block,need;bool ready,stopping,sending;{Guard lock;status=state->status;note=state->notice;error=state->error;pairAction=state->pairAction;number=state->pairNumber;pr=state->peersRevision;nr=state->nodesRevision;mr=state->messagesRevision;ready=state->ready;stopping=state->stop;sending=state->send||state->writeActive;drawn=state->revision;free=state->memoryFree;block=state->memoryBlock;need=state->memoryNeed;}
 setText(statusLabel,tr(statusKey(status)));char text[240];if(note==Notice::Internal||note==Notice::InternalBlock)snprintf(text,sizeof(text),tr("%s — RAM interne libre %u Kio, bloc %u Kio, demande %u Kio"),tr(noticeKey(note)),unsigned(free/1024),unsigned(block/1024),unsigned(need/1024));else if(note==Notice::External||note==Notice::ExternalBlock)snprintf(text,sizeof(text),tr("%s — PSRAM libre %u Kio, bloc %u Kio, demande %u Kio"),tr(noticeKey(note)),unsigned(free/1024),unsigned(block/1024),unsigned(need/1024));else if(error&&note!=Notice::None)snprintf(text,sizeof(text),"%s (%d)",tr(noticeKey(note)),error);else snprintf(text,sizeof(text),"%s",tr(noticeKey(note)));setText(noticeLabel,text);if(*text)lv_obj_remove_flag(noticeLabel,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(noticeLabel,LV_OBJ_FLAG_HIDDEN);
 enable(scanButton,!running.load()||(!stopping&&(status==Status::Idle||status==Status::Failed||status==Status::Scanning)));enable(disconnectButton,running.load());enable(sendButton,ready&&!sending);enable(unpaired,status==Status::Idle||status==Status::Scanning||!running.load());
 bool hadPair=!lv_obj_has_flag(pairRow,LV_OBJ_FLAG_HIDDEN);
 if(pairAction==BLE_SM_IOACT_INPUT||pairAction==BLE_SM_IOACT_NUMCMP){lv_obj_remove_flag(pairRow,LV_OBJ_FLAG_HIDDEN);if(pairAction==BLE_SM_IOACT_INPUT){setText(pairLabel,tr("Code affiché sur la radio :"));lv_obj_remove_flag(pin,LV_OBJ_FLAG_HIDDEN);if(!hadPair){Tab5Focus.editObject(pin);if(hooks.keyboard){keyboardOpen=true;hooks.keyboard(pin,false);}}}else{snprintf(text,sizeof(text),"%s %06lu",tr("Confirmer ce code sur les deux appareils :"),static_cast<unsigned long>(number));setText(pairLabel,text);lv_obj_add_flag(pin,LV_OBJ_FLAG_HIDDEN);}}else{lv_obj_add_flag(pairRow,LV_OBJ_FLAG_HIDDEN);if(hadPair)lv_textarea_set_text(pin,"");}
 if(pr!=drawnPeers)renderPeers();if(nr!=drawnNodes)renderNodes();if(mr!=drawnMessages)renderMessages();
}
}
void begin(Hooks value){hooks=value;M::Preferences loaded;if(Tab5MeshStore::loadPreferences(loaded))preferences=loaded;if(preferences.knownCount||preferences.options.background||preferences.options.showOnMap)ensureState();}
void openSettings(lv_obj_t* returnScope){
 if(optionsPanel)return;
 if(hooks.closeKeyboard)hooks.closeKeyboard();keyboardOpen=false;
 optionsReturnScope=overlay?nullptr:returnScope;
 if(optionsReturnScope)Tab5Focus.remember(optionsReturnScope);
 Tab5Focus.clear();
 optionsBackdrop=lv_obj_create(overlay?overlay:lv_layer_top());lv_obj_remove_style_all(optionsBackdrop);
 lv_obj_set_size(optionsBackdrop,LV_PCT(100),LV_PCT(100));lv_obj_remove_flag(optionsBackdrop,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_set_style_bg_color(optionsBackdrop,lv_color_black(),0);lv_obj_set_style_bg_opa(optionsBackdrop,LV_OPA_70,0);
 optionsPanel=lv_obj_create(optionsBackdrop);lv_obj_set_size(optionsPanel,620,LV_SIZE_CONTENT);lv_obj_center(optionsPanel);surface(optionsPanel);
 if(hooks.font)lv_obj_set_style_text_font(optionsPanel,hooks.font(),0);
 lv_obj_set_flex_flow(optionsPanel,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(optionsPanel,20,0);lv_obj_set_style_pad_row(optionsPanel,18,0);
 lv_obj_set_style_max_height(optionsPanel,lv_display_get_vertical_resolution(nullptr)-48,0);lv_obj_set_scroll_dir(optionsPanel,LV_DIR_VER);
 label(optionsPanel,"Réglages Meshtastic");Options current=options();
 const char* keys[]={"Reconnexion automatique","Veille Mesh en arrière-plan","Afficher les nœuds sur la carte"};
 for(unsigned i=0;i<3;++i){
  auto* c=lv_checkbox_create(optionsPanel);lv_obj_set_width(c,LV_PCT(100));lv_checkbox_set_text(c,tr(keys[i]));
  if(i==0?current.autoReconnect:i==1?current.background:current.showOnMap)lv_obj_add_state(c,LV_STATE_CHECKED);
  Tab5UiStyle::control(c);
  lv_obj_add_event_cb(c,[](lv_event_t*e){unsigned i=uintptr_t(lv_event_get_user_data(e));Options next=options();bool yes=lv_obj_has_state(lv_event_get_target_obj(e),LV_STATE_CHECKED);if(i==0)next.autoReconnect=yes;else if(i==1)next.background=yes;else next.showOnMap=yes;setOptions(next);},LV_EVENT_VALUE_CHANGED,reinterpret_cast<void*>(uintptr_t(i)));
 }
 button(optionsPanel,"Fermer",closeOptions,180);Tab5Focus.focusInitial(optionsPanel);
}
void closeSettings(){closeOptions();}
bool settingsVisible(){return optionsPanel!=nullptr;}
void open(){
 if(overlay)return;destroyOptions(false);paused=false;ensureState();memset(peerRows,0,sizeof(peerRows));peerEmpty=nullptr;displayBuffer=static_cast<char*>(heap_caps_malloc(2048,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));historyPage=0;
 overlay=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(overlay);lv_obj_set_size(overlay,LV_PCT(100),LV_PCT(100));lv_obj_set_style_bg_color(overlay,lv_color_black(),0);lv_obj_set_style_bg_opa(overlay,LV_OPA_70,0);
 panel=lv_obj_create(overlay);lv_obj_set_size(panel,std::min<int>(1200,lv_display_get_horizontal_resolution(nullptr)-32),std::min<int>(680,lv_display_get_vertical_resolution(nullptr)-32));lv_obj_center(panel);lv_obj_set_flex_flow(panel,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(panel,12,0);lv_obj_set_style_pad_row(panel,7,0);surface(panel);if(hooks.font)lv_obj_set_style_text_font(panel,hooks.font(),0);
 auto* top=row(panel);auto* title=label(top,"Meshtastic");lv_obj_set_flex_grow(title,1);button(top,LV_SYMBOL_SETTINGS,openOptions);button(top,LV_SYMBOL_CLOSE,close);
 toolbarRow=row(panel);scanButton=button(toolbarRow,"",requestScan);searchIcon(scanButton);disconnectButton=button(toolbarRow,LV_SYMBOL_POWER,stop);unpaired=lv_checkbox_create(toolbarRow);lv_checkbox_set_text(unpaired,tr("Autoriser sans appairage"));statusLabel=label(toolbarRow,"Bluetooth arrêté");lv_obj_set_flex_grow(statusLabel,1);
 noticeLabel=label(panel,"");lv_obj_set_width(noticeLabel,LV_PCT(100));
 pairRow=row(panel);pairLabel=label(pairRow,"");lv_obj_set_flex_grow(pairLabel,1);pin=lv_textarea_create(pairRow);lv_obj_set_size(pin,170,48);lv_textarea_set_one_line(pin,true);lv_textarea_set_max_length(pin,6);lv_textarea_set_accepted_chars(pin,"0123456789");Tab5TextInput::protect(pin);lv_obj_add_event_cb(pin,focusInput,LV_EVENT_CLICKED,nullptr);lv_obj_add_event_cb(pin,[](lv_event_t*){pairYes();},LV_EVENT_READY,nullptr);pairAccept=button(pairRow,LV_SYMBOL_OK,pairYes);button(pairRow,LV_SYMBOL_CLOSE,pairNo);lv_obj_add_flag(pairRow,LV_OBJ_FLAG_HIDDEN);
 bodyRow=row(panel);lv_obj_set_flex_grow(bodyRow,1);lv_obj_set_height(bodyRow,80);peerList=lv_obj_create(bodyRow);lv_obj_set_width(peerList,LV_PCT(28));lv_obj_set_height(peerList,LV_PCT(100));lv_obj_set_flex_flow(peerList,LV_FLEX_FLOW_COLUMN);lv_obj_set_scroll_dir(peerList,LV_DIR_VER);surface(peerList);
 auto* historyColumn=lv_obj_create(bodyRow);lv_obj_remove_style_all(historyColumn);lv_obj_set_flex_grow(historyColumn,1);lv_obj_set_height(historyColumn,LV_PCT(100));lv_obj_set_flex_flow(historyColumn,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(historyColumn,4,0);
 historyPane=lv_obj_create(historyColumn);lv_obj_set_width(historyPane,LV_PCT(100));lv_obj_set_height(historyPane,10);lv_obj_set_flex_grow(historyPane,1);lv_obj_set_flex_flow(historyPane,LV_FLEX_FLOW_COLUMN);lv_obj_set_scroll_dir(historyPane,LV_DIR_VER);lv_obj_set_style_pad_all(historyPane,6,0);surface(historyPane);
 auto* pager=row(historyColumn);olderButton=button(pager,LV_SYMBOL_LEFT,[](){++historyPage;renderMessages();},42);historyRange=label(pager,"");lv_obj_set_flex_grow(historyRange,1);newerButton=button(pager,LV_SYMBOL_RIGHT,[](){if(historyPage)--historyPage;renderMessages();},42);lv_obj_set_height(olderButton,32);lv_obj_set_height(newerButton,32);
 auto* destination=row(panel);recipient=lv_dropdown_create(destination);lv_obj_set_width(recipient,LV_PCT(65));lv_dropdown_set_options(recipient,tr("Tous les nœuds"));recipientIds[0]=P::Broadcast;recipientCount=1;label(destination,"Canal");channel=lv_dropdown_create(destination);lv_obj_set_width(channel,100);lv_dropdown_set_options(channel,"0\n1\n2\n3\n4\n5\n6\n7");
 auto* compose=row(panel);input=lv_textarea_create(compose);lv_obj_set_flex_grow(input,1);lv_obj_set_height(input,52);lv_textarea_set_one_line(input,true);lv_textarea_set_max_length(input,233);lv_textarea_set_placeholder_text(input,tr("Message"));Tab5TextInput::track(input);lv_obj_add_event_cb(input,focusInput,LV_EVENT_CLICKED,nullptr);lv_obj_add_event_cb(input,[](lv_event_t*){requestSend();},LV_EVENT_READY,nullptr);auto* keys=button(compose,LV_SYMBOL_KEYBOARD,showKeyboard);Tab5Focus.describe(keys,tr("Clavier tactile"));sendButton=button(compose,LV_SYMBOL_RIGHT,requestSend,60);
 Tab5UiStyle::tree(panel);enable(sendButton,false);enable(disconnectButton,false);drawn=drawnPeers=drawnNodes=drawnMessages=0;if(state){markRead();render();}else setText(noticeLabel,tr("Mémoire insuffisante pour Bluetooth"));if(!displayBuffer)enable(scanButton,false);focusComposer();
 if(preferences.options.autoReconnect&&preferences.knownCount){startRequested=true;startAutomatic=true;}
}
void close(){if(!overlay){destroyOptions(true);return;}destroyOptions(false);if(hooks.closeKeyboard&&keyboardOpen)hooks.closeKeyboard();keyboardOpen=false;if(overlay){Tab5Focus.clear();lv_obj_delete(overlay);overlay=panel=statusLabel=noticeLabel=peerList=recipient=channel=input=messages=pairRow=pairLabel=pin=pairAccept=unpaired=scanButton=disconnectButton=sendButton=toolbarRow=bodyRow=helpLabel=historyPane=historyRange=olderButton=newerButton=optionsPanel=optionsBackdrop=nullptr;}heap_caps_free(displayBuffer);displayBuffer=nullptr;lastPersist=millis()-5000;if(!preferences.options.background){startRequested=false;Guard lock;if(state){discardCommands();state->stop=true;state->status=Status::Stopping;changed();}}}
void stop(){paused=true;startRequested=false;Guard lock;if(state){discardCommands();state->stop=true;state->manualDisconnect=true;state->scan=state->connect=false;state->status=Status::Stopping;changed();}}
void resize(int height){if(!panel)return;const bool compact=height<500;for(auto* object:{toolbarRow}){if(compact)lv_obj_add_flag(object,LV_OBJ_FLAG_HIDDEN);else lv_obj_remove_flag(object,LV_OBJ_FLAG_HIDDEN);}lv_obj_set_height(panel,std::min(680,std::max(240,height-24)));lv_obj_align(panel,LV_ALIGN_TOP_MID,0,12);}
void service(){
 if((preferences.options.background||preferences.options.showOnMap)&&!state)ensureState();
 persistenceService();
 bool launchBlocked=false;{Guard lock;launchBlocked=state&&state->launchBlocked;}
 if(!paused&&!launchBlocked&&preferences.options.background&&!running.load()&&state&&int32_t(millis()-nextLaunch)>=0){startRequested=true;startAutomatic=preferences.options.autoReconnect;}
 if(startRequested&&!running.load()&&state&&(!storageAvailable()||state->historyLoaded||state->historyLoadFailed))startRadio();
 if(overlay&&state&&uint32_t(millis()-lastUi)>=100){lastUi=millis();uint32_t revision;{Guard lock;revision=state->revision;}if(revision!=drawn)render();if(historyPage==0&&historyPane&&lv_obj_get_scroll_bottom(historyPane)<=16)markRead();}
 if(!overlay&&!running.load(std::memory_order_acquire)&&!io&&!preferences.options.background&&!preferences.options.showOnMap&&state){bool clean;{Guard lock;clean=state->dataRevision==state->savedDataRevision&&preferencesRevision==savedPreferencesRevision&&!M::unread(*state->history);}if(clean){heap_caps_free(state->history);heap_caps_free(state);state=nullptr;}}
}
bool visible(){return overlay!=nullptr||optionsPanel!=nullptr;}
bool busy(){return running.load(std::memory_order_acquire)||ioRunning.load(std::memory_order_acquire);}
lv_obj_t* scope(){return optionsPanel?optionsPanel:overlay;}
void key(const writing::KeyEvent& e){if(!panel&&!optionsPanel)return;auto* focused=Tab5Focus.focused();if(e.key==writing::Key::Escape){if(optionsPanel)closeOptions();else if(focused&&lv_obj_check_type(focused,&lv_dropdown_class)&&lv_dropdown_is_open(focused)){lv_dropdown_close(focused);Tab5Focus.finishEditing();}else close();return;}if(!optionsPanel&&e.key==writing::Key::Enter&&focused==input){requestSend();return;}if(focused&&lv_obj_check_type(focused,&lv_textarea_class)&&Tab5TextInput::key(focused,e))return;Tab5Focus.key(e,optionsPanel?optionsPanel:panel);}
Options options(){Guard lock;return preferences.options;}
void setOptions(Options value){bool shouldStop=false,resume=false;{Guard lock;const Options old=preferences.options;if(old.autoReconnect==value.autoReconnect&&old.background==value.background&&old.showOnMap==value.showOnMap)return;shouldStop=old.background&&!value.background&&!overlay;resume=(!old.background&&value.background)||(!old.autoReconnect&&value.autoReconnect);preferences.options=value;++preferencesRevision;if(state){++state->positionRevision;changed();if(!old.autoReconnect&&value.autoReconnect){state->manualDisconnect=false;state->nextReconnect=millis();}}}if(resume)paused=false;ensureState();lastPersist=millis()-5000;if(shouldStop)stop();}
Badge badge(){Badge b;Guard lock;b.ready=preferences.knownCount!=0;if(state){b.active=running.load()&&state->status==Status::Ready&&state->ready&&!state->stop;b.pairing=state->pairAction!=0;b.unread=M::unread(*state->history);}b.revision=(b.active?1u:0u)|(b.ready?2u:0u)|(b.pairing?4u:0u)|(uint32_t(b.unread)<<8);return b;}
void markRead(){Guard lock;if(!state)return;bool changedAny=false;for(unsigned i=0;i<state->history->messageCount;++i){auto&m=state->history->messages[(state->history->messageStart+i)%MaxMessages];if(m.unread){m.unread=false;changedAny=true;}}if(changedAny)dataChanged();}
size_t positionCount(){Guard lock;if(!state)return 0;unsigned n=0;for(unsigned i=0;i<state->history->nodeCount;++i)n+=state->history->nodes[i].hasPosition;return n;}
bool position(size_t index,Position& result){Guard lock;if(!state)return false;for(unsigned i=0;i<state->history->nodeCount;++i){const auto&n=state->history->nodes[i];if(!n.hasPosition)continue;if(index--==0){result={};result.node=n.id;result.timestamp=n.positionTime;result.latitude=double(n.latitude)*1e-7;result.longitude=double(n.longitude)*1e-7;memcpy(result.name,n.name,sizeof(result.name));return true;}}return false;}
uint32_t positionRevision(){Guard lock;return state?state->positionRevision:0;}
bool showOnMap(uint32_t id){Position p;bool found=false;{Guard lock;if(state){int i=M::findNode(*state->history,id);if(i>=0){const auto&n=state->history->nodes[i];if(n.hasPosition){p.node=id;p.latitude=double(n.latitude)*1e-7;p.longitude=double(n.longitude)*1e-7;found=true;}}}}if(found&&hooks.openMap)hooks.openMap(p.node,p.latitude,p.longitude);return found;}
}
