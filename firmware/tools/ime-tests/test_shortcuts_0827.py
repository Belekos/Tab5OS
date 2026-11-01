"""Exercise the real HID decoder with the distributed keyboard mappings."""
from pathlib import Path
import json,hashlib,subprocess,tempfile,shutil
here=Path(__file__).resolve().parent;root=here.parents[1];sketch=root/'Tab5OS_Arduino_v0.8.27_Ecriture'
keyboards=json.loads((sketch/'sdcard/System/keyboards.json').read_text(encoding='utf8'))['keyboards']
selected=[x['id'] for x in keyboards];tables=[];calls=[]
for n,code in enumerate(selected):
    layout=next(x for x in keyboards if x['id']==code);rows=layout.get('hid',[])
    if rows:
        table=[]
        for row in rows:
            usage,lower,upper,altgr,letter,dead=row
            table.append('{%d,%d,%s,{%s}}'%(usage,dead,'true' if letter else 'false',','.join(json.dumps(v,ensure_ascii=False) for v in [lower,upper,altgr])))
        tables.append('static const writing::KeyMapping layout%d[]={%s};'%(n,','.join(table)))
    calls.append('check(%s,%s,%s,%s);'%(json.dumps(code), 'true' if code=='fr-FR' else 'false', 'layout%d'%n if rows else 'nullptr',str(len(rows))))
source=r'''
#include "WriterKeys.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace writing;
static unsigned assertions=0;
static void expect(KeyDecoder& decoder,uint8_t usage,uint8_t modifiers,const char* wanted){
  auto e=decoder.press(usage,modifiers);
  if(e.key!=Key::Text||strcmp(e.text,wanted)){fprintf(stderr,"usage=%u modifiers=%u wanted='%s' got='%s' key=%u\n",usage,modifiers,wanted,e.text,unsigned(e.key));abort();}
  assert(e.ctrl==bool(modifiers&0x11));assert(e.meta==bool(modifiers&0x88));assert(e.alt==bool(modifiers&0x44));++assertions;
}
static void check(const char* code,bool french,const KeyMapping* layout,size_t size){
  KeyDecoder d;d.french(french);d.mapping(layout,size);
  const char* commands="acxvzyyy";
  for(const char* c=commands;*c;++c){uint8_t usage=uint8_t(4+*c-'a');
    if(french&&*c=='a')usage=20;if(french&&*c=='z')usage=26;
    if(!strcmp(code,"de-DE")&&*c=='z')usage=28;if(!strcmp(code,"de-DE")&&*c=='y')usage=29;if(!strcmp(code,"de-DE")&&*c=='y')usage=29;if(!strcmp(code,"de-DE")&&*c=='y')usage=29;
    char expected[2]={*c,0};
    for(uint8_t m:{uint8_t(1),uint8_t(8),uint8_t(3),uint8_t(10)})expect(d,usage,m,expected);
  }
  expect(d,17,4,"n");expect(d,17,5,"n");
  d.press(0x39,0);for(const char* c=commands;*c;++c){uint8_t usage=uint8_t(4+*c-'a');if(french&&*c=='a')usage=20;if(french&&*c=='z')usage=26;if(!strcmp(code,"de-DE")&&*c=='z')usage=28;if(!strcmp(code,"de-DE")&&*c=='y')usage=29;if(!strcmp(code,"de-DE")&&*c=='y')usage=29;if(!strcmp(code,"de-DE")&&*c=='y')usage=29;char expected[2]={*c,0};expect(d,usage,1,expected);}d.press(0x39,0);
  if(!strcmp(code,"ru-RU")){expect(d,4,0,"ф");expect(d,4,2,"Ф");expect(d,51,1,";");}
  else if(!strcmp(code,"ar")){expect(d,4,0,"ش");expect(d,20,2,"َ");expect(d,47,1,"[");}
  else expect(d,french?20:4,0,"a");
  for(uint8_t m:{uint8_t(0x40),uint8_t(0x41)}){
    uint8_t usage=!strcmp(code,"de-DE")?8:4;const char* expected=!strcmp(code,"de-DE")?"€":!strcmp(code,"ru-RU")||!strcmp(code,"ar")?"a":nullptr;
    if(expected){auto e=d.press(usage,m);assert(e.key==Key::Text&&!strcmp(e.text,expected)&&!e.ctrl&&!e.alt&&!e.meta);++assertions;}
  }
  auto menu=d.press(0x65,0);assert(menu.key==Key::ContextMenu&&!repeatable(menu));++assertions;
  for(uint8_t modifier:{uint8_t(2),uint8_t(0x20)}){auto e=d.press(0x43,modifier);assert(e.key==Key::ContextMenu&&e.shift&&!repeatable(e));++assertions;}
  assert(d.press(0x43,0).key==Key::None);++assertions;
  assert(d.press(0x3a,0).key==Key::F1);++assertions;
  auto rename=d.press(0x3b,0);assert(rename.key==Key::F2&&!repeatable(rename));++assertions;
  for(uint8_t modifier:{uint8_t(3),uint8_t(6),uint8_t(10)}){assert(d.press(0x43,modifier).key!=Key::ContextMenu);++assertions;}
  CommandLatch super;uint8_t none[6]{};assert(!super.report(0x08,none));assert(super.report(0,none));++assertions;
  printf("PASS shortcuts, native text and modifiers: %s\n",code);
}
'''
source='#include <cstdlib>\n#include <initializer_list>\n'+source+'\n'+ '\n'.join(tables)+'\nint main(){'+''.join(calls)+'printf("PASS %u real decoder checks\\n",assertions);}\n'
with tempfile.TemporaryDirectory(prefix='tab5-shortcuts-') as temp:
    work=Path(temp)
    for name in ['WriterKeys.cpp','WriterKeys.h']:shutil.copy2(sketch/name,work/name)
    (work/'test.cpp').write_text(source,encoding='utf8')
    command=['cl','/nologo','/std:c++17','/EHsc','/utf-8','/W4','/I.','WriterKeys.cpp','test.cpp','/Fe:test.exe']
    c=subprocess.run(command,cwd=work,capture_output=True,text=True,encoding='utf8',errors='replace')
    if c.returncode:print(c.stdout+c.stderr);raise SystemExit(c.returncode)
    run=subprocess.run([str(work/'test.exe')],cwd=work,capture_output=True,text=True,encoding='utf8',errors='replace')
    print(run.stdout+run.stderr)
    report={'version':'0.8.27','exitCode':run.returncode,'physicalDeviceTested':False,'passed':run.returncode==0,'layouts':selected,'sources':{p:hashlib.sha256((sketch/p).read_bytes()).hexdigest() for p in ['WriterKeys.cpp','WriterKeys.h']},'keyboardSha256':hashlib.sha256((sketch/'sdcard/System/keyboards.json').read_bytes()).hexdigest(),'output':run.stdout+run.stderr,'harness':{str(p.relative_to(root)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),Path(__file__).with_suffix('.cmd')]}}
    (root/'tools/verification/keyboard-shortcuts-0827.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    raise SystemExit(run.returncode)
