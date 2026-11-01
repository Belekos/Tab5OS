#pragma once
#include <stdint.h>
// UI-only scheduling: mutations are debounced; the periodic scan also catches
// files written by external/POSIX clients which did not publish a revision.
namespace Tab5AppRefresh {
struct Schedule {
  static constexpr uint32_t DebounceMs=750,MinimumMs=2000,PollMs=5000;
  uint32_t seen=0,changedAt=0,lastStart=0,lastFinish=0;
  bool initialized=false,dirty=false,running=false;
  void observe(uint32_t now,uint32_t revision){
    if(!initialized){seen=revision;initialized=true;return;}
    if(seen!=revision){seen=revision;changedAt=now;dirty=true;}
  }
  bool due(uint32_t now)const{
    return !running&&uint32_t(now-lastStart)>=MinimumMs
      &&((dirty&&uint32_t(now-changedAt)>=DebounceMs)||uint32_t(now-lastFinish)>=PollMs);
  }
  void started(uint32_t now){running=true;dirty=false;lastStart=now;}
  void finished(uint32_t now){running=false;lastFinish=now;}
};
}
