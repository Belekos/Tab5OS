#include "Tab5MeshtasticProtocol.h"
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>
using namespace Tab5MeshtasticProtocol;
static unsigned checks=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"FAIL %u\n",checks);abort();}}
static std::vector<uint8_t> hex(const char* s){std::vector<uint8_t> r;unsigned v;int n;while(sscanf(s,"%2x%n",&v,&n)==1){r.push_back(v);s+=n;while(*s==' ')++s;}return r;}
static bool parse(const char* s,Decoded& d){auto v=hex(s);return decode(v.data(),v.size(),d);}
int main(){
 Decoded d;uint8_t out[512]{};
 // Independent golden vectors, matching the published .proto wire numbers.
 check(configuration(out,sizeof(out),0x1234)==3&&!memcmp(out,"\x18\xb4\x24",3));
 check(text(out,sizeof(out),"hi",2,Broadcast,0,0x01020304)==24);
 auto expected=hex("0a 16 15 ff ff ff ff 22 06 08 01 12 02 68 69 35 04 03 02 01 48 03 50 01");
 check(!memcmp(out,expected.data(),expected.size()));
 check(parse("12 17 0d 44 33 22 11 15 ff ff ff ff 22 06 08 01 12 02 68 69 35 04 03 02 01",d));
 check(d.kind==Kind::Text&&d.from==0x11223344&&d.to==Broadcast&&d.id==0x01020304&&!strcmp(d.text,"hi"));
 check(parse("22 0b 08 2a 12 07 12 05 72 61 64 69 6f",d)&&d.kind==Kind::Node&&d.number==42&&!strcmp(d.name,"radio"));
 check(parse("22 0b 08 2a 12 07 12 05 61 0a 62 0d 63",d)&&!strcmp(d.name,"a b c"));
 check(parse("1a 02 08 2a",d)&&d.kind==Kind::MyNode&&d.number==42);
 check(parse("38 b4 24",d)&&d.kind==Kind::ConfigComplete&&d.nonce==0x1234);
 check(parse("5a 04 08 02 20 09",d)&&d.kind==Kind::Queue&&d.queueResult==2&&d.id==9);
 check(parse("40 01",d)&&d.kind==Kind::Reboot);
 check(parse("12 09 0d 2a 00 00 00 2a 02 ff ff",d)&&d.kind==Kind::Ignored); // Encrypted packet ignored.
 check(parse("38 7b a2 06 03 61 62 63 a9 06 00 00 00 00 00 00 00 00 b5 06 11 22 33 44 b8 06 01",d)&&d.nonce==123); // Future fields.
 check(parse("1a 02 08 2a 38 0b",d)&&d.kind==Kind::ConfigComplete&&d.nonce==11&&d.number==0); // Oneof last wins.
 const char* invalid[]={"00","02 00","08 80","0e","ff ff ff ff ff ff ff ff ff 02","12 80 80 80 80 10","12 05 0d 01 02","12 04 08 01 10 01","22 03 08 80 80","12 08 22 06 08 01 12 02 c0 af","12 09 22 07 08 01 12 03 ed a0 80"};
 for(auto s:invalid){Decoded before;before.kind=Kind::Text;before.id=987;strcpy(before.text,"unchanged");d=before;check(!parse(s,d));check(d.kind==before.kind&&d.id==before.id&&!strcmp(d.text,before.text));}
 check(utf8(reinterpret_cast<const uint8_t*>(u8"Été 日本 🌐\n"),strlen(u8"Été 日本 🌐\n")));
 for(auto s:{"c0 af","e0 80 80","f0 80 80 80","ed a0 80","f4 90 80 80","f5 80 80 80","80","c2","00","1b"}){auto v=hex(s);check(!utf8(v.data(),v.size()));}
 std::string max(MaxText,'a');check(text(out,sizeof(out),max.data(),max.size(),Broadcast,7,1)>0);
 max.push_back('x');check(!text(out,sizeof(out),max.data(),max.size(),Broadcast,0,1));
 check(!text(out,sizeof(out),"x",1,Broadcast,8,1));check(!text(out,sizeof(out),"x",1,Broadcast,0,0));
 check(!text(nullptr,512,"x",1,Broadcast,0,1));check(!configuration(nullptr,1,1));
 for(size_t cap=0;cap<24;++cap){memset(out,0xA5,sizeof(out));check(!text(out,cap,"hi",2,Broadcast,0,0x01020304));check(out[cap]==0xA5);}
 // Fuzz random lengths/bytes and every truncation of a valid packet under ASan/UBSan.
 std::mt19937 random(0x0839);uint8_t buf[600];for(unsigned i=0;i<100000;++i){size_t n=random()%sizeof(buf);for(size_t k=0;k<n;++k)buf[k]=random();Decoded before;before.id=0xdeadbeef;d=before;bool ok=decode(buf,n,d);if(!ok)check(d.id==before.id);else check(strlen(d.text)<=MaxText);}
 auto full=hex("12 19 0d 44 33 22 11 15 ff ff ff ff 22 08 08 01 12 04 74 65 73 74 35 04 03 02 01");
 for(size_t n=1;n<full.size();++n)check(!decode(full.data(),n,d));
 std::printf("{\"checks\":%u,\"fuzzPackets\":100000,\"passed\":true}\n",checks);
}
