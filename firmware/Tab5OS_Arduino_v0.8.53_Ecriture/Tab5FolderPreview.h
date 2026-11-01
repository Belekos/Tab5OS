#pragma once
#include "Tab5VolumeFS.h"
#include "Tab5HiddenFiles.h"
#include "Tab5DirectoryModel.h"
#include <vector>

// Scan cooperatively into PSRAM; only one page owns LVGL rows. Cached metadata
// provides global sorting and prevents rescanning SD/USB on every page change.
class Tab5FolderPreview {
 public:
  struct Entry {String path,name;bool directory=false,hidden=false;};
  static constexpr size_t PageSize=48;
  std::vector<Entry> entries;
  size_t count=0,offset=0;
  bool done=true,failed=false,dirty=false;
  String path;
  time_t modified=0;
  void cancel(){directory.close();model.clear();done=true;dirty=false;std::vector<Entry>().swap(entries);path="";}
  bool begin(const String& folder,size_t first=0){
    if(first&&path==folder&&done&&!failed&&showHidden==Tab5Files::showHidden){offset=first;model.sort(Tab5FileSort::selected);page();return true;}
    cancel();path=folder;offset=first;count=0;failed=false;done=false;dirty=true;showHidden=Tab5Files::showHidden;
    directory=tab5FS.open(path,FILE_READ);
    if(!directory||!directory.isDirectory()){directory.close();failed=true;done=true;return false;}
    modified=directory.getLastWrite();return true;
  }
  void service(){
    if(done)return;
    const uint32_t started=millis();
    for(unsigned i=0;i<8&&millis()-started<5;++i){
      File item=directory.openNextFile();
      if(!item){directory.close();done=true;model.sort(Tab5FileSort::selected);page();return;}
      String name=item.name();int slash=name.lastIndexOf('/');if(slash>=0)name=name.substring(slash+1);
      const bool folder=item.isDirectory();const uint64_t bytes=item.size();const int64_t modified=item.getLastWrite();item.close();
      if(!name.length()||name=="."||name=="..")continue;
      String full=(path=="/"?String("/"):path+"/")+name;
      const bool hidden=Tab5HiddenFiles::hidden(full.c_str());
      if(hidden&&!Tab5Files::showHidden)continue;
      if(!model.add(name.c_str(),folder,bytes,modified)){directory.close();failed=true;done=true;dirty=true;return;}
      ++count;dirty=true;
    }
  }
 private:
  File directory;
  bool showHidden=false;
  Tab5DirectoryModel model;
  void page(){
    entries.clear();if(offset>=count&&offset)offset=count?((count-1)/PageSize)*PageSize:0;
    for(size_t i=offset;i<count&&entries.size()<PageSize;++i){const auto& item=model[i];String full=(path=="/"?String("/"):path+"/")+item.name;entries.push_back({full,item.name,item.directory,Tab5HiddenFiles::hidden(full.c_str())});}
    dirty=true;
  }
};
