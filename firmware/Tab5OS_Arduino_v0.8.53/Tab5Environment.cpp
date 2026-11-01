#include "Tab5Environment.h"
#include "Tab5Secrets.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <cstdlib>
#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif
namespace {
constexpr size_t Slots=64,NameBytes=48,ValueBytes=512;
struct Entry {char name[NameBytes+1],value[ValueBytes+1];bool secret;};
Entry* entries=nullptr;size_t count=0;std::mutex mutex;
void wipe(void* p,size_t n){volatile unsigned char* b=static_cast<volatile unsigned char*>(p);while(n--)*b++=0;}
bool name(const char* p){if(!p||!(*p=='_'||(*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')))return false;size_t n=0;for(;*p;++p){if(++n>NameBytes||!(*p=='_'||(*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')))return false;}return true;}
bool allocate(){if(entries)return true;
#ifdef ARDUINO
 entries=static_cast<Entry*>(heap_caps_calloc(Slots,sizeof(Entry),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#else
 entries=static_cast<Entry*>(calloc(Slots,sizeof(Entry)));
#endif
 return entries!=nullptr;}
int put(const char* key,const char* value,bool secret){if(!name(key)||!value||strnlen(value,ValueBytes+1)>ValueBytes||(secret&&!name(value)))return -EINVAL;std::lock_guard<std::mutex> lock(mutex);if(!allocate())return -ENOMEM;size_t i=0;for(;i<count&&strcmp(entries[i].name,key);++i){}if(i==Slots)return -ENOSPC;if(i<count&&entries[i].secret&&!secret)return -EACCES;if(i==count)++count;wipe(entries+i,sizeof(Entry));strcpy(entries[i].name,key);strcpy(entries[i].value,value);entries[i].secret=secret;return 0;}
}
extern "C" int tab5_env_get(const char* key,char* output,size_t capacity){if(output&&capacity)memset(output,0,capacity);if(!name(key)||!output||!capacity)return -EINVAL;
 char reference[NameBytes+1]{};{std::lock_guard<std::mutex> lock(mutex);size_t i=0;for(;i<count&&strcmp(entries[i].name,key);++i){}if(i==count)return -ENOENT;if(!entries[i].secret){size_t n=strlen(entries[i].value);if(n>=capacity)return -ENOSPC;memcpy(output,entries[i].value,n+1);return int(n);}strcpy(reference,entries[i].value);}
 // Do not retain a resolved secret, or hold the environment mutex during SD IO.
 if(!Tab5Secrets::available())return -EACCES;if(!Tab5Secrets::get(reference,output,capacity)){memset(output,0,capacity);return -EACCES;}return int(strlen(output));
}
extern "C" int tab5_env_set(const char* key,const char* value){return put(key,value,false);}
extern "C" int tab5_env_bind(const char* key,const char* reference){return put(key,reference,true);}
extern "C" int tab5_env_unset(const char* key){if(!name(key))return -EINVAL;std::lock_guard<std::mutex> lock(mutex);for(size_t i=0;i<count;++i)if(!strcmp(entries[i].name,key)){wipe(entries+i,sizeof(Entry));if(i+1<count)memmove(entries+i,entries+i+1,(count-i-1)*sizeof(Entry));wipe(entries+--count,sizeof(Entry));break;}return 0;}
extern "C" int tab5_env_import_secrets(){if(!Tab5Secrets::available())return -EACCES;auto keys=Tab5Secrets::names();for(const auto& key:keys){int rc=tab5_env_bind(key.c_str(),key.c_str());if(rc<0)return rc;}return int(keys.size());}
namespace Tab5Environment {
std::vector<std::string> names(){std::lock_guard<std::mutex> lock(mutex);std::vector<std::string> result;for(size_t i=0;i<count;++i)result.emplace_back(entries[i].name);std::sort(result.begin(),result.end());return result;}
void clear(){std::lock_guard<std::mutex> lock(mutex);if(entries){wipe(entries,Slots*sizeof(Entry));free(entries);entries=nullptr;}count=0;}
bool privateCommand(const char* text){if(!text)return false;if(strchr(text,'='))return true;std::string word;auto sensitive=[](const std::string& v){return v=="set"||v=="export"||v=="--password"||v=="--token"||v=="--user"||v=="-u";};for(const char* p=text;;++p){unsigned char c=*p;if(!c||c==' '||c=='\t'||c=='\n'||c==';'||c=='|'||c=='&'){if(sensitive(word))return true;word.clear();if(!c)break;}else if(c!='\''&&c!='"'&&c!='\\')word+=char(c);}return false;}
}
extern "C" int tab5_env_call(int op,int a,const void* input,size_t length,void* output,size_t capacity){if(capacity>4096)return -EINVAL;if(output&&capacity)memset(output,0,capacity);const auto* p=static_cast<const char*>(input);if(op==109&&a==1){if(length||!output||!capacity)return -EINVAL;std::string result;for(const auto& key:Tab5Environment::names()){result+=key;result+='\n';}if(result.size()>=capacity){memset(output,0,capacity);return -ENOSPC;}memcpy(output,result.c_str(),result.size()+1);return int(result.size());}
 if(!p||!length||length>NameBytes+ValueBytes+2||p[length-1])return -EINVAL;size_t n=strnlen(p,length);if(n==length)return -EINVAL;
 if(op==107)return n+1==length?tab5_env_get(p,static_cast<char*>(output),capacity):-EINVAL;
 if(op==109&&a==0)return n+1==length?tab5_env_unset(p):-EINVAL;
 if(op==108&&(a==0||a==1)&&n+1<length){const char* value=p+n+1;if(strnlen(value,length-n-1)!=length-n-2)return -EINVAL;return a?tab5_env_bind(p,value):tab5_env_set(p,value);}return -EINVAL;}
