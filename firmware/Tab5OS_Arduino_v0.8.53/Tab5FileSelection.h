#ifndef TAB5_FILE_SELECTION_H
#define TAB5_FILE_SELECTION_H
#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

// Paths, never widget pointers, own the selection. Paging/rebuilding rows does
// not silently discard items selected on another page.
class Tab5FileSelection {
 public:
  std::vector<std::string> paths;
  std::string anchor;
  bool contains(const std::string& path) const {
    return std::find(paths.begin(), paths.end(), path) != paths.end();
  }
  void clear() { paths.clear(); anchor.clear(); }
  void add(const std::string& path) {
    if (!path.empty() && !contains(path)) paths.push_back(path);
  }
  void choose(const std::string& path, const std::vector<std::string>& rows,
              bool toggle, bool range, bool additive = false) {
    auto first = std::find(rows.begin(), rows.end(), anchor);
    auto last = std::find(rows.begin(), rows.end(), path);
    if (range && first != rows.end() && last != rows.end()) {
      if (!additive) paths.clear();
      if (last < first) std::swap(first, last);
      for (; first <= last; ++first) add(*first);
      return;
    }
    if (!toggle && !additive) paths.clear();
    auto selected = std::find(paths.begin(), paths.end(), path);
    if (toggle && selected != paths.end()) paths.erase(selected);
    else add(path);
    anchor = path;
  }
  // A recursive operation on a selected folder already includes descendants.
  // Collapse overlapping search results so each item is mutated only once.
  std::vector<std::string> roots() const {
    std::vector<std::string> result;
    const std::unordered_set<std::string> selected(paths.begin(), paths.end());
    for (const auto& path : paths) {
      bool covered = false;
      for (size_t slash=path.find('/',1);slash!=std::string::npos;slash=path.find('/',slash+1))
        if(selected.count(path.substr(0,slash))){covered=true;break;}
      if (!covered) result.push_back(path);
    }
    return result;
  }
};
#endif
