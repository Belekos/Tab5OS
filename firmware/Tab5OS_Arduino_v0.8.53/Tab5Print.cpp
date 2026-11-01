#include "Tab5Serial.h"
#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5Task.h"
#include "Tab5ImageDecode.h"
#include "Tab5HttpUpgrade.h"
#include "Tab5Localization.h"
#include "Tab5Print.h"
#include "Tab5UsbPrinter.h"
#include "Tab5Diagnostics.h"
#include "Tab5PrintProtocol.h"
#include "Tab5PrintText.h"
#include "Tab5PrintPolicy.h"
#include "Tab5Cooperate.h"
#include "Tab5PrintFontCheck.h"
#include "Tab5Memory.h"
#include "Tab5NetworkGate.h"
#include "WriterRich.h"
#include <FS.h>
#include <SD.h>
#include "Tab5Audio.h"
#include <M5GFX.h>
#include <M5Unified.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include "Tab5PlainSocket.h"
#include <HTTPClient.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <memory>
// Font raster scratch is ordinary data. Never let per-glyph malloc consume
// internal RAM that TCP/TLS and task stacks require.
namespace {
struct PrintFontScratch { bool failed=false; };
void* printFontAllocate(size_t bytes,void* context){
 void* p=heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!p&&bytes&&context)static_cast<PrintFontScratch*>(context)->failed=true;
 return p;
}
}
#define STBTT_malloc(bytes,userdata) printFontAllocate((bytes),(userdata))
// An expandable pool of 32 active edges avoids reserving 800 entries per
// glyph. Further chunks are allocated only for unusually complex outlines.
#define STBTT_HEAP_FACTOR_SIZE_32 32
#define STBTT_HEAP_FACTOR_SIZE_128 32
#define STBTT_HEAP_FACTOR_SIZE_DEFAULT 32
#define STBTT_free(pointer,userdata) heap_caps_free(pointer)
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "src/print/stb_truetype.h"
namespace {
std::atomic<bool> working{false},cancelled{false};std::atomic<unsigned> percent{0};
std::atomic<const char*> state{"Prêt"};char details[512]{};portMUX_TYPE detailMux=portMUX_INITIALIZER_UNLOCKED;
Tab5Print::Options options;bool probing=false;std::atomic<bool> measuring{false};std::atomic<unsigned> reportedPages{0};uint32_t requestId=1;unsigned pageCount=0;uint32_t usbPrinterToken=0;
void setDetail(const String& s){portENTER_CRITICAL(&detailMux);snprintf(details,sizeof(details),"%s",s.c_str());portEXIT_CRITICAL(&detailMux);}
// Trust is bound to a host:port and explicitly accepted in the print screen.
// A first-contact TLS handshake retrieves only the certificate, never a document.
struct TrustedPrinter {char host[256]{};uint16_t port=0;uint8_t fingerprint[32]{};};
String upgradeUri;
TrustedPrinter pendingCertificate;std::atomic<bool> pendingTrust{false};
bool readPin(const tab5print::Endpoint& e,uint8_t out[32]){
 Preferences p;if(!p.begin("tab5print",true))return false;
 for(unsigned i=0;i<8;++i){char key[8];snprintf(key,sizeof(key),"tls%u",i);TrustedPrinter entry;
  if(p.getBytesLength(key)==sizeof(entry)&&p.getBytes(key,&entry,sizeof(entry))==sizeof(entry)&&memchr(entry.host,0,sizeof(entry.host))&&entry.port==e.port&&e.host==entry.host){memcpy(out,entry.fingerprint,32);return true;}}
 return false;
}
// readStringUntil() uses Stream::timedRead(), which busy-polls read() in
// this core. A fragmented HTTP header must also let the idle task run.
bool printerReadReady(NetworkClient& client,Tab5TimeSlice& slice,size_t bytes){
 if(bytes&&slice.due(millis()))vTaskDelay(1);
 if(cancelled){client.setTimeout(0);client.stop();return false;}
 return true;
}
class PlainPrinterClient:public NetworkClient {
 Tab5TimeSlice receiveSlice;
 public:
 int read(uint8_t* out,size_t n)override{return printerReadReady(*this,receiveSlice,n)?NetworkClient::read(out,n):-1;}
 int read()override{uint8_t byte;return read(&byte,1)==1?byte:-1;}
 int available()override{return printerReadReady(*this,receiveSlice,0)?NetworkClient::available():0;}
 uint8_t connected()override{return printerReadReady(*this,receiveSlice,0)?NetworkClient::connected():0;}
};
class PinnedPrinterClient:public NetworkClientSecure {
 Tab5TimeSlice receiveSlice;
 public:uint8_t expected[32]{};bool pin=false,mismatch=false,upgrade=false,established=false;int upgradeStage=0,upgradeHttp=0;bool rawPhase=false;Tab5Resources::Reservation* startup=nullptr;
 int available()override{if(!printerReadReady(*this,receiveSlice,0))return 0;return rawPhase?tab5socket::available(fd()):NetworkClientSecure::available();}
 int read(uint8_t* p,size_t n)override{if(!printerReadReady(*this,receiveSlice,n))return -1;return rawPhase?tab5socket::read(fd(),p,n):NetworkClientSecure::read(p,n);}
 int read()override{uint8_t c;return read(&c,1)==1?c:-1;}
 size_t write(const uint8_t* p,size_t n)override{return rawPhase?tab5socket::write(fd(),p,n):NetworkClientSecure::write(p,n);}
 uint8_t connected()override{if(!printerReadReady(*this,receiveSlice,0))return 0;return rawPhase?tab5socket::connected(fd()):NetworkClientSecure::connected();}
 void stop()override{rawPhase=false;NetworkClientSecure::stop();}
 String diagnostic(){if(upgradeStage==1)return String("HTTP Upgrade · ")+(upgradeHttp?String("HTTP ")+upgradeHttp:Tab5Locale.text("Réponse absente ou incomplète"));char error[128]{};int code=lastError(error,sizeof(error));return String("TLS ")+code+" · #"+upgradeStage+" · "+(code<0?error:Tab5Locale.text("Connexion interrompue"));}
 int connect(const char* host,uint16_t port,int32_t timeout)override{
  rawPhase=false;upgradeStage=upgradeHttp=0;mismatch=false;established=false;
  if(upgrade)setPlainStart();
  // The core's 3-argument overload calls the virtual 2-argument overload.
  // Calling it here re-enters this subclass forever. Bypass that dispatch.
  setConnectionTimeout(timeout);
  if(!NetworkClientSecure::connect(host,port))return 0;
  auto clock=[](){return millis();};auto pause=[](){delay(1);};auto cancel=[](){return cancelled.load();};
  if(upgrade){
   upgradeStage=1;rawPhase=true;
   String request=String("OPTIONS * HTTP/1.1\r\nHost: ")+host+":"+String(port)+"\r\nConnection: Upgrade\r\nUpgrade: TLS/1.0, HTTP/1.1\r\nContent-Length: 0\r\n\r\n";
   size_t sent=0;uint32_t start=millis();while(sent<request.length()&&millis()-start<5000&&!cancelled){size_t n=write((const uint8_t*)request.c_str()+sent,request.length()-sent);if(!n){delay(1);continue;}sent+=n;}
   tab5http::Reply reply;
   bool received=sent==request.length()&&tab5http::headers(*this,reply,clock,pause,cancel);upgradeHttp=reply.status;
   if(!received||reply.status!=101||!reply.upgrade||!reply.connectionUpgrade){stop();return 0;}
   rawPhase=false;upgradeStage=2;if(!startTLS()){stop();return 0;}
  }
  if(pin){uint8_t got[32];unsigned diff=0;if(!getFingerprintSHA256(got))diff=1;else for(unsigned i=0;i<32;++i)diff|=got[i]^expected[i];
   if(diff){mismatch=true;stop();return 0;}}
  if(upgrade){
   upgradeStage=3;tab5http::Reply reply;
   if(!tab5http::headers(*this,reply,clock,pause,cancel)||reply.status<200||reply.status>=300||!tab5http::drain(*this,reply,clock,pause,cancel)){stop();return 0;}
   upgradeStage=0;
  }
  established=true;if(startup)startup->release();return 1;
 }
 int connect(const char* host,uint16_t port)override{return connect(host,port,5000);}
};
// Arduino HTTPClient only calls delay(0) after a successful upload block.
// That does not block this priority-1 task and therefore does not let Idle0
// service its watchdog. Cooperate here even when SD and the socket stay ready.
class Payload:public Stream {
 const std::vector<uint8_t>& prefix;File file;NetworkClient* connection=nullptr;
 size_t prefixPosition=0,filePosition=0,fileLength=0;
 bool sourceValid=true,sourceFailed=false,printing=false,completeReported=false;
 Tab5TimeSlice slice;uint32_t lastReport=0;
 void abortTransfer(){if(connection){connection->setTimeout(0);connection->stop();}}
 void report(){
  if(!printing)return;
  const uint32_t now=millis();if(now-lastReport<500)return;lastReport=now;
  const size_t consumed=prefixPosition+filePosition;
  percent=std::min<unsigned>(99,unsigned(uint64_t(consumed)*100/size()));
  char text[112];snprintf(text,sizeof(text),"Lecture du travail : %u / %u Kio",unsigned(consumed/1024),unsigned((size()+1023)/1024));
  setDetail(text);tab5DiagnosticMark(2,"print-stream",text,1);
 }
 public:
 Payload(const std::vector<uint8_t>& h,const String& path,bool printJob=false):prefix(h),printing(printJob){
  if(path.length()){file=tab5FS.open(path);sourceValid=bool(file);if(sourceValid)fileLength=file.size();sourceValid=sourceValid&&fileLength>0&&fileLength<=128U*1024U*1024U;}
 }
 size_t size()const{return prefix.size()+fileLength;}
 void bind(NetworkClient& client){connection=&client;}
 bool valid()const{return sourceValid;}
 bool failed()const{return sourceFailed;}
 int available()override{
  // HTTPClient calls available() twice. A negative value racing with an
  // interruption on its second call becomes an enormous size_t read.
  // Close this task's own socket and return zero on both calls instead.
  if(cancelled||sourceFailed||!sourceValid){abortTransfer();return 0;}
  const size_t remaining=prefix.size()-prefixPosition+fileLength-filePosition;
  if(!remaining&&printing&&!completeReported){
   completeReported=true;percent=99;state="Attente de la réponse de l’imprimante…";
   tab5DiagnosticMark(2,"print-response","corps HTTP transmis, attente IPP",1);
  }
  return int(remaining); // PWG file limit is 128 MiB, well below INT_MAX.
 }
 int read()override{uint8_t byte;return readBytes(reinterpret_cast<char*>(&byte),1)==1?byte:-1;}
 int peek()override{return cancelled||sourceFailed?-1:prefixPosition<prefix.size()?prefix[prefixPosition]:filePosition<fileLength?file.peek():-1;}
 size_t readBytes(char* out,size_t n)override{
  if(slice.due(millis()))vTaskDelay(1);
  if(cancelled||sourceFailed||!sourceValid){abortTransfer();return 0;}
  if(n>HTTP_TCP_TX_BUFFER_SIZE||n>size()-prefixPosition-filePosition){sourceFailed=true;abortTransfer();return 0;}
  const size_t headerBytes=std::min(n,prefix.size()-prefixPosition);
  if(headerBytes)memcpy(out,prefix.data()+prefixPosition,headerBytes);
  prefixPosition+=headerBytes;size_t got=headerBytes;
  const size_t wanted=std::min(n-got,fileLength-filePosition);
  if(wanted){
   const size_t count=file.read(reinterpret_cast<uint8_t*>(out)+got,wanted);
   // File::read can return size_t(-1) for an invalid descriptor. Never
   // expose it as a huge write length; a short read terminates this request.
   // HTTPClient's short-write retry uses the requested read length.
   // Returning a partial block could make it resend uninitialised bytes.
   // A block is therefore complete or not sent at all.
   if(count!=wanted){sourceFailed=true;abortTransfer();return 0;}
   filePosition+=count;got+=count;
  }
  if(cancelled){abortTransfer();return 0;}
  report();return got;
 }
 size_t write(uint8_t)override{return 0;}
};
class ResponseSink:public Stream {
 Tab5TimeSlice slice;
 public:std::vector<uint8_t> bytes;bool overflow=false;
 int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}
 size_t write(uint8_t b)override{return write(&b,1);}
 size_t write(const uint8_t* p,size_t n)override{
  if(slice.due(millis()))vTaskDelay(1);
  if(cancelled)return 0;
  if(n>65536-bytes.size()){overflow=true;return 0;}bytes.insert(bytes.end(),p,p+n);return n;
 }
};

