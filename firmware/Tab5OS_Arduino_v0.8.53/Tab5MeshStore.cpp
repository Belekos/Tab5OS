#include "Tab5MeshStore.h"
#include "Tab5MeshCodec.h"
#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include <Preferences.h>
#include <algorithm>
#include <esp_heap_caps.h>
namespace Tab5MeshStore {
namespace {
constexpr const char* paths[]={"/System/mesh/history-a.bin","/System/mesh/history-b.bin"};
struct Buffer {uint8_t* data=static_cast<uint8_t*>(heap_caps_malloc(Tab5MeshCodec::MaxBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));~Buffer(){heap_caps_free(data);}};
bool read(const char*path,uint8_t*buffer,size_t& size){File f=tab5FS.open(path);if(!f||f.isDirectory()||f.size()>Tab5MeshCodec::MaxBytes){if(f)f.close();return false;}size=f.size();size_t at=0;while(at<size){size_t n=f.read(buffer+at,std::min(size_t(4096),size-at));if(!n){f.close();return false;}at+=n;}f.close();return true;}
}
bool loadPreferences(Tab5MeshModel::Preferences& out){::Preferences p;if(!p.begin("tab5-mesh",true))return true;size_t n=p.getBytesLength("settings");if(!n){p.end();return true;}uint8_t bytes[Tab5MeshCodec::PreferenceBytes];if(n>sizeof(bytes)){p.end();return false;}size_t got=p.getBytes("settings",bytes,n);p.end();return got==n&&Tab5MeshCodec::decode(bytes,n,out);}
bool savePreferences(const Tab5MeshModel::Preferences& in){uint8_t bytes[Tab5MeshCodec::PreferenceBytes];size_t n=Tab5MeshCodec::encode(in,bytes,sizeof(bytes));if(!n)return false;::Preferences p;if(!p.begin("tab5-mesh",false))return false;bool ok=p.putBytes("settings",bytes,n)==n;p.end();return ok;}
bool loadHistory(Tab5MeshModel::History& out){Tab5StorageUse lease(true);if(!lease||!tab5SdAvailable())return false;Buffer buffer;if(!buffer.data)return false;bool found=false;uint32_t sequence=0;int chosen=-1;
 for(unsigned i=0;i<2;++i){size_t n=0;if(read(paths[i],buffer.data,n)&&Tab5MeshCodec::decode(buffer.data,n,out)&&(!found||int32_t(out.sequence-sequence)>0)){sequence=out.sequence;chosen=int(i);found=true;}}
 if(!found){out={};return !tab5FS.exists(paths[0])&&!tab5FS.exists(paths[1]);}size_t n=0;return read(paths[chosen],buffer.data,n)&&Tab5MeshCodec::decode(buffer.data,n,out);}
bool saveHistory(const Tab5MeshModel::History& in){Tab5StorageUse lease(true);if(!lease||!tab5SdAvailable())return false;Buffer buffer;if(!buffer.data)return false;size_t n=Tab5MeshCodec::encode(in,buffer.data,Tab5MeshCodec::MaxBytes);if(!n)return false;if(!tab5FS.exists("/System")&&!tab5FS.mkdir("/System"))return false;if(!tab5FS.exists("/System/mesh")&&!tab5FS.mkdir("/System/mesh"))return false;const char* target=paths[in.sequence&1];const char* temp="/System/mesh/history.tmp";if(tab5FS.exists(temp)&&!tab5FS.remove(temp))return false;File f=tab5FS.open(temp,FILE_WRITE);if(!f)return false;size_t at=0;while(at<n){size_t k=std::min(size_t(4096),n-at);size_t wrote=f.write(buffer.data+at,k);if(wrote!=k)break;at+=k;}f.flush();f.close();if(at!=n){tab5FS.remove(temp);return false;}size_t checkSize=0;if(!read(temp,buffer.data,checkSize)||checkSize!=n){tab5FS.remove(temp);return false;}Tab5MeshCodec::Input tail{buffer.data+n-4,4};if(Tab5MeshCodec::crc(buffer.data,n-4)!=tail.number()){tab5FS.remove(temp);return false;}
 // Only the older/inactive generation is replaced. The other valid generation
 // remains readable across power loss between remove and rename.
 if(tab5FS.exists(target)&&!tab5FS.remove(target)){tab5FS.remove(temp);return false;}if(!tab5FS.rename(temp,target)){tab5FS.remove(temp);return false;}return true;}
}
