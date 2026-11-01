#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
// Small independent implementation of the documented Meshtastic protobuf wire
// fields. No generated third-party sources or dynamic allocation. Unknown fields
// are skipped; malformed packets never partially modify the caller's model.
namespace Tab5MeshtasticProtocol {
constexpr size_t MaxPacket=512, MaxText=233;
constexpr uint32_t Broadcast=0xffffffffu;
struct Field {uint32_t number=0;uint8_t wire=0;uint64_t value=0;const uint8_t* bytes=nullptr;size_t size=0;};
class Reader {
 const uint8_t* p_;size_t n_,at_=0;bool valid_=true;
 bool var(uint64_t& out){out=0;for(unsigned shift=0;shift<70;shift+=7){if(at_>=n_)return valid_=false;uint8_t c=p_[at_++];if(shift==63&&(c&0xfe))return valid_=false;out|=uint64_t(c&127)<<shift;if(!(c&128))return true;}return valid_=false;}
 public:
 Reader(const uint8_t* p,size_t n):p_(p),n_(n){if((!p&&n)||n>MaxPacket)valid_=false;}
 bool next(Field& f){f={};if(!valid_||at_==n_)return false;uint64_t tag;if(!var(tag)||!(tag>>3)||(tag>>3)>0x1fffffffu)return valid_=false;f.number=uint32_t(tag>>3);f.wire=tag&7;
  if(f.wire==0)return var(f.value);
  if(f.wire==1||f.wire==5){size_t k=f.wire==1?8:4;if(k>n_-at_)return valid_=false;for(size_t j=0;j<k;++j)f.value|=uint64_t(p_[at_++])<<(8*j);return true;}
  if(f.wire==2){uint64_t k;if(!var(k)||k>n_-at_)return valid_=false;f.bytes=p_+at_;f.size=size_t(k);at_+=f.size;return true;}
  return valid_=false;
 }
 bool ok()const{return valid_&&at_==n_;}
};
inline bool utf8(const uint8_t* p,size_t n){if(!p&&n)return false;for(size_t i=0;i<n;){uint32_t c=p[i++];if(c<128){if(c==0||(c<32&&c!=9&&c!=10&&c!=13))return false;continue;}unsigned k;uint32_t low;if(c>=0xc2&&c<=0xdf){k=1;low=0x80;c&=31;}else if(c>=0xe0&&c<=0xef){k=2;low=0x800;c&=15;}else if(c>=0xf0&&c<=0xf4){k=3;low=0x10000;c&=7;}else return false;if(k>n-i)return false;while(k--){uint8_t b=p[i++];if((b&0xc0)!=0x80)return false;c=(c<<6)|(b&63);}if(c<low||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return false;}return true;}
inline bool copyText(char* out,size_t cap,const uint8_t* p,size_t n){if(!cap||n>=cap||!utf8(p,n))return false;if(n)memcpy(out,p,n);out[n]=0;return true;}
class Writer {
 uint8_t* p_;size_t cap_,n_=0;bool ok_=true;
 bool byte(uint8_t b){if(!ok_||n_>=cap_)return ok_=false;p_[n_++]=b;return true;}
 public:
 Writer(uint8_t* p,size_t n):p_(p),cap_(n){if(!p&&n)ok_=false;}
 bool var(uint64_t v){do{uint8_t b=v&127;v>>=7;if(!byte(b|(v?128:0)))return false;}while(v);return true;}
 bool number(unsigned f,uint32_t v){return var(uint64_t(f)*8)&&var(v);}
 bool fixed(unsigned f,uint32_t v){if(!var(uint64_t(f)*8+5))return false;for(unsigned i=0;i<4;++i)if(!byte(uint8_t(v>>(8*i))))return false;return true;}
 bool bytes(unsigned f,const uint8_t* p,size_t n){if(!ok_||(!p&&n)||!var(uint64_t(f)*8+2)||!var(n)||n>cap_-n_)return ok_=false;if(n)memcpy(p_+n_,p,n);n_+=n;return true;}
 size_t size()const{return ok_?n_:0;}
};
enum class Kind:uint8_t {Ignored,Text,Position,Node,MyNode,ConfigComplete,Queue,Reboot};
struct Decoded {Kind kind=Kind::Ignored;uint32_t from=0,to=0,id=0,number=0,time=0,channel=0,nonce=0;int32_t queueResult=0,latitude=0,longitude=0;uint32_t positionTime=0;bool hasPosition=false;char text[MaxText+1]{};char name[40]{};};
inline bool name(const uint8_t* p,size_t n,char* out,size_t cap){Reader r(p,n);Field f;char shortName[16]{};while(r.next(f)){if(f.number==2){if(f.wire!=2||!copyText(out,cap,f.bytes,f.size))return false;}else if(f.number==3){if(f.wire!=2||!copyText(shortName,sizeof(shortName),f.bytes,f.size))return false;}}if(!r.ok())return false;if(!out[0]&&shortName[0]){size_t k=strlen(shortName);if(k>=cap)return false;memcpy(out,shortName,k+1);}for(char* c=out;*c;++c)if(*c=='\n'||*c=='\r'||*c=='\t')*c=' ';return true;}
inline bool position(const uint8_t* p,size_t n,Decoded& out){Reader r(p,n);Field f;bool lat=false,lon=false;uint32_t time=0,stamp=0;while(r.next(f)){
 if(f.number==1||f.number==2){if(f.wire!=5)return false;int32_t v=int32_t(uint32_t(f.value));if(f.number==1){out.latitude=v;lat=true;}else{out.longitude=v;lon=true;}}
 else if(f.number==4||f.number==7){if(f.wire!=5)return false;if(f.number==4)time=uint32_t(f.value);else stamp=uint32_t(f.value);}
 }if(!r.ok())return false;
 if(lat&&lon){if(out.latitude< -900000000||out.latitude>900000000||out.longitude< -1800000000||out.longitude>1800000000)return false;out.hasPosition=true;out.positionTime=stamp?stamp:time;}
 return true;}
inline bool node(const uint8_t* p,size_t n,Decoded& out){Reader r(p,n);Field f;while(r.next(f)){if(f.number==1){if(f.wire!=0||f.value>UINT32_MAX)return false;out.number=f.value;}else if(f.number==2){if(f.wire!=2||!name(f.bytes,f.size,out.name,sizeof(out.name)))return false;}else if(f.number==3){if(f.wire!=2||!position(f.bytes,f.size,out))return false;}}if(!r.ok())return false;out.kind=out.number?Kind::Node:Kind::Ignored;return true;}
inline bool data(const uint8_t* p,size_t n,Decoded& out){Reader r(p,n);Field f;uint32_t port=0;const uint8_t* payload=nullptr;size_t count=0;while(r.next(f)){if(f.number==1){if(f.wire!=0||f.value>UINT32_MAX)return false;port=f.value;}else if(f.number==2){if(f.wire!=2)return false;payload=f.bytes;count=f.size;}}if(!r.ok())return false;if(port==1&&count){if(!copyText(out.text,sizeof(out.text),payload,count))return false;out.kind=Kind::Text;}else if(port==3){if(!position(payload,count,out))return false;out.kind=out.hasPosition?Kind::Position:Kind::Ignored;}else if(port==4){if(!name(payload,count,out.name,sizeof(out.name)))return false;out.kind=Kind::Node;out.number=out.from;}return true;}
inline bool packet(const uint8_t* p,size_t n,Decoded& out){Reader r(p,n);Field f;const uint8_t* payload=nullptr;size_t size=0;while(r.next(f)){if(f.number==1||f.number==2||f.number==6||f.number==7){if(f.wire!=5)return false;uint32_t v=f.value;if(f.number==1)out.from=v;if(f.number==2)out.to=v;if(f.number==6)out.id=v;if(f.number==7)out.time=v;}else if(f.number==3){if(f.wire!=0||f.value>7)return false;out.channel=f.value;}else if(f.number==4){if(f.wire!=2)return false;payload=f.bytes;size=f.size;}else if(f.number==5){if(f.wire!=2)return false;payload=nullptr;size=0;}}return r.ok()&&(!payload||data(payload,size,out));}
inline bool decode(const uint8_t* p,size_t n,Decoded& out){Decoded result;Reader r(p,n);Field f;while(r.next(f)){if(f.number==2||f.number==3||f.number==4||f.number==7||f.number==8||f.number==11)result={};if(f.number==2){if(f.wire!=2||!packet(f.bytes,f.size,result))return false;}else if(f.number==4){if(f.wire!=2||!node(f.bytes,f.size,result))return false;}else if(f.number==3){if(f.wire!=2)return false;Reader sub(f.bytes,f.size);Field a;while(sub.next(a))if(a.number==1){if(a.wire!=0||a.value>UINT32_MAX)return false;result.number=a.value;}if(!sub.ok())return false;result.kind=Kind::MyNode;}else if(f.number==7){if(f.wire!=0||f.value>UINT32_MAX)return false;result.kind=Kind::ConfigComplete;result.nonce=f.value;}else if(f.number==8){if(f.wire!=0||f.value>1)return false;if(f.value)result.kind=Kind::Reboot;}else if(f.number==11){if(f.wire!=2)return false;Reader sub(f.bytes,f.size);Field a;while(sub.next(a)){if(a.number==1){if(a.wire!=0)return false;result.queueResult=int32_t(a.value);}if(a.number==4){if(a.wire!=0||a.value>UINT32_MAX)return false;result.id=a.value;}}if(!sub.ok())return false;result.kind=Kind::Queue;}}
 if(!r.ok())return false;out=result;return true;
}
inline size_t configuration(uint8_t* out,size_t cap,uint32_t nonce){if(!nonce)return 0;Writer w(out,cap);w.number(3,nonce);return w.size();}
inline size_t text(uint8_t* out,size_t cap,const char* value,size_t n,uint32_t to,uint8_t channel,uint32_t id){if(!n||n>MaxText||!value||!utf8(reinterpret_cast<const uint8_t*>(value),n)||!id||channel>7)return 0;uint8_t payload[MaxText+8],mesh[MaxText+48];Writer d(payload,sizeof(payload));d.number(1,1);d.bytes(2,reinterpret_cast<const uint8_t*>(value),n);if(!d.size())return 0;Writer m(mesh,sizeof(mesh));m.fixed(2,to);if(channel)m.number(3,channel);m.bytes(4,payload,d.size());m.fixed(6,id);m.number(9,3);m.number(10,1);if(!m.size())return 0;Writer w(out,cap);w.bytes(1,mesh,m.size());return w.size();}
}