void memoryFailure(const Tab5Resources::AdmissionReport& report) {
 using Tab5Resources::Denial;
 switch(report.reason){
  case Denial::Internal:state="Impression : RAM interne libre insuffisante";break;
  case Denial::InternalBlock:state="Impression : RAM interne fragmentée";break;
  case Denial::DmaBlock:state="Impression : bloc DMA insuffisant";break;
  case Denial::Psram:state="Impression : PSRAM libre insuffisante";break;
  case Denial::PsramBlock:state="Impression : PSRAM fragmentée";break;
  case Denial::AdmissionBusy:case Denial::SlotsFull:state="Autre démarrage en cours : réessayez";break;
  default:state="Allocation de la pile d’impression impossible";break;
 }
 char text[512];Tab5Resources::describeAdmission(report,text,sizeof(text));setDetail(text);
 Tab5DebugSerial.printf("[PRINT][MEM] %s\n",text);
}
Tab5Resources::Reservation reserveTls(Tab5AudioPriorityLease& priority) {
 const char* phase=state.load();Tab5Resources::AdmissionReport report;
 state="Libération des caches pour l’impression…";
 auto memory=tab5ReserveMemory(Tab5Resources::Owner::Tools,Tab5Resources::tlsBudget(),"TLS impression",&report);
 if(!memory&&!cancelled&&priority.tryAcquire())
  memory=tab5ReserveMemory(Tab5Resources::Owner::Tools,Tab5Resources::tlsBudget(),"TLS impression prioritaire",&report);
 if(memory){state=phase;return memory;}
 if(cancelled)state="Opération annulée";else memoryFailure(report);
 return {};
}
std::unique_ptr<Tab5NetworkLease> networkSession(const char* owner,Tab5AudioPriorityLease& priority){
 auto stopping=[](void*){return cancelled.load();};
 auto lease=std::unique_ptr<Tab5NetworkLease>(new Tab5NetworkLease(owner,1500,stopping));
 if(*lease)return lease;
 lease.reset();
 // A radio owns its socket on the audio task. It must close that socket
 // itself before this task takes the network gate; never cross-free it.
 if(!cancelled&&priority.tryAcquire()){
  lease.reset(new Tab5NetworkLease(owner,1500,stopping));
  if(*lease)return lease;
 }
 state=cancelled?"Opération annulée":"Connexion réseau momentanément indisponible";
 return {};
}

