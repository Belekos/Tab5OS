#pragma once
#include <cstdint>
#include <cstring>

// One session preference shared by Explorer, directory previews and pickers.
// Directories stay together before files; unknown dates/sizes use zero.
namespace Tab5FileSort {
enum class Order : uint8_t { NameAscending, NameDescending, DateAscending, DateDescending, SizeAscending, SizeDescending };
inline Order selected=Order::NameAscending;
inline const char* caption(unsigned order){
  static const char* const labels[]={"Nom : A à Z","Nom : Z à A","Date : anciennes d'abord","Date : récentes d'abord","Taille : croissante","Taille : décroissante"};
  return labels[order<6?order:0];
}
inline int names(const char* a,const char* b){
  const auto* x=reinterpret_cast<const unsigned char*>(a);const auto* y=reinterpret_cast<const unsigned char*>(b);
  while(*x&&*y){const unsigned ac=*x>='A'&&*x<='Z'?*x+32:*x,bc=*y>='A'&&*y<='Z'?*y+32:*y;if(ac!=bc)return ac<bc?-1:1;++x;++y;}
  if(*x!=*y)return *x?1:-1;return std::strcmp(a,b);
}
template<class Entry> inline bool less(const Entry& a,const Entry& b,Order order){
  if(a.directory!=b.directory)return a.directory;
  const unsigned field=unsigned(order)/2;int result=0;
  if(field==1&&a.modified!=b.modified)result=a.modified<b.modified?-1:1;
  if(field==2&&a.bytes!=b.bytes)result=a.bytes<b.bytes?-1:1;
  if(!result){const char* an=std::strrchr(a.name,'/');const char* bn=std::strrchr(b.name,'/');result=names(an?an+1:a.name,bn?bn+1:b.name);if(!result)result=names(a.name,b.name);}
  return unsigned(order)&1?result>0:result<0;
}
}
