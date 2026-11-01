#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,subprocess,os,re,argparse
from extract import extract,closing
r=Path(__file__).resolve().parents[2];t=Path(__file__).parent;s=r/'Tab5OS_Arduino_v0.8.43_Ecriture';b=r.parent.parent/'build_0843/root-integration';b.mkdir(parents=True,exist_ok=True)
base=r.parent.parent/'tab5os_0842/Tab5OS_Ecriture_v0.8.42/Tab5OS_Arduino_v0.8.42_Ecriture'
lvgl=r.parent.parent/'build_0842/host-dependencies/lvgl-9.6.0';lib=r.parent.parent/'host_lvgl_0842'
names=['setLabelIfChanged','setTopActionVisible','updateClockLabels','updateTopAudioIndicator','stopNetworkTime','networkTimeReceived','requestNetworkTime','applyClockPolicy','serviceNetworkRtc','ntpEvent','timeZoneEvent','isImageProject','isExecutableExtension','isFirmwareExtension','isPythonExtension','isShellExtension','selectedOpenEvent','selectedExecuteEvent']
def block(text,start):
 p=text.index(start);return text[p:closing(text,text.index('{',p))]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
records={}
for mode,folder in [('before',base),('after',s)]:
 source=(folder/(folder.name+'.ino')).read_text();audio=(folder/'Tab5Audio.cpp').read_text();header=(folder/'Tab5Audio.h').read_text();methods={}
 for name in ['snapshot','wifiEnabled','wifiConnected']+(['indicatorSnapshot','wifiSnapshot'] if mode=='after' else []):methods[name]=block(audio,'bool Tab5AudioEngine::'+name+'(')
 helper=block(audio,'template <typename T>\nstatic T audioSnapshotValue')
 functions={n:extract(source,n) for n in names}
 if mode=='before':functions['isLegacyImageEditor']=extract(source,'isLegacyImageEditor')
 line=source.index('toolsHooks.runExecutable=');start=source.index('[]',line);end=closing(source,source.index('{',start));hook='static auto runExecutable='+source[start:end]+';'
 structs=block(header,'enum class Tab5AudioMode')+';\n'+block(header,'struct Tab5AudioSnapshot')+';'
 generated='#define NEW '+str(int(mode=='after'))+'\n'+structs+'\n'+helper+'\n'+'\n'.join(methods.values())+'\n'
 (b/(mode+'-audio.inc')).write_text(generated)
 (b/(mode+'-ui.inc')).write_text('\n'.join(functions[n] for n in ['setLabelIfChanged','setTopActionVisible','updateClockLabels','updateTopAudioIndicator','networkTimeReceived','stopNetworkTime','requestNetworkTime','applyClockPolicy','serviceNetworkRtc','ntpEvent','timeZoneEvent']))
 dispatch=['isImageProject','isExecutableExtension','isFirmwareExtension','isPythonExtension','isShellExtension']+(['isLegacyImageEditor'] if mode=='before' else [])+['selectedOpenEvent','selectedExecuteEvent']
 (b/(mode+'-dispatch.inc')).write_text('\n'.join(functions[n] for n in dispatch)+'\n'+hook)
 # Audio struct definitions precede the fixture engine; methods follow it.
 (b/(mode+'-structs.inc')).write_text('#define NEW '+str(int(mode=='after'))+'\n'+structs)
 (b/(mode+'-methods.inc')).write_text(helper+'\n'+'\n'.join(methods.values()))
 flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-cpp','-Wno-deprecated-declarations','-DLV_CONF_PATH="'+str(lib/'lv_conf.h')+'"','-I'+str(lvgl),'-I'+str(s),'-I'+str(b),'-DSTRUCTS="'+mode+'-structs.inc"','-DMETHODS="'+mode+'-methods.inc"','-DUI="'+mode+'-ui.inc"','-DDISPATCH="'+mode+'-dispatch.inc"']
 cmd=['g++',*flags,str(t/'main.cpp'),str(lib/'liblvgl.a'),'-pthread','-o',str(b/mode)]
 build=subprocess.run(cmd,capture_output=True,text=True);(b/(mode+'.build.log')).write_text(build.stdout+build.stderr)
 if build.returncode:print(build.stderr[-7000:]);raise SystemExit(build.returncode)
 run=subprocess.run([str(b/mode)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));(b/(mode+'.run.log')).write_text(run.stdout+run.stderr)
 print(run.stdout+run.stderr);run.check_returncode();metrics=json.loads(run.stdout.strip())
 records[mode]={'metrics':metrics,'functionHashes':{n:hashlib.sha256(v.encode()).hexdigest() for n,v in functions.items()},'hookSha256':hashlib.sha256(hook.encode()).hexdigest(),'audioStructsSha256':hashlib.sha256(structs.encode()).hexdigest(),'audioMethods':{n:hashlib.sha256(v.encode()).hexdigest() for n,v in methods.items()},'mainSha256':hashlib.sha256(source.encode()).hexdigest(),'audioSha256':hashlib.sha256(audio.encode()).hexdigest(),'logs':{p.name:sha(p) for p in b.glob(mode+'.*log')}}
 # Refuse source changes that affect executed functions, while unrelated main edits can be identified separately.
 assert all(extract((folder/(folder.name+'.ino')).read_text(),n)==v for n,v in functions.items())
o=r/'tools/verification/root-integration-0843';o.mkdir(parents=True,exist_ok=True)
import shutil
for p in b.glob('*.log'):shutil.copy2(p,o/p.name)
report={'version':'0.8.43','passed':True,'physicalDeviceTested':False,'before':records['before'],'after':records['after'],'imageFormatsSha256':sha(s/'Tab5ImageFormats.h'),'testSourceHashes':{str(p.relative_to(r)):sha(p) for p in t.glob('*') if p.is_file()},'lvglArchiveSha256':sha(lib/'liblvgl.a'),'method':'Exact extracted main UI/NTP/dispatch functions and runExecutable lambda; exact Audio snapshot/indicator/wifi accessors, actual Tab5AudioSnapshot and image format classifier. Real LVGL; counted String and RTOS mutex boundary, deterministic clock, RTC/SNTP/filesystem/app launch boundaries. ASan/UBSan.','limitations':['No complete firmware main loop, radio, RTC, real NTP traffic or native ELF execution on host.','String copy counts are controlled host measurements, not ESP32 heap measurements.','LSan disabled in this executor; ASan/UBSan active.']}
(r/'tools/verification/root-integration-0843.json').write_text(json.dumps(report,indent=2)+'\n')
