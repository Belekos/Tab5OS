#include "Tab5LaunchProfile.h"
#include "Tab5LaunchDefaults.h"
#include "Tab5Storage.h"
#include "Tab5VolumeFS.h"
#include <Preferences.h>
#include <stdio.h>
namespace {
struct Record {uint32_t magic;char executable[261];Tab5LaunchProfile profile;};
bool identity(const char* input,char (&path)[261],char (&key)[16]){
 if(!tab5LaunchPath(input,path,sizeof(path)))return false;
 uint64_t hash=14695981039346656037ULL;
 for(char* p=path;*p;++p){if(*p>='A'&&*p<='Z')*p+=32;hash=(hash^uint8_t(*p))*1099511628211ULL;}
 snprintf(key,sizeof(key),"p%014llx",static_cast<unsigned long long>(hash&0xffffffffffffffULL));return true;
}
bool valid(const Tab5LaunchProfile& p){
 if(p.mode==Tab5LaunchMode::Ask||p.mode==Tab5LaunchMode::None)return !p.argument[0];
 char resolved[261];return p.mode==Tab5LaunchMode::File&&memchr(p.argument,0,sizeof(p.argument))&&tab5LaunchPath(p.argument,resolved,sizeof(resolved));
}
bool packagedDefault(const char* executable,Tab5LaunchProfile& profile){
 char path[261],sidecar[280];if(!tab5LaunchPath(executable,path,sizeof(path)))return false;
 const int length=snprintf(sidecar,sizeof(sidecar),"%s.launch.json",path);if(length<0||size_t(length)>=sizeof(sidecar))return false;
 Tab5StorageUse lease;if(!lease)return false;
 File file=tab5FS.open(sidecar,FILE_READ);if(!file||file.isDirectory()||!file.size()||file.size()>1024)return false;
 char text[1025];const size_t size=file.size();if(file.read(reinterpret_cast<uint8_t*>(text),size)!=size)return false;text[size]=0;
 return tab5ParseLaunchDefaults(text,size,profile);
}
}
bool tab5LaunchProfileLoad(const char* executable,Tab5LaunchProfile& profile){
 profile={};char path[261],key[16];if(!identity(executable,path,key))return false;
 Preferences prefs;if(!prefs.begin("tab5launch",true))return packagedDefault(executable,profile);
 const bool stored=prefs.isKey(key);
 Record record{};const bool read=prefs.getBytesLength(key)==sizeof(record)&&prefs.getBytes(key,&record,sizeof(record))==sizeof(record);prefs.end();
 if(!stored)return packagedDefault(executable,profile);
 if(!read||record.magic!=0x1735504c||!memchr(record.executable,0,sizeof(record.executable))||strcmp(path,record.executable)||!valid(record.profile))return false;
 profile=record.profile;return true;
}
bool tab5LaunchProfileSave(const char* executable,const Tab5LaunchProfile& profile){
 char path[261],key[16];if(!identity(executable,path,key)||!valid(profile))return false;
 Preferences prefs;if(!prefs.begin("tab5launch",false))return false;
 // A hash collision must never replace another application's configuration.
 Record previous{};if(prefs.isKey(key)&&(prefs.getBytesLength(key)!=sizeof(previous)||prefs.getBytes(key,&previous,sizeof(previous))!=sizeof(previous)||!memchr(previous.executable,0,sizeof(previous.executable))||strcmp(previous.executable,path))){prefs.end();return false;}
 Record record{};record.magic=0x1735504c;strcpy(record.executable,path);record.profile=profile;
 if(profile.mode==Tab5LaunchMode::File){char canonical[261];tab5LaunchPath(profile.argument,canonical,sizeof(canonical));strcpy(record.profile.argument,canonical);}
 const bool ok=prefs.putBytes(key,&record,sizeof(record))==sizeof(record);prefs.end();return ok;
}
bool tab5LaunchProfileForget(const char* executable){
 Tab5LaunchProfile profile;if(!tab5LaunchProfileLoad(executable,profile))return false;
 char path[261],key[16];if(!identity(executable,path,key))return false;Preferences prefs;if(!prefs.begin("tab5launch",false))return false;const bool ok=prefs.remove(key);prefs.end();return ok;
}
