#include "Tab5NetworkHttp.h"
#include "Tab5SecureClient.h"
#include "Tab5CooperativeClient.h"
#include "Tab5NetworkGate.h"
#include "Tab5TerminalPath.h"
#include "Tab5Storage.h"
#include "Tab5VolumeFS.h"
#include "Tab5UsbStorage.h"
#include <HTTPClient.h>
#include <NetworkClient.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <memory>
#include <cstdio>
#include <cerrno>

namespace Tab5NetworkTools {
namespace {
struct Transfer {
  Output output,errors;Cancel cancel;Translate translate;uint32_t started,timeout;
  bool stopped()const{return (cancel&&cancel())||!tab5NetworkEnabled();}
  bool expired()const{return millis()-started>=timeout;}
  static bool cancelled(void* p){auto& t=*static_cast<Transfer*>(p);return t.stopped()||t.expired();}
  std::string tr(const char* s)const{return translate?translate(s):s;}
  void print(const std::string& s){output(s.data(),s.size());}
  void diagnostic(const std::string& s){errors(s.data(),s.size());}
  int fail(const char* s){diagnostic(tr(s)+"\n");return stopped()?130:1;}
};
// HTTPClient decodes chunked responses into this sink. It never buffers the
// whole download in RAM. Existing files are untouched until transfer succeeds.
class Sink final:public Stream {
 public:
  Sink(Transfer& state,int descriptor):state(state),fd(descriptor){}
  size_t write(uint8_t b)override{return write(&b,1);}
  size_t write(const uint8_t* data,size_t size)override{
    if(state.stopped()||state.expired()||failed)return 0;
    if(fd<0){if(bytes+size>32768){failed=true;return 0;}state.output(reinterpret_cast<const char*>(data),size);bytes+=size;return size;}
    size_t done=0;while(done<size){ssize_t n=::write(fd,data+done,size-done);if(n<=0){failed=true;break;}done+=size_t(n);}
    bytes+=done;return done;
  }
  int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}void flush()override{}
  Transfer& state;int fd;uint64_t bytes=0;bool failed=false;
};
bool destination(const std::string& input,const std::string& cwd,std::string& path){
  return Tab5TerminalPath::logical(input,cwd,path,true)&&path.size()<=220;
}
std::string redirect(const std::string& current,const std::string& location){
  return Tab5NetworkArgs::resolveUrl(current,location);
}
}
int http(const Tab5NetworkArgs::Options& options,Output output,Cancel cancel,Translate translate,const std::string& cwd,Output errors){
  Transfer state{output,errors?errors:output,cancel,translate,millis(),options.timeoutMs};
  std::string url=Tab5NetworkArgs::normalizeUrl(options.url),path,temporary,backup;int fd=-1;
  std::unique_ptr<Tab5StorageUse> storage;
  if(!options.destination.empty()){
    if(!destination(options.destination,cwd,path))return state.fail("Destination invalide. Utilisez sd:/Dossier/fichier ou usb:/Dossier/fichier.");
    storage.reset(new(std::nothrow)Tab5StorageUse);if(!storage||!bool(*storage))return state.fail("Stockage occupé. Réessayez après l'opération en cours.");
    const bool usb=path=="/USB"||path.compare(0,5,"/USB/")==0;
    if(usb?!Tab5UsbStorage::mounted():!tab5SdAvailable())return state.fail("Le support de destination n'est pas disponible.");
    path="/sd"+path;temporary=path+".tab5-download-part";backup=path+".tab5-download-previous";
    struct stat existing{};const bool exists=stat(path.c_str(),&existing)==0;
    // Only ENOENT proves absence. Intermittent SD/USB metadata errors must
    // not bypass --overwrite or destroy a recovery copy on a later rename.
    if(!exists&&errno!=ENOENT)return state.fail("Le support de destination n'est pas disponible.");
    if(exists&&!S_ISREG(existing.st_mode))return state.fail("La destination n'est pas un fichier.");
    if(exists&&!options.overwrite)return state.fail("Le fichier existe déjà. Choisissez un autre nom ou ajoutez --overwrite.");
    if(access(backup.c_str(),F_OK)==0)return state.fail("Une sauvegarde du téléchargement précédent existe. Renommez-la avant de recommencer.");
    if(errno!=ENOENT)return state.fail("Impossible de conserver le fichier précédent.");
    fd=::open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL,0666);
    if(fd<0)return state.fail("Impossible de créer le fichier temporaire. Vérifiez le dossier et l'espace disponible.");
  }
  struct Temporary {int& fd;std::string& path;~Temporary(){if(fd>=0)::close(fd);if(!path.empty())::unlink(path.c_str());}} cleanup{fd,temporary};
  Sink sink(state,fd);int status=0;bool complete=false;
  for(unsigned hop=0;hop<=5;++hop){
    if(state.stopped())return 130;if(state.expired())return state.fail("Délai du transfert dépassé.");
    const bool tls=url.compare(0,8,"https://")==0;
    if(tls&&!Tab5SecureClient::clockReady())return state.fail("Réglez la date et l'heure avant d'utiliser HTTPS.");
    Tab5CooperativeClient<NetworkClient> plain(Transfer::cancelled,&state);
    Tab5CooperativeClient<Tab5SecureClient> secure(Transfer::cancelled,&state);
    const uint32_t left=std::min<uint32_t>(30000U,options.timeoutMs-(millis()-state.started));
    secure.setHandshakeTimeout((left+999)/1000);plain.setTimeout(left);secure.setTimeout(left);
    HTTPClient request;request.setConnectTimeout(left);request.setTimeout(left);
    request.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);request.setReuse(false);
    if(!(tls?request.begin(secure,url.c_str()):request.begin(plain,url.c_str())))return state.fail("Impossible de préparer la requête HTTP.");
    const char* names[]={"Location","Content-Type","Content-Length","Server","Strict-Transport-Security","Content-Security-Policy","X-Content-Type-Options","X-Frame-Options","Referrer-Policy","Cache-Control","ETag","Last-Modified"};
    request.collectHeaders(names,sizeof(names)/sizeof(names[0]));request.addHeader("Accept-Encoding","identity");
    status=options.head?request.sendRequest("HEAD"):request.GET();
    if(state.stopped()){request.end();return 130;}
    if(state.expired()){request.end();return state.fail("Délai du transfert dépassé.");}
    if(status<=0){request.end();return state.fail(tls?"Échec HTTPS : vérifiez la date, le certificat, le nom d'hôte et la connexion.":"Échec HTTP : vérifiez l'adresse et la connexion.");}
    const bool moved=status==301||status==302||status==303||status==307||status==308;
    if(moved&&options.follow){
      std::string next=redirect(url,request.header("Location").c_str());request.end();
      if(hop==5)return state.fail("Trop de redirections (maximum : 5).");
      if(!Tab5NetworkArgs::httpUrl(next))return state.fail("Adresse de redirection invalide.");
      if(tls&&next.compare(0,8,"https://")!=0)return state.fail("Redirection HTTPS vers HTTP refusée.");
      url=next;continue;
    }
    if(options.head){
      state.print(url+"\nHTTP "+std::to_string(status)+"\n"+state.tr(tls?"TLS : certificat et nom d'hôte vérifiés.":"HTTP : connexion non chiffrée.")+"\n");
      for(const char* name:names){String value=request.header(name);if(value.length())state.print(std::string(name)+": "+value.c_str()+"\n");}
      if(!request.hasHeader("Strict-Transport-Security")&&tls)state.print(state.tr("HSTS absent : le serveur n'impose pas HTTPS aux navigateurs.")+"\n");
      if(!request.hasHeader("X-Content-Type-Options"))state.print(state.tr("X-Content-Type-Options absent : protection contre la détection automatique du type non annoncée.")+"\n");
      if(!request.hasHeader("Content-Security-Policy"))state.print(state.tr("Content-Security-Policy absente : à examiner pour les pages HTML.")+"\n");
      state.print(state.tr("Ces observations ne prouvent pas à elles seules une vulnérabilité.")+"\n");
      request.end();return status>=200&&status<400?0:1;
    }
    if(status<200||status>=300){request.end();state.diagnostic("HTTP "+std::to_string(status)+"\n");return state.fail(moved?"Redirection reçue. Ajoutez -L pour la suivre.":"Le serveur a refusé le téléchargement.");}
    int transferred=request.writeToStream(&sink);request.end();
    if(state.stopped())return 130;
    if(state.expired())return state.fail("Délai du transfert dépassé.");
    if(transferred<0||sink.failed)return state.fail(fd<0?"Affichage limité à 32 Kio. Utilisez -o fichier pour télécharger le contenu complet.":"Téléchargement interrompu ou écriture impossible ; le fichier précédent est conservé.");
    complete=true;break;
  }
  if(!complete)return state.fail("Téléchargement incomplet.");
  if(fd>=0){
    bool ok=::fsync(fd)==0;if(::close(fd))ok=false;fd=-1;if(!ok)return state.fail("Impossible de terminer l'écriture du fichier.");
    bool exists=access(path.c_str(),F_OK)==0;
    if(!exists&&errno!=ENOENT)return state.fail("Le support de destination n'est pas disponible.");
    if(exists&&!options.overwrite)return state.fail("La destination vient d'être créée par une autre opération.");
    if(exists&&::rename(path.c_str(),backup.c_str()))return state.fail("Impossible de conserver le fichier précédent.");
    if(::rename(temporary.c_str(),path.c_str())){if(exists)::rename(backup.c_str(),path.c_str());return state.fail("Impossible de finaliser le téléchargement.");}
    temporary.clear();if(exists)::unlink(backup.c_str());tab5FsChanged();
    state.print(options.destination+" — "+std::to_string(sink.bytes)+" "+state.tr("octets enregistrés")+"\n");
  }else state.print("\n");
  return 0;
}
}
