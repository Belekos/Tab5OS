#include "Tab5CatalogueRevision.h"
#include <cassert>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

int main(){
 std::atomic<uint32_t> tracks{2},stations{2};unsigned checks=0;
 auto check=[&](bool value){assert(value);++checks;};
 check(tab5CatalogueRevision(tracks)==2);
 {Tab5CatalogueChange change(tracks);check(tab5CatalogueRevision(tracks)==0);check(tab5CatalogueRevision(stations)==2);}
 check(tab5CatalogueRevision(tracks)==4);
 tracks.store(UINT32_MAX-1);{Tab5CatalogueChange change(tracks);check(tab5CatalogueRevision(tracks)==0);}check(tab5CatalogueRevision(tracks)==2);
 std::mutex mutex;std::vector<unsigned> data(128,0);std::atomic<bool> done{false};std::atomic<unsigned> accepted{0};
 std::thread writer([&]{for(unsigned n=1;n<=20000;++n){std::lock_guard<std::mutex> lock(mutex);Tab5CatalogueChange change(tracks);for(auto& x:data)x=n;}done=true;});
 do {
  const auto before=tab5CatalogueRevision(tracks);if(!before)continue;
  std::vector<unsigned> snapshot;
  for(unsigned i=0;i<data.size();++i){std::lock_guard<std::mutex> lock(mutex);snapshot.push_back(data[i]);}
  if(before==tab5CatalogueRevision(tracks)){for(auto x:snapshot)check(x==snapshot.front());++accepted;}
 }while(!done);
 writer.join();
 {std::lock_guard<std::mutex> lock(mutex);const auto before=tab5CatalogueRevision(tracks);for(auto x:data)check(x==20000);check(before==tab5CatalogueRevision(tracks)&&before!=0);++accepted;}
 check(tab5CatalogueRevision(stations)==2);check(tab5CatalogueRevision(tracks)!=0);
 printf("PASS %u checks: real revision helper, 20000 protected mutations, odd-state rejection, independent catalogues, wrap and mixed-snapshot rejection; accepted=%u.\n",checks,unsigned(accepted));
}
