#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>
namespace Tab5Ssh {
constexpr size_t SessionBudget=512*1024, KeyLimit=16384, SecretLimit=1024;
constexpr uint32_t HandshakeTimeout=20000, TransferIdleTimeout=30000;
constexpr size_t ChannelWindow=65536, ChannelPacket=16384;
struct Options {
  std::string action,host,user,key,password,passphrase,command,remote,local,fingerprint;
  uint16_t port=22;
  uint32_t timeout=TransferIdleTimeout;
};
inline bool hostname(std::string& host) {
  if(host.empty()||host.size()>253||host.front()=='.'||host.back()=='.')return false;
  for(char& c:host){if(c>='A'&&c<='Z')c+=32;if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='.'))return false;}
  return true;
}
inline bool validFingerprint(const std::string& value) {
  if(value.size()!=50||value.compare(0,7,"SHA256:"))return false;
  for(size_t n=7;n<value.size();++n){char c=value[n];if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='+'||c=='/'))return false;}
  return true;
}
inline bool parse(const std::vector<std::string>& args,Options& out) {
  if(args.size()<2)return false;
  size_t n=1;
  if(args[0]=="sftp") {
    out.action=args[n++];if(out.action!="get"&&out.action!="put"&&out.action!="ls")return false;
  } else if(args[n]=="keyscan"||args[n]=="trust"||args[n]=="forget")out.action=args[n++];
  else out.action="exec";
  for(;n<args.size();++n){const auto& a=args[n];if(a=="--"){++n;break;}if(a.empty()||a[0]!='-')break;
    if(n+1>=args.size())return false;const auto& value=args[++n];
    if(a=="-p"){if(value.empty()||value.size()>5||value.find_first_not_of("0123456789")!=std::string::npos)return false;unsigned long p=strtoul(value.c_str(),nullptr,10);if(p<1||p>65535)return false;out.port=static_cast<uint16_t>(p);}
    else if(a=="--timeout"){if(value.empty()||value.size()>4||value.find_first_not_of("0123456789")!=std::string::npos)return false;unsigned long seconds=strtoul(value.c_str(),nullptr,10);if(seconds<1||seconds>3600)return false;out.timeout=static_cast<uint32_t>(seconds*1000);}
    else if(a=="-i")out.key=value;
    else if(a=="--password-file")out.password=value;
    else if(a=="--passphrase-file")out.passphrase=value;
    else return false;
  }
  if(n>=args.size())return false;std::string target=args[n++];auto at=target.find('@');
  const bool authenticated=out.action=="exec"||args[0]=="sftp";
  if(authenticated){if(at==std::string::npos||!at||at>64)return false;out.user=target.substr(0,at);out.host=target.substr(at+1);for(unsigned char c:out.user)if(c<33||c==127||c=='@')return false;
    if(out.key.empty()==out.password.empty()||(!out.passphrase.empty()&&out.key.empty()))return false;
  }else{if(at!=std::string::npos||!out.key.empty()||!out.password.empty()||!out.passphrase.empty())return false;out.host=target;}
  if(!hostname(out.host))return false;
  if(out.action=="exec"){if(n>=args.size())return false;for(;n<args.size();++n){if(!out.command.empty())out.command+=' ';out.command+=args[n];if(out.command.size()>4096)return false;}return !out.command.empty();}
  if(out.action=="trust"){if(n+1!=args.size()||!validFingerprint(args[n]))return false;out.fingerprint=args[n];return true;}
  if(out.action=="keyscan"||out.action=="forget")return n==args.size();
  if(n>=args.size())return false;out.remote=args[n++];if(out.remote.empty()||out.remote.size()>1024)return false;
  if(out.action!="ls"){if(n>=args.size())return false;out.local=args[n++];}
  return n==args.size();
}
// Comparing a pinned public fingerprint is not secret-dependent; process all
// bytes nevertheless, and never accept a shortened/prefix fingerprint.
inline bool sameFingerprint(const char* a,const char* b) {
  unsigned diff=0;for(size_t n=0;n<50;++n)diff|=static_cast<unsigned char>(a[n]^b[n]);return diff==0;
}
}
