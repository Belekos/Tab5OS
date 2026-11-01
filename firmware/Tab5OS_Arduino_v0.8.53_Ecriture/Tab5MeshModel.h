#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "Tab5MeshtasticProtocol.h"
namespace Tab5MeshModel {
constexpr unsigned MaxKnown=8,MaxNodes=32,MaxMessages=128;
struct Options {bool autoReconnect=false,background=false,showOnMap=false;};
struct Known {uint8_t type=0,address[6]{};bool unpaired=false;char name[40]{};uint32_t lastUsed=0;};
struct Node {uint32_t id=0,positionTime=0;int32_t latitude=0,longitude=0;bool hasPosition=false;char name[40]{};};
struct Message {
 uint32_t from=0,to=0,id=0,time=0,positionTime=0;
 int32_t latitude=0,longitude=0;
 uint8_t channel=0;bool outgoing=false,positionOnly=false,hasPosition=false,unread=false;
 char text[Tab5MeshtasticProtocol::MaxText+1]{};
};
struct Preferences {Options options{};uint8_t knownCount=0,preferred=0;Known known[MaxKnown]{};};
struct History {uint32_t sequence=0;uint16_t messageCount=0,messageStart=0;uint8_t nodeCount=0;Node nodes[MaxNodes]{};Message messages[MaxMessages]{};};
inline bool coordinates(int32_t lat,int32_t lon){return lat>=-900000000&&lat<=900000000&&lon>=-1800000000&&lon<=1800000000;}
inline bool sameAddress(const Known&a,uint8_t type,const uint8_t* value){return a.type==type&&!memcmp(a.address,value,6);}
inline unsigned unread(const History& h){unsigned count=0;for(unsigned i=0;i<h.messageCount;++i)if(h.messages[(h.messageStart+i)%MaxMessages].unread)++count;return count;}
inline int findNode(const History& h,uint32_t id){for(unsigned i=0;i<h.nodeCount;++i)if(h.nodes[i].id==id)return int(i);return -1;}
inline bool duplicate(const History& h,uint32_t id,uint32_t from){if(!id)return false;for(unsigned i=0;i<h.messageCount;++i){const auto&m=h.messages[(h.messageStart+i)%MaxMessages];if(m.id==id&&m.from==from)return true;}return false;}
// Only used when merging a loaded snapshot with live data; radio messages
// without packet IDs must still remain idempotent if the merge retries.
inline bool sameMessage(const History& h,const Message& value){for(unsigned i=0;i<h.messageCount;++i){const auto&m=h.messages[(h.messageStart+i)%MaxMessages];if(value.id){if(m.id==value.id&&m.from==value.from)return true;}else if(!m.id&&m.from==value.from&&m.to==value.to&&m.time==value.time&&m.positionOnly==value.positionOnly&&m.latitude==value.latitude&&m.longitude==value.longitude&&!strcmp(m.text,value.text))return true;}return false;}
inline Message& append(History& h){unsigned at=(h.messageStart+h.messageCount)%MaxMessages;if(h.messageCount==MaxMessages)h.messageStart=(h.messageStart+1)%MaxMessages;else ++h.messageCount;h.messages[at]={};return h.messages[at];}
}
