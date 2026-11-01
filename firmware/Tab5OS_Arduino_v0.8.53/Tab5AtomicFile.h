#include "Tab5VolumeFS.h"
#pragma once
#include <SD.h>
#include <algorithm>
#include <cerrno>
#include "Tab5Storage.h"
// A false exists() is not proof of absence: preserve committed files and
// backups whenever stat fails for a reason other than ENOENT.
inline bool tab5AtomicProbe(const char* path,bool& present){
 errno=0;present=tab5FS.exists(path);return present||errno==ENOENT;
}
// Keep a caller's existing backup naming and restore the old primary if the
// final promotion fails. A failed rollback leaves the backup untouched.
inline bool tab5AtomicReplace(const char* temporary,const char* path,const char* backup){
 bool original=false,present=false;if(!tab5AtomicProbe(path,original))return false;
 if(original){
  if(!tab5AtomicProbe(backup,present)||(present&&!tab5FS.remove(backup))||!tab5FS.rename(path,backup))return false;
 }
 if(!tab5FS.rename(temporary,path)){const int error=errno?errno:EIO;if(original)tab5FS.rename(backup,path);errno=error;return false;}
 return true;
}
inline bool tab5AtomicRecover(const char* path){
 String p=path;int slash=p.lastIndexOf('/');if(slash<0||p.length()>260)return false;
 bool present=false;if(!tab5AtomicProbe(p.c_str(),present))return false;if(present)return true;
 String backup=p.substring(0,slash)+"/.Tab5Backups/"+p.substring(slash+1)+".bak";
 if(!tab5AtomicProbe(backup.c_str(),present))return false;
 return !present||tab5FS.rename(backup,p);
}
// The caller holds a storage lease. A failed replacement restores the old file.
inline bool tab5AtomicWrite(const char* path,const void* bytes,size_t length){
 String p=path;int slash=p.lastIndexOf('/');if(slash<0||p.length()>260)return false;
 String directory=p.substring(0,slash),name=p.substring(slash+1);
 String backups=directory+"/.Tab5Backups",backup=backups+"/"+name+".bak",temp=directory+"/."+name+".tmp";
 bool present=false;if(!tab5AtomicProbe(backups.c_str(),present)||(!present&&!tab5FS.mkdir(backups)))return false;
 if(!tab5AtomicRecover(path))return false;
 if(!tab5AtomicProbe(temp.c_str(),present)||(present&&!tab5FS.remove(temp)))return false;
 File f=tab5FS.open(temp,FILE_WRITE);if(!f)return false;
 int error=0;size_t done=0;
 while(done<length){
  const size_t n=std::min(size_t(4096),length-done);errno=0;
  if(f.write(static_cast<const uint8_t*>(bytes)+done,n)!=n){error=errno?errno:EIO;break;}
  done+=n;
 }
 errno=0;f.flush();if(errno&&!error)error=errno;
 if(!error){errno=0;const size_t stored=f.size();if(errno||done!=length||f.getWriteError()||stored!=length)error=errno?errno:EIO;}
 errno=0;f.close();if(errno&&!error)error=errno;
 // Preserve resource/path errors through synchronization and cleanup: a full
 // card or allocation failure must not be misreported as a broken SD bus.
 if(error){tab5FS.remove(temp);tab5StorageFault("Écriture",path,error);errno=error;return false;}
 return tab5AtomicReplace(temp.c_str(),p.c_str(),backup.c_str());
}
