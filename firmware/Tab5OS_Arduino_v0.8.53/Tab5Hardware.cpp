#include "Tab5Hardware.h"
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <esp32-hal-i2c.h>
#include <driver/uart.h>
#include <M5Unified.h>
#include <freertos/semphr.h>
#include <cerrno>
#include <algorithm>
namespace Tab5Hardware {namespace {
struct PinState {uint8_t owner=0,kind=0;};PinState pins[55];
uint8_t i2cOwner=0,spiOwner=0,uartOwner=0;
TwoWire externalI2C(0);SPIClass externalSPI(HSPI);HardwareSerial externalUART(1);
StaticSemaphore_t mutexStorage;
SemaphoreHandle_t mutex(){static auto m=xSemaphoreCreateMutexStatic(&mutexStorage);return m;}
struct Lock {Lock(){xSemaphoreTake(mutex(),portMAX_DELAY);}~Lock(){xSemaphoreGive(mutex());}};
bool allowed(int p){for(int pin:{2,3,4,5,6,7,16,17,18,19,20,21,34,35,37,38,45,47,48,51,52,53,54})if(p==pin)return true;return false;}
bool canClaim(int p,uint8_t owner,int kind){return allowed(p)&&(!pins[p].owner||(pins[p].owner==owner&&(pins[p].kind==kind||pins[p].kind==1)));}
void claim(int p,uint8_t owner,int kind){pins[p]={owner,uint8_t(kind)};}
void clear(uint8_t owner,int kind){for(unsigned p=0;p<55;++p)if(pins[p].owner==owner&&pins[p].kind==kind){pinMode(p,INPUT);pins[p]={};}}
}
void release(Owner who){
 Lock lock;uint8_t owner=uint8_t(who);
 if(i2cOwner==owner){externalI2C.end();i2cOwner=0;}
 if(spiOwner==owner){externalSPI.end();spiOwner=0;}
 if(uartOwner==owner){externalUART.end();uartOwner=0;}
 for(unsigned p=0;p<55;++p)if(pins[p].owner==owner){if(pins[p].kind==2)ledcDetach(p);pinMode(p,INPUT);pins[p]={};}
}
int call(Owner who,int op,int a,int b,int c,const void* input,size_t len,void* output,size_t capacity){
 if(len>4096||capacity>4096||(len&&!input)||(capacity&&!output))return -EINVAL;
 const uint8_t owner=uint8_t(who);if(owner!=1&&owner!=2)return -EINVAL;Lock lock;
 auto has=[&](int p,int kind){return allowed(p)&&pins[p].owner==owner&&pins[p].kind==kind;};
 switch(op){
 case 2:if(b<0||b>2||c<0||c>2||!allowed(a))return -EINVAL;if(!canClaim(a,owner,1))return -EBUSY;claim(a,owner,1);pinMode(a,b==1?OUTPUT:b==2?OUTPUT_OPEN_DRAIN:c==1?INPUT_PULLUP:c==2?INPUT_PULLDOWN:INPUT);return 0;
 case 3:if(!has(a,1))return -EBUSY;return digitalRead(a);
 case 4:if(!has(a,1))return -EBUSY;digitalWrite(a,b!=0);return 0;
 case 5:
#if SOC_ADC_SUPPORTED
  if(!allowed(a)||digitalPinToAnalogChannel(a)<0||b<0||b>3)return -EINVAL;
  if(!canClaim(a,owner,6))return -EBUSY;claim(a,owner,6);analogSetPinAttenuation(a,adc_attenuation_t(b));return c?int(analogReadMilliVolts(a)):int(analogRead(a));
#else
  return -ENOTSUP;
#endif
 case 6:{if(b<1||b>40000||c<0||c>65535||!allowed(a))return -EINVAL;if(!canClaim(a,owner,2))return -EBUSY;bool attached=has(a,2);if(attached?!ledcChangeFrequency(a,b,12):!ledcAttach(a,b,12))return -EIO;claim(a,owner,2);ledcWrite(a,(uint32_t(c)*4095)/65535);return 0;}
 case 7:if(!has(a,2))return -EBUSY;ledcDetach(a);pinMode(a,INPUT);pins[a]={};return 0;
 case 9:if(!allowed(a))return -EINVAL;if(pins[a].owner&&pins[a].owner!=owner)return -EBUSY;if(pins[a].kind>2&&pins[a].kind!=6)return -EBUSY;if(pins[a].kind==2)ledcDetach(a);pinMode(a,INPUT);pins[a]={};return 0;
 case 10:if(i2cOwner||i2cIsInit(0))return -EBUSY;if(a==b||c<10000||c>1000000||!allowed(a)||!allowed(b))return -EINVAL;if(!canClaim(a,owner,3)||!canClaim(b,owner,3))return -EBUSY;externalI2C.setTimeOut(100);if(!externalI2C.begin(a,b,c))return -EIO;claim(a,owner,3);claim(b,owner,3);i2cOwner=owner;return 0;
 case 11:if(i2cOwner!=owner)return -EBUSY;if(a<8||a>119)return -EINVAL;externalI2C.beginTransmission(a);if(externalI2C.write((const uint8_t*)input,len)!=len){externalI2C.endTransmission(true);return -ENOSPC;}return externalI2C.endTransmission(b!=0)?-EIO:int(len);
 case 12:if(i2cOwner!=owner)return -EBUSY;if(a<8||a>119||capacity>128)return -EINVAL;{size_t got=externalI2C.requestFrom(uint8_t(a),capacity,b!=0);for(size_t i=0;i<got;++i)((uint8_t*)output)[i]=externalI2C.read();return int(got);}
 case 13:if(i2cOwner&&i2cOwner!=owner)return -EBUSY;if(i2cOwner)externalI2C.end();i2cOwner=0;clear(owner,3);return 0;
 case 20:if(spiOwner)return -EBUSY;if(a==b||a==c||b==c||!allowed(a)||!allowed(b)||!allowed(c))return -EINVAL;if(!canClaim(a,owner,4)||!canClaim(b,owner,4)||!canClaim(c,owner,4))return -EBUSY;if(!externalSPI.begin(a,c,b,-1))return -EIO;claim(a,owner,4);claim(b,owner,4);claim(c,owner,4);spiOwner=owner;return 0;
 case 21:if(spiOwner!=owner)return -EBUSY;if(a<1000||a>10000000||b<0||b>3||len!=capacity)return -EINVAL;externalSPI.beginTransaction(SPISettings(a,MSBFIRST,b));externalSPI.transferBytes((const uint8_t*)input,(uint8_t*)output,len);externalSPI.endTransaction();return int(len);
 case 22:if(spiOwner&&spiOwner!=owner)return -EBUSY;if(spiOwner)externalSPI.end();spiOwner=0;clear(owner,4);return 0;
 case 30:if(uartOwner||uart_is_driver_installed(UART_NUM_1))return -EBUSY;if(a==b||c<1200||c>1000000||!allowed(a)||!allowed(b))return -EINVAL;if(!canClaim(a,owner,5)||!canClaim(b,owner,5))return -EBUSY;externalUART.begin(c,SERIAL_8N1,b,a);if(!externalUART)return -EIO;claim(a,owner,5);claim(b,owner,5);uartOwner=owner;return 0;
 case 31:if(uartOwner!=owner)return -EBUSY;{size_t n=std::min(len,size_t(std::max(0,externalUART.availableForWrite())));return n?int(externalUART.write((const uint8_t*)input,n)):0;}
 case 32:if(uartOwner!=owner)return -EBUSY;{size_t n=0;while(n<capacity&&externalUART.available())((uint8_t*)output)[n++]=externalUART.read();return int(n);}
 case 33:if(uartOwner&&uartOwner!=owner)return -EBUSY;if(uartOwner)externalUART.end();uartOwner=0;clear(owner,5);return 0;
 case 34:return uartOwner==owner?externalUART.available():-EBUSY;
 case 35:return uartOwner==owner?(uart_wait_tx_done(UART_NUM_1,0)==ESP_OK?1:0):-EBUSY;
 case 36: // GPIO availability: 0 board-reserved, 1 free/owned, 2 another app.
  return allowed(a)?(!pins[a].owner||pins[a].owner==owner?1:2):0;
 case 40:M5.Power.setExtOutput(a!=0,m5::ext_PA);return 0;
 default:return -ENOSYS;
 }
}
}
