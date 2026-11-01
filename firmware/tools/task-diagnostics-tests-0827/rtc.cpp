#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <esp_system.h>
static esp_reset_reason_t resetReason=ESP_RST_POWERON;
esp_reset_reason_t esp_reset_reason(){return resetReason;}
#include "Tab5Diagnostics.cpp"
static unsigned checks=0;
static void check(bool value,const char* text){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
static void boot(esp_reset_reason_t reason){resetReason=reason;tab5DiagnosticsBegin();}
static void empty(){check(!*tab5DiagnosticPrevious(),"no previous active context");}
int main(){
 check(sizeof(Record)==968&&sizeof(Record)<1024,"seven-slot RTC record remains below 1 KiB");
 ready=false;Record before=retained;tab5DiagnosticMark(6,"unlock","ignored",1);check(!memcmp(&before,&retained,sizeof(before)),"marks before initialization do not write RTC");
 boot(ESP_RST_POWERON);empty();check(Format==4&&retained.version==4&&valid(retained),"new version has valid checksum");
 before=retained;tab5DiagnosticMark(7,"outside","",1);tab5DiagnosticMark(~0U,"outside","",1);check(!memcmp(&before,&retained,sizeof(before)),"out of range slots do not write RTC");
 const char* phases[]={"unlock","auth","preload","await-ui","ui-rebuild"};
 for(const char* phase:phases){
  boot(ESP_RST_POWERON);tab5DiagnosticMark(6,phase,"PIN-or-token-must-not-be-retained",1);
  check(retained.stages[6].active==1&&!retained.stages[6].detail[0],"vault active and detail discarded");
  before=retained;tab5DiagnosticMark(6,phase,"another discarded value",1);check(!memcmp(&before,&retained,sizeof(before)),"identical mark does not rewrite RTC");
  boot(ESP_RST_PANIC);std::string previous=tab5DiagnosticPrevious();check(previous.find("Coffre : "+std::string(phase))!=std::string::npos,"every in-progress vault phase survives warm reset");
  check(previous.find("cause non établie")!=std::string::npos,"context is not presented as cause");
  check(previous.find("PIN")==std::string::npos&&previous.find("token")==std::string::npos,"secret detail absent after reset");
  tab5DiagnosticMark(0,"new-boot","",1);check(previous==tab5DiagnosticPrevious(),"previous report immutable during current boot");
 }
 boot(ESP_RST_POWERON);for(const char* phase:phases)tab5DiagnosticMark(6,phase,"",1);
 check(retained.stages[6].active&&std::string(retained.stages[6].phase)=="ui-rebuild","handoff stays active through UI reconstruction");
 tab5DiagnosticMark(6,"ready","",0);boot(ESP_RST_PANIC);empty();
 boot(ESP_RST_POWERON);tab5DiagnosticMark(6,"unlock","",1);tab5DiagnosticMark(6,"failed","",0);boot(ESP_RST_TASK_WDT);empty();
 const esp_reset_reason_t warm[]={ESP_RST_EXT,ESP_RST_SW,ESP_RST_PANIC,ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT,ESP_RST_DEEPSLEEP,ESP_RST_SDIO,ESP_RST_USB,ESP_RST_JTAG,ESP_RST_CPU_LOCKUP};
 for(auto reason:warm){boot(ESP_RST_POWERON);tab5DiagnosticMark(6,"preload","",1);boot(reason);check(strstr(tab5DiagnosticPrevious(),"Coffre : preload"),"supported warm reset preserves vault marker");}
 const esp_reset_reason_t cold[]={ESP_RST_UNKNOWN,ESP_RST_POWERON,ESP_RST_BROWNOUT,ESP_RST_PWR_GLITCH};
 for(auto reason:cold){boot(ESP_RST_POWERON);tab5DiagnosticMark(6,"preload","",1);boot(reason);empty();}
 boot(ESP_RST_POWERON);tab5DiagnosticMark(6,"preload","",1);Record good=retained;
 for(size_t byte=0;byte<sizeof(Record);++byte){retained=good;reinterpret_cast<unsigned char*>(&retained)[byte]^=1;boot(ESP_RST_PANIC);empty();check(valid(retained),"corruption replaced with fresh valid record");}
 retained=good;retained.version=3;commit();boot(ESP_RST_PANIC);empty();check(retained.version==4,"previous RTC format rejected without reinterpretation");
 retained=good;retained.size-=sizeof(Stage);commit();boot(ESP_RST_PANIC);empty();
 retained=good;retained.stages[6].active=2;commit();boot(ESP_RST_PANIC);empty();
 retained=good;memset(retained.stages[6].phase,'x',sizeof(retained.stages[6].phase));commit();boot(ESP_RST_PANIC);empty();
 retained=good;memset(retained.stages[0].detail,'x',sizeof(retained.stages[0].detail));commit();boot(ESP_RST_PANIC);empty();
 std::string phase(100,'p'),detail(200,'d');boot(ESP_RST_POWERON);
 for(unsigned slot=0;slot<Slots;++slot)tab5DiagnosticMark(slot,phase.c_str(),detail.c_str(),1);
 boot(ESP_RST_PANIC);check(strstr(tab5DiagnosticPrevious(),"Coffre : ppppppppppppppppppppppppppppppp"),"last slot remains fully visible with all slots at capacity");
 check(strlen(tab5DiagnosticPrevious())<sizeof(previous)-1,"maximum report not truncated");
 boot(ESP_RST_POWERON);std::string unicode(30,'p');unicode+="é";tab5DiagnosticMark(6,unicode.c_str(),"",1);check(strlen(retained.stages[6].phase)==30,"phase truncation preserves UTF-8 boundary");
 tab5DiagnosticMark(6,nullptr,nullptr,0);check(!retained.stages[6].phase[0]&&!retained.stages[6].active,"null strings end activity safely");
 std::printf("PASS %u RTC diagnostic checks; recordBytes=%zu; slots=%u; format=%u; corruptions=%zu; SD_writes=0; hardware_tested=false\n",checks,sizeof(Record),Slots,Format,sizeof(Record));return 0;
}
