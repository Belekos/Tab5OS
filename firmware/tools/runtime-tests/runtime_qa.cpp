#include "Tab5ElfValidate.h"
#include "Tab5ResourceManager.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <iterator>
#include <cstring>
using namespace Tab5Resources;
static MemorySample sample{128*KiB,64*KiB,32*KiB,4*MiB,2*MiB};
static MemorySample sampleFn(){return sample;}
static MemoryTotals totalsFn(){return {sample.internal,sample.psram};}
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
static bool cancel(){return false;}
int main(int argc,char** argv){
 unsigned passed=0;bool cancellationTested=false;
 for(int i=1;i<argc;++i){
  std::ifstream f(argv[i],std::ios::binary);require(bool(f),"read ELF");
  std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)),{}),mutated;
  Tab5ElfValidate::Image image;const char* reason=nullptr;
  if(!Tab5ElfValidate::validate(file.data(),file.size(),image,&reason)){std::fprintf(stderr,"FAIL %s: %s\n",argv[i],reason);return 1;}
  std::printf("PASS %s: image=%u stack=%u\n",argv[i],image.bytes,image.stack);++passed;
  for(size_t n=0;n<52;++n)require(!Tab5ElfValidate::validate(file.data(),n,image,&reason),"short ELF must reject");
  require(!Tab5ElfValidate::validate(file.data(),file.size()/2,image,&reason),"truncated ELF must reject");
  mutated=file;mutated[0]=0;require(!Tab5ElfValidate::validate(mutated.data(),mutated.size(),image,&reason),"bad magic");
  mutated=file;std::memset(mutated.data()+28,0xff,4);require(!Tab5ElfValidate::validate(mutated.data(),mutated.size(),image,&reason),"bad program header offset");
  mutated=file;std::memset(mutated.data()+32,0xff,4);require(!Tab5ElfValidate::validate(mutated.data(),mutated.size(),image,&reason),"bad section header offset");
  if(file.size()>500000){require(!Tab5ElfValidate::validate(file.data(),file.size(),image,&reason,cancel),"cooperative validation cancellation");cancellationTested=true;}
 }
 require(passed>0,"ELF corpus");
 require(cancellationTested,"large ELF cancellation tested");
 Manager manager(sampleFn,totalsFn);
 Budget b{};b.psram=2*MiB;b.psramBlock=2*MiB;
 auto a=manager.reserve(Owner::Tools,b);require(bool(a),"first reserve");
 AdmissionReport report;auto denied=manager.reserve(Owner::Tools,b,&report);require(!denied&&report.reason==Denial::Psram,"pending budget excludes second reserve");
 a.release();sample.psramBlock=128*KiB;b.psram=256*KiB;b.psramBlock=256*KiB;
 denied=manager.reserve(Owner::Tools,b,&report);require(!denied&&report.reason==Denial::PsramBlock,"fragmented PSRAM stack rejected");
 sample.psramBlock=2*MiB;auto recovered=manager.reserve(Owner::Tools,b);require(bool(recovered),"reservation reusable");
 recovered.allocated(0,256*KiB);recovered.release();
 std::printf("PASS memory admission / release / fragmented stack. ELF corpus=%u\n",passed);
 return 0;
}
