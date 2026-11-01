#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cctype>

// Platform-independent parsing: the console and network audit app use the
// same bounds, diagnostics and option semantics.
namespace Tab5NetworkArgs {
struct Options {
  std::string command, host, url, destination, error;
  std::vector<unsigned> ports;
  unsigned count=4, timeoutMs=1500;
  bool head=false, follow=false, overwrite=false, remoteName=false;
};
inline bool integer(const std::string& s,unsigned low,unsigned high,unsigned& out) {
  if(s.empty())return false;unsigned long n=0;
  for(unsigned char c:s){if(!std::isdigit(c))return false;n=n*10+c-'0';if(n>high)return false;}
  if(n<low)return false;out=unsigned(n);return true;
}
inline bool seconds(const std::string& s,unsigned& milliseconds,double maximum=30) {
  if(s.empty()||s[0]=='-'||s[0]=='+')return false;char* end=nullptr;
  double value=std::strtod(s.c_str(),&end);
  if(*end||!std::isfinite(value)||value<0.05||value>maximum)return false;
  milliseconds=unsigned(value*1000);return true;
}
inline bool portList(const std::string& text,std::vector<unsigned>& ports) {
  ports.clear();size_t pos=0;if(text.empty())return false;
  while(pos<text.size()){
    size_t comma=text.find(',',pos);if(comma==std::string::npos)comma=text.size();
    std::string part=text.substr(pos,comma-pos);size_t dash=part.find('-');unsigned first=0,last=0;
    if(!integer(part.substr(0,dash),1,65535,first))return false;
    if(dash==std::string::npos)last=first;else if(!integer(part.substr(dash+1),first,65535,last))return false;
    if(last-first+1>1024)return false;
    for(unsigned p=first;p<=last;++p)if(std::find(ports.begin(),ports.end(),p)==ports.end()){
      if(ports.size()==1024)return false;ports.push_back(p);
    }
    if(comma+1==text.size())return false;pos=comma+1;
  }
  return !ports.empty();
}
inline bool httpUrl(const std::string& value) {
  size_t start=value.compare(0,8,"https://")==0?8:value.compare(0,7,"http://")==0?7:0;
  if(!start||start==value.size())return false;
  for(unsigned char c:value)if(c<=32||c==127||c=='\\')return false;
  size_t end=value.find_first_of("/?#",start);std::string authority=value.substr(start,end-start);
  if(authority.empty()||authority.find('@')!=std::string::npos)return false;
  // The network stack in this build uses IPv4. IPv6 literals must not be
  // mistaken for host:port; reject them with an actionable URL diagnostic.
  if(authority.find('[')!=std::string::npos||authority.find(']')!=std::string::npos)return false;
  size_t colon=authority.find(':');if(colon!=std::string::npos){unsigned p;
    if(!colon||!integer(authority.substr(colon+1),1,65535,p))return false;authority.resize(colon);}
  for(unsigned char c:authority)if(!std::isalnum(c)&&c!='-'&&c!='.')return false;
  return true;
}
// RFC 3986 dot-segment removal, without decoding percent escapes or
// collapsing consecutive slashes (both can matter to the remote server).
inline std::string urlPath(std::string input) {
  std::string output;
  while(!input.empty()){
    if(input.compare(0,3,"../")==0)input.erase(0,3);
    else if(input.compare(0,2,"./")==0)input.erase(0,2);
    else if(input.compare(0,3,"/./")==0)input.erase(0,2);
    else if(input=="/.")input="/";
    else if(input.compare(0,4,"/../")==0||input=="/.."){
      if(input=="/..")input="/";else input.erase(0,3);
      size_t slash=output.rfind('/');output.erase(slash==std::string::npos?0:slash);
    }else if(input=="."||input=="..")input.clear();
    else {size_t slash=input.find('/',input[0]=='/'?1:0);if(slash==std::string::npos)slash=input.size();output+=input.substr(0,slash);input.erase(0,slash);}
  }
  return output;
}
inline std::string normalizeUrl(const std::string& value) {
  if(!httpUrl(value))return {};
  std::string result=value.substr(0,value.find('#'));
  const size_t host=result.find("://")+3;
  size_t start=result.find_first_of("/?",host);
  if(start==std::string::npos)return result+"/";
  if(result[start]=='?')result.insert(start,"/");
  const size_t query=result.find('?',start);
  return result.substr(0,start)+urlPath(result.substr(start,query==std::string::npos?query:query-start))+
    (query==std::string::npos?"":result.substr(query));
}
inline std::string resolveUrl(const std::string& current,const std::string& location) {
  if(location.empty())return {};
  const std::string base=normalizeUrl(current);if(base.empty())return {};
  std::string next=location.substr(0,location.find('#'));
  if(next.compare(0,7,"http://")==0||next.compare(0,8,"https://")==0)return normalizeUrl(next);
  const size_t scheme=base.find("://"),path=base.find('/',scheme+3);
  const std::string origin=base.substr(0,path);
  if(next.compare(0,2,"//")==0)return normalizeUrl(base.substr(0,scheme+1)+next);
  // A fragment-only reference retains the entire current query; a query-only
  // reference replaces the query but keeps the current filename.
  if(next.empty())return base;
  const size_t query=base.find('?',path);const std::string document=base.substr(0,query);
  if(next[0]=='?')return normalizeUrl(document+next);
  if(next[0]=='/')return normalizeUrl(origin+next);
  const size_t colon=next.find(':'),separator=next.find_first_of("/?");
  if(colon!=std::string::npos&&(separator==std::string::npos||colon<separator))return {};
  return normalizeUrl(document.substr(0,document.rfind('/')+1)+next);
}
inline bool parse(const std::vector<std::string>& args,Options& o) {
  o={};if(args.empty())return false;o.command=args[0];
  if(o.command=="nslookup")o.command="dns";
  if(o.command=="ifconfig"||o.command=="ip")o.command="netinfo";
  if(o.command=="nc")o.command="tcp";
  const auto& c=o.command;
  if(c!="dns"&&c!="ping"&&c!="tcp"&&c!="ports"&&c!="netinfo"&&c!="wget"&&c!="curl"&&c!="headers")return false;
  o.timeoutMs=c=="ping"?1000:(c=="wget"||c=="curl"||c=="headers")?120000:1500;
  o.head=c=="headers";std::vector<std::string> positional;
  auto fail=[&](const char* text){o.error=text;return true;};
  bool options=true;
  for(size_t i=1;i<args.size();++i){const auto& a=args[i];if(options&&a=="--"){options=false;continue;}
    if(options&&a.size()>1&&a[0]=='-'){
      auto value=[&]()->const std::string*{return i+1<args.size()?&args[++i]:nullptr;};
      if(c=="ping"&&(a=="-c"||a=="--count")){auto* v=value();if(!v||!integer(*v,1,100,o.count))return fail("-c : indiquez un nombre de requêtes de 1 à 100.");}
      else if((c=="ping"&&(a=="-W"||a=="--timeout"))||((c=="tcp"||c=="ports")&&(a=="-w"||a=="--timeout"))){auto* v=value();if(!v||!seconds(*v,o.timeoutMs))return fail("Délai : indiquez une durée de 0,05 à 30 secondes (exemple : 1.5).");}
      else if((c=="wget"||c=="curl"||c=="headers")&&(a=="-T"||a=="--max-time"||a=="--timeout")){auto* v=value();if(!v||!seconds(*v,o.timeoutMs,3600)||o.timeoutMs<1000)return fail("Durée totale : indiquez de 1 à 3600 secondes.");}
      else if(c=="curl"&&(a=="-O"||a=="--remote-name"))o.remoteName=true;
      else if((c=="curl"||c=="wget")&&(a=="-o"||(c=="wget"&&a=="-O")||a=="--output"||a=="--output-document")){auto* v=value();if(!v||v->empty())return fail("Indiquez le fichier de destination après -o ou -O.");o.destination=*v;}
      else if((c=="curl"||c=="headers")&&(a=="-I"||a=="--head"))o.head=true;
      else if((c=="wget"||c=="curl"||c=="headers")&&(a=="-L"||a=="--location"))o.follow=true;
      else if((c=="wget"||c=="curl")&&a=="--overwrite")o.overwrite=true;
      else if(c=="tcp"&&a=="-z"){} // nc -z: connect without sending a payload.
      else return fail("Option inconnue ou non prise en charge. Consultez help suivi du nom de la commande.");
    }else positional.push_back(a);
  }
  if(c=="netinfo"){
    if(!positional.empty()&&!(args[0]=="ip"&&positional.size()==1&&(positional[0]=="addr"||positional[0]=="route")))return fail("netinfo ne prend pas de paramètre ; ip addr et ip route sont également disponibles.");
  }else if(c=="wget"||c=="curl"||c=="headers"){
    if(positional.size()!=1||!httpUrl(positional[0]))return fail("Indiquez une URL http:// ou https:// valide, sans identifiant ni mot de passe.");o.url=positional[0];
    if(o.remoteName&&!o.destination.empty())return fail("Choisissez -o fichier ou -O ; ces options ne se combinent pas.");
    if((c=="wget"||o.remoteName)&&o.destination.empty()){
      auto path=o.url.substr(o.url.find("://")+3);auto slash=path.find('/');path=slash==std::string::npos?"":path.substr(slash+1);
      path.resize(path.find_first_of("?#")==std::string::npos?path.size():path.find_first_of("?#"));
      size_t last=path.rfind('/');o.destination=last==std::string::npos?path:path.substr(last+1);if(o.destination.empty())o.destination="index.html";
    }
    if(o.head&&!o.destination.empty())return fail("-I affiche les en-têtes ; retirez -I pour enregistrer le contenu.");
  }else{
    const size_t required=(c=="tcp"||c=="ports")?2:1;
    if(positional.size()!=required)return fail("Indiquez un hôte, puis le port ou la liste de ports si nécessaire.");o.host=positional[0];
    if(c=="tcp"){unsigned port;if(!integer(positional[1],1,65535,port))return fail("Port : indiquez un entier de 1 à 65535.");o.ports.push_back(port);}
    if(c=="ports"&&!portList(positional[1],o.ports))return fail("Ports : utilisez 22,80,443 ou 8000-8010 ; au maximum 1024 ports distincts.");
  }
  return true;
}
}