bool exchange(const tab5print::Endpoint& endpoint,uint16_t op,const char* format,const String& path,const char* media,int dpi,tab5print::Response& response,Tab5Resources::Reservation& tlsAdmission,bool& requestUpgrade,bool allowUpgrade){
 auto header=tab5print::request(op,requestId,endpoint.uri,format,media,dpi,0,options.landscape?4:3);
 Payload body(header,path,op==2);if(!body.valid()){state="Lecture du fichier impossible";return false;}
 PlainPrinterClient plain;PinnedPrinterClient secure;NetworkClient* client=&plain;String certificate;
 bool upgraded=upgradeUri==endpoint.uri.c_str();secure.upgrade=upgraded;
 if(endpoint.tls||upgraded){
  tab5DiagnosticMark(2,"print-tls","negociation imprimante",1);
  if(!tlsAdmission)return false;
  secure.startup=&tlsAdmission;
 }
 if(endpoint.tls||upgraded){
  secure.setHandshakeTimeout(15);File cert=tab5FS.open("/System/printer-ca.pem");
  if(cert&&cert.size()>0&&cert.size()<=16384){certificate=cert.readString();secure.setCACert(certificate.c_str());}
  else if(readPin(endpoint,secure.expected)){secure.pin=true;secure.setInsecure();} // Pin checked before any HTTP bytes.
  else {
   secure.setInsecure(); // Certificate discovery only; no print data on this connection; OPTIONS metadata only.
   if(!secure.connect(endpoint.host.c_str(),endpoint.port,5000)||!secure.getFingerprintSHA256(pendingCertificate.fingerprint)){state=secure.upgradeStage==1?"Passage HTTP vers TLS refusé":"Négociation TLS de l’imprimante impossible";setDetail(secure.diagnostic());return false;}
   snprintf(pendingCertificate.host,sizeof(pendingCertificate.host),"%s",endpoint.host.c_str());pendingCertificate.port=endpoint.port;secure.stop();
   String fingerprint=String(endpoint.host.c_str())+":"+endpoint.port+"\nSHA-256: ";
   for(unsigned i=0;i<32;++i){char byte[4];snprintf(byte,sizeof(byte),"%02X",pendingCertificate.fingerprint[i]);if(i&&i%16==0)fingerprint+='\n';else if(i)fingerprint+=':';fingerprint+=byte;}
   setDetail(fingerprint);state="Vérifier l’empreinte du certificat avant de faire confiance";pendingTrust=true;return false;
  }
  client=&secure;
 }
 body.bind(*client);
 HTTPClient http;String url=String(endpoint.tls?"https://":"http://")+endpoint.host.c_str()+":"+endpoint.port+endpoint.path.c_str();
 http.setConnectTimeout(5000);http.setTimeout(tab5print::responseTimeout(op==2));http.setReuse(false);
 if(!http.begin(*client,url)){state="Connexion à l’imprimante impossible";return false;}
 const char* headers[]={"Upgrade","Location"};http.collectHeaders(headers,2);
 tab5DiagnosticMark(2,op==2?"print-send":"print-query",op==2?"transfert du fichier":"capacites imprimante",1);
 http.addHeader("Content-Type","application/ipp");int code=http.sendRequest("POST",&body,body.size());
 ResponseSink sink;const int responseBytes=code==200?http.writeToStream(&sink):0;String upgrade=http.header("Upgrade");http.end();
 if(cancelled){state=op==2?"Envoi interrompu : vérifier la file de l’imprimante":"Opération annulée";return false;}
 if(body.failed()){state=op==2?"Lecture SD interrompue : vérifier la file de l’imprimante":"Lecture du fichier interrompue";return false;}
 if(code==426){
  String advertised=upgrade;advertised.toLowerCase();
  // Only an idempotent capabilities query is retried automatically, never a print job.
  if(allowUpgrade&&op==0x000b&&!endpoint.tls&&advertised.indexOf("tls/")>=0){upgradeUri=endpoint.uri.c_str();requestUpgrade=true;return false;}
  state="HTTP 426 : rechercher l’adresse IPPS de l’imprimante";setDetail(String("HTTP 426 · Upgrade: ")+(upgrade.length()?upgrade:String("—")));return false;}
 if(secure.mismatch){state="Certificat modifié : connexion bloquée";return false;}
 if(code<0){state=secure.upgradeStage==1?"Passage HTTP vers TLS refusé":tab5print::transportFailure(code,endpoint.tls||upgraded,secure.established,op==2);setDetail(String("HTTP ")+code+" · "+secure.diagnostic());
#ifdef ARDUINO
  Tab5DebugSerial.printf("[PRINT] HTTP=%d, TLS etabli=%d, operation=%u\n",code,int(secure.established),unsigned(op));tab5LogMemory("imprimante: echec transport");
#endif
  return false;}
 if(code!=200||responseBytes<0||sink.overflow||!tab5print::parse(sink.bytes.data(),sink.bytes.size(),requestId++,response)){state="Réponse de l’imprimante invalide";setDetail(String("HTTP ")+code);return false;}
 if(response.status>0x00ff){state="Travail refusé par l’imprimante";setDetail(String("IPP 0x")+String(response.status,HEX));return false;}return true;
}
bool exchangeSession(const tab5print::Endpoint& endpoint,uint16_t op,const char* format,const String& path,const char* media,int dpi,tab5print::Response& response,Tab5AudioPriorityLease& priority){
 if(Tab5UsbPrinter::uri(options.uri.c_str())){
  auto header=tab5print::request(op,requestId,"ipp://localhost/ipp/print",format,media,dpi,0,options.landscape?4:3);
  Payload payload(header,path,op==2);if(!payload.valid()){state="Lecture du fichier impossible";return false;}
  Tab5UsbPrinter::Response bytes;int code=0;
  if(!Tab5UsbPrinter::post(payload,payload.size(),bytes,code,usbPrinterToken,tab5print::responseTimeout(op==2),[](void*){return cancelled.load();},nullptr)){
   state=cancelled?(op==2?"Envoi interrompu : vérifier la file de l’imprimante":"Opération annulée"):Tab5UsbPrinter::status();return false;
  }
  if(code!=200||!tab5print::parse(bytes.data(),bytes.size(),requestId++,response)){state="Réponse de l’imprimante invalide";setDetail(String("HTTP ")+code);return false;}
  if(response.status>0x00ff){state="Travail refusé par l’imprimante";setDetail(String("IPP 0x")+String(response.status,HEX));return false;}
  return true;
 }
 for(unsigned attempt=0;attempt<2&&!cancelled;++attempt){
  Tab5Resources::Reservation admission;
  // Recover memory and ask the audio owner to suspend BEFORE taking the
  // network gate. The audio task may need that gate to finish its command.
  if(endpoint.tls||upgradeUri==endpoint.uri.c_str()){
   admission=reserveTls(priority);if(!admission)return false;
  }
  bool upgrade=false;
  {
   auto lease=networkSession(op==2?"print-send":"print-query",priority);if(!lease)return false;
   if(exchange(endpoint,op,format,path,media,dpi,response,admission,upgrade,attempt==0))return true;
  }
  if(!upgrade)return false;
 }
 return false;
}

