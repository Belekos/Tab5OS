#pragma once
#include <stdint.h>
struct BridgeMock {uint32_t status=0,reads=0,acknowledgements=0;};
extern BridgeMock bridgeMock;
#define MIPI_DSI_BRG_LL_EVENT_UNDERRUN 1u
#define MIPI_DSI_BRG_LL_EVENT_VSYNC 2u
#define MIPI_DSI_LL_GET_BRG(n) (&bridgeMock)
inline uint32_t mipi_dsi_brg_ll_get_interrupt_status(BridgeMock* b){++b->reads;return b->status;}
