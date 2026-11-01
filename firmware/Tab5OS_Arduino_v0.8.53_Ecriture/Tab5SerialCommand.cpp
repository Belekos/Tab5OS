#include "Tab5Serial.h"
#include <cstdlib>
#include <cerrno>
#ifdef ARDUINO
#include <Arduino.h>
#else
#include <chrono>
#include <thread>
static uint32_t millis(){return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
static void delay(unsigned n){std::this_thread::sleep_for(std::chrono::milliseconds(n));}
#endif
namespace Tab5Serial {
int command(const std::vector<std::string>& args,Output out,Cancel cancel,Translate tr,Input input){
 if(args.empty()||args[0]!="serial")return -1;
 auto print=[&](const std::string& text){out(text.data(),text.size());};
 auto fail=[&](const char* text,int code=1){print(tr(text)+"\n");return code;};
 auto stopped=[&]{return cancel&&cancel();};
 auto number=[](const std::string& text,unsigned& result,unsigned low,unsigned high){char* end=nullptr;errno=0;unsigned long n=strtoul(text.c_str(),&end,10);if(text.empty()||text[0]=='-'||errno||*end||n<low||n>high)return false;result=unsigned(n);return true;};
 if(args.size()==1||args[1]=="status"){
  if(args.size()>2)return fail("Paramètres invalides.",2);auto s=stats();
  print(std::string(s.config.port==Port::Usb?"USB-C":"RS485")+" | "+tr(s.config.mode==Mode::Debug?"Débogage":"Données")+" | "+tr(s.ready?(s.connected?"Connecté":"Déconnecté"):"Indisponible")+"\n");
  if(s.config.port==Port::Rs485)print(std::to_string(s.config.baud)+" | "+(s.config.frame==Frame::E81?"8E1":s.config.frame==Frame::O81?"8O1":s.config.frame==Frame::N82?"8N2":"8N1")+"\n");
  print("RX="+std::to_string(s.received)+" TX="+std::to_string(s.sent)+" pending="+std::to_string(s.pending)+" available="+std::to_string(s.available)+" dropped="+std::to_string(s.dropped)+"\n");return 0;
 }
 const auto& action=args[1];Config next=config();
 if(action=="port"||action=="mode"||action=="baud"||action=="frame"||action=="ending"){
  if(args.size()!=3)return fail("Paramètres invalides.",2);const auto& value=args[2];
  if(action=="port"){if(value!="usb"&&value!="rs485")return fail("Paramètres invalides.",2);next.port=value=="usb"?Port::Usb:Port::Rs485;}
  if(action=="mode"){if(value!="debug"&&value!="data")return fail("Paramètres invalides.",2);next.mode=value=="debug"?Mode::Debug:Mode::Data;}
  if(action=="baud"){unsigned valueNumber;if(!number(value,valueNumber,1200,921600))return fail("Paramètres invalides.",2);next.baud=valueNumber;}
  if(action=="frame"){if(value!="8N1"&&value!="8E1"&&value!="8O1"&&value!="8N2")return fail("Paramètres invalides.",2);next.frame=value=="8E1"?Frame::E81:value=="8O1"?Frame::O81:value=="8N2"?Frame::N82:Frame::N81;}
  if(action=="ending"){if(value!="none"&&value!="lf"&&value!="crlf")return fail("Paramètres invalides.",2);next.ending=value=="none"?Ending::None:value=="lf"?Ending::LF:Ending::CRLF;}
  return configure(next)?0:fail("Série : réglage impossible ou envoi en cours.");
 }
 if(action=="clear"){if(args.size()!=2)return fail("Paramètres invalides.",2);clear();return 0;}
 if(action=="send"||action=="write"){
  auto status=stats();if(!status.ready||status.config.mode!=Mode::Data)return fail("Choisissez le mode Données dans les réglages série.");
  auto send=[&](const char* p,size_t n){uint32_t progress=millis();while(n){if(stopped())return 130;size_t accepted=write(p,n);p+=accepted;n-=accepted;if(accepted)progress=millis();else if(uint32_t(millis()-progress)>=10000)return 1;else delay(5);}return 0;};
  if(args.size()==2){if(!input)return fail("Paramètres invalides.",2);char bytes[512];size_t n;while((n=input(bytes,sizeof(bytes)))){int rc=send(bytes,n);if(rc)return fail(rc==130?"Interrompu":"Série : délai d'envoi dépassé.",rc);}}
  else for(size_t i=2;i<args.size();++i){int rc=i>2?send(" ",1):0;if(!rc)rc=send(args[i].data(),args[i].size());if(rc)return fail(rc==130?"Interrompu":"Série : délai d'envoi dépassé.",rc);}
  if(action=="send"&&status.config.ending!=Ending::None){int rc=send(status.config.ending==Ending::LF?"\n":"\r\n",status.config.ending==Ending::LF?1:2);if(rc)return fail("Série : délai d'envoi dépassé.",rc);}return 0;
 }
 if(action=="read"||action=="listen"){
  unsigned count=BufferSize,timeout=action=="listen"?60000:1000;bool listen=action=="listen";
  if((listen&&args.size()>3)||(!listen&&args.size()>4)||(!listen&&args.size()>=3&&!number(args[2],count,1,1048576))||(args.size()>=(listen?3u:4u)&&!number(args[listen?2:3],timeout,0,3600000)))return fail("Paramètres invalides.",2);
  if(!stats().ready)return fail("Série indisponible.");
  if(stats().config.mode!=Mode::Data)return fail("Choisissez le mode Données dans les réglages série.");
  if(!claimReceive(Client::Terminal))return fail("Série occupée par une autre session.");
  struct ReceiveLease {~ReceiveLease(){releaseReceive(Client::Terminal);}} lease;
  uint32_t started=millis();char bytes[512];
  do{if(stopped())return 130;size_t n=read(bytes,listen?sizeof(bytes):std::min(sizeof(bytes),size_t(count)),Client::Terminal);if(n){out(bytes,n);if(!listen){count-=n;if(!count)break;}}else if(!timeout)break;else delay(5);}while(!timeout||uint32_t(millis()-started)<timeout);return 0;
 }
 return fail("Paramètres invalides.",2);
}
}