struct Font {
 uint8_t* data=nullptr;stbtt_fontinfo info{};PrintFontScratch scratch;
 ~Font(){heap_caps_free(data);}
 bool load(const char* path,bool required=false){
  File f=tab5FS.open(path);if(!f||f.size()<100||f.size()>2*1024*1024)return false;
  const size_t n=f.size();Tab5Resources::AdmissionReport report;
  auto admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,{0,0,0,n,n},"Police d’impression",&report);
  if(!admission){if(required)memoryFailure(report);return false;}
  data=(uint8_t*)heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  admission.release(); // The actual font allocation is in the next heap sample.
  if(!data){if(required)state="Allocation PSRAM de la police impossible";return false;}
  tab5DiagnosticMark(2,"print-font",path,1);
  // A slow card must not monopolize the worker core through a single
  // multi-megabyte fread. Bound each read and let idle/audio tasks run.
  for(size_t read=0;read<n;){
   const size_t amount=std::min<size_t>(4096,n-read);
   if(cancelled||f.read(data+read,amount)!=amount){heap_caps_free(data);data=nullptr;return false;}
   read+=amount;vTaskDelay(1);
  }
  tab5DiagnosticMark(2,"print-font-check",path,1);
  const auto checked=tab5print::checkTrueType(data,n,[](){vTaskDelay(1);});
  if(!checked||!stbtt_InitFont(&info,data,checked.offset)){
   if(required)state=checked.error?checked.error:"Police TrueType non prise en charge";
   heap_caps_free(data);data=nullptr;return false;
  }
  info.userdata=&scratch;
  return true;
 }
};
class PrintFonts {
 Font fonts[4];bool attempted[4]{};
 public:
 bool begin(){attempted[0]=true;return fonts[0].load("/System/font.ttf",true);}
 Font& get(unsigned style){
  style&=3;
  if(!attempted[style]){
   static const char* paths[]={"/System/font.ttf","/System/fonts/bold.ttf","/System/fonts/italic.ttf","/System/fonts/bolditalic.ttf"};
   attempted[style]=true;fonts[style].load(paths[style]);
  }
  return fonts[style].data?fonts[style]:fonts[0];
 }
};
class PrintBytes {
 uint8_t* bytes=nullptr;
 public:
 explicit PrintBytes(size_t n){if(n){
  Tab5Resources::AdmissionReport report;
  auto admission=tab5ReserveMemory(Tab5Resources::Owner::Tools,{0,0,0,n,n},"Tampon d’impression",&report);
  if(!admission){memoryFailure(report);return;}
  bytes=static_cast<uint8_t*>(heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 }}
 ~PrintBytes(){heap_caps_free(bytes);}
 PrintBytes(const PrintBytes&)=delete;PrintBytes& operator=(const PrintBytes&)=delete;
 uint8_t* data(){return bytes;}explicit operator bool()const{return bytes!=nullptr;}
};
class Raster {
 public:File out;bool measure=false;tab5print::Layout layout;const char* media;unsigned y=0,pages=0;bool opened=false;PrintBytes row,encoded;size_t encodedCapacity;
 Raster(const String& path,tab5print::Layout p,const char* m):measure(measuring.load()),layout(p),media(m),row(measure?0:p.width*3),encoded(measure?0:tab5print::rasterRowCapacity(p.width)),encodedCapacity(tab5print::rasterRowCapacity(p.width)){if(!measure&&row&&encoded){memset(row.data(),255,p.width*3);out=tab5FS.open(path,FILE_WRITE);if(out)out.write((const uint8_t*)"RaS2",4);}}
 bool line(const uint8_t* rgb){if(cancelled||!out||out.size()>128*1024*1024)return false;const size_t n=tab5print::encodeRasterRow(rgb,layout.width,encoded.data(),encodedCapacity);if(!n||out.write(encoded.data(),n)!=n)return false;++y;if((y&15)==0){percent=std::min<unsigned>(95,y*95/layout.height);vTaskDelay(1);}return true;}
 bool blank(unsigned n){if(measure){if(cancelled)return false;y+=n;return true;}if(!row||!encoded||!out)return false;memset(row.data(),255,layout.width*3);while(n--)if(!line(row.data()))return false;return true;}
 bool begin(){if(pages>=256)return false;if(!measure){if(!row||!encoded||!out)return false;auto h=tab5print::rasterHeader(layout,0,media);if(out.write(h.data(),h.size())!=h.size())return false;}opened=true;y=0;++pages;pageCount=pages;return blank(layout.margin);}
 bool finish(){if(!opened)return true;if(!blank(layout.height-y))return false;opened=false;return true;}
 bool room(unsigned height){if(height>layout.height-layout.margin*2)return false;if(!opened&&!begin())return false;if(y+height>layout.height-layout.margin){if(!finish()||!begin())return false;}return true;}
};
double textPointSize(unsigned heading){static const double points[]={11,20,17,15,13,12,11};return points[std::min(heading,6u)];}
bool glyphLine(Raster& raster,const writing::ExternalVector<writing::RichGlyph>& line,PrintFonts& fonts,unsigned heading){
 const unsigned fontPx=std::lround(raster.layout.dpi*textPointSize(heading)*options.scalePercent/7200.0);
 const unsigned height=unsigned(fontPx*1.45)+2;
 if(!raster.room(height))return false;
 if(raster.measure||line.empty())return raster.blank(height);
 // Only retain columns which contain ink/background. A five-byte word
 // does not need an A4-wide colour band; the reusable PWG row fills the rest.
 unsigned width=raster.layout.margin;int right=raster.layout.margin;
 for(auto& g:line){Font& font=fonts.get(g.style);const float scale=stbtt_ScaleForMappingEmToPixels(&font.info,fontPx);
  int advance,lsb,x0,y0,x1,y1;stbtt_GetCodepointHMetrics(&font.info,g.code,&advance,&lsb);
  const int step=std::max(1,int(std::lround(advance*scale)));
  stbtt_GetCodepointBitmapBox(&font.info,g.code,scale,scale,&x0,&y0,&x1,&y1);
  width=std::max(width,unsigned(std::max(0,std::min<int>(raster.layout.width,right+std::max(step,x1)))));right+=step;
 }
 tab5DiagnosticMark(2,"print-glyph","rasterisation TrueType",1);
 const size_t bandBytes=size_t(width)*height*3;
 Tab5Resources::AdmissionReport bandReport;
 auto bandAdmission=tab5ReserveMemory(Tab5Resources::Owner::Tools,{0,0,0,bandBytes,bandBytes},"Ligne d’impression",&bandReport);
 if(!bandAdmission){memoryFailure(bandReport);return false;}
 auto* band=(uint8_t*)heap_caps_malloc(bandBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);bandAdmission.release();
 if(!band){state="PSRAM insuffisante pour préparer une ligne d’impression";return false;}
 memset(band,255,bandBytes);int x=raster.layout.margin;unsigned glyphs=0;
 for(auto& g:line){Font& font=fonts.get(g.style);float scale=stbtt_ScaleForMappingEmToPixels(&font.info,fontPx);
  int advance,lsb,bw,bh,dx,dy;stbtt_GetCodepointHMetrics(&font.info,g.code,&advance,&lsb);int step=std::max(1,int(std::lround(advance*scale)));
  uint32_t fg=g.foreground==writing::NoColor?0:g.foreground,bg=g.background;
  if(bg!=writing::NoColor)for(unsigned yy=0;yy<height;++yy)for(int xx=x;xx<std::min<int>(x+step,width);++xx){size_t p=(yy*width+xx)*3;band[p]=bg>>16;band[p+1]=bg>>8;band[p+2]=bg;}
  uint8_t* bitmap=stbtt_GetCodepointBitmap(&font.info,scale,scale,g.code,&bw,&bh,&dx,&dy);
  if(font.scratch.failed){stbtt_FreeBitmap(bitmap,&font.scratch);heap_caps_free(band);state="Mémoire insuffisante pour le rendu de la police";return false;}
  if(bitmap){for(int yy=0;yy<bh;++yy)for(int xx=0;xx<bw;++xx){int bx=x+dx+xx,by=int(fontPx)+dy+yy;if(bx<0||bx>=int(width)||by<0||by>=int(height))continue;unsigned a=bitmap[yy*bw+xx];size_t p=(by*width+bx)*3;for(int c=0;c<3;++c)band[p+c]=(band[p+c]*(255-a)+((fg>>(16-c*8))&255)*a)/255;}stbtt_FreeBitmap(bitmap,nullptr);}
  x+=step;if((++glyphs&7)==0)vTaskDelay(1);
 }
 tab5DiagnosticMark(2,"print-spool","ecriture des lignes",1);
 memset(raster.row.data(),255,size_t(raster.layout.width)*3);
 bool ok=true;for(unsigned i=0;i<height&&ok;++i){memcpy(raster.row.data(),band+size_t(i)*width*3,size_t(width)*3);ok=raster.line(raster.row.data());}heap_caps_free(band);return ok;
}
bool renderImage(Raster& raster,const String& path);
bool renderText(Raster& raster,const String& path){
 File file=tab5FS.open(path);if(!file){state="Lecture du document impossible";return false;}
 if(file.size()>2*1024*1024){state="Document trop volumineux pour l’impression (limite : 2 Mio)";return false;}
 const uint32_t end=options.textEnd?options.textEnd:uint32_t(file.size());
 if((options.textSourceSize&&options.textSourceSize!=file.size())||options.textStart>end||end>file.size()||!file.seek(options.textStart)){
  state="Document modifié : rouvrir le choix de la section";return false;
 }
 PrintFonts fonts;if(!fonts.begin())return false;
 tab5DiagnosticMark(2,"print-text",path.c_str(),1);
 String lower=path;lower.toLowerCase();bool markdown=lower.endsWith(".md")||lower.endsWith(".markdown");tab5print::MarkdownFence fence;
 PrintBytes readBuffer(4096);if(!readBuffer){state="Allocation du tampon de lecture impossible";return false;}size_t count=0,index=0;std::string source;bool readFailed=false;
 auto get=[&]()->int{if(index==count){size_t remaining=end-file.position();count=remaining?file.read(readBuffer.data(),std::min<size_t>(4096,remaining)):0;index=0;if(remaining&&!count)readFailed=true;}return count?uint8_t(readBuffer.data()[index++]):-1;};
 for(;;){source.clear();int c;while((c=get())>=0&&c!='\n'){if(c!='\r')source+=char(c);if(source.size()>32768){state="Paragraphe trop long pour l’impression : ajouter un saut de ligne (32 Kio)";return false;}}
  if(readFailed){state="Lecture du document interrompue";return false;}
  if(c<0&&source.empty())break;
  if(markdown&&fence.line(source.data(),source.size()))continue;
  if(markdown&&!fence.inside()){
   // Match the menu's ATX headings, including indentation and closing hashes.
   size_t indent=0;while(indent<source.size()&&indent<4&&source[indent]==' ')++indent;
   size_t hashes=indent;while(hashes<source.size()&&source[hashes]=='#')++hashes;
   if(indent<4&&hashes>indent&&hashes-indent<=6&&(hashes==source.size()||source[hashes]==' '||source[hashes]=='\t')){
    size_t title=hashes;while(title<source.size()&&(source[title]==' '||source[title]=='\t'))++title;
    size_t stop=source.size();while(stop>title&&(source[stop-1]==' '||source[stop-1]=='\t'))--stop;
    size_t closing=stop;while(closing>title&&source[closing-1]=='#')--closing;
    if(closing<stop&&(closing==title||source[closing-1]==' '||source[closing-1]=='\t')){stop=closing;while(stop>title&&(source[stop-1]==' '||source[stop-1]=='\t'))--stop;}
    source=std::string(hashes-indent,'#')+" "+source.substr(title,stop-title);
   }
  }
  // RichText grows glyph vectors geometrically. Account for old+new arrays
  // during a reallocation before parsing an unusually long paragraph.
  const size_t paragraphBytes=(source.size()+1)*sizeof(writing::RichGlyph)*2+8192;
  Tab5Resources::AdmissionReport paragraphReport;
  auto paragraphAdmission=tab5ReserveMemory(Tab5Resources::Owner::Tools,{0,0,0,paragraphBytes,paragraphBytes/2},"Paragraphe d’impression",&paragraphReport);
  if(!paragraphAdmission){memoryFailure(paragraphReport);return false;}
  tab5DiagnosticMark(2,"print-layout","mise en page du texte",1);
  writing::RichText rich;rich.parse(source,!markdown||fence.inside());paragraphAdmission.release();
  for(auto& line:rich.lines){
   if(line.block==2){
    std::string imagePath=line.imagePath,decoded;
    auto nibble=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};
    for(size_t i=0;i<imagePath.size();++i){if(imagePath[i]=='%'&&i+2<imagePath.size()&&nibble(imagePath[i+1])>=0&&nibble(imagePath[i+2])>=0){decoded+=char(nibble(imagePath[i+1])*16+nibble(imagePath[i+2]));i+=2;}else decoded+=imagePath[i];}
    String full=decoded.c_str();if(!full.startsWith("/"))full=path.substring(0,path.lastIndexOf('/')+1)+full;
    if(!raster.finish()||!renderImage(raster,full))return false;continue;
   }
   writing::ExternalVector<writing::RichGlyph> part;float x=0;unsigned fontPx=std::lround(raster.layout.dpi*textPointSize(line.heading)*options.scalePercent/7200.0);unsigned content=raster.layout.width-raster.layout.margin*2;
   for(auto& g:line.glyphs){Font& f=fonts.get(g.style);int adv,lsb;stbtt_GetCodepointHMetrics(&f.info,g.code,&adv,&lsb);float step=adv*stbtt_ScaleForMappingEmToPixels(&f.info,fontPx);
    if(x+step>content&&!part.empty()){if(!glyphLine(raster,part,fonts,line.heading))return false;part.clear();x=0;}
    part.push_back(g);x+=step;
   }
   if(!glyphLine(raster,part,fonts,line.heading))return false;
  }
  if(rich.lines.empty()){writing::ExternalVector<writing::RichGlyph> empty;if(!glyphLine(raster,empty,fonts,0))return false;}
  if(cancelled)return false;
  if(raster.measure)vTaskDelay(1);
 }if(!raster.pages&&!raster.begin())return false;return raster.finish();
}
bool imageSize(File& f,unsigned& width,unsigned& height){
 uint8_t b[24];if(f.read(b,24)!=24)return false;
 if(!memcmp(b,"\x89PNG\r\n\x1a\n",8)){width=uint32_t(b[16])<<24|uint32_t(b[17])<<16|unsigned(b[18])<<8|b[19];height=uint32_t(b[20])<<24|uint32_t(b[21])<<16|unsigned(b[22])<<8|b[23];return width&&height;}
 if(b[0]!=0xff||b[1]!=0xd8)return false;f.seek(2);
 while(f.available()){int lead=f.read();if(lead!=255)return false;int marker;do{marker=f.read();}while(marker==255);if(marker<0||marker==0xda||marker==0xd9)return false;
  uint8_t size[2];if(f.read(size,2)!=2)return false;unsigned n=(unsigned(size[0])<<8)|size[1];if(n<2||n-2>f.size()-f.position())return false;
  if(marker==0xc0||marker==0xc1||marker==0xc2){uint8_t sof[5];if(n<7||f.read(sof,5)!=5)return false;height=(unsigned(sof[1])<<8)|sof[2];width=(unsigned(sof[3])<<8)|sof[4];return width&&height;}
  if(!f.seek(f.position()+n-2))return false;
 }return false;
}
// Both estimation and output use the same tiling layout: 100% fits the printable area.
// Enlargements span sheets; they are never silently cropped at the paper boundary.
template<class PixelRow>
bool imagePages(Raster& raster,unsigned width,unsigned height,PixelRow pixels){
 auto p=tab5print::imagePlacement(raster.layout,width,height,options.scalePercent);if(!p.pages())return false;
 unsigned cw=raster.layout.width-raster.layout.margin*2,ch=raster.layout.height-raster.layout.margin*2;
 for(unsigned py=0;py<p.rows;++py)for(unsigned px=0;px<p.columns;++px){
  if(!raster.begin())return false;unsigned top=py*ch,left=px*cw,dh=std::min(ch,p.height-top),dw=std::min(cw,p.width-left);
  unsigned oy=p.height<ch?(ch-p.height)/2:0,ox=raster.layout.margin+(p.width<cw?(cw-p.width)/2:0);
  if(raster.measure){if(!raster.blank(oy+dh)||!raster.finish())return false;continue;}
  if(!raster.blank(oy))return false;
  for(unsigned y=0;y<dh;++y){memset(raster.row.data(),255,raster.layout.width*3);
   if(!pixels(top+y,left,dw,p.width,p.height,raster.row.data()+ox*3)||!raster.line(raster.row.data()))return false;
  }
  if(!raster.finish())return false;
 }return true;
}
bool renderImage(Raster& raster,const String& path){
 File file=tab5FS.open(path);unsigned width=0,height=0;
 if(!file||file.size()>12*1024*1024||!imageSize(file,width,height)||width>20000||height>20000){state="Image illisible ou dimensions non prises en charge";return false;}
 if(raster.measure)return imagePages(raster,width,height,[](unsigned,unsigned,unsigned,unsigned,unsigned,uint8_t*){return true;});
 constexpr size_t MaxPixels=4U*1024U*1024U;
 // Match the decoder's power-of-two reduction. In particular 3024x4032
 // uses the JPEG decoder’s native /2 output (1512x2016), with no canvas affine pass.
 uint8_t signature[2];if(!file.seek(0)||file.read(signature,2)!=2)return false;
 const bool jpeg=signature[0]==0xff&&signature[1]==0xd8;
 unsigned w=width,h=height,shift=0;
 while(size_t(w)>MaxPixels/h){
  ++shift;if(jpeg&&shift>3){state="Image trop grande pour le décodeur JPEG";return false;}
  const unsigned round=jpeg?0:(1U<<shift)-1;
  w=(width+round)>>shift;h=(height+round)>>shift;
  if(!w||!h){state="Dimensions d’image invalides";return false;}
 }
 Tab5Resources::AdmissionReport imageReport;const size_t imageBytes=size_t(w)*h*2;
 auto imageAdmission=tab5ReserveMemory(Tab5Resources::Owner::Tools,{0,0,0,imageBytes+256*1024,imageBytes},"Image d’impression",&imageReport);
 if(!imageAdmission){memoryFailure(imageReport);return false;}
 Tab5ImageDecode::Image image;String decodeError;
 state="Décodage de l’image…";percent=0;tab5DiagnosticMark(2,"print-image-decode",path.c_str(),1);
 const bool decoded=Tab5ImageDecode::load(file,image,decodeError,
  {[](){return cancelled.load();},[](unsigned done){percent=done;},nullptr,&imageAdmission,
   [](void* reservation,size_t bytes){static_cast<Tab5Resources::Reservation*>(reservation)->allocated(0,bytes);}},MaxPixels);
 imageAdmission.release();file.close();
 if(!decoded){state=cancelled?"Opération annulée":"Décodage de l’image impossible";setDetail(decodeError);return false;}
 // Decoder context has already been destroyed; the pixels belong only to
 // this job. No display/image lock survives into the lengthy PWG/SD pass.
 state="Préparation des pages…";percent=0;tab5DiagnosticMark(2,"print-image-raster","conversion des pixels vers PWG",1);
 return imagePages(raster,image.sourceWidth,image.sourceHeight,[&](unsigned y,unsigned x,unsigned n,unsigned dw,unsigned dh,uint8_t* out){
  const unsigned sy=std::min(image.height-1,unsigned(uint64_t(y)*image.height/dh));
  for(unsigned i=0;i<n;++i){
   const unsigned sx=std::min(image.width-1,unsigned(uint64_t(x+i)*image.width/dw));const uint16_t pixel=image.pixels[size_t(sy)*image.width+sx];
   out[i*3]=((pixel>>11)&31)*255/31;out[i*3+1]=((pixel>>5)&63)*255/63;out[i*3+2]=(pixel&31)*255/31;
  }return true;
 });
}

