// Include the actual production translation unit to inspect retained-byte
// corruption explicitly. No test-only production branches or replacement logic.
#include "Tab5UiDiagnostics.cpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

static uint64_t nowUs=1000000;
static unsigned clockCalls=0,displayStarts=0,displayEnds=0,checks=0;
static esp_reset_reason_t reason=ESP_RST_POWERON;
static unsigned allocationCalls=0,freeCalls=0,liveAllocations=0,failedAllocations=0;
static size_t allocatedBytes=0;
static bool failAllocation=false;
void* heap_caps_malloc(size_t bytes,uint32_t caps){
 ++allocationCalls;
 if(caps!=(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))std::abort();
 if(failAllocation){failAllocation=false;++failedAllocations;return nullptr;}
 auto* p=std::malloc(bytes);if(p){++liveAllocations;allocatedBytes+=bytes;}return p;
}
void heap_caps_free(void* p){if(!p)return;if(!liveAllocations)std::abort();++freeCalls;--liveAllocations;allocatedBytes-=sizeof(Tab5UiDiagnostics::Runtime);std::free(p);}
int64_t esp_timer_get_time(){++clockCalls;return static_cast<int64_t>(nowUs);}
esp_reset_reason_t esp_reset_reason(){return reason;}
namespace Tab5DisplayActivity {
const char* sourceName(Source){return "Écran";}
Scope::Scope(Source):previous_(Source::Other){++displayStarts;}
Scope::~Scope(){++displayEnds;}
}
static void check(bool condition,const char* text){++checks;if(!condition){std::cerr<<text<<'\n';std::exit(1);}}
using namespace Tab5UiDiagnostics;
static void duration(unsigned phase,uint64_t us){Scope scope(phase);nowUs+=us;}
static std::string output(bool previous=false){char data[2048];const size_t n=recent(data,sizeof(data),previous);check(n==strlen(data),"output length");return data;}
static void fresh(){reason=ESP_RST_POWERON;begin();check(enable(true),"fresh activation succeeds");nowUs+=2000000;}
static bool utf8(const std::string& text){
 unsigned need=0;uint32_t value=0,minimum=0;
 for(unsigned char c:text){if(!need){if(c<0x80)continue;if((c&0xe0)==0xc0){need=1;value=c&31;minimum=0x80;}else if((c&0xf0)==0xe0){need=2;value=c&15;minimum=0x800;}else if((c&0xf8)==0xf0){need=3;value=c&7;minimum=0x10000;}else return false;}
 else{if((c&0xc0)!=0x80)return false;value=(value<<6)|(c&63);if(!--need&&(value<minimum||value>0x10ffff||(value>=0xd800&&value<=0xdfff)))return false;}}
 return !need;
}
int main(){
 reason=ESP_RST_POWERON;begin();check(!enabled(),"disabled default");
 check(allocationCalls==0&&liveAllocations==0,"no allocation at startup");
 check(enable(false)&&allocationCalls==0,"disabling untouched profiler allocates nothing");
 failAllocation=true;const auto oomClock=clockCalls;
 check(!enable(true)&&!enabled()&&!runtime&&liveAllocations==0,"PSRAM refusal does not enable or fall back");
 check(clockCalls==oomClock&&snapshot().phases[Input].calls==0,"failed activation leaves zero state and reads no clock");
 check(enable(true)&&runtime&&liveAllocations==1,"retry after OOM succeeds with one state");
 duration(Input,25000);const auto retained=snapshot();const auto retainedJournal=output();
 const auto allocBeforeDisable=allocationCalls;
 check(enable(false)&&!enabled()&&liveAllocations==1,"disable retains one inspectable state");
 check(snapshot().phases[Input].calls==retained.phases[Input].calls&&output()==retainedJournal,"disabled snapshot and journal retained");
 check(enable(false)&&enable(true)&&allocationCalls==allocBeforeDisable,"reenable reuses state without allocation");
 check(snapshot().phases[Input].calls==0,"reenable resets counters");
 begin();check(liveAllocations==0&&!runtime&&!enabled(),"begin releases optional state");
 {enable(true);Scope old(Input);nowUs+=30000;begin();enable(true);nowUs+=30000;}
 check(snapshot().phases[Input].calls==0,"reset and possible pointer reuse reject stale scope");
 begin();

 const auto calls=clockCalls;
 {Scope a(Input);Activity b(Tab5DisplayActivity::Source::Map);loopBegin();loopEnd();}
 check(clockCalls==calls,"disabled path reads no clock");
 check(displayStarts==1&&displayEnds==1,"display attribution retained while timings disabled");
 check(output().empty()&&output(true).empty(),"cold starts empty");
 fresh();duration(Input,19999);auto s=snapshot();
 check(s.phases[Input].calls==1&&s.phases[Input].slow==0&&s.phases[Input].totalUs==19999,"short sample counters");
 check(output().empty(),"threshold exclusive below 20ms");
 duration(Input,20000);check(snapshot().phases[Input].slow==1&&currentJournal.count==1,"20ms threshold inclusive");
 duration(Input,50000);check(snapshot().phases[Input].slow==2&&currentJournal.count==1,"same phase rate limit");
 duration(Services,20000);check(currentJournal.count==2,"rate limit independent per phase");
 nowUs=runtime->lastEvent[Input]+1000000-20000;duration(Input,20000);check(currentJournal.count==3,"rate at exactly one second");
 fresh();{Scope old(Input);nowUs+=30000;enable(false);enable(true);nowUs+=30000;}
 check(snapshot().phases[Input].calls==0&&output().empty(),"generation rejects scope spanning reenable");
 {Scope old(Input);nowUs+=30000;enable(false);}
 check(snapshot().phases[Input].calls==0,"disabled scope end ignored");
 enable(true);{Scope old(Input);nowUs+=30000;enable(true);}
 check(snapshot().phases[Input].calls==1,"idempotent enable keeps active scope");
 auto before=snapshot();duration(Count+4,30000);check(snapshot().phases[Input].calls==before.phases[Input].calls,"invalid phase ignored");
 fresh();loopBegin();nowUs+=5000;loopEnd();nowUs+=90000;loopBegin();nowUs+=20000;loopEnd();s=snapshot();
 check(s.phases[Loop].calls==2&&s.phases[Loop].totalUs==25000&&s.phases[Loop].maxUs==20000,"active loop excludes inter-loop wait");
 check(s.maxGapUs==95000,"gap measures start to start separately");
 loopBegin();enable(false);nowUs+=100000;loopEnd();enable(true);loopEnd();check(snapshot().phases[Loop].calls==0,"loop generation rejects reenable");
 fresh();std::vector<uint32_t> times;
 for(unsigned i=0;i<12;++i){nowUs+=1000000;duration(Input,20000+i);times.push_back(uint32_t(nowUs/1000));}
 check(currentJournal.count==8&&currentJournal.next==4&&valid(currentJournal),"ring bounded and checksum valid after wrap");
 for(unsigned i=0;i<8;++i)check(currentJournal.events[(currentJournal.next+i)%8].atMs==times[i+4],"ring retains chronological newest eight");
 auto lines=output();check(lines.find(std::to_string(times[4])+" ms")==0,"formatted oldest retained first");
 check(lines.find(std::to_string(times[11])+" ms")!=std::string::npos,"formatted newest retained");
 fresh();duration(Input,20000);duration(Services,20000);const auto full=output();check(utf8(full),"fixture UTF8");
 for(size_t cap=1;cap<=full.size()+3;++cap){std::vector<char> buffer(cap+8,'!');const auto n=recent(buffer.data(),cap);check(n<cap&&buffer[n]==0,"bounded terminator");check(strlen(buffer.data())==n,"bounded length");check(utf8(buffer.data()),"never split multibyte label");check(full.compare(0,n,buffer.data())==0,"whole chronological prefix");for(size_t i=cap;i<buffer.size();++i)check(buffer[i]=='!',"guard bytes untouched");if(n)check(n==full.size()||full[n]=='\n',"whole lines only");}
 char untouched='!';check(recent(&untouched,0)==0&&untouched=='!',"zero capacity untouched");check(recent(nullptr,20)==0,"null output accepted");
 const esp_reset_reason_t warmReasons[]={ESP_RST_EXT,ESP_RST_SW,ESP_RST_PANIC,ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT,ESP_RST_DEEPSLEEP,ESP_RST_SDIO,ESP_RST_USB,ESP_RST_JTAG,ESP_RST_CPU_LOCKUP};
 for(auto r:warmReasons){fresh();duration(Input,24000);auto old=output();reason=r;begin();check(!enabled()&&output().empty()&&output(true)==old,"warm reset retains only valid prior journal");}
 fresh();duration(Input,25000);const auto prior=output();reason=ESP_RST_PANIC;begin();
 failAllocation=true;check(!enable(true)&&!enabled()&&output(true)==prior,"failed activation preserves retained reset evidence");
 check(enable(true)&&liveAllocations==1,"second OOM retry succeeds");
 const esp_reset_reason_t coldReasons[]={ESP_RST_UNKNOWN,ESP_RST_POWERON,ESP_RST_BROWNOUT};
 for(auto r:coldReasons){fresh();duration(Input,24000);reason=r;begin();check(output().empty()&&output(true).empty(),"unreliable reset discards journal");}
 for(unsigned fault=0;fault<5;++fault){fresh();duration(Input,24000);switch(fault){case 0:currentJournal.magic=0;seal(currentJournal);break;case 1:currentJournal.count=9;seal(currentJournal);break;case 2:currentJournal.next=8;seal(currentJournal);break;case 3:currentJournal.events[0].phase=Count;seal(currentJournal);break;case 4:currentJournal.checksum^=1;break;}check(output().empty(),"invalid retained data not formatted");reason=ESP_RST_PANIC;begin();check(output(true).empty(),"invalid retained data not restored");}
 fresh();duration(Input,uint64_t(UINT32_MAX)+123);s=snapshot();check(s.phases[Input].maxUs==UINT32_MAX,"duration maximum saturates");check(s.phases[Input].totalUs==uint64_t(UINT32_MAX)+123,"duration total preserves 64-bit value");
 check(2*sizeof(Journal)==216,"retained journals exactly 216 bytes");
 begin();check(liveAllocations==0&&allocatedBytes==0&&allocationCalls==freeCalls+failedAllocations,"all successful optional allocations released; injected refusals excluded");
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"retainedBytes\":"<<2*sizeof(Journal)<<",\"statsBytes\":"<<sizeof(Stats)<<",\"runtimeBytes\":"<<sizeof(Runtime)<<",\"failedAllocations\":"<<failedAllocations<<",\"physicalDeviceTested\":false}\n";
}
