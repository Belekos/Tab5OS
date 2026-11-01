"""Compile the real IME engine and SD reader against tiny host I/O adapters."""
from pathlib import Path
import argparse,subprocess,tempfile,shutil,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--cxx',default='cl');a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parents[1];sketch=release/'Tab5OS_Arduino_v0.8.24_Ecriture'
logs=[]
with tempfile.TemporaryDirectory(prefix='tab5-ime-') as temporary:
    work=Path(temporary)
    for name in ['Tab5Ime.h','Tab5ImeDictionary.h','Tab5ImeDictionary.cpp','Tab5CjkFonts.h','Tab5CjkFonts.cpp','Tab5LanguageMetadata.h']:shutil.copy2(sketch/name,work/name)
    for name in ['Arduino.h','Tab5VolumeFS.h','lvgl.h','esp_heap_caps.h','test_ime.cpp','test_dictionary.cpp','test_fonts.cpp','test_metadata.cpp']:shutil.copy2(here/name,work/name)
    for name,files,args in [('composition',['test_ime.cpp'],[]),('dictionary',['Tab5ImeDictionary.cpp','test_dictionary.cpp'],[str(sketch/'sdcard')]),('fonts',['Tab5CjkFonts.cpp','test_fonts.cpp'],[str(sketch/'sdcard')]),('metadata',['test_metadata.cpp'],[str(p) for p in (sketch/'sdcard/System/languages').glob('*.json')])]:
        command=[a.cxx,'/nologo','/std:c++17','/EHsc','/W4','/utf-8','/I.']+files+['/Fe:'+name+'.exe']
        result=subprocess.run(command,cwd=work,capture_output=True,text=True,encoding='utf8',errors='replace');logs.append(result.stdout+result.stderr)
        if result.returncode:print(''.join(logs));raise SystemExit(result.returncode)
        result=subprocess.run([str(work/(name+'.exe'))]+args,cwd=work,capture_output=True,text=True,encoding='utf8',errors='replace');logs.append(result.stdout+result.stderr)
        if result.returncode:print(''.join(logs));raise SystemExit(result.returncode)
report={'version':'0.8.24','passed':True,'physicalDisplayTested':False,'checks':['real composer: all 11172 modern Hangul syllables','romaji, hiragana, katakana, backspace, buffer bounds and Latin toggle','actual SD reader: candidates through final page, missing and truncated dictionaries'],
        'sources':{name:hashlib.sha256((sketch/name).read_bytes()).hexdigest() for name in ['Tab5Ime.h','Tab5ImeDictionary.h','Tab5ImeDictionary.cpp','Tab5CjkFonts.h','Tab5CjkFonts.cpp','Tab5LanguageMetadata.h']},
        'assets':{name:hashlib.sha256((sketch/'sdcard'/name).read_bytes()).hexdigest() for name in ['System/ime/zh-pinyin.dat','System/ime/ja-kana.dat']},'output':''.join(logs)}
(release/'tools/verification/ime-native-0824.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(''.join(logs))
