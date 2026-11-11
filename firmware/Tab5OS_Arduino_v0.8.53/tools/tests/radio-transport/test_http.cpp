#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <optional>
#include <string>
#include <vector>
#include "Tab5RadioBodyReader.h"
#include "Tab5RadioCodec.h"

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
struct String {
  std::string value;
  String() = default;
  String(const char* s): value(s ? s : "") {}
  String(const std::string& s): value(s) {}
  const char* c_str() const { return value.c_str(); }
  bool startsWith(const char* s) const { return value.rfind(s, 0) == 0; }
  int indexOf(const char* s) const { const auto p=value.find(s); return p==std::string::npos ? -1 : static_cast<int>(p); }
  void toLowerCase() { for (char& c:value) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
  int32_t toInt() const { return static_cast<int32_t>(std::strtol(value.c_str(),nullptr,10)); }
};
static uint32_t clockMs;
static bool networkEnabled=true, commandPending=false;
static uint32_t millis() { return clockMs; }
static void vTaskDelay(unsigned ticks) { clockMs+=ticks; }
static bool tab5NetworkEnabled() { return networkEnabled; }
static bool audioCommandPending() { return commandPending; }
constexpr int WL_CONNECTED=3, MALLOC_CAP_INTERNAL=1, HTTPC_STRICT_FOLLOW_REDIRECTS=1;
struct Wifi { int status() { return WL_CONNECTED; } } WiFi;
struct Audio { int wifiRssi() { return -40; } } Tab5Audio;
struct Log { template<class... Args> void printf(const char*,Args...) {} void flush() {} } Tab5DebugSerial;
static size_t heap_caps_get_free_size(int) { return 100*1024; }
static size_t heap_caps_get_largest_free_block(int) { return 80*1024; }
#define TAB5_RADIO_REQUEST_ICY 0
#define TAB5_LOG(...) ((void)0)
struct Fixture {
  std::string bytes, encoding, mime, metaint;
  size_t visible=0, position=0, shortRead=3;
  bool connected=true;
  int status=200;
} fixture;
class NetworkClient {
 public:
  virtual ~NetworkClient()=default;
  virtual int available() { return static_cast<int>(fixture.visible-fixture.position); }
  virtual int read(uint8_t* p,size_t n) {
    n=std::min({n,fixture.visible-fixture.position,fixture.shortRead});
    std::memcpy(p,fixture.bytes.data()+fixture.position,n);fixture.position+=n;return static_cast<int>(n);
  }
  virtual int read() { uint8_t byte;return read(&byte,1)==1 ? byte : -1; }
  virtual uint8_t connected() { return fixture.connected; }
};
class NetworkClientSecure: public NetworkClient {
 public: void setHandshakeTimeout(unsigned) {} int lastError(char*,size_t) { return 0; }
};
using Tab5SecureClient=NetworkClientSecure;
template<class T> class Tab5CooperativeClient: public T {
 public: Tab5CooperativeClient(bool(*)(void*),void*) {}
};
class HTTPClient {
 public:
  bool begin(NetworkClient& c,const String&) { client=&c;return true; }
  void useHTTP10(bool) {} void setFollowRedirects(int) {} void setConnectTimeout(unsigned) {}
  void setTimeout(unsigned) {} void setUserAgent(const char*) {} void addHeader(const char*,const char*) {}
  void collectHeaders(const char** names,size_t n) { for(size_t i=0;i<n;++i) collected.emplace_back(names[i]); }
  int GET() { return fixture.status; }
  static String errorToString(int) { return String("fixture"); }
  String header(const char* name) {
    if(std::find(collected.begin(),collected.end(),name)==collected.end())return String();
    if(!std::strcmp(name,"Transfer-Encoding"))return fixture.encoding;
    if(!std::strcmp(name,"Content-Type"))return fixture.mime;
    if(!std::strcmp(name,"icy-metaint"))return fixture.metaint;
    return String();
  }
  NetworkClient* getStreamPtr() { return client; }
 private: NetworkClient* client=nullptr;std::vector<std::string> collected;
};
class AudioFileSource {
 public:
  virtual ~AudioFileSource()=default;
  virtual bool open(const char*)=0;
  virtual uint32_t read(void*,uint32_t)=0;
  virtual uint32_t readNonBlock(void*,uint32_t)=0;
  virtual bool seek(int32_t,int)=0;
  virtual bool close()=0;
  virtual bool isOpen()=0;
  virtual uint32_t getSize()=0;
  virtual uint32_t getPos()=0;
};
#include "http_source.inc"

// The actual buffered-source read methods must replay the codec probe exactly
// once, including requests crossing the prefix/ring boundary.
#define pdMS_TO_TICKS(value) (value)
class PrefixSource: public AudioFileSource {
 public:
  explicit PrefixSource(const std::string& value) {
    _prefixSize=std::min(value.size(),sizeof(_prefix));
    std::memcpy(_prefix,value.data(),_prefixSize);
    _ring.bytes=value.substr(_prefixSize);
  }
  bool open(const char*) override { return true; }
  bool close() override { return true; }
  bool seek(int32_t,int) override { return false; }
  bool isOpen() override { return _prefixPosition<_prefixSize || _ring.at<_ring.bytes.size(); }
  uint32_t getSize() override { return 0; }
  uint32_t getPos() override { return _position; }
  #include "prefix_source.inc"
 private:
  struct Ring {
    std::string bytes;size_t at=0;
    size_t read(uint8_t* data,size_t length,unsigned =0) {
      length=std::min(length,bytes.size()-at);
      std::memcpy(data,bytes.data()+at,length);at+=length;return length;
    }
  } _ring;
  uint8_t _prefix[16]={};size_t _prefixSize=0,_prefixPosition=0;
  uint32_t _position=0,_refillWaits=0,_shortReads=0;
  bool _running=false,_abort=false;
  static constexpr uint32_t READ_TOTAL_TIMEOUT_MS=8000,CONSUMER_WAIT_MS=5;
};

static std::string chunks(const std::string& body,unsigned size) {
  std::string result;
  for(size_t at=0;at<body.size();at+=size) {
    const auto count=std::min<size_t>(size,body.size()-at);
    char line[40];std::snprintf(line,sizeof(line),"%x;x=test\r\n",static_cast<unsigned>(count));
    result+=line;result.append(body,at,count);result+="\r\n";
  }
  return result+"0\r\nX-Trailer: done\r\n\r\n";
}
static void reset(const std::string& bytes,const char* encoding="",const char* mime="audio/mpeg",const char* metaint="") {
  fixture={};fixture.bytes=bytes;fixture.encoding=encoding;fixture.mime=mime;fixture.metaint=metaint;
  networkEnabled=true;commandPending=false;clockMs=0;
}
static void decode(const std::string& wire,const std::string& expected,const char* encoding,const char* metaint,unsigned fragment) {
  reset(wire,encoding,"Audio/MPEG; charset=binary",metaint);
  Tab5HttpAudioSource source;CHECK(source.open("https://fixture.invalid/live"));
  CHECK(source.codec()==Tab5RadioCodec::Mp3);
  std::string actual;
  for(size_t i=0;i<wire.size()+16;++i) {
    fixture.visible=std::min(wire.size(),fixture.visible+fragment);
    if(fixture.visible==wire.size())fixture.connected=false;
    uint8_t bytes[11];const auto got=source.readNonBlock(bytes,sizeof(bytes));
    CHECK(got<=sizeof(bytes));actual.append(reinterpret_cast<char*>(bytes),got);
    if(fixture.position==wire.size()&&!got)break;
  }
  CHECK(actual==expected);CHECK(source.getPos()==expected.size());CHECK(clockMs==0);
  CHECK(!source.isOpen());CHECK(source.close());CHECK(source.codec()==Tab5RadioCodec::Unknown);
}
int main() {
  std::string audio="abc";audio+=char(0);audio+="def";audio+="\xff";audio+="ghijklmnop";
  std::string icy="abc";icy+=char(1);icy+="StreamTitle='x';";
  icy+="def";icy+=char(0);icy+="ghi";icy+=char(1);icy.append(16,'M');icy+="jkl";icy+=char(0);
  for(bool nonBlock:{false,true}) for(unsigned request:{1u,3u,16u,17u,31u,512u}) {
    PrefixSource prefix(audio+audio+audio);std::string result;
    while(prefix.isOpen()) {
      uint8_t buffer[512];const auto n=nonBlock ? prefix.readNonBlock(buffer,request) : prefix.read(buffer,request);
      CHECK(n>0 && n<=request);result.append(reinterpret_cast<char*>(buffer),n);
    }
    CHECK(result==audio+audio+audio);CHECK(prefix.getPos()==result.size());
  }
  for(unsigned fragment=1;fragment<=17;++fragment) {
    decode(audio,audio,"","",fragment);
    decode(icy,"abcdefghijkl","","3",fragment);
    for(unsigned size=1;size<=19;++size) {
      decode(chunks(audio,size),audio,"Chunked","",fragment);
      decode(chunks(icy,size),"abcdefghijkl","cHuNkEd","3",fragment);
    }
  }
  reset("", "", "audio/aacp");Tab5HttpAudioSource source;
  CHECK(source.open("https://fixture.invalid/opaque"));CHECK(source.codec()==Tab5RadioCodec::Aac);
  uint8_t byte=0;CHECK(source.readNonBlock(&byte,1)==0);CHECK(clockMs==0);
  CHECK(source.read(&byte,1)==0);CHECK(clockMs==5000);
  std::atomic<bool> abort{true};source.setAbortFlag(&abort);const auto before=clockMs;
  CHECK(source.read(&byte,1)==0);CHECK(clockMs==before);source.close();source.setAbortFlag(nullptr);
  reset("ignored","", "text/html");fixture.visible=fixture.bytes.size();
  CHECK(source.open("https://fixture.invalid/error"));CHECK(source.codec()==Tab5RadioCodec::Unsupported);source.close();
  reset("bad");fixture.status=404;CHECK(!source.open("https://fixture.invalid/missing"));
  reset("bad");commandPending=true;CHECK(!source.open("https://fixture.invalid/cancel"));commandPending=false;
  reset("5\r\nabc","chunked");fixture.visible=fixture.bytes.size();fixture.connected=false;
  CHECK(source.open("http://fixture.invalid/truncated"));uint8_t data[10];CHECK(source.read(data,10)==3);
  CHECK(source.read(data,10)==0);CHECK(!source.isOpen());source.close();
  reset("X\r\n","chunked");fixture.visible=fixture.bytes.size();
  CHECK(source.open("http://fixture.invalid/invalid"));CHECK(source.read(data,10)==0);CHECK(!source.isOpen());
  std::printf("RADIO HTTP PASS checks=%u: production HTTP source, chunked/ICY fragmentation, EOF, timeout, cancellation and MIME\n",checks);
}
