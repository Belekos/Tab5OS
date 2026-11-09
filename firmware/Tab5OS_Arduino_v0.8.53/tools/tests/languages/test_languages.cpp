#include "host_platform.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#define private public
#include "Tab5Localization.h"
#undef private
#include "Tab5Localization.cpp"

static size_t checks=0,packCount=0,oomScenarios=0;
static void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static bool parse(const std::string& text,size_t capacity=4096){
 std::vector<char> bytes(text.begin(),text.end());bytes.push_back(0);
 std::vector<Tab5LanguageReader::Entry> entries(capacity+1);
 entries.back()={"guard-source","guard-target"};Tab5LanguageReader::Pack out;
 const bool okay=Tab5LanguageReader::parse(bytes.data(),text.size(),entries.data(),capacity,out);
 check(std::strcmp(entries.back().source,"guard-source")==0&&std::strcmp(entries.back().translated,"guard-target")==0,"Entry capacity overrun");
 return okay;
}
static void badInputs(){
 for(const char* bad:{"", "[]", "{}", "{\"id\":\"\",\"translations\":{}}", "{\"id\":\"en-US\",\"translations\":{},}",
  "{\"id\":\"a\",\"id\":\"b\",\"translations\":{}}", "{\"id\":\"a\",\"translations\":{},\"translations\":{}}",
  "{\"id\":\"a\",\"translations\":{\"x\":\"a\",\"x\":\"b\"}}", "{\"id\":\"a\",\"translations\":{\"%s %d\":\"%d %s\"}}",
  "{\"id\":\"a\",\"translations\":{\"%s\":\"%n\"}}", "{\"id\":\"a\",\"translations\":{\"%s\":\"%1$s\"}}",
  "{\"id\":\"a\",\"translations\":{\"%*.*f\":\"%.2f\"}}", "{\"id\":\"a\",\"translations\":{\"hello\":\"\\u0000\"}}",
  "{\"id\":\"a\",\"translations\":{\"hello\":\"\\ud800\"}}", "{\"id\":\"a\",\"translations\":{\"hello\":\"\\udc00\"}}",
  "{\"id\":\"a\",\"translations\":{\"hello\":\"\\q\"}}", "{\"id\":\"a\",\"translations\":{\"hello\":\"bad\nstring\"}}",
  "{\"id\":\"a\",\"translations\":{}} garbage"})check(!parse(bad),"Malformed/unsafe pack accepted");
 check(!parse(std::string("{\"id\":\"a\",\"translations\":{\"x\":\"")+char(0xc0)+char(0xaf)+"\"}}"),"Overlong UTF-8 accepted");
 check(!parse("{\"id\":\"a\",\"translations\":{\"x\":\"a\",\"y\":\"b\"}}",1),"Dictionary capacity bypassed");
 check(parse("{\"unknown\":[true,false,null,{\"a\":-1.25e+2}],\"id\":\"custom\",\"translations\":{\"%d %s\":\"%04d %s\",\"0-200 % (default)\":\"0-200 % (initial)\",\"100%\":\"100 %\",\"emoji\":\"\\ud83d\\ude00\"}}"),"Valid custom pack rejected");
 const std::string small="{\"id\":\"a\",\"translations\":{\"x\":\"hello\"}}";
 for(size_t i=0;i<small.size();++i)check(!parse(small.substr(0,i)),"Truncated JSON accepted");
 using Tab5TranslationFormat::compatible;
 check(compatible("%s %*.*f %zu %%","%s %*.*f %zu %%"),"Valid format rejected");
 check(!compatible("%s (100%%)","%s (100 % pointer)"),"Percent prose heuristic allowed extra printf argument");
 check(!compatible("0-200 %, 100 % par defaut","0-200 %, 100% by default"),"Broken historical help regression no longer detected");
 check(compatible("0-200 %, 100 % (par defaut)","0-200 %, 100% (by default)"),"Unambiguous percentage text rejected");
}
static void loaderCases(){
 Host::files.clear();Host::preferences.clear();
 const std::string original="{\"id\":\"custom\",\"name\":\"Custom\",\"translations\":{\"label\":\"old\"}}";
 Host::put("/System/languages/custom-file.json",original);
 {
  Tab5Localization locale;Tab5Localization::LanguageEntry selected{"custom","Custom","/System/languages/custom-file.json"};
  check(locale.loadLanguage(selected),"Initial custom pack failed");
  const size_t retained=Host::liveBytes;
  const std::string replacement="{\"id\":\"custom\",\"name\":\"New\",\"translations\":{\"label\":\"new\"}}";
  Host::put(selected.path.c_str(),replacement);
  for(size_t fail=0;fail<4;++fail){Host::calls=0;Host::failAt=fail;check(!locale.loadLanguage(selected),"Allocation failure not propagated");check(std::strcmp(locale.text("label"),"old")==0,"Failed load replaced active translation");check(locale.selectedName()=="Custom","Failed load replaced active name");check(Host::liveBytes==retained,"Failed load leaked PSRAM");++oomScenarios;}
  Host::failAt=SIZE_MAX;
  Host::readFailureAt=12;check(!locale.loadLanguage(selected),"Truncated SD read accepted");check(std::strcmp(locale.text("label"),"old")==0,"Read failure replaced active language");Host::readFailureAt=SIZE_MAX;
  Host::put(selected.path.c_str(),"{\"id\":\"wrong\",\"translations\":{}}");check(!locale.loadLanguage(selected),"Mismatched language ID accepted");
  Host::put(selected.path.c_str(),"{\"id\":\"custom\",\"translations\":{\"%s\":\"%n\"}}");check(!locale.loadLanguage(selected),"Unsafe replacement installed");check(std::strcmp(locale.text("label"),"old")==0,"Unsafe replacement erased active pack");
  Host::put(selected.path.c_str(),"");String emptyId,emptyName;check(!locale.loadLanguage(selected),"Empty pack accepted");check(!locale.readLanguageMetadata(selected.path,emptyId,emptyName),"Empty metadata accepted");check(std::strcmp(locale.text("label"),"old")==0,"Empty pack erased active language");
  Host::put(selected.path.c_str(),std::string(MAX_LANGUAGE_FILE+1,' '));Host::calls=0;check(!locale.loadLanguage(selected),"Oversized pack accepted");check(Host::calls==0,"Oversized pack allocated PSRAM");String id,name;check(!locale.readLanguageMetadata(selected.path,id,name),"Oversized catalogue pack accepted");
  std::string boundary=replacement;boundary.resize(MAX_LANGUAGE_FILE,' ');Host::put(selected.path.c_str(),boundary);check(locale.loadLanguage(selected),"Maximum-sized valid pack rejected");check(locale.readLanguageMetadata(selected.path,id,name)&&id=="custom","Maximum-sized metadata rejected");
  Host::put(selected.path.c_str(),replacement);Host::readLimit=7;check(locale.loadLanguage(selected),"Short positive SD reads failed");check(std::strcmp(locale.text("label"),"new")==0,"Replacement translation missing");Host::readLimit=SIZE_MAX;
  const char* reordered="{\"translations\":{\"label\":\"reordered\"},\"name\":\"Reordered\",\"id\":\"custom\"}";
  Host::put(selected.path.c_str(),reordered);check(locale.readLanguageMetadata(selected.path,id,name)&&id=="custom"&&name=="Reordered","Custom member order failed metadata fallback");
  check(locale.loadLanguage(selected)&&std::strcmp(locale.text("label"),"reordered")==0,"Custom member order failed loader");
  locale._sdAvailable=true;locale.discoverLanguages();size_t slot=0;for(;slot<locale.count()&&locale.id(slot)!="custom";++slot){}
  check(slot<locale.count()&&locale.select(slot),"Custom filename selector failed");check(Host::preferences["language_path"]==selected.path.c_str(),"Custom filename not persisted");
  {Tab5Localization restarted;check(restarted.begin(true)&&restarted.selectedId()=="custom"&&std::strcmp(restarted.text("label"),"reordered")==0,"Custom pack selection did not survive restart");}
 }
 check(Host::allocations.empty()&&Host::liveBytes==0,"Loader leaked retained translation buffers");
}
static std::string unhex(const std::string& text){
 std::string out;for(size_t i=0;i<text.size();i+=2)out.push_back(char(std::stoul(text.substr(i,2),nullptr,16)));return out;
}
int main(int argc,char** argv){
 try{
  check(argc==3,"Expected language pack directory and expected translations");badInputs();loaderCases();Host::files.clear();Host::preferences.clear();
  std::map<std::string,std::vector<std::pair<std::string,std::string>>> expected;
  std::ifstream expectedFile(argv[2]);check(bool(expectedFile),"Expected translation fixture missing");std::string line;
  while(std::getline(expectedFile,line)){
   const size_t first=line.find('\t'),second=line.find('\t',first+1);
   if(first==std::string::npos||second==std::string::npos)throw std::runtime_error("Invalid expected translation fixture");
   expected[line.substr(0,first)].emplace_back(unhex(line.substr(first+1,second-first-1)),unhex(line.substr(second+1)));
  }
  std::vector<std::string> ids;
  for(const auto& entry:std::filesystem::directory_iterator(argv[1]))if(entry.path().extension()==".json"){
   std::ifstream in(entry.path(),std::ios::binary);std::string raw((std::istreambuf_iterator<char>(in)),{});
   const std::string id=entry.path().stem().string(),path="/System/languages/"+entry.path().filename().string();
   check(raw.size()<=MAX_LANGUAGE_FILE,"Official pack exceeds loader cap");check(parse(raw),"Official pack rejected by production parser");Host::put(path,raw);ids.push_back(id);++packCount;
  }
  check(packCount==14,"Expected all 14 official packs");
  {Tab5Localization locale;check(locale.begin(true),"Locale startup failed");locale.discoverLanguages();check(locale.count()==14,"Official pack absent from Settings catalogue");
   for(const auto& id:ids){size_t slot=0;for(;slot<locale.count()&&locale.id(slot)!=id.c_str();++slot){}
    check(slot<locale.count(),"Official language missing");check(locale.select(slot),"Settings rejected official language");check(locale.selectedId()==id.c_str(),"Wrong selected language");
    size_t retained=0;check(!expected.at(id).empty(),"No expected translations for official pack");
    for(const auto& value:expected.at(id)){
     check(locale.text(value.first.c_str())==value.second,"Official translation differs from independently parsed JSON");
     if(id!="fr-FR"||value.first!=value.second)++retained;
    }
    check(locale._translationCount==retained,"Official translation count differs from JSON");
    char copy[128];check(locale.copyText("Couleur de la lumière",copy,sizeof(copy))>0&&copy[0],"Translated copy missing");
    locale.applyFontAvailability(false);check(locale.effectiveId()=="en-US"&&locale.selectedId()==id.c_str(),"Font fallback changed language selection");locale.applyFontAvailability(true);check(locale.effectiveId()==id.c_str(),"Restored font lost language");
   }
  }
  check(Host::allocations.empty()&&Host::liveBytes==0,"Final localization leak");
  std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"officialPacks\":"<<packCount<<",\"allocationFailureScenarios\":"<<oomScenarios<<",\"maxLanguageFileBytes\":"<<MAX_LANGUAGE_FILE<<",\"peakHostPsramBytes\":"<<Host::peakBytes<<"}\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
