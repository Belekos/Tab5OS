#include "Tab5DisplayPendingMirror.h"
#include <array>
#include <cassert>
#include <cstdio>
int main(){unsigned cases=0;constexpr int N=12;for(int l=0;l<N;l++)for(int r=l+1;r<=N;r++)for(int a=0;a<N;a++)for(int b=a+1;b<=N;b++)for(int c=0;c<N;c++)for(int d=c+1;d<=N;d++){
 Tab5DisplayPendingMirror::Row row;Tab5DisplayPendingMirror::reset(row,l,r);std::array<unsigned,N> front{},back{},expected{};std::array<bool,N> written{};for(int x=0;x<N;x++){front[x]=100+x;back[x]=expected[x]=x>=l&&x<r?1000+x:front[x];expected[x]=front[x];}
 auto copy=[&](uint16_t first,uint16_t end){assert(first<end&&end<=N);for(int x=first;x<end;x++){assert(!written[x]);back[x]=front[x];}};
 Tab5DisplayPendingMirror::overwrite(row,a,b,copy);for(int x=a;x<b;x++){back[x]=expected[x]=2000+x;written[x]=true;}
 Tab5DisplayPendingMirror::overwrite(row,c,d,copy);for(int x=c;x<d;x++){back[x]=expected[x]=3000+x;written[x]=true;}
 // A third independent middle write forces excess fragmentation where possible.
 Tab5DisplayPendingMirror::overwrite(row,5,6,copy);back[5]=expected[5]=4005;written[5]=true;
 Tab5DisplayPendingMirror::flush(row,copy);assert(back==expected);assert(row.spans[0].first==0&&row.spans[0].end==0&&row.spans[1].first==0&&row.spans[1].end==0);++cases;
 }std::printf("{\"passed\":true,\"cases\":%u}\n",cases);}
