#include "Platform.h"
#include "Tab5AtomicFile.h"
#include "Tab5JsonFile.h"
#include <ArduinoJson.h>
#include "Tab5HiddenFiles.h"
#include "read-json.inc"
#include <iostream>
void put(const char* p,const std::string& value){auto n=std::make_shared<Node>();n->bytes=value;files[p]=n;}
std::string bytes(const char* p){auto at=files.find(p);return at==files.end()?std::string():at->second->bytes;}
void reset(){files.clear();failOperation=-1;statErrorPath.clear();openErrorPath.clear();statErrorAfterOpen=openFailed=false;}
int main(int argc,char**){bool baseline=argc>1;const char* primary="/System/test.json";const char* backup="/System/.Tab5Backups/test.json.bak";
 reset();put(primary,"old");statErrorPath=primary;bool primaryStat=!tab5AtomicRecover(primary)&&bytes(primary)=="old";
 reset();put(backup,"old");statErrorPath=backup;bool backupStat=!tab5AtomicRecover(primary)&&bytes(backup)=="old";
 reset();put(primary,"{\"keep\":1}");openErrorPath=statErrorPath=primary;statErrorAfterOpen=true;JsonDocument doc;bool jsonOpen=!readJson(primary,doc)&&bytes(primary)=="{\"keep\":1}";
 reset();put("/.tab5-hidden.json","{\"items\":{\"/private\":true}}");openErrorPath=statErrorPath="/.tab5-hidden.json";statErrorAfterOpen=true;Tab5HiddenFiles::cards[0].loaded=false;bool hiddenOpen=!Tab5HiddenFiles::load(false)&&!Tab5HiddenFiles::set("/new",true)&&bytes("/.tab5-hidden.json")=="{\"items\":{\"/private\":true}}";
 reset();doc.clear();bool missingAccepted=readJson(primary,doc)&&tab5AtomicWrite(primary,"first",5)&&bytes(primary)=="first";
 reset();put(backup,"recovered");bool backupRecovered=tab5AtomicRecover(primary)&&bytes(primary)=="recovered";
 unsigned faults=0;bool faultsPreserve=true;
 for(int i=0;i<40;++i){reset();put(primary,"original");put(backup,"older");failOperation=i;bool ok=tab5AtomicWrite(primary,"replacement",11);failOperation=-1;if(ok)faultsPreserve&=bytes(primary)=="replacement";else faultsPreserve&=bytes(primary)=="original"||bytes(backup)=="original";++faults;}
 bool all=primaryStat&&backupStat&&jsonOpen&&hiddenOpen&&missingAccepted&&backupRecovered&&faultsPreserve;
 std::cout<<"{\"primaryStatErrorPreserved\":"<<primaryStat<<",\"backupStatErrorPreserved\":"<<backupStat<<",\"jsonOpenErrorNotEmpty\":"<<jsonOpen<<",\"hiddenRegistryOpenErrorNotEmpty\":"<<hiddenOpen<<",\"missingFilesAccepted\":"<<missingAccepted<<",\"interruptedCommitRecovered\":"<<backupRecovered<<",\"operationFaultsPreserveOriginal\":"<<faultsPreserve<<",\"faultInjectionPoints\":"<<faults<<",\"passed\":"<<all<<"}\n";
 return baseline||all?0:1;
}
