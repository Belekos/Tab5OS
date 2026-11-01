#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>

// Composition never edits the document until committed. Raw keystrokes are
// retained, so Backspace reverses one key, including combined Hangul finals.
namespace tab5ime {
enum class Mode : uint8_t { None, Pinyin, Japanese, Korean };
inline Mode modeFor(const char* id) {
  if (!id) return Mode::None;
  if (!strncmp(id,"zh",2)) return Mode::Pinyin;
  if (!strncmp(id,"ja",2)) return Mode::Japanese;
  if (!strncmp(id,"ko",2)) return Mode::Korean;
  return Mode::None;
}
inline void utf8(std::string& out,uint32_t cp) {
  if(cp<0x80)out+=char(cp);
  else if(cp<0x800){out+=char(0xc0|(cp>>6));out+=char(0x80|(cp&63));}
  else if(cp<0x10000){out+=char(0xe0|(cp>>12));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}
  else{out+=char(0xf0|(cp>>18));out+=char(0x80|((cp>>12)&63));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}
}
struct Kana { const char* latin; const char* kana; };
inline const Kana* kanaTable(size_t& count) {
  static const Kana table[]={
    {"a","あ"},{"i","い"},{"u","う"},{"e","え"},{"o","お"},
    {"ka","か"},{"ki","き"},{"ku","く"},{"ke","け"},{"ko","こ"},
    {"ga","が"},{"gi","ぎ"},{"gu","ぐ"},{"ge","げ"},{"go","ご"},
    {"sa","さ"},{"shi","し"},{"si","し"},{"su","す"},{"se","せ"},{"so","そ"},
    {"za","ざ"},{"ji","じ"},{"zi","じ"},{"zu","ず"},{"ze","ぜ"},{"zo","ぞ"},
    {"ta","た"},{"chi","ち"},{"ti","ち"},{"tsu","つ"},{"tu","つ"},{"te","て"},{"to","と"},
    {"da","だ"},{"di","ぢ"},{"du","づ"},{"de","で"},{"do","ど"},
    {"na","な"},{"ni","に"},{"nu","ぬ"},{"ne","ね"},{"no","の"},
    {"ha","は"},{"hi","ひ"},{"fu","ふ"},{"hu","ふ"},{"he","へ"},{"ho","ほ"},
    {"ba","ば"},{"bi","び"},{"bu","ぶ"},{"be","べ"},{"bo","ぼ"},
    {"pa","ぱ"},{"pi","ぴ"},{"pu","ぷ"},{"pe","ぺ"},{"po","ぽ"},
    {"ma","ま"},{"mi","み"},{"mu","む"},{"me","め"},{"mo","も"},
    {"ya","や"},{"yu","ゆ"},{"yo","よ"},
    {"ra","ら"},{"ri","り"},{"ru","る"},{"re","れ"},{"ro","ろ"},
    {"wa","わ"},{"wi","うぃ"},{"we","うぇ"},{"wo","を"},{"nn","ん"},{"n'","ん"},
    {"kya","きゃ"},{"kyu","きゅ"},{"kyo","きょ"},{"gya","ぎゃ"},{"gyu","ぎゅ"},{"gyo","ぎょ"},
    {"sha","しゃ"},{"shu","しゅ"},{"sho","しょ"},{"she","しぇ"},
    {"sya","しゃ"},{"syu","しゅ"},{"syo","しょ"},
    {"ja","じゃ"},{"ju","じゅ"},{"jo","じょ"},{"je","じぇ"},
    {"jya","じゃ"},{"jyu","じゅ"},{"jyo","じょ"},{"zya","じゃ"},{"zyu","じゅ"},{"zyo","じょ"},
    {"cha","ちゃ"},{"chu","ちゅ"},{"cho","ちょ"},{"che","ちぇ"},
    {"tya","ちゃ"},{"tyu","ちゅ"},{"tyo","ちょ"},{"cya","ちゃ"},{"cyu","ちゅ"},{"cyo","ちょ"},
    {"dya","ぢゃ"},{"dyu","ぢゅ"},{"dyo","ぢょ"},
    {"nya","にゃ"},{"nyu","にゅ"},{"nyo","にょ"},
    {"hya","ひゃ"},{"hyu","ひゅ"},{"hyo","ひょ"},
    {"bya","びゃ"},{"byu","びゅ"},{"byo","びょ"},
    {"pya","ぴゃ"},{"pyu","ぴゅ"},{"pyo","ぴょ"},
    {"mya","みゃ"},{"myu","みゅ"},{"myo","みょ"},
    {"rya","りゃ"},{"ryu","りゅ"},{"ryo","りょ"},
    {"fa","ふぁ"},{"fi","ふぃ"},{"fe","ふぇ"},{"fo","ふぉ"},{"fyu","ふゅ"},
    {"va","ゔぁ"},{"vi","ゔぃ"},{"vu","ゔ"},{"ve","ゔぇ"},{"vo","ゔぉ"},
    {"tsa","つぁ"},{"tsi","つぃ"},{"tse","つぇ"},{"tso","つぉ"},
    {"thi","てぃ"},{"thu","てゅ"},{"dhi","でぃ"},{"dhu","でゅ"},{"twu","とぅ"},{"dwu","どぅ"},
    {"xa","ぁ"},{"xi","ぃ"},{"xu","ぅ"},{"xe","ぇ"},{"xo","ぉ"},
    {"la","ぁ"},{"li","ぃ"},{"lu","ぅ"},{"le","ぇ"},{"lo","ぉ"},
    {"xya","ゃ"},{"xyu","ゅ"},{"xyo","ょ"},{"lya","ゃ"},{"lyu","ゅ"},{"lyo","ょ"},
    {"xtsu","っ"},{"xtu","っ"},{"ltsu","っ"},{"ltu","っ"},{"xwa","ゎ"},{"lwa","ゎ"},
    {"xka","ゕ"},{"xke","ゖ"},{"-","ー"}
  };
  count=sizeof(table)/sizeof(table[0]);return table;
}
inline std::string katakana(const std::string& source) {
  std::string out;
  for(size_t i=0;i<source.size();){uint32_t c=uint8_t(source[i++]);
    if((c&0xe0)==0xc0&&i<source.size())c=((c&31)<<6)|(uint8_t(source[i++])&63);
    else if((c&0xf0)==0xe0&&i+1<source.size()){c=((c&15)<<12)|((uint8_t(source[i])&63)<<6)|(uint8_t(source[i+1])&63);i+=2;}
    if(c>=0x3041&&c<=0x3096)c+=0x60;utf8(out,c);
  }return out;
}
inline std::string japanese(const std::string& raw,bool finish=false) {
  std::string out;size_t count;const auto* table=kanaTable(count);
  for(size_t i=0;i<raw.size();){char c=raw[i];
    if(c=='n'&&i+2<raw.size()&&raw[i+1]=='n'&&strchr("aiueoy",raw[i+2])){out+="ん";++i;continue;}
    if(c=='n'&&((i+1==raw.size()&&finish)||(i+1<raw.size()&&!strchr("aiueoyn'",raw[i+1])))){out+="ん";++i;continue;}
    if(i+1<raw.size()&&c==raw[i+1]&&c!='n'&&!strchr("aiueo",c)){out+="っ";++i;continue;}
    const Kana* best=nullptr;size_t length=0;
    for(size_t k=0;k<count;++k){size_t n=strlen(table[k].latin);if(n>length&&raw.compare(i,n,table[k].latin)==0){best=table+k;length=n;}}
    if(best){out+=best->kana;i+=length;}else{out+=c;++i;}
  }return out;
}
inline int combineVowel(int a,int b){
  if(a==8){if(b==0)return 9;if(b==1)return 10;if(b==20)return 11;}
  if(a==13){if(b==4)return 14;if(b==5)return 15;if(b==20)return 16;}
  if(a==18&&b==20)return 19;return -1;
}
inline int initialToFinal(int c){static const int map[]={1,2,4,7,0,8,16,17,0,19,20,21,22,0,23,24,25,26,27};return c>=0&&c<19?map[c]:0;}
inline int finalToInitial(int t){static const int map[]={-1,0,1,-1,2,-1,-1,3,5,-1,-1,-1,-1,-1,-1,-1,6,7,-1,9,10,11,12,14,15,16,17,18};return t>=0&&t<28?map[t]:-1;}
inline int combineFinal(int a,int b){
  if(a==1&&b==19)return 3;if(a==4){if(b==22)return 5;if(b==27)return 6;}
  if(a==8){if(b==1)return 9;if(b==16)return 10;if(b==17)return 11;if(b==19)return 12;if(b==25)return 13;if(b==26)return 14;if(b==27)return 15;}
  if(a==17&&b==19)return 18;return 0;
}
inline int splitFinal(int t,int& remains){
  static const int left[]={0,0,0,1,0,4,4,0,0,8,8,8,8,8,8,8,0,0,17,0,0,0,0,0,0,0,0,0};
  static const int right[]={0,1,2,19,4,22,27,7,8,1,16,17,19,25,26,27,16,17,19,19,20,21,22,23,24,25,26,27};
  remains=left[t];return finalToInitial(right[t]);
}
inline std::string korean(const std::string& raw){
  std::string out;int l=-1,v=-1,t=0;
  auto emit=[&](){if(l>=0&&v>=0)utf8(out,0xac00+(l*21+v)*28+t);else if(l>=0){static const int cp[]={0x3131,0x3132,0x3134,0x3137,0x3138,0x3139,0x3141,0x3142,0x3143,0x3145,0x3146,0x3147,0x3148,0x3149,0x314a,0x314b,0x314c,0x314d,0x314e};utf8(out,cp[l]);}else if(v>=0)utf8(out,0x314f+v);l=-1;v=-1;t=0;};
  for(char c:raw){int ci=-1,vi=-1;const char* consonants="rRseEfaqQtTdwWczxvg";const char* vowels="koiOjpuPhynbml";
    if(const char* x=strchr(consonants,c))ci=int(x-consonants);
    if(const char* x=strchr(vowels,c)){static const int map[]={0,1,2,3,4,5,6,7,8,12,13,17,18,20};vi=map[x-vowels];}
    if(ci<0&&vi<0&&c>='A'&&c<='Z'){char low=c+32;if(const char* x=strchr(consonants,low))ci=int(x-consonants);if(const char* x=strchr(vowels,low)){static const int map[]={0,1,2,3,4,5,6,7,8,12,13,17,18,20};vi=map[x-vowels];}}
    if(ci>=0){if(l<0&&v<0){l=ci;continue;}if(l>=0&&v>=0){int f=initialToFinal(ci);if(!t&&f){t=f;continue;}int compound=f?combineFinal(t,f):0;if(compound){t=compound;continue;}}emit();l=ci;}
    else if(vi>=0){if(v<0){v=vi;continue;}if(l>=0&&t){int remains=0;int next=splitFinal(t,remains);t=remains;emit();l=next;v=vi;continue;}int compound=combineVowel(v,vi);if(compound>=0){v=compound;continue;}emit();v=vi;}
    else{emit();out+=c;}
  }emit();return out;
}
class Composer {
 public:
  void setMode(Mode mode){if(mode!=_mode){clear();_mode=mode;_katakana=false;_latin=false;}}
  Mode mode()const{return _mode;}bool enabled()const{return _mode!=Mode::None&&!_latin;}
  bool empty()const{return _raw.empty();}const std::string& raw()const{return _raw;}
  void clear(){_raw.clear();}void toggleLatin(){clear();_latin=!_latin;}bool latin()const{return _latin;}
  void toggleKana(){_katakana=!_katakana;}bool isKatakana()const{return _katakana;}
  bool feed(char c){if(!enabled()||_raw.size()>=96)return false;
    if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(_mode!=Mode::Korean&&(c=='\''||c=='-'))){if(_mode!=Mode::Korean&&c>='A'&&c<='Z')c+=32;_raw+=c;return true;}return false;}
  bool backspace(){if(_raw.empty())return false;_raw.pop_back();return true;}
  std::string preview(bool finish=false)const{if(_mode==Mode::Japanese){auto s=japanese(_raw,finish);return _katakana?katakana(s):s;}if(_mode==Mode::Korean)return korean(_raw);return _raw;}
  std::string lookupKey()const{if(_mode==Mode::Japanese)return japanese(_raw,true);std::string s;for(char c:_raw)if(c!='\''&&c!='-')s+=c;return s;}
 private:Mode _mode=Mode::None;std::string _raw;bool _katakana=false,_latin=false;
};
}
