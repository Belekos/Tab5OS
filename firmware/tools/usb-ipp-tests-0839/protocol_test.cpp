#include "Tab5UsbIppProtocol.h"
#include <cassert>
#include <string>
#include <iostream>
#include <random>
using namespace Tab5UsbIppProtocol;
static unsigned checks=0;
#define CHECK(x) do{++checks;assert(x);}while(0)
static Error parse(const std::string& text,std::vector<uint8_t>& body,int& status){size_t at=0;return response([&](){return at<text.size()?int(static_cast<unsigned char>(text[at++])):-1;},body,status);}
int main(){
 std::vector<uint8_t> body;int status;
 CHECK(parse("HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nABCD",body,status)==Error::None);CHECK(status==200);CHECK(std::string(body.begin(),body.end())=="ABCD");
 CHECK(parse("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nx",body,status)==Error::None);CHECK(body.size()==1&&body[0]=='x');
 CHECK(parse("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n",body,status)==Error::None);CHECK(status==400&&body.empty());
 CHECK(parse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3;example=x\r\nabc\r\n2\r\nde\r\n0\r\nX-Status: ready\r\n\r\n",body,status)==Error::None);CHECK(std::string(body.begin(),body.end())=="abcde");
 const std::string prefix="HTTP/1.1 200 OK\r\n";
 for(const std::string& headers:{"Content-Length: 2\r\nContent-Length: 2\r\n","Content-Length: 1\r\nTransfer-Encoding: chunked\r\n","Transfer-Encoding: gzip\r\n","Content-Length: -1\r\n","Content-Length: 65537\r\n","Content-Length: 9999999999999999999999999\r\n","Connection: close\r\n"})CHECK(parse(prefix+headers+"\r\nabc",body,status)!=Error::None);
 CHECK(parse(prefix+"Content-Length: 4\r\n\r\nabc",body,status)==Error::Truncated);
 CHECK(parse(prefix+"Content-Length: 1\n\nx",body,status)==Error::Headers);
 CHECK(parse(prefix+"Transfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n",body,status)==Error::Truncated);
 CHECK(parse(prefix+"Transfer-Encoding: chunked\r\n\r\n0\r\nContent-Length: 1\r\n\r\n",body,status)==Error::Framing);
 CHECK(parse(prefix+"Transfer-Encoding: chunked\r\n\r\n10001\r\n",body,status)==Error::TooLarge);
 CHECK(parse(prefix+"Long: "+std::string(600,'a')+"\r\nContent-Length: 0\r\n\r\n",body,status)==Error::Headers);
 CHECK(parse(prefix+"Content-Length: 65536\r\n\r\n"+std::string(65536,'z'),body,status)==Error::None);CHECK(body.size()==65536);
 const uint8_t descriptor[]={9,2,32,0,1,1,0,0x80,50,9,4,2,1,2,7,1,4,0,7,5,0x81,2,0,2,0,7,5,0x02,2,0,2,0};
 auto found=interface(descriptor,sizeof(descriptor));CHECK(found.valid);CHECK(found.number==2&&found.alternate==1&&found.in==0x81&&found.out==2&&found.packetIn==512);
 for(size_t n=0;n<sizeof(descriptor);++n)CHECK(!interface(descriptor,n).valid);
 auto data=std::vector<uint8_t>(descriptor,descriptor+sizeof(descriptor));data[16]=2;CHECK(!interface(data.data(),data.size()).valid);data[16]=4;data[22]=3;CHECK(!interface(data.data(),data.size()).valid);
 std::mt19937 random(39);
 for(unsigned i=0;i<20000;++i){std::string s;const unsigned len=random()%256;for(unsigned j=0;j<len;++j)s+=char(random()%256);size_t n=0;auto e=response([&](){CHECK(n<=s.size());return n<s.size()?int(static_cast<unsigned char>(s[n++])):-1;},body,status);CHECK(body.size()<=65536);(void)e;auto d=interface(reinterpret_cast<const uint8_t*>(s.data()),s.size());CHECK(!d.valid||d.in&&d.out&&d.packetIn<=512);}
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<"}\n";
}
