#pragma once
#include "boundary.h"
struct Tab5DisplaySignalsStats{bool active=false;uint32_t underruns=0;};
inline Tab5DisplaySignalsStats tab5DisplaySignalsStats(){return {Boundary::displayObserved,Boundary::underruns.load()};}
