#pragma once
#include <set>
#include <string>
namespace Tab5Files {inline bool showHidden=false;}
namespace Tab5HiddenFiles {
inline std::set<std::string> hiddenPaths;
inline bool hidden(const char* path) {
  const std::string p(path); const size_t slash=p.rfind('/');
  const std::string name=p.substr(slash==std::string::npos?0:slash+1);
  return (!name.empty()&&name[0]=='.')||hiddenPaths.count(p)!=0;
}
}
