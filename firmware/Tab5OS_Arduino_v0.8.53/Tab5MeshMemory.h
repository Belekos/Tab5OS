#pragma once
#include <stddef.h>
#include "Tab5MeshAllocator.h"
// Admission estimates for the selected SDK host allocator. These
// values are deliberately not advertised as measured board heap consumption.
// MSYS allowances include mbuf/header/alignment; ACL/events include transport
// headers. Keep room for host objects, NVS and allocator metadata as well.
namespace Tab5MeshMemory {
constexpr size_t max(size_t a,size_t b){return a>b?a:b;}
constexpr size_t align(size_t value,size_t unit){return (value+unit-1)/unit*unit;}
constexpr size_t Msys1=CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT*(CONFIG_BT_NIMBLE_MSYS_1_BLOCK_SIZE+64U);
constexpr size_t Msys2=CONFIG_BT_NIMBLE_MSYS_2_BLOCK_COUNT*(CONFIG_BT_NIMBLE_MSYS_2_BLOCK_SIZE+64U);
constexpr size_t Acl=CONFIG_BT_NIMBLE_TRANSPORT_ACL_FROM_LL_COUNT*(CONFIG_BT_NIMBLE_TRANSPORT_ACL_SIZE+8U);
constexpr size_t Events=(CONFIG_BT_NIMBLE_TRANSPORT_EVT_COUNT+CONFIG_BT_NIMBLE_TRANSPORT_EVT_DISCARD_COUNT)*(CONFIG_BT_NIMBLE_TRANSPORT_EVT_SIZE+8U);
constexpr size_t HostStack=CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE;
constexpr size_t Pools=Msys1+Msys2+Acl+Events;
constexpr size_t HostAndMetadata=16U*1024+4U*1024;
constexpr size_t PoolBlock=max(max(Msys1,Msys2),max(Acl,Events));
// Keep the previous 20 KiB host/NVS/allocator allowance on the internal heap:
// OS queues, Hosted transport and task metadata are not redirected. Independently
// reserve the same allowance in PSRAM for host objects and pool metadata.
constexpr size_t Internal=align((TAB5_MESH_EXTERNAL_POOLS?0:Pools)+HostStack+HostAndMetadata,4096);
constexpr size_t LargestBlock=align((TAB5_MESH_EXTERNAL_POOLS?HostStack:max(PoolBlock,HostStack))+1024,1024);
constexpr size_t External=TAB5_MESH_EXTERNAL_POOLS?align(Pools+HostAndMetadata,4096):0;
constexpr size_t ExternalBlock=TAB5_MESH_EXTERNAL_POOLS?align(PoolBlock+1024,1024):0;
}
