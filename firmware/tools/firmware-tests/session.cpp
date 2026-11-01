#include "Platform.h"
#include "Tab5Firmware.cpp"
int main(int argc,char** argv){
 assert(argc==2);const std::string scenario=argv[1];
 using namespace Tab5Firmware;
 mock::file("/firmware.bin");
 auto review=[](){assert(inspect("/firmware.bin"));assert(busy());mock::execute();assert(ready());assert(mock::begins==0);};
 auto run=[](const char* path="/firmware.bin"){assert(install(path));assert(busy());assert(!inspect(path));assert(!install(path));mock::execute();assert(!busy());};
 if(scenario=="direct"){assert(!install());run();assert(installed()&&mock::boots==1&&mock::ends==1&&mock::aborts==0);assert(!install("/firmware.bin"));}
 else if(scenario=="reviewed"){review();assert(install());mock::execute();assert(installed()&&mock::boots==1);}
 else if(scenario=="changed-size"){review();mock::files["/firmware.bin"]->push_back(1);run();assert(!installed()&&mock::begins==0&&mock::boots==0);}
 else if(scenario=="changed-hash"){review();mock::files["/firmware.bin"]->back()^=1;run();assert(!installed()&&mock::aborts==1&&mock::ends==0&&mock::boots==0);}
 else if(scenario=="same-path-case"){review();mock::files["/firmware.bin"]->back()^=1;run("/FIRMWARE.BIN");assert(!installed()&&mock::aborts==1&&mock::boots==0);}
 else if(scenario=="different-file"){review();mock::file("/other.bin",4096);run("/other.bin");assert(installed()&&mock::boots==1);}
 else if(scenario=="clear-review"){review();clearReview();mock::files["/firmware.bin"]->back()^=1;run();assert(installed()&&mock::boots==1);}
 else if(scenario=="invalid-path"){assert(!install("../firmware.bin"));assert(!install("/firmware.elf"));assert(!install("/bad.bin."));assert(mock::begins==0&&!busy());}
 else if(scenario=="missing-file"){run("/missing.bin");assert(!installed()&&mock::begins==0&&mock::boots==0);}
 else if(scenario=="wrong-chip"){auto* h=reinterpret_cast<esp_image_header_t*>(mock::files["/firmware.bin"]->data());h->chip_id=99;run();assert(!installed()&&mock::begins==0&&mock::boots==0);}
 else if(scenario=="wrong-revision"){auto* h=reinterpret_cast<esp_image_header_t*>(mock::files["/firmware.bin"]->data());h->min_chip_rev_full=400;run();assert(!installed()&&mock::begins==0&&mock::boots==0);}
 else if(scenario=="truncated"){mock::files["/firmware.bin"]->resize(4);run();assert(!installed()&&mock::begins==0&&mock::boots==0);}
 else if(scenario=="oversize"){mock::target.size=4096;run();assert(!installed()&&mock::begins==0&&mock::boots==0);}
 else if(scenario=="begin-failure"){mock::failBegin=true;run();assert(!installed()&&mock::boots==0&&mock::writes==0);}
 else if(scenario=="write-failure"){mock::failWrite=true;run();assert(!installed()&&mock::aborts==1&&mock::boots==0);}
 else if(scenario=="integrity-failure"){mock::failEnd=true;run();assert(!installed()&&mock::ends==1&&mock::boots==0);}
 else if(scenario=="cancel"){mock::cancelWrite=true;run();assert(!installed()&&mock::aborts==1&&mock::ends==0&&mock::boots==0);}
 else if(scenario=="busy-storage"){mock::leaseAvailable=false;run();assert(!installed()&&mock::begins==0&&mock::boots==0);}
 else if(scenario=="task-failure"){mock::taskAvailable=false;assert(!install("/firmware.bin"));assert(!busy()&&!installed()&&mock::boots==0);}
 else if(scenario=="usb-file"){mock::file("/USB/Update.bin");run("/USB/Update.bin");assert(installed()&&mock::lastOpen=="/USB/Update.bin"&&mock::boots==1);}
 else assert(false);
 std::printf("PASS %s\n",scenario.c_str());
}
