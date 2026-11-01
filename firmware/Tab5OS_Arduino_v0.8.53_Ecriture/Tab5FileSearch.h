#include "Tab5VolumeFS.h"
#pragma once
#include <SD.h>
#include "Tab5Search.h"
#include "Tab5FilePolicy.h"
#include "Tab5HiddenFiles.h"
#include <vector>
// Cooperative, UI-thread traversal: at most one directory and one entry open.
// No recursive call stack, file-content reads or unbounded result allocation.
class Tab5FileSearch {
  File directory;
  String current;
  std::vector<String> pending;
  tab5search::Query query;
  bool active=false,partial=false;
  size_t found=0,examined=0;
 public:
  static constexpr size_t MaxResults=200,MaxPending=512,MaxPath=512;
  void cancel(){directory.close();directory=File();pending.clear();active=false;}
  void begin(const String& root,const String& text){cancel();found=examined=0;partial=false;query=tab5search::Query(text.c_str());if(!query.empty()){pending.push_back(root);active=true;}}
  template<class Visitor> void step(Visitor visitor,unsigned budget=12){
    for(unsigned used=0;active&&used<budget;++used){
      if(!directory){
        if(pending.empty()){active=false;break;}
        current=pending.back();pending.pop_back();directory=tab5FS.open(current.c_str());
        if(!directory||!directory.isDirectory()){partial=true;directory.close();directory=File();continue;}
      }
      File item=directory.openNextFile();
      if(!item){directory.close();directory=File();continue;}
      String name=item.name();int slash=name.lastIndexOf('/');if(slash>=0)name=name.substring(slash+1);
      const bool folder=item.isDirectory();item.close();++examined;
      if(!name.length()||name=="."||name=="..")continue;
      String path=(current=="/"?String("/"):current+"/")+name;
      if(!Tab5Files::showHidden&&Tab5HiddenFiles::hidden(path.c_str()))continue;
      if(path.length()>=MaxPath){partial=true;continue;}
      if(folder){if(pending.size()<MaxPending)pending.push_back(path);else partial=true;}
      if(query.matches(name.c_str())){
        if(!visitor(path,name,folder)){partial=true;cancel();break;}
        if(++found>=MaxResults){partial=true;cancel();break;}
      }
    }
  }
  bool busy()const{return active;}bool incomplete()const{return partial;}
  size_t count()const{return found;}size_t visited()const{return examined;}
};
