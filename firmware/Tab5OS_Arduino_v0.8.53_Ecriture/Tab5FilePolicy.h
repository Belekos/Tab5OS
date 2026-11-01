#pragma once
#include <cctype>
#include <string>
#include <vector>

namespace Tab5Files {
inline bool showHidden=false;
inline bool hidden(const char* name){
 if(!name||!*name)return false;const std::string s=name;return s[0]=='.'||(s.size()>4&&s.compare(s.size()-4,4,".bak")==0);
}

// FAT is case insensitive. Reject ambiguous FAT aliases, control characters,
// short-name aliases and paths escaping the root. Resolve dot segments first.
inline bool canonical(const std::string& path, std::string& out) {
  out.clear();
  if (path.empty() || (path[0] != '/' && path[0] != '\\')) return false;
  std::vector<std::string> parts;
  std::string part;
  for (size_t i = 1; i <= path.size(); ++i) {
    unsigned char c = i == path.size() ? '/' : static_cast<unsigned char>(path[i]);
    if (c == '/' || c == '\\') {
      if (part == "..") {
        if (parts.empty()) return false;
        parts.pop_back();
      } else if (!part.empty() && part != ".") {
        if (part.back() == ' ' || part.back() == '.' || part.find('~') != std::string::npos)
          return false;
        parts.push_back(part);
      }
      part.clear();
    } else {
      if (c < 32 || c == 127 || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
          c == '>' || c == '|')
        return false;
      part += static_cast<char>(std::tolower(c));
    }
  }
  if(!parts.empty()&&parts[0]=="m5writer")parts[0]="system";
  out = "/";
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i) out += '/';
    out += parts[i];
  }
  return true;
}
inline bool inside(const std::string& path, const std::string& root) {
  return path == root || (path.size() > root.size() && path.compare(0, root.size(), root) == 0 &&
                          path[root.size()] == '/');
}
inline bool protectedPath(const std::string& path) {
  std::string p;
  if (!canonical(path, p)) return true;
  return p == "/" || p == "/radios.json" || p == "/videos.json" ||
         p.compare(0, 13, "/radios.json.") == 0 || p.compare(0, 13, "/videos.json.") == 0 ||
         inside(p, "/system") || inside(p, "/system volume information") ||
         inside(p, "/$recycle.bin");
}
inline bool userWritable(const std::string& path) {
  std::string p;
  return canonical(path, p) && p != "/";
}
inline bool sameOrDescendant(const std::string& path, const std::string& root) {
  std::string a, b;
  return canonical(path, a) && canonical(root, b) && (b == "/" || inside(a, b));
}
// System-file classification is advisory. Valid SD paths may be updated by
// the user; only malformed paths and mutation of the volume root are rejected.
}  // namespace Tab5Files
