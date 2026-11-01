#include "Tab5Ssh.h"
#include "Tab5SshPolicy.h"
#include "Tab5TerminalPath.h"
#ifdef TAB5_SSH_HOST_TEST
#include "SshPlatform.h"
#else
#include "Tab5NetworkGate.h"
#include "Tab5ResourceManager.h"
#include "Tab5Audio.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <lwip/dns.h>
#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include "src/tab5ssh/libssh2.h"
#include "src/tab5ssh/libssh2_sftp.h"
#include <atomic>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <new>
#include <cstddef>
#include <sys/stat.h>

namespace Tab5Ssh {
namespace {
std::string localPath(const std::string& path){
#ifdef TAB5_SSH_HOST_TEST
  return sshTestVolume()+path;
#else
  return "/sd"+path;
#endif
}
// The gate serializes this library's process-global crypto context as well as
// the complete network session. No global mbedTLS allocator is ever replaced.
struct Budget {
  struct alignas(std::max_align_t) Header {size_t bytes;};
  size_t used=0,peak=0;
  void* allocate(size_t size){if(!size)size=1;if(size>SessionBudget-sizeof(Header)||used>SessionBudget-size-sizeof(Header))return nullptr;
    auto* p=static_cast<Header*>(heap_caps_aligned_alloc(alignof(Header),size+sizeof(Header),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!p)return nullptr;
    p->bytes=size;used+=size+sizeof(Header);peak=std::max(peak,used);return p+1;}
  void release(void* data){if(!data)return;auto* p=static_cast<Header*>(data)-1;used-=p->bytes+sizeof(Header);heap_caps_free(p);}
  void* resize(void* data,size_t size){if(!data)return allocate(size);if(!size){release(data);return nullptr;}auto* old=static_cast<Header*>(data)-1;size_t previous=old->bytes;
    // Preserve malloc alignment on every platform. A growth temporarily owns
    // both blocks, and that temporary peak is included in the same hard cap.
    void* next=allocate(size);if(!next)return nullptr;memcpy(next,data,std::min(previous,size));release(data);return next;}
};
struct Bytes {
  Budget& budget;char* data=nullptr;size_t size=0,capacity=0;
  explicit Bytes(Budget& b):budget(b){}
  ~Bytes(){if(data){volatile char* wipe=data;for(size_t n=0;n<capacity;++n)wipe[n]=0;budget.release(data);}}
  bool create(size_t amount){data=static_cast<char*>(budget.allocate(amount));if(!data)return false;capacity=amount;memset(data,0,amount);return true;}
  bool readFile(const std::string& path,const std::string& cwd,size_t limit,bool stripLine){std::string normal;if(!Tab5TerminalPath::logical(path,cwd,normal))return false;
    FILE* file=fopen(localPath(normal).c_str(),"rb");if(!file)return false;struct stat st{};
    bool ok=!fstat(fileno(file),&st)&&S_ISREG(st.st_mode)&&st.st_size>0&&static_cast<uint64_t>(st.st_size)<=limit&&create(static_cast<size_t>(st.st_size)+1);
    if(ok){size=fread(data,1,capacity-1,file);ok=size==capacity-1&&!ferror(file)&&!memchr(data,0,size);}fclose(file);
    if(ok&&stripLine){while(size&&(data[size-1]=='\n'||data[size-1]=='\r'))data[--size]=0;ok=size>0;}return ok;}
};
struct Socket {
  libssh2_socket_t fd=LIBSSH2_INVALID_SOCKET;
  ~Socket(){close();}
  void close(){if(fd!=LIBSSH2_INVALID_SOCKET){::LIBSSH2_SOCKET_CLOSE(fd);fd=LIBSSH2_INVALID_SOCKET;}}
  bool open(){fd=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(fd==LIBSSH2_INVALID_SOCKET)return false;
#ifdef TAB5_SSH_HOST_TEST
    u_long nonblocking=1;return ioctlsocket(fd,FIONBIO,&nonblocking)==0;
#else
    return fcntl(fd,F_SETFL,O_NONBLOCK)==0;
#endif
  }
};
int socketError(){
#ifdef TAB5_SSH_HOST_TEST
  return WSAGetLastError();
#else
  return errno;
#endif
}
bool pendingSocket(int error){
#ifdef TAB5_SSH_HOST_TEST
  return error==WSAEWOULDBLOCK||error==WSAEINPROGRESS||error==WSAEALREADY;
#else
  return error==EWOULDBLOCK||error==EINPROGRESS||error==EALREADY;
#endif
}
struct Session {
  Output output,errors;Cancel cancel;Translate translate;Budget budget;Socket socket;
  LIBSSH2_SESSION* ssh=nullptr;bool initialized=false,closing=false;
  Session(Output o,Output e,Cancel c,Translate t):output(std::move(o)),errors(std::move(e)),cancel(std::move(c)),translate(std::move(t)){}
  bool stopped()const{return (cancel&&cancel())||!tab5NetworkEnabled();}
  std::string tr(const char* text)const{return translate?translate(text):text;}
  int fail(const char* text){std::string message=tr(text)+"\n";errors(message.data(),message.size());
#ifdef TAB5_SSH_HOST_TEST
    if(ssh){char* detail=nullptr;int length=0;int error=libssh2_session_last_error(ssh,&detail,&length,0);if(error&&detail)fprintf(stderr,"[fixture libssh2 %d: %.*s]\n",error,length,detail);}
#endif
    return stopped()?130:1;}
  void print(const std::string& text){output(text.data(),text.size());}
  static void* allocate(size_t n,void** abstract){return static_cast<Session*>(*abstract)->budget.allocate(n);}
  static void release(void* p,void** abstract){static_cast<Session*>(*abstract)->budget.release(p);}
  static void* resize(void* p,size_t n,void** abstract){return static_cast<Session*>(*abstract)->budget.resize(p,n);}
  static ssize_t sendData(libssh2_socket_t fd,const void* data,size_t n,int flags,void** abstract){auto& s=*static_cast<Session*>(*abstract);if(s.closing)return -ECONNRESET;
    int result=::send(fd,static_cast<const char*>(data),static_cast<int>(n),flags);if(result>=0)return result;int error=socketError();return pendingSocket(error)?-EAGAIN:-ECONNRESET;}
  static ssize_t readData(libssh2_socket_t fd,void* data,size_t n,int flags,void** abstract){auto& s=*static_cast<Session*>(*abstract);if(s.closing)return -ECONNRESET;
    int result=::recv(fd,static_cast<char*>(data),static_cast<int>(n),flags);if(result>=0)return result;int error=socketError();return pendingSocket(error)?-EAGAIN:-ECONNRESET;}
  ~Session(){
    // Turn all further libssh2 socket callbacks into hard EOF errors. Freeing
    // never waits for an uncooperative peer and cannot yield EAGAIN on close.
    closing=true;
#ifdef TAB5_SSH_HOST_TEST
    if(std::getenv("SSH_TRACE"))fprintf(stderr,"[fixture closing session %p]\n",ssh);
#endif
    if(ssh){libssh2_session_set_blocking(ssh,0);libssh2_session_disconnect(ssh,"Tab5OS");libssh2_session_free(ssh);ssh=nullptr;}
#ifdef TAB5_SSH_HOST_TEST
    if(std::getenv("SSH_TRACE"))fprintf(stderr,"[fixture closed session]\n");
#endif
    socket.close();if(initialized)libssh2_exit();
#ifdef TAB5_SSH_HOST_TEST
    sshTestReleased(budget.used,budget.peak);
#endif
  }
  bool wait(uint32_t started,uint32_t timeout,bool connect=false){if(stopped()||millis()-started>=timeout)return false;
    int directions=connect?LIBSSH2_SESSION_BLOCK_OUTBOUND:libssh2_session_block_directions(ssh);
    if(!directions){delay(5);return !stopped()&&millis()-started<timeout;}
    do{fd_set readset,writeset;FD_ZERO(&readset);FD_ZERO(&writeset);FD_SET(socket.fd,&readset);FD_SET(socket.fd,&writeset);timeval tv{0,20000};
      int rc=select(static_cast<int>(socket.fd+1),directions&LIBSSH2_SESSION_BLOCK_INBOUND?&readset:nullptr,directions&LIBSSH2_SESSION_BLOCK_OUTBOUND?&writeset:nullptr,nullptr,&tv);
      if(rc<0)return false;delay(1);
      // TCP is connected only after readiness, never merely after a 20 ms
      // select timeout with SO_ERROR still zero. Protocol retries may poll.
      if(!connect||rc>0)return !stopped()&&millis()-started<timeout;
    }while(!stopped()&&millis()-started<timeout);return false;}
  template<class Fn> int retry(Fn operation,uint32_t timeout=HandshakeTimeout){uint32_t began=millis();int result;
    do{if(stopped())return LIBSSH2_ERROR_SOCKET_DISCONNECT;result=operation();if(result!=LIBSSH2_ERROR_EAGAIN)return result;}while(wait(began,timeout));return LIBSSH2_ERROR_TIMEOUT;}
};
#ifndef TAB5_SSH_HOST_TEST
struct DnsJob {std::atomic<unsigned> references{2};std::atomic<bool> done{false},abandoned{false};bool success=false;char name[254]{};ip_addr_t address{};};
std::atomic<bool> dnsPending{false};
void releaseDns(DnsJob* job){if(job->references.fetch_sub(1)==1)delete job;}
void dnsComplete(const char*,const ip_addr_t* address,void* context){auto* job=static_cast<DnsJob*>(context);if(address&&IP_IS_V4(address)&&!ip_addr_isany(address)){job->address=*address;job->success=true;}job->done.store(true,std::memory_order_release);dnsPending=false;releaseDns(job);}
void dnsBegin(void* context){auto* job=static_cast<DnsJob*>(context);if(job->abandoned.load()){dnsComplete(nullptr,nullptr,job);return;}ip_addr_t address{};err_t rc=dns_gethostbyname_addrtype(job->name,&address,dnsComplete,job,LWIP_DNS_ADDRTYPE_IPV4);if(rc==ERR_OK)dnsComplete(nullptr,&address,job);else if(rc!=ERR_INPROGRESS)dnsComplete(nullptr,nullptr,job);}
#endif
bool resolve(const std::string& host,sockaddr_in& address,Session& session){address={};address.sin_family=AF_INET;
  if(inet_pton(AF_INET,host.c_str(),&address.sin_addr)!=1){
#ifdef TAB5_SSH_HOST_TEST
    addrinfo hints{},*answer=nullptr;hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;if(getaddrinfo(host.c_str(),nullptr,&hints,&answer))return false;
    address=*reinterpret_cast<sockaddr_in*>(answer->ai_addr);freeaddrinfo(answer);
#else
    bool expected=false;if(!dnsPending.compare_exchange_strong(expected,true))return false;
    auto* job=new(std::nothrow)DnsJob;if(!job){dnsPending=false;return false;}memcpy(job->name,host.c_str(),host.size()+1);
    if(tcpip_try_callback(dnsBegin,job)!=ERR_OK){dnsPending=false;delete job;return false;}
    uint32_t began=millis();while(!job->done.load(std::memory_order_acquire)&&!session.stopped()&&millis()-began<5000)delay(10);
    bool success=job->done.load(std::memory_order_acquire)&&job->success;if(success)address.sin_addr.s_addr=ip_2_ip4(&job->address)->addr;job->abandoned=true;releaseDns(job);if(!success)return false;
#endif
  }
  const uint32_t ip=ntohl(address.sin_addr.s_addr);return ip&&ip!=0xffffffffU&&(ip>>28)!=14;
}
// NVS entries are individually atomic and survive SD removal/replacement.
// Full host and port are retained, avoiding truncated-hash key collisions.
struct Pin {uint32_t version=1;uint16_t port=0;char host[254]{};char fingerprint[51]{};};
int pin(const Options& options,Pin& result,bool save=false,bool remove=false){Preferences prefs;if(!prefs.begin("tab5-ssh",false))return -2;int empty=-1,found=-1;Pin current;
  for(int i=0;i<32;++i){char name[8];snprintf(name,sizeof(name),"host%02d",i);size_t n=prefs.getBytesLength(name);if(!n){if(empty<0)empty=i;continue;}
    if(n!=sizeof(Pin)||prefs.getBytes(name,&current,sizeof(current))!=sizeof(current)||current.version!=1||!memchr(current.host,0,sizeof(current.host))||!memchr(current.fingerprint,0,sizeof(current.fingerprint))||!validFingerprint(current.fingerprint)){prefs.end();return -2;}
    if(current.port==options.port&&options.host==current.host){found=i;result=current;break;}}
  int slot=found>=0?found:empty;int rc=found>=0?1:0;
  if(remove&&found>=0){char name[8];snprintf(name,sizeof(name),"host%02d",found);rc=prefs.remove(name)?1:-2;}
  if(save){if(found>=0){rc=sameFingerprint(result.fingerprint,options.fingerprint.c_str())?1:-1;}
    else if(slot<0)rc=-3;else{Pin next;next.port=options.port;memcpy(next.host,options.host.c_str(),options.host.size()+1);memcpy(next.fingerprint,options.fingerprint.c_str(),51);char name[8];snprintf(name,sizeof(name),"host%02d",slot);rc=prefs.putBytes(name,&next,sizeof(next))==sizeof(next)?1:-2;}}
  prefs.end();return rc;
}
std::string fingerprint(LIBSSH2_SESSION* ssh){const auto* digest=reinterpret_cast<const unsigned char*>(libssh2_hostkey_hash(ssh,LIBSSH2_HOSTKEY_HASH_SHA256));if(!digest)return {};
  static const char b64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string value="SHA256:";unsigned bits=0,available=0;
  for(unsigned n=0;n<32;++n){bits=(bits<<8)|digest[n];available+=8;while(available>=6){available-=6;value+=b64[(bits>>available)&63];}}if(available)value+=b64[(bits<<(6-available))&63];return value;
}
int connect(Session& s,const Options& o){sockaddr_in address{};if(!resolve(o.host,address,s))return s.fail("Résolution DNS impossible.");address.sin_port=htons(o.port);
  if(!s.socket.open())return s.fail("Connexion TCP indisponible.");int rc=::connect(s.socket.fd,reinterpret_cast<sockaddr*>(&address),sizeof(address));
  if(rc){if(!pendingSocket(socketError()))return s.fail("Connexion SSH impossible.");uint32_t began=millis();
    if(!s.wait(began,5000,true))return s.fail("Délai SSH dépassé.");int error=0;socklen_t size=sizeof(error);if(getsockopt(s.socket.fd,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&size)||error)return s.fail("Connexion SSH impossible.");}
  if(libssh2_init(0))return s.fail("Initialisation SSH impossible.");s.initialized=true;
  s.ssh=libssh2_session_init_ex(Session::allocate,Session::release,Session::resize,&s);if(!s.ssh)return s.fail("Mémoire insuffisante.");libssh2_session_set_blocking(s.ssh,0);
#ifdef TAB5_SSH_HOST_TEST
  if(std::getenv("SSH_TRACE"))libssh2_trace(s.ssh,LIBSSH2_TRACE_KEX|LIBSSH2_TRACE_ERROR|LIBSSH2_TRACE_TRANS);
#endif
  libssh2_session_callback_set2(s.ssh,LIBSSH2_CALLBACK_SEND,reinterpret_cast<libssh2_cb_generic*>(Session::sendData));
  libssh2_session_callback_set2(s.ssh,LIBSSH2_CALLBACK_RECV,reinterpret_cast<libssh2_cb_generic*>(Session::readData));
  if(libssh2_session_method_pref(s.ssh,LIBSSH2_METHOD_KEX,"ecdh-sha2-nistp256,diffie-hellman-group14-sha256")||
     libssh2_session_method_pref(s.ssh,LIBSSH2_METHOD_HOSTKEY,"ecdsa-sha2-nistp256,rsa-sha2-512,rsa-sha2-256")||
     libssh2_session_method_pref(s.ssh,LIBSSH2_METHOD_CRYPT_CS,"aes128-ctr,aes256-ctr")||
     libssh2_session_method_pref(s.ssh,LIBSSH2_METHOD_CRYPT_SC,"aes128-ctr,aes256-ctr")||
     libssh2_session_method_pref(s.ssh,LIBSSH2_METHOD_MAC_CS,"hmac-sha2-256-etm@openssh.com,hmac-sha2-256")||
     libssh2_session_method_pref(s.ssh,LIBSSH2_METHOD_MAC_SC,"hmac-sha2-256-etm@openssh.com,hmac-sha2-256"))return s.fail("Algorithmes SSH indisponibles.");
  if(s.retry([&]{return libssh2_session_handshake(s.ssh,s.socket.fd);}))return s.fail("Négociation SSH impossible.");return 0;
}
int authenticate(Session& s,const Options& o,const std::string& cwd){const std::string actual=fingerprint(s.ssh);if(actual.empty())return s.fail("Empreinte SSH indisponible.");Pin saved;int state=pin(o,saved);
  if(state!=1){s.fail(state<0?"Confiance SSH illisible.":"Hôte SSH inconnu. Vérifiez son empreinte puis utilisez ssh trust.");s.errors(actual.data(),actual.size());s.errors("\n",1);return 1;}
  if(!sameFingerprint(saved.fingerprint,actual.c_str()))return s.fail("Clé SSH modifiée : connexion refusée. Vérifiez le serveur avant ssh forget.");
  Bytes key(s.budget),publicKey(s.budget),secret(s.budget);
  if(!o.password.empty()){if(!secret.readFile(o.password,cwd,SecretLimit,true))return s.fail("Fichier secret illisible ou trop grand.");
    if(s.retry([&]{return libssh2_userauth_password_ex(s.ssh,o.user.c_str(),o.user.size(),secret.data,secret.size,nullptr);}))return s.fail("Authentification SSH refusée.");}
  else{if(!key.readFile(o.key,cwd,KeyLimit,false))return s.fail("Clé privée illisible ou trop grande.");if(!o.passphrase.empty()&&!secret.readFile(o.passphrase,cwd,SecretLimit,true))return s.fail("Fichier secret illisible ou trop grand.");
    if(strncmp(key.data,"-----BEGIN ",11)||strstr(key.data,"-----BEGIN OPENSSH PRIVATE KEY-----"))return s.fail("Utilisez une clé privée PEM RSA ou ECDSA.");
    // mbedTLS libssh2 can derive an RSA public key, but ECDSA needs the
    // standard OpenSSH .pub companion generated by ssh-keygen.
    bool hasPublic=publicKey.readFile(o.key+".pub",cwd,4096,true);
    if(!hasPublic&&strstr(key.data,"-----BEGIN EC PRIVATE KEY-----"))return s.fail("Clé publique manquante : placez le fichier .pub à côté de la clé privée.");
    if(s.retry([&]{return libssh2_userauth_publickey_frommemory(s.ssh,o.user.c_str(),o.user.size(),hasPublic?publicKey.data:nullptr,hasPublic?publicKey.size:0,key.data,key.size,secret.data);}))return s.fail("Authentification SSH refusée.");}
  return 0;
}
int execute(Session& s,const Options& o,Input input){LIBSSH2_CHANNEL* channel=nullptr;
  int rc=s.retry([&]{channel=libssh2_channel_open_ex(s.ssh,"session",7,ChannelWindow,ChannelPacket,nullptr,0);return channel?0:libssh2_session_last_errno(s.ssh);});if(rc)return s.fail("Canal SSH indisponible.");
  if(s.retry([&]{return libssh2_channel_exec(channel,o.command.c_str());}))return s.fail("Commande SSH refusée.");
  Bytes incoming(s.budget),outgoing(s.budget);if(!incoming.create(2048)||!outgoing.create(2048))return s.fail("Mémoire insuffisante.");
  size_t buffered=0,sent=0;bool inputEnded=false,eofSent=false;uint32_t progress=millis();
  while(!s.stopped()){
    bool advanced=false;
    if(!inputEnded&&sent==buffered){buffered=input?input(outgoing.data,outgoing.capacity):0;sent=0;inputEnded=!buffered;if(buffered>outgoing.capacity)return s.fail("Entrée SSH invalide.");}
    if(sent<buffered){ssize_t n=libssh2_channel_write(channel,outgoing.data+sent,buffered-sent);if(n>0){sent+=n;advanced=true;}else if(n<0&&n!=LIBSSH2_ERROR_EAGAIN)return s.fail("Écriture SSH interrompue.");}
    if(inputEnded&&!eofSent){rc=libssh2_channel_send_eof(channel);if(!rc){eofSent=true;advanced=true;}else if(rc!=LIBSSH2_ERROR_EAGAIN)return s.fail("Écriture SSH interrompue.");}
    // Drain both streams fairly. A verbose stderr must never block stdout.
    for(int stream=0;stream<2;++stream){ssize_t n=libssh2_channel_read_ex(channel,stream,incoming.data,incoming.capacity);if(n>0){(stream?s.errors:s.output)(incoming.data,n);advanced=true;}else if(n<0&&n!=LIBSSH2_ERROR_EAGAIN)return s.fail("Lecture SSH interrompue.");}
    if(advanced){progress=millis();delay(1);continue;}if(libssh2_channel_eof(channel))break;
    if(!s.wait(progress,o.timeout))return s.fail("Délai SSH dépassé.");
  }
  if(s.stopped())return 130;
  if(s.retry([&]{return libssh2_channel_close(channel);}))return s.fail("Fermeture SSH interrompue.");
  int status=libssh2_channel_get_exit_status(channel);char* signal=nullptr;size_t length=0;libssh2_channel_get_exit_signal(channel,&signal,&length,nullptr,nullptr,nullptr,nullptr);if(signal){libssh2_free(s.ssh,signal);status=128;}return status;
}
int transfer(Session& s,const Options& o,const std::string& cwd){
  struct Sftp {Session& owner;LIBSSH2_SFTP* value=nullptr;~Sftp(){if(value){owner.closing=true;libssh2_sftp_shutdown(value);}}} sftp{s};
  if(s.retry([&]{sftp.value=libssh2_sftp_init(s.ssh);return sftp.value?0:libssh2_session_last_errno(s.ssh);}))return s.fail("Service SFTP indisponible.");
  // libssh2 creates an SFTP channel using its default window. Its receive
  // allocations remain constrained by our session allocator's 512 KiB cap.
  struct Handle {Session& owner;LIBSSH2_SFTP_HANDLE* value=nullptr;~Handle(){if(value){owner.closing=true;libssh2_sftp_close_handle(value);}}} handle{s};
  Bytes buffer(s.budget);if(!buffer.create(4096))return s.fail("Mémoire insuffisante.");
  if(o.action=="ls"){
    if(s.retry([&]{handle.value=libssh2_sftp_opendir(sftp.value,o.remote.c_str());return handle.value?0:libssh2_session_last_errno(s.ssh);}))return s.fail("Dossier SFTP inaccessible.");
    for(;;){LIBSSH2_SFTP_ATTRIBUTES attributes{};int length=s.retry([&]{return libssh2_sftp_readdir_ex(handle.value,buffer.data,buffer.capacity-1,nullptr,0,&attributes);},o.timeout);
      if(length<0)return s.fail("Lecture SFTP interrompue.");if(!length)return 0;
      if(length>=static_cast<int>(buffer.capacity))return s.fail("Réponse SFTP invalide.");buffer.data[length]=0;
      // No terminal control sequences from a remote filename.
      for(int n=0;n<length;++n)if(static_cast<unsigned char>(buffer.data[n])<32||buffer.data[n]==127)buffer.data[n]='?';
      s.output(buffer.data,length);s.output("\n",1);delay(1);
    }
  }
  std::string normal;if(!Tab5TerminalPath::logical(o.local,cwd,normal,o.action=="get"))return s.fail("Destination invalide. Utilisez sd:/Dossier/fichier ou usb:/Dossier/fichier.");
  std::string local=localPath(normal),remote=o.remote;
  struct LocalFile {FILE* value=nullptr;std::string part;~LocalFile(){if(value)fclose(value);if(!part.empty())::remove(part.c_str());}} file;
  struct RemotePart {Session& owner;LIBSSH2_SFTP* sftp;std::string path;~RemotePart(){if(!path.empty()&&!owner.stopped())owner.retry([&]{return libssh2_sftp_unlink(sftp,path.c_str());},1000);}} remotePart{s,sftp.value,{}};
  if(o.action=="get"){
    struct stat existing{};if(!stat(local.c_str(),&existing))return s.fail("Le fichier existe déjà. Choisissez un autre nom.");
    file.part=local+".tab5-sftp-part";int fd=::open(file.part.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0){file.part.clear();return s.fail("Impossible de créer le fichier temporaire. Vérifiez le dossier et l'espace disponible.");}
    file.value=fdopen(fd,"wb");if(!file.value){::close(fd);return s.fail("Écriture locale impossible.");}
    if(s.retry([&]{handle.value=libssh2_sftp_open_ex(sftp.value,remote.c_str(),remote.size(),LIBSSH2_FXF_READ,0,LIBSSH2_SFTP_OPENFILE);return handle.value?0:libssh2_session_last_errno(s.ssh);}))return s.fail("Fichier SFTP inaccessible.");
    for(;;){int length=s.retry([&]{return static_cast<int>(libssh2_sftp_read(handle.value,buffer.data,buffer.capacity));},o.timeout);if(length<0)return s.fail("Lecture SFTP interrompue.");if(!length)break;
      if(fwrite(buffer.data,1,length,file.value)!=static_cast<size_t>(length))return s.fail("Écriture locale impossible.");delay(1);}
    if(fflush(file.value)||fsync(fileno(file.value)))return s.fail("Écriture locale impossible.");int closeResult=fclose(file.value);file.value=nullptr;if(closeResult)return s.fail("Écriture locale impossible.");
    if(s.stopped())return 130;
    // FAT does not replace an existing destination through rename. The first
    // check also gives users a precise message before downloading any data.
    if(!stat(local.c_str(),&existing)||::rename(file.part.c_str(),local.c_str()))return s.fail("Finalisation du transfert impossible.");file.part.clear();return 0;
  }
  file.value=fopen(local.c_str(),"rb");if(!file.value)return s.fail("Fichier local inaccessible.");struct stat source{};
  if(fstat(fileno(file.value),&source)||!S_ISREG(source.st_mode))return s.fail("Fichier local inaccessible.");
  LIBSSH2_SFTP_ATTRIBUTES attributes{};int existing=s.retry([&]{return libssh2_sftp_stat(sftp.value,remote.c_str(),&attributes);});
  if(!existing)return s.fail("Le fichier existe déjà. Choisissez un autre nom.");
  if(libssh2_sftp_last_error(sftp.value)!=LIBSSH2_FX_NO_SUCH_FILE)return s.fail("Destination SFTP inaccessible.");
  const std::string staging=remote+".tab5-sftp-part";
  if(s.retry([&]{handle.value=libssh2_sftp_open_ex(sftp.value,staging.c_str(),staging.size(),LIBSSH2_FXF_WRITE|LIBSSH2_FXF_CREAT|LIBSSH2_FXF_EXCL,0600,LIBSSH2_SFTP_OPENFILE);return handle.value?0:libssh2_session_last_errno(s.ssh);}))return s.fail("Destination SFTP inaccessible.");remotePart.path=staging;
  while(!s.stopped()){size_t length=fread(buffer.data,1,buffer.capacity,file.value);if(!length){if(ferror(file.value))return s.fail("Lecture locale impossible.");break;}
    size_t sent=0;while(sent<length){int amount=s.retry([&]{return static_cast<int>(libssh2_sftp_write(handle.value,buffer.data+sent,length-sent));},o.timeout);if(amount<=0)return s.fail("Écriture SFTP interrompue.");sent+=amount;}delay(1);}
  if(s.stopped())return 130;
  if(s.retry([&]{int result=libssh2_sftp_close_handle(handle.value);
    // libssh2 consumes the handle on every final result, including an error
    // status from the server. Only EAGAIN leaves it available for retry.
    if(result!=LIBSSH2_ERROR_EAGAIN)handle.value=nullptr;return result;}))return s.fail("Fermeture SFTP interrompue.");
  // SFTP v3 flags=0: the server must refuse an existing destination; never
  // request overwrite/posix-rename semantics for an implicit file transfer.
  if(s.retry([&]{return libssh2_sftp_rename_ex(sftp.value,staging.c_str(),staging.size(),remote.c_str(),remote.size(),0);}))return s.fail("Finalisation du transfert impossible.");remotePart.path.clear();return 0;
}
} // namespace
int run(const std::vector<std::string>& args,Output output,Cancel cancel,Translate translate,const std::string& cwd,Output errors,Input input){
  if(args.empty()||(args[0]!="ssh"&&args[0]!="sftp"))return -1;Session s(output,errors?errors:output,cancel,translate);Options options;
  if(!parse(args,options)){s.fail("Paramètres SSH invalides. Consultez help ssh ou help sftp.");return 2;}
  if(options.action=="trust"||options.action=="forget"){Pin saved;int rc=pin(options,saved,options.action=="trust",options.action=="forget");
    if(rc==-1)return s.fail("Clé SSH modifiée : connexion refusée. Vérifiez le serveur avant ssh forget.");if(rc==-3)return s.fail("Liste SSH pleine : retirez un hôte avec ssh forget.");if(rc<0)return s.fail("Enregistrement de la confiance SSH impossible.");return 0;}
  if(s.stopped())return s.fail("Réseau désactivé.");
#ifndef TAB5_SSH_HOST_TEST
  // ESP-IDF reports stack watermarks in bytes. Deep shell substitutions must
  // fail before entering crypto rather than overflowing the shared worker.
  // This is conservative (historical minimum), not a claimed hardware proof.
  if(uxTaskGetStackHighWaterMark(nullptr)<8192)return s.fail("Pile insuffisante pour SSH. Lancez la commande directement dans le terminal.");
#endif
  Tab5NetworkLease lease("ssh",5000,[](void* value){return static_cast<Session*>(value)->stopped();},&s);if(!lease)return s.fail("Réseau occupé. Réessayez dans un instant.");
#ifndef TAB5_SSH_HOST_TEST
  auto budget=Tab5Resources::tlsBudget();budget.psram+=SessionBudget;budget.psramBlock=std::max<size_t>(budget.psramBlock,128*1024);
  auto reservation=tab5ReserveMemory(Tab5Resources::Owner::Tools,budget,"SSH");if(!reservation)return s.fail("Mémoire insuffisante.");
#endif
  // Session must close before the lease is released: run the actual SSH owner
  // in the inner scope (the outer Session only handles local argument errors).
  Session active(output,errors?errors:output,cancel,translate);
  int rc=connect(active,options);if(rc)return rc;
#ifdef TAB5_SSH_HOST_TEST
  if(std::getenv("SSH_TRACE"))fprintf(stderr,"[fixture connect complete]\n");
#endif
  if(options.action=="keyscan"){std::string hash=fingerprint(active.ssh);if(hash.empty())return active.fail("Empreinte SSH indisponible.");active.print(options.host+" "+std::to_string(options.port)+" "+hash+"\n");return 0;}
  rc=authenticate(active,options,cwd);if(rc)return rc;
  if(options.action=="exec")return execute(active,options,input);
  return transfer(active,options,cwd);
}
}
