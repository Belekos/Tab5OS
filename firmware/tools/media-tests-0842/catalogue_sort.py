#!/usr/bin/env python3
"""Compare exact old/new sorting predicates in the firmware's C locale."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,tempfile,zipfile
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.42_Ecriture'
with zipfile.ZipFile(a.baseline_zip) as z:before=z.read(next(n for n in z.namelist() if n.endswith('/Tab5Audio.cpp'))).decode()
after=(sketch/'Tab5Audio.cpp').read_text()
def predicate(text,start):
 pos=text.index(start);pos=text.index('[](',pos);brace=text.index('{',pos);depth=0
 for end in range(brace,len(text)):
  if text[end]=='{':depth+=1
  elif text[end]=='}':
   depth-=1
   if not depth:return text[pos:end+1]
 raise AssertionError(start)
fixture=r'''
#include <cassert>
#include <cctype>
#include <clocale>
#include <cstring>
#include <cstdint>
#include <string>
#include <strings.h>
#include <iostream>
struct String {
 std::string value;static size_t copies;
 String()=default;String(const char* s):value(s){}String(std::string s):value(std::move(s)){}
 String(const String& s):value(s.value){++copies;}String& operator=(const String& s){value=s.value;++copies;return *this;}
 const char* c_str()const{return value.c_str();}
 bool operator!=(const String& s)const{return value!=s.value;}
 void toLowerCase(){for(char& c:value)c=tolower(c);} // SDK 3.3.9's byte lowering loop
 int compareTo(const String& s)const{return strcmp(c_str(),s.c_str());} // SDK byte ordering
};size_t String::copies=0;
struct Tab5AudioStation{String name,style,url;};
auto oldTrack=OLD_TRACK;auto newTrack=NEW_TRACK;
auto oldStation=OLD_STATION;auto newStation=NEW_STATION;
int main(){
 assert(setlocale(LC_ALL,"C"));size_t checks=0;
 // All non-NUL byte pairs, ties, prefixes and UTF-8 bytes; no Unicode-folding
 // claim (the previous firmware also lowercases bytes in the C locale).
 for(unsigned a=1;a<256;++a)for(unsigned b=1;b<256;++b){
  String x(std::string(1,char(a))),y(std::string(1,char(b)));
  assert(oldTrack(x,y)==newTrack(x,y));++checks;
 }
 const char* names[]={"","a","A","aa","Aa","B","Music/ÉTÉ.mp3","music/été.MP3","東京.wav","Z_01","x09","x10"};
 for(const char* a:names)for(const char* b:names){
  assert(oldTrack(String(a),String(b))==newTrack(String(a),String(b)));++checks;
  for(const char* c:names){
   Tab5AudioStation x{String(a),String(c),String()},y{String(b),String(c),String()};
   assert(oldStation(x,y)==newStation(x,y));++checks;
   y.style=String(b);assert(oldStation(x,y)==newStation(x,y));++checks;
  }
 }
 String x("alpha-track"),y("beta-track");Tab5AudioStation sx{x,String("Jazz"),String()},sy{y,String("JAZZ"),String()};
 String::copies=0;for(int i=0;i<10000;++i){oldTrack(x,y);oldStation(sx,sy);}size_t oldCopies=String::copies;
 String::copies=0;for(int i=0;i<10000;++i){newTrack(x,y);newStation(sx,sy);}assert(String::copies==0);
 std::cout<<"{\"passed\":true,\"comparisonPairs\":"<<checks<<",\"iterations\":10000,\"oldStringCopies\":"<<oldCopies<<",\"newStringCopies\":"<<String::copies<<",\"locale\":\"C\"}"<<std::endl;
}
'''
for label,source in [('OLD',before),('NEW',after)]:
 fixture=fixture.replace(label+'_TRACK',predicate(source,'std::sort(_tracks.begin()')).replace(label+'_STATION',predicate(source,'std::stable_sort(_stations.begin()'))
build=Path(tempfile.mkdtemp(prefix='tab5-sort-0842-'));(build/'test.cpp').write_text(fixture)
subprocess.run(['g++','-std=c++17','-O1','-g','-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined',str(build/'test.cpp'),'-o',str(build/'test')],check=True)
run=subprocess.run([str(build/'test')],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));print(run.stdout+run.stderr,end='');run.check_returncode()
report={'version':'0.8.42',**json.loads(run.stdout.strip()),'scope':'Exact before/after track and station sort predicates, host String byte operations matching inspected Arduino 3.3.9 WString.cpp. C locale (no setlocale in firmware). ASan/UBSan; not device timing.','sourceSha256':hashlib.sha256(after.encode()).hexdigest()}
(root/'tools/audit-0842/catalogue-sort.json').write_text(json.dumps(report,indent=2)+'\n')
