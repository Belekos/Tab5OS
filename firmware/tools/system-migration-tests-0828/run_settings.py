from pathlib import Path
import datetime,hashlib,json,os,subprocess,sys
here=Path(__file__).resolve().parent;release=here.parent.parent
sketch=release/'Tab5OS_Arduino_v0.8.28_Ecriture';build=Path('C:/workspace/tab5-settings-recovery-0828');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
def block(text,signature):
    a=text.index(signature);i=text.index('{',a);depth=1;end=i+1
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    return text[a:end]
text=(sketch/(sketch.name+'.ino')).read_text(encoding='utf-8')
names=['static bool migrateSystemDirectory()', 'static bool saveSettings()', 'static void loadSettings()', 'static bool loadSettingsSources()', 'static bool reloadSettingsAfterMigration()']
chunks=[block(text,name) for name in names];structure=block(text,'struct AppSettings')+';'
source=(here/'settings.cpp').read_text(encoding='utf-8').replace('/* SETTINGS STRUCT */',structure).replace('/* CONTROLLERS */','\n'.join(chunks));(build/'settings.cpp').write_text(source,encoding='utf-8')
recovery=block(text,'static void serviceSdRecovery(')
assert recovery.index('sdReady=migrateSystemDirectory()')<recovery.index('reloadSettingsAfterMigration()')<recovery.index('Tab5Audio.setStorageAvailable')
setup=block(text,'void setup() {')
assert setup.index('sdReady=migrateSystemDirectory()')<setup.index('Tab5SystemResources::start(sdReady)')<setup.index('loadSettingsSources()')
assert 'Tab5MediaSession::begin(settings.audioVolume,sdReady&&settingsReadComplete);' in setup
compiled=subprocess.run(['cl','/nologo','/utf-8','/EHsc','/std:c++17','/fsanitize=address','/Zi','/I'+str(here),'/I'+str(sketch),str(build/'settings.cpp'),str(sketch/'Tab5SystemMigration.cpp'),'/Fe:'+str(build/'settings.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
(build/'compile.log').write_text(compiled.stdout+compiled.stderr,encoding='utf-8')
if compiled.returncode:print(compiled.stdout+compiled.stderr);raise SystemExit(compiled.returncode)
result=subprocess.run([str(build/'settings.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=30);print(result.stdout+result.stderr)
paths=[sketch/'Tab5SystemMigration.cpp',sketch/'Tab5SystemMigration.h',here/'settings.cpp',here/'Arduino.h',here/'FS.h',here/'run_settings.py']
report={'version':'0.8.28','checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'passed':result.returncode==0,'physicalDeviceTested':False,
 'method':'Production migration module plus .ino settings save/load, sources, migration and recovery controllers extracted verbatim, compiled with MSVC AddressSanitizer. Arduino filesystem boundary simulated, media recovery boundary simulated (actual session tested separately). Read interrupted at every settings byte; boot/recovery ordering asserted; absence versus I/O errors, backup, invalid files, zero mute, protected theme loading, guarded writes and recovery volume precedence. Does not simulate physical FAT or run the full board setup.',
 'result':{'exitCode':result.returncode,'output':result.stdout+result.stderr},
 'sources':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
 'controllers':{name:hashlib.sha256(value.encode()).hexdigest() for name,value in zip(names+['struct AppSettings','static void serviceSdRecovery(','void setup() {'],chunks+[structure[:-1],recovery,setup])}}
(release/'tools/verification/settings-recovery-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');raise SystemExit(result.returncode)
