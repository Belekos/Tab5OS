#include "Tab5Ime.h"
#include <cassert>
#include <iostream>
using namespace tab5ime;
static void equal(const std::string& actual,const std::string& wanted){if(actual!=wanted){std::cerr<<"Expected "<<wanted<<" got "<<actual<<"\n";abort();}}
int main(){
  equal(japanese("nihongo"),"にほんご");equal(japanese("konnichiha"),"こんにちは");
  equal(japanese("kan'i"),"かんい");equal(japanese("gakkou"),"がっこう");
  equal(japanese("ryokou"),"りょこう");equal(japanese("shinbun",true),"しんぶん");
  equal(japanese("kya"),"きゃ");equal(katakana(japanese("konpyu-ta-")),"コンピューター");
  equal(japanese("n"),"n");equal(japanese("n",true),"ん");equal(japanese("anna"),"あんな");
  equal(korean("gksrmf"),"한글");equal(korean("dkssudgktpdy"),"안녕하세요");
  equal(korean("rhk"),"과");equal(korean("rnj"),"궈");equal(korean("rml"),"긔");
  equal(korean("rkqt"),"값");equal(korean("rkqtdl"),"값이");equal(korean("rkqtk"),"갑사");
  equal(korean("Rk"),"까");equal(korean("Wk"),"짜");equal(korean("r"),"ㄱ");equal(korean("k"),"ㅏ");
  Composer c;c.setMode(Mode::Korean);for(char ch:std::string("rkqt"))assert(c.feed(ch));equal(c.preview(),"값");
  c.backspace();equal(c.preview(),"갑");c.backspace();equal(c.preview(),"가");c.backspace();equal(c.preview(),"ㄱ");c.backspace();assert(c.empty());
  c.setMode(Mode::Pinyin);for(char ch:std::string("xi'an"))assert(c.feed(ch));equal(c.lookupKey(),"xian");
  c.clear();for(int i=0;i<96;++i)assert(c.feed('a'));assert(!c.feed('b'));assert(c.raw().size()==96);
  c.toggleLatin();assert(!c.enabled()&&!c.feed('a'));c.setMode(Mode::Japanese);assert(c.enabled());
  for(char ch:std::string("nihongo"))c.feed(ch);c.toggleKana();equal(c.preview(),"ニホンゴ");equal(c.lookupKey(),"にほんご");
  // Every modern Hangul syllable is reachable from a standard two-beolsik
  // keystroke sequence, including compound medial vowels and final clusters.
  const char* initials[]={"r","R","s","e","E","f","a","q","Q","t","T","d","w","W","c","z","x","v","g"};
  const char* vowels[]={"k","o","i","O","j","p","u","P","h","hk","ho","hl","y","n","nj","np","nl","b","m","ml","l"};
  const char* finals[]={"","r","R","rt","s","sw","sg","e","f","fr","fa","fq","ft","fx","fv","fg","a","q","qt","t","T","d","w","c","z","x","v","g"};
  size_t samples=0;
  for(int l=0;l<19;++l)for(int v=0;v<21;++v)for(int t=0;t<28;++t){std::string want;utf8(want,0xac00+(l*21+v)*28+t);equal(korean(std::string(initials[l])+vowels[v]+finals[t]),want);++samples;}
  std::cout<<"PASS: romaji/kana/kanji lookup keys, reversible composition, length bounds, modes; "<<samples<<" modern Hangul syllables.\n";
}
