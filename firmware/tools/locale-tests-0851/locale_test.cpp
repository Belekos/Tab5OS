#include "Platform.h"
#include "Tab5Localization.h"
#include "Tab5LanguageReader.h"
#include <chrono>
#include <ArduinoJson.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <filesystem>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": "<<#x<<"\n";std::abort();}}while(0)
static void resetCounters(){opens=reads=readBytesCount=directoryScans=allocCalls=failAllocation=ioFailureAt=0;ioLimit=SIZE_MAX;peakBytes=liveBytes;}
static size_t languageIndex(Tab5Localization& locale,const char* id){for(size_t i=0;i<locale.count();++i)if(locale.id(i)==id)return i;return locale.count();}
int main(int argc,char** argv){
 CHECK(argc==2);for(const auto& entry:std::filesystem::directory_iterator(argv[1]))if(entry.path().extension()==".json"){
  std::ifstream file(entry.path());fsFiles["/System/languages/"+entry.path().filename().string()]=std::string(std::istreambuf_iterator<char>(file),{});
 }
 CHECK(fsFiles.size()==14);
 // Explicit identity entries outside French must suppress the English fallback.
 // Metadata may appear before or after the translations object.
 for(const auto* json:{R"({"id":"de-DE","translations":{"Accueil":"Startseite","Liste":"Liste"}})",R"({"translations":{"Accueil":"Startseite","Liste":"Liste"},"id":"de-DE"})"}){
  std::string bytes=json;Tab5LanguageReader::Entry entries[2];Tab5LanguageReader::Pack pack;
  CHECK(Tab5LanguageReader::parse(bytes.data(),bytes.size(),entries,2,pack));CHECK(pack.count==2);CHECK(std::string(entries[1].translated)=="Liste");
 }
 {std::string bytes=R"({"id":"fr-FR","translations":{"Accueil":"Accueil","Liste":"Ma liste"}})";Tab5LanguageReader::Entry entries[2];Tab5LanguageReader::Pack pack;
  CHECK(Tab5LanguageReader::parse(bytes.data(),bytes.size(),entries,2,pack));CHECK(pack.count==1);CHECK(std::string(entries[0].translated)=="Ma liste");}
 {std::string bytes=R"({"id":"de-DE","translations":{"Liste":"Liste","Liste":"Liste"}})";Tab5LanguageReader::Entry entries[2];Tab5LanguageReader::Pack pack;
  CHECK(!Tab5LanguageReader::parse(bytes.data(),bytes.size(),entries,2,pack));}

 size_t frBytes=0,frReads=0,frAlloc=0,frPeak=0,enReads=0,enAlloc=0,enPeak=0;
 {Tab5Localization locale;prefs["language"]="fr-FR";resetCounters();
  CHECK(locale.begin(true));CHECK(locale.selectedId()=="fr-FR");CHECK(std::string(locale.text("Accueil"))=="Accueil");
  CHECK(opens==1);CHECK(directoryScans==0);frBytes=readBytesCount;frReads=reads;frAlloc=allocCalls;frPeak=peakBytes;
  CHECK(frReads==(frBytes+4095)/4096);CHECK(frAlloc==4);CHECK(liveBytes>0);CHECK(locale.count()==2);
  locale.discoverLanguages();CHECK(locale.count()==14);CHECK(directoryScans>0);size_t scans=directoryScans;locale.discoverLanguages();CHECK(scans==directoryScans);
  resetCounters();CHECK(locale.select(languageIndex(locale,"en-US")));CHECK(locale.selectedId()=="en-US");CHECK(std::string(locale.text("Accueil"))=="Home");
  CHECK(opens==1);CHECK(directoryScans==0);enReads=reads;enAlloc=allocCalls;enPeak=peakBytes;CHECK(enAlloc==4);CHECK(liveBytes>0);
  auto before=liveBytes;auto active=locale.selectedId();const std::string text=locale.text("Accueil");
  // Every allocation refusal leaves the active dictionary and chosen language unchanged.
  for(unsigned failure=1;failure<=4;++failure){resetCounters();failAllocation=failure;CHECK(!locale.select(languageIndex(locale,"de-DE")));CHECK(liveBytes==before);CHECK(locale.selectedId()==active);CHECK(std::string(locale.text("Accueil"))==text);}
  resetCounters();ioFailureAt=2;CHECK(!locale.select(languageIndex(locale,"de-DE")));CHECK(liveBytes==before);CHECK(locale.selectedId()==active);
  resetCounters();ioLimit=11;CHECK(locale.select(languageIndex(locale,"de-DE")));CHECK(std::string(locale.text("Accueil"))!="Accueil");
  char output[8];auto complete=locale.copyText("Accueil",output,sizeof(output));CHECK(complete>=std::strlen(output));CHECK(std::strlen(output)<sizeof(output));
  locale.applyFontAvailability(false);CHECK(locale.effectiveId()=="en-US");CHECK(locale.selectedId()=="de-DE");CHECK(std::string(locale.text("Accueil"))=="Home");
  locale.applyFontAvailability(true);CHECK(locale.effectiveId()=="de-DE");
 }
 CHECK(liveBytes==0);
 // All 14 current compact packs must load and every dictionary value must match.
 {Tab5Localization locale;prefs["language"]="en-US";prefs["language_path"]="";resetCounters();CHECK(locale.begin(true));locale.discoverLanguages();
  for(const auto& file:fsFiles){
   JsonDocument expected;CHECK(!deserializeJson(expected,file.second));
   const char* id=expected["id"].as<const char*>();CHECK(id);resetCounters();CHECK(locale.select(languageIndex(locale,id)));CHECK(locale.selectedId()==id);
   for(JsonPair entry:expected["translations"].as<JsonObject>()){
    auto actual=std::string(locale.text(entry.key().c_str()));auto wanted=entry.value().as<std::string>();
    if(actual!=wanted)std::cerr<<"locale="<<id<<" key="<<entry.key().c_str()<<" wanted="<<wanted<<" actual="<<actual<<"\n";
    CHECK(actual==wanted);
   }
  }
 }
 CHECK(liveBytes==0);
 // A modified French pack must remain effective; don't bypass FR based on its ID.
 fsFiles["/System/languages/zz-custom-file.json"]=R"({"translations":{"Accueil":"Mon accueil","Identité":"Identité"},"other":[{"x":1}],"name":"Français personnalisé","id":"fr-FR"})";
 {Tab5Localization locale;prefs["language"]="fr-FR";prefs["language_path"]="/System/languages/zz-custom-file.json";resetCounters();CHECK(locale.begin(true));CHECK(opens==1);CHECK(directoryScans==0);CHECK(locale.selectedName()=="Français personnalisé");CHECK(std::string(locale.text("Accueil"))=="Mon accueil");CHECK(std::string(locale.text("Identité"))=="Identité");}
 CHECK(liveBytes==0);
 // Read failure, oversize and malformed packs never destroy a selected dictionary.
 {Tab5Localization locale;prefs["language"]="en-US";prefs["language_path"]="";resetCounters();CHECK(locale.begin(true));locale.discoverLanguages();const auto index=languageIndex(locale,"fr-FR");
  const auto saved=fsFiles["/System/languages/zz-custom-file.json"];fsFiles["/System/languages/zz-custom-file.json"]=R"({"id":"fr-FR","translations":{"A %s":"B %d"}})";
  CHECK(!locale.select(index));CHECK(locale.selectedId()=="en-US");CHECK(std::string(locale.text("Accueil"))=="Home");
  fsFiles["/System/languages/zz-custom-file.json"]=std::string(512*1024+1,' ');resetCounters();CHECK(!locale.select(index));CHECK(reads==0);CHECK(locale.selectedId()=="en-US");
  fsFiles["/System/languages/zz-custom-file.json"]=saved;
 }
 CHECK(liveBytes==0);
 {Tab5Localization locale;prefs["language"]="fr-FR";resetCounters();CHECK(locale.begin(false));CHECK(opens==0);CHECK(locale.selectedId()=="fr-FR");CHECK(std::string(locale.text("Accueil"))=="Accueil");}
 CHECK(liveBytes==0);
 {Tab5Localization locale;prefs["language"]="zh-CN";resetCounters();CHECK(locale.begin(false));CHECK(opens==0);CHECK(locale.selectedId()=="en-US");CHECK(std::string(locale.text("Accueil"))=="Home");}
 CHECK(liveBytes==0);
 std::cout<<"{\"checks\":"<<checks<<",\"passed\":true,\"frBytes\":"<<frBytes<<",\"frReadCalls\":"<<frReads<<",\"frAllocationCalls\":"<<frAlloc<<",\"frPeakHostBytes\":"<<frPeak<<",\"enReadCalls\":"<<enReads<<",\"enAllocationCalls\":"<<enAlloc<<",\"enPeakHostBytes\":"<<enPeak<<",\"liveBytesAtExit\":"<<liveBytes<<"}\n";
}
