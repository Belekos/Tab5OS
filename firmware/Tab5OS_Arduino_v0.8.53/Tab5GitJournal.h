#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <functional>

namespace Tab5GitJournal {
// FAT has no replace-existing rename. This journal protects the previous
// complete file until the new file has its final name. The filesystem adapter
// flushes the intent before either rename. A power failure can still damage
// FAT itself; no software transaction claims otherwise.
struct Ops {
 std::function<int(const std::string&)> exists; // 1 / 0 / -1 I/O error
 std::function<bool(const std::string&,const std::string&)> rename;
 std::function<bool(const std::string&)> remove;
 std::function<bool(const std::string&,const void*,size_t)> writeExclusive;
 std::function<bool(const std::string&,void*,size_t)> readExact;
};
struct Record {uint32_t magic=0x54474954u;char from[512]{},to[512]{};uint32_t checksum=0;};
inline uint32_t checksum(const Record& r){uint32_t c=2166136261u;const auto* p=reinterpret_cast<const unsigned char*>(&r);for(size_t i=0;i<offsetof(Record,checksum);++i)c=(c^p[i])*16777619u;return c;}
inline bool within(const std::string& path,const std::string& root){
 if(path.size()<=root.size()||path.compare(0,root.size(),root)||path[root.size()]!='/')return false;
 for(unsigned char c:path)if(c<32||c==127||c=='\\')return false;
 size_t at=root.size()+1;while(at<=path.size()){size_t end=path.find('/',at);if(end==std::string::npos)end=path.size();auto part=path.substr(at,end-at);if(part.empty()||part=="."||part=="..")return false;at=end+1;}
 return true;
}
inline std::string journal(const std::string& root){return root+"/.git/tab5-transaction";}
inline std::string backup(const std::string& path){return path+".tab5-previous";}
inline bool recover(const std::string& root,const Ops& ops){
 const std::string log=journal(root);int present=ops.exists(log);if(present<0)return false;if(!present)return true;
 Record r{};if(!ops.readExact(log,&r,sizeof(r))||r.magic!=0x54474954u||r.checksum!=checksum(r)||!memchr(r.from,0,sizeof(r.from))||!memchr(r.to,0,sizeof(r.to)))return false;
 std::string from=r.from,to=r.to,old=backup(to);if(!within(from,root)||!within(to,root)||from==to||from==old||to==log||from==log)return false;
 int src=ops.exists(from),dst=ops.exists(to),saved=ops.exists(old);if(src<0||dst<0||saved<0)return false;
 if(saved&&!dst){if(!ops.rename(old,to))return false;if(src&&!ops.remove(from))return false;}
 else if(saved&&dst){if(src)return false;if(!ops.remove(old))return false;}
 else if(!dst)return false;
 else if(src&&!ops.remove(from))return false;
 return ops.remove(log);
}
inline bool replace(const std::string& root,const std::string& from,const std::string& to,const Ops& ops){
 if(!within(from,root)||!within(to,root)||from==to||from.size()>=512||to.size()>=512||to==journal(root)||!recover(root,ops))return false;
 int target=ops.exists(to);if(target<0)return false;if(!target)return ops.rename(from,to);
 std::string old=backup(to);if(ops.exists(old)!=0)return false;
 Record r{};memcpy(r.from,from.c_str(),from.size()+1);memcpy(r.to,to.c_str(),to.size()+1);r.checksum=checksum(r);
 if(!ops.writeExclusive(journal(root),&r,sizeof(r)))return false;
 if(!ops.rename(to,old)){recover(root,ops);return false;}
 if(!ops.rename(from,to)){recover(root,ops);return false;}
 // Reporting success requires cleanup too. Recovery can complete it later,
 // while both a complete new file and the previous complete file remain.
 if(!ops.remove(old))return false;
 return ops.remove(journal(root));
}
}
