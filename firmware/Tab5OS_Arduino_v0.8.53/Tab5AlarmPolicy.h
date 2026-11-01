#pragma once
#include <stdint.h>
#include <time.h>
namespace Tab5AlarmPolicy {
struct State {
  uint32_t version=1;
  uint8_t hour=7,minute=0,enabled=0,daily=1,volume=200,reserved[3]{};
  int64_t next=0,snooze=0,sleepUtc=0,sleepRtc=0;
};
inline bool validTime(time_t now){return now>=1704067200LL&&now<4102444800LL;}
inline bool valid(const State& s){return s.version==1&&s.hour<24&&s.minute<60&&s.enabled<2&&s.daily<2&&s.volume&&s.next>=0&&s.next<4102444800LL&&s.snooze>=0&&s.snooze<4102444800LL&&s.sleepUtc>=0&&s.sleepUtc<4102444800LL&&s.sleepRtc>=0&&s.sleepRtc<4102444800LL;}
// RTC stores local wall time. This serial value measures its elapsed time
// without applying timezone rules that can change while the board is off.
inline int64_t civilSeconds(int year,unsigned month,unsigned day,unsigned hour,unsigned minute,unsigned second){
 if(year<2024||year>2099||month<1||month>12||day<1||day>31||hour>23||minute>59||second>59)return 0;
 year-=month<=2;const int era=year/400;const unsigned y=unsigned(year-era*400);
 const unsigned doy=(153*(month+(month>2?-3:9))+2)/5+day-1;
 const unsigned doe=y*365+y/4-y/100+doy;
 return (int64_t(era)*146097+doe-719468)*86400+hour*3600+minute*60+second;
}
inline time_t restoredClock(const State& s,int64_t rtc){
 const int64_t elapsed=rtc-s.sleepRtc;
 return validTime(s.sleepUtc)&&s.sleepRtc>0&&elapsed>=0&&elapsed<=3*86400?s.sleepUtc+elapsed:0;
}
inline time_t nextLocal(time_t now,unsigned hour,unsigned minute){
 if(!validTime(now)||hour>23||minute>59)return 0;
 tm local{};if(!localtime_r(&now,&local))return 0;
 for(int day=0;day<3;++day){tm candidate=local;candidate.tm_mday+=day;candidate.tm_hour=hour;candidate.tm_min=minute;candidate.tm_sec=0;candidate.tm_isdst=-1;time_t result=mktime(&candidate);if(result>now)return result;}
 return 0;
}
inline time_t deadline(const State& s){return s.snooze?s.snooze:(s.enabled?s.next:0);}
inline void schedule(State& s,time_t now){s.snooze=0;s.next=s.enabled?nextLocal(now,s.hour,s.minute):0;}
enum class Event {None,Ring,Missed};
inline Event poll(State& s,time_t now){
 if(!validTime(now))return Event::None;
 time_t due=deadline(s);if(!due||now<due)return Event::None;
 if(s.snooze){s.snooze=0;}
 else if(s.daily){
  s.next=nextLocal(now,s.hour,s.minute);
  tm fired{},candidate{};localtime_r(&due,&fired);time_t next=s.next;localtime_r(&next,&candidate);
  if(fired.tm_year==candidate.tm_year&&fired.tm_yday==candidate.tm_yday)s.next=nextLocal(next+60,s.hour,s.minute);
 }
 else {s.enabled=0;s.next=0;}
 // A reboot must not ring an alarm days late, or replay an already consumed event.
 return now-due<=600?Event::Ring:Event::Missed;
}
}
