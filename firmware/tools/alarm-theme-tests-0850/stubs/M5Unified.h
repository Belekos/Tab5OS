#pragma once
#include "Arduino.h"
namespace m5 {struct rtc_datetime_t {struct {int year=2026,month=10,date=29,weekDay=4;}date;struct {int hours=12,minutes=0,seconds=0;}time;};}
struct M5Stub {struct RtcStub {bool isEnabled(){return false;}void disableIRQ(){}void clearIRQ(){}bool getDateTime(m5::rtc_datetime_t*){return false;}void setDateTime(m5::rtc_datetime_t&){}uint32_t setTimerIRQ(uint32_t n){return n;}} Rtc;struct Expander{void digitalWrite(int,bool){}} expander;Expander&getIOExpander(int){return expander;}struct PowerStub {void powerOff(){}}Power;};
extern M5Stub M5;
