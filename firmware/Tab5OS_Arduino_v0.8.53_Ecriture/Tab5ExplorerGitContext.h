#pragma once
#include <string>
namespace Tab5ExplorerGit {
// Component boundaries matter: /repo2 must never be treated as /repo.
inline std::string repositoryPath(const char* root,const char* relative){
 std::string value=root?root:"";if(value.empty()||!relative||!*relative)return value;
 if(value.back()!='/')value+='/';value+=relative;return value;
}
inline std::string relativePath(const char* root,const char* path){
 const std::string base=root?root:"",value=path?path:"";
 if(base.empty()||value.empty())return "";
 if(base=="/")return value.front()=='/'?value.substr(1):"";
 return value.size()>base.size()&&value.compare(0,base.size(),base)==0&&value[base.size()]=='/'?value.substr(base.size()+1):"";
}
}
