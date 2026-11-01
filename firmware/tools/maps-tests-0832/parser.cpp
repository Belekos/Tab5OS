#include "Tab5MapSearchParser.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>
static unsigned checks=0;
static void check(bool ok,const char* text){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",text);exit(1);}}
/* ITEM */
using Item=Tab5MapSearchItem;
struct Allocator:ArduinoJson::Allocator {
 std::map<void*,size_t> live;size_t total=0,peak=0;int fail=-1;
 bool allowed(){if(fail==0)return false;if(fail>0)--fail;return true;}
 void* allocate(size_t n)override{if(!allowed())return nullptr;void* p=malloc(n);if(p){live[p]=n;total+=n;peak=std::max(peak,total);}return p;}
 void deallocate(void* p)override{if(!p)return;check(live.count(p)!=0,"owned JSON allocation released");total-=live[p];live.erase(p);free(p);}
 // ArduinoJson explicitly requires shrinking realloc to succeed. Preserve the
 // allocation in that case and inject failures only into new/growing storage.
 void* reallocate(void* p,size_t n)override{if(!p)return allocate(n);size_t old=live.at(p);if(n<=old)return p;if(!allowed())return nullptr;void* q=realloc(p,n);if(q){live.erase(p);live[q]=n;total+=n-old;peak=std::max(peak,total);}return q;}
} allocator;
static Item items[8];
static int decode(const std::string& text,const char* language="fr-FR"){
 const int count=Tab5MapSearch::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),language,items,8,&allocator);
 check(allocator.total==0&&allocator.live.empty(),"JSON temporary memory fully released");return count;
}
class String:public std::string {public:using std::string::string;String(const std::string& x):std::string(x){};};
/* URL_ENCODER */
int main(){
 check(urlEncoded("Saint Nazaire")=="Saint%20Nazaire","two-word URL encoded");
 check(urlEncoded("Lann Bihoué")=="Lann%20Bihou%C3%A9","accent and space encoded");
 check(urlEncoded("New  York")=="New%20%20York","repeated spaces encoded");
 check(urlEncoded("A&B + C/D?#%")=="A%26B%20%2B%20C%2FD%3F%23%25","reserved URL bytes remain in query");
 check(urlEncoded("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~")=="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~","all RFC3986 unreserved bytes preserved");
 check(urlEncoded(nullptr).empty(),"null input safe");
 for(unsigned c=1;c<256;++c){char bytes[2]={static_cast<char>(c),0};String encoded=urlEncoded(bytes);bool unreserved=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~';if(unreserved)check(encoded==bytes,"unreserved byte preserved");else {char wanted[4];snprintf(wanted,sizeof(wanted),"%%%02X",c);check(encoded==wanted,"all other bytes percent encoded");}}

 const std::string one=R"({"lat":"48.85","lon":"2.35","name":"Paris","display_name":"Paris, France","place_rank":16,"namedetails":{"name:fr-FR":"Paris régional","name:fr":"Paris français","name":"Paris générique","alt_name":"Lutèce"}})";
 check(decode("["+one+"]")==1,"valid localized result");check(!strcmp(items[0].name,"Paris régional")&&!strcmp(items[0].alternative,"Lutèce"),"regional name and alias retained");
 check(items[0].latitude==48.85&&items[0].longitude==2.35&&items[0].zoom==10,"coordinates and previous zoom policy preserved");
 check(decode("["+one+"]","fr-CA")==1&&!strcmp(items[0].name,"Paris français"),"base language fallback");
 check(decode("["+one+"]","de-DE")==1&&!strcmp(items[0].name,"Paris"),"provider localized name fallback");
 for(const char* invalid:{"nan","NaN","inf","1e9999","-91","91","nonsense","","12abc","12.3 "}){
  std::string text="[{\"lat\":\""+std::string(invalid)+"\",\"lon\":\"0\",\"name\":\"invalid\"},"+one+"]";
  check(decode(text)==1&&!strcmp(items[0].name,"Paris régional"),"invalid numeric coordinates skipped");
 }
 for(const char* invalid:{"181","-181","nan","1x"})check(decode("[{\"lat\":\"0\",\"lon\":\""+std::string(invalid)+"\"}]")==0,"invalid longitude skipped");
 check(decode(R"([{"lat":"-90","lon":"180"},{"lat":"90","lon":"-180"}])")==2,"coordinate boundary accepted");
 for(const char* invalid:{"","{}","null","42","[","[{\"lat\":\"1\"}"})check(decode(invalid)<0,"malformed or wrong response shape rejected");
 check(decode("[]")==0,"empty successful query distinct from invalid response");
 std::string many="[";for(unsigned i=0;i<24;++i){if(i)many+=",";many+=one;}many+="]";check(decode(many)==8,"output strictly capped at eight items");
 std::string longName;for(unsigned i=0;i<100;++i)longName+="é";
 check(decode("[{\"lat\":\"0\",\"lon\":\"0\",\"name\":\""+longName+"\"}]")==1,"long UTF8 label accepted");check(strlen(items[0].name)==94,"UTF8 truncation preserves complete characters");
 std::string large="[{\"lat\":\"48.85\",\"lon\":\"2.35\",\"name\":\"Paris\",\"namedetails\":{";
 for(unsigned i=0;i<800;++i){if(i)large+=",";large+="\"name:unused"+std::to_string(i)+"\":\"translation-"+std::to_string(i)+std::string(70,'x')+"\"";}large+="}}]";
 allocator.peak=0;check(decode(large)==1,"large multilingual provider payload");const size_t filtered=allocator.peak;
 allocator.peak=0;{ArduinoJson::JsonDocument baseline(&allocator);check(!ArduinoJson::deserializeJson(baseline,large.c_str(),large.size()),"previous full JSON parse fixture valid");}const size_t full=allocator.peak;
 check(filtered*10<full,"filtered JSON peak less than one tenth of previous full document");
 for(int failure=0;failure<24;++failure){allocator.fail=failure;int result=decode("["+one+"]");check(result==-2||result==1,"allocation failure returns error without partial success or leak");allocator.fail=-1;}
 for(unsigned cycle=0;cycle<300;++cycle)check(decode("["+one+"]")==1,"repeated search parse");
 printf("PASS %u map parser checks; real ArduinoJson; filtered peak=%zu bytes vs full=%zu bytes; 24 allocation-fault positions; 300 cycles\n",checks,filtered,full);
}