bool renderMap(Raster& raster,const String& path){
 File f=tab5FS.open(path);uint8_t header[16];if(!f||f.read(header,16)!=16||memcmp(header,"T5RGB565",8))return false;
 auto get=[](const uint8_t* b){return uint32_t(b[0])|uint32_t(b[1])<<8|uint32_t(b[2])<<16|uint32_t(b[3])<<24;};unsigned w=get(header+8),h=get(header+12);
 if(!w||!h||w>8000||h>8000||f.size()!=16+size_t(w)*h*2)return false;
 std::vector<uint16_t> source(raster.measure?0:w);unsigned last=~0u;
 return imagePages(raster,w,h,[&](unsigned y,unsigned x,unsigned n,unsigned dw,unsigned dh,uint8_t* out){
  unsigned sy=std::min(h-1,unsigned(uint64_t(y)*h/dh));if(sy!=last){if(!f.seek(16+size_t(sy)*w*2)||f.read(reinterpret_cast<uint8_t*>(source.data()),w*2)!=w*2)return false;last=sy;}
  for(unsigned i=0;i<n;++i){uint16_t p=source[std::min(w-1,unsigned(uint64_t(x+i)*w/dw))];out[i*3]=((p>>11)&31)*255/31;out[i*3+1]=((p>>5)&63)*255/63;out[i*3+2]=(p&31)*255/31;}return true;
 });
}
unsigned supportedDpi(const tab5print::Response& caps,unsigned requested){
 unsigned chosen=0;bool pwg=false;for(auto& a:caps.attributes)if(a.name=="pwg-raster-document-resolution-supported")pwg=true;
 for(auto& a:caps.attributes)if((a.name==(pwg?"pwg-raster-document-resolution-supported":"printer-resolution-supported"))&&a.tag==0x32&&a.value.size()==9){auto get=[](const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];};unsigned x=get(a.value.data()),y=get(a.value.data()+4);if(a.value[8]==4){x=std::lround(x*2.54);y=std::lround(y*2.54);}if(x==y&&x>=72&&x<=requested&&x>chosen)chosen=x;}
 return chosen;
}
void runTask(){
 Tab5AudioPriorityLease priority;
 Tab5StorageUse storage;if(!storage){state="Reconnexion SD en cours — réessayez";return;}
 tab5print::Endpoint endpoint;tab5print::endpoint(options.uri.c_str(),endpoint);
 String spool=String("/System/print-")+millis()+".pwg.tmp";
 bool success=false;
 do {
  tab5print::Response caps;
  if(!measuring&&!exchangeSession(endpoint,0x000b,nullptr,"",nullptr,0,caps,priority))break;
  if(probing){state="Imprimante joignable";String info="";for(auto* f:{"image/pwg-raster","image/jpeg","application/pdf","text/plain"})if(caps.has("document-format-supported",f)){if(info.length())info+=" · ";info+=f;}setDetail(info);success=true;break;}
  const char* media=options.paper==1?"na_letter_8.5x11in":options.paper==2?"iso_a5_148x210mm":"iso_a4_210x297mm";
  if(!measuring&&!caps.has("media-supported",media)){state="Format de papier non pris en charge";break;}
  if(!measuring&&(!caps.has("document-format-supported","image/pwg-raster")||!caps.has("pwg-raster-document-type-supported","srgb_8"))){state="Cette imprimante ne prend pas en charge PWG Raster couleur";break;}
  unsigned dpi=measuring?options.dpi:supportedDpi(caps,options.dpi);if(!dpi){state="Résolution non prise en charge (72 à 600 dpi)";break;}
  std::vector<tab5print::Attribute>().swap(caps.attributes); // Query metadata is no longer needed during rendering or the next TLS session.
  double w=options.paper==1?215.9:options.paper==2?148:210,h=options.paper==1?279.4:options.paper==2?210:297;
  if(options.landscape)std::swap(w,h);auto layout=tab5print::page(w,h,dpi);
  if(!layout.width||tab5FS.exists(spool)){state="Préparation de l’impression impossible";break;}
  state="Préparation des pages…";tab5TaskCheckpoint("print before raster");tab5DiagnosticMark(2,"print-raster",options.path.c_str(),1);
  {Raster raster(spool,layout,media);String name=options.path;name.toLowerCase();bool picture=name.endsWith(".jpg")||name.endsWith(".jpeg")||name.endsWith(".png");
   bool ok=name.endsWith(".t5r")?renderMap(raster,options.path):picture?renderImage(raster,options.path):renderText(raster,options.path);raster.out.flush();raster.out.close();
   if(!ok){if(cancelled)state="Opération annulée";else if(!strcmp(state.load(),"Préparation des pages…"))state="Préparation impossible : fichier, police, mémoire ou limite de 256 pages";break;}}
  if(cancelled){state="Opération annulée";break;}
  tab5TaskCheckpoint("print after raster");tab5DiagnosticMark(2,"print-spool-ready","pages preparees",1);reportedPages=pageCount;
  if(measuring){state="Estimation prête";success=true;break;}
  state="Envoi à l’imprimante…";percent=0;
  tab5print::Response result;if(!exchangeSession(endpoint,2,"image/pwg-raster",spool,media,dpi,result,priority))break;
  int job=result.integer("job-id");state="Travail accepté par l’imprimante";setDetail(String("#")+job+" · "+pageCount+" p · "+dpi+" dpi");percent=100;success=true;
 }while(false);
 if(tab5FS.exists(spool))tab5FS.remove(spool);
 if(!measuring&&options.temporarySource&&options.path.startsWith("/System/print-map-")&&options.path.endsWith(".t5r"))tab5FS.remove(options.path);
 (void)success;
}
void task(void*){
 tab5DiagnosticMark(2,measuring?"print-estimate":"print-start",options.path.c_str(),1);
 runTask();tab5DiagnosticMark(2,"print-end",state.load(),0);
 // runTask's image, raster rows, fonts, socket and audio/storage leases are
 // destroyed here. The remaining worker stack is reclaimed by FreeRTOS idle.
 tab5LogMemory("imprimante: ressources liberees");tab5TaskCheckpoint("print");
 working.store(false,std::memory_order_release);vTaskDelete(nullptr);
}
}
namespace Tab5Print {
bool start(const Options& requested,bool probeOnly){tab5print::Endpoint e;if(working)return false;
 const bool usb=Tab5UsbPrinter::uri(requested.uri.c_str());
 if(usb&&!Tab5UsbPrinter::ready()){state=Tab5UsbPrinter::status();setDetail("");return false;}
 if(!usb&&!tab5print::endpoint(requested.uri.c_str(),e)){state="Adresse IPP / IPPS invalide";setDetail("");return false;}
 if(requested.dpi<72||requested.dpi>600||requested.scalePercent<25||requested.scalePercent>200){state="Réglages d’impression invalides";setDetail("");return false;}
 options=requested;usbPrinterToken=usb?Tab5UsbPrinter::connectionToken():0;probing=probeOnly;pendingTrust=false;measuring=false;reportedPages=0;cancelled=false;percent=0;pageCount=0;state="Interrogation de l’imprimante…";setDetail("");working=true;
 Tab5Resources::AdmissionReport report;
 if(tab5CreateTask(task,"tab5_print",24576,nullptr,1,nullptr,0,&report)!=pdPASS){working=false;memoryFailure(report);return false;}return true;}
bool estimate(const Options& requested){if(working)return false;
 if(requested.dpi<72||requested.dpi>600||requested.scalePercent<25||requested.scalePercent>200){state="Réglages d’impression invalides";setDetail("");return false;}
 options=requested;probing=false;measuring=true;reportedPages=0;cancelled=false;percent=0;pageCount=0;state="Calcul des pages…";setDetail("");working=true;
 Tab5Resources::AdmissionReport report;
 if(tab5CreateTask(task,"tab5_pages",24576,nullptr,1,nullptr,0,&report)!=pdPASS){working=false;memoryFailure(report);return false;}return true;
}
bool certificatePending(){return !working&&pendingTrust.load();}
bool trustCertificate(){if(working||!pendingTrust)return false;Preferences p;if(!p.begin("tab5print",false))return false;
 int slot=-1;for(unsigned i=0;i<8;++i){char key[8];snprintf(key,sizeof(key),"tls%u",i);if(!p.isKey(key)){slot=i;break;}}
 if(slot<0){state="Limite de 8 certificats atteinte";return false;}char key[8];snprintf(key,sizeof(key),"tls%u",unsigned(slot));
 bool ok=p.putBytes(key,&pendingCertificate,sizeof(pendingCertificate))==sizeof(pendingCertificate);p.end();
 if(ok){pendingTrust=false;state="Certificat mémorisé : vérifier puis imprimer";setDetail("");}return ok;
}
unsigned pages(){return reportedPages.load();}bool estimating(){return measuring.load();}
bool busy(){return working.load();}void cancel(){cancelled=true;}const char* status(){return state.load();}unsigned progress(){return percent.load();}
String detail(){char copy[512];portENTER_CRITICAL(&detailMux);memcpy(copy,details,sizeof(copy));portEXIT_CRITICAL(&detailMux);return String(copy);}
}
