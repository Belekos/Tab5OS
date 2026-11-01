#pragma once
#include <vector>
#include <cstring>
extern std::vector<unsigned char> nvsBytes;
extern bool nvsOpenOk,nvsWriteOk;
extern unsigned nvsWrites;
struct Preferences {
 bool begin(const char*,bool){return nvsOpenOk;}
 size_t getBytes(const char*,void* data,size_t size){if(nvsBytes.size()!=size)return 0;memcpy(data,nvsBytes.data(),size);return size;}
 size_t putBytes(const char*,const void* data,size_t size){++nvsWrites;if(!nvsWriteOk)return 0;nvsBytes.assign((const unsigned char*)data,(const unsigned char*)data+size);return size;}
 void end(){}
};
