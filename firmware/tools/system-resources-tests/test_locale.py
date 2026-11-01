"""Exercise the actual localisation lookup and font switch methods on the host.

The SD pack parser/Preferences adapter is outside this focused test. No lookup
or font-switch implementation is duplicated: method bodies come from firmware.
"""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument('sketch',type=Path)
parser.add_argument('--cxx',default='g++')
parser.add_argument('--msvc',action='store_true')
args=parser.parse_args()
source=(args.sketch/'Tab5Localization.cpp').read_text(encoding='utf-8')
english=json.loads((args.sketch/'sdcard/System/languages/en-US.json').read_text(encoding='utf-8'))['translations']
methods=[]
for signature in ('const char* Tab5Localization::text(const char* source) const {',
                  'void Tab5Localization::applyFontAvailability(bool unicodeFontAvailable) {'):
    start=source.index(signature)
    methods.append(source[start:source.index('\n}',start)+2])
prefix=r'''
#include <string>
#include <cstring>
#include <cassert>
#include <cstdio>
#include "Tab5EnglishFallback.h"
#include "Tab5EnglishBitmap.h"
#include "Tab5TranslationFormat.h"
constexpr int portMAX_DELAY=0;
void xSemaphoreTake(void*,int){}
void xSemaphoreGive(void*){}
class Tab5Localization {
 public:
 bool _fontFallback=false;std::string _selectedId="fr-FR";
 size_t _translationCount=0;const Tab5EnglishEntry* _translations=nullptr;
 void* _translationMutex=nullptr;
 const char* text(const char*)const;
 void applyFontAvailability(bool);
};
'''
main=r'''
int main(){
 Tab5Localization locale;
 for(const auto& entry:TAB5_ENGLISH_BITMAP)assert(!strcmp(locale.text(entry.source),entry.source));
 locale._selectedId="en-US";
 unsigned fullEnglish=0;
 for(const auto& entry:TAB5_ENGLISH_EXPECTED){assert(!strcmp(locale.text(entry.source),entry.translated));++fullEnglish;}
 locale._selectedId="fr-FR";
 locale.applyFontAvailability(false);
 assert(locale._selectedId=="fr-FR");
 const char* previous=nullptr;unsigned tested=0;
 for(const auto& entry:TAB5_ENGLISH_BITMAP){
  assert(!strcmp(locale.text(entry.source),entry.translated));
  for(const unsigned char* p=(const unsigned char*)entry.translated;*p;++p)assert(*p<128);
  if(previous)assert(strcmp(previous,entry.source)<0);previous=entry.source;
  assert(Tab5TranslationFormat::compatible(entry.source,entry.translated));++tested;
 }
 locale.applyFontAvailability(true);assert(locale._selectedId=="fr-FR");
 for(const auto& entry:TAB5_ENGLISH_BITMAP)assert(!strcmp(locale.text(entry.source),entry.source));
 const Tab5EnglishEntry custom{"test","custom language"};locale._translations=&custom;locale._translationCount=1;
 assert(!strcmp(locale.text("test"),"custom language"));locale.applyFontAvailability(false);
 assert(!strcmp(locale.text("test"),"test"));locale.applyFontAvailability(true);
 assert(!strcmp(locale.text("test"),"custom language"));
 printf("PASS %u bitmap translations: lookup, ASCII glyph range, order, printf placeholders, temporary override and unchanged language selection\n",tested);
 printf("PASS %u English translations with the SD font: translated labels and omitted identity entries\n",fullEnglish);
}
'''
with tempfile.TemporaryDirectory(prefix='tab5-locale-') as temp:
    build=Path(temp)
    for name in ('Tab5EnglishFallback.h','Tab5EnglishBitmap.h','Tab5TranslationFormat.h'):
        shutil.copy2(args.sketch/name,build/name)
    all_english='static const Tab5EnglishEntry TAB5_ENGLISH_EXPECTED[] = {\n'+',\n'.join('{'+json.dumps(k,ensure_ascii=False)+','+json.dumps(v,ensure_ascii=False)+'}' for k,v in english.items())+'\n};\n'
    (build/'main.cpp').write_text(prefix+'\n'.join(methods)+all_english+main,encoding='utf-8')
    exe=build/('test.exe' if os.name=='nt' else 'test')
    command=([args.cxx,'/nologo','/std:c++17','/EHsc','/utf-8','/I.','main.cpp','/Fe:'+str(exe)]) if args.msvc else ([args.cxx,'-std=c++17','-I.','main.cpp','-o',str(exe)])
    subprocess.run(command,cwd=build,check=True)
    subprocess.run([str(exe)],cwd=build,check=True)
