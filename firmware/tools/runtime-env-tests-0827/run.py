"""Production environment/shell/Git callbacks with host storage/vault boundary."""
from pathlib import Path
import hashlib,json,subprocess,tempfile,shutil,types,sys,runpy,os
here=Path(__file__).resolve().parent;release=here.parents[1];sketch=release/'Tab5OS_Arduino_v0.8.27_Ecriture'
if not os.environ.get('VSCMD_VER'):
    helper=Path('C:/workspace/tab5-runtime-env-0827.cmd')
    helper.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(helper)]))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs=[sketch/x for x in ['Tab5Environment.cpp','Tab5Environment.h','Tab5Secrets.h','Tab5Shell.cpp','Tab5Shell.h','Tab5ShellHelp.h','Tab5Terminal.cpp','Tab5Terminal.h','Tab5Git.cpp','Tab5Git.h','Tab5GitPort.cpp','Tab5GitPort.h','Tab5GitPull.cpp','Tab5GitPull.h','Tab5Python.cpp','Tab5Executable.cpp','Tab5AppApi.h','sdcard/MicroPython/lib/tab5.py']]+[here/'environment.cpp',here/'git-secret.cpp',here/'run.py',release/'tools/network-tests/ShellPlatform.h']
before={str(p.relative_to(release)):sha(p) for p in inputs}
build=Path(tempfile.mkdtemp(prefix='runtime-env-0827-',dir='C:/workspace'))
shutil.copy2(release/'tools/network-tests/ShellPlatform.h',build/'ShellPlatform.h')
for name in ['unistd.h','dirent.h','fnmatch.h']:(build/name).write_text('#include "ShellPlatform.h"\n')
(build/'Arduino.h').write_text('#pragma once\n#include <string>\nusing String=std::string;\n')
(build/'esp_heap_caps.h').write_text('#pragma once\n#include <cstdlib>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\ninline void* heap_caps_calloc(size_t n,size_t s,int){return calloc(n,s);}\n')
source=(sketch/'Tab5Terminal.cpp').read_text(encoding='utf-8');start=source.index('bool runScript(');end=source.index('\nbool busy()',start);(build/'script-entrypoint.inc').write_text(source[start:end],encoding='utf-8')
reports=[]
for test in ['environment','git-secret']:
 command=['cl','/nologo','/std:c++17','/EHsc','/MD','/utf-8','/wd4996','/wd4267','/DNOMINMAX','/DARDUINO','/DTAB5_SHELL_HOST','/I'+str(build),'/I'+str(sketch),'/FI'+str(build/'ShellPlatform.h'),str(here/(test+'.cpp')),'/Fe:'+test+'.exe']
 if test=='git-secret':command += [str(sketch/'Tab5GitPull.cpp'),str(release.parent/'libgit2-host-0822/git2.lib'),'winhttp.lib','rpcrt4.lib','crypt32.lib','ole32.lib','ws2_32.lib','secur32.lib','bcrypt.lib','advapi32.lib']
 subprocess.run(command,cwd=build,check=True)
 result=subprocess.run([str(build/(test+'.exe')),(build/(test+'-volume')).as_posix()],cwd=build,capture_output=True,text=True,encoding='utf-8');print(result.stdout);print(result.stderr);result.check_returncode();reports.append(json.loads(result.stdout.strip().splitlines()[-1]))
calls=[]
def bridge(op,a=0,b=0,c=0,data=None,capacity=0):
 calls.append((op,a,data,capacity))
 if op==107:
  if data==b'MISSING\0':raise OSError(2)
  if data==b'LOCKED\0':raise OSError(13)
  return b'fixture-value'
 if op==109 and a==1:return b'TOKEN\nPUBLIC\n'
 return 0
sys.modules['_tab5']=types.SimpleNamespace(call=bridge)
module=runpy.run_path(str(sketch/'sdcard/MicroPython/lib/tab5.py'))
assert module['env']('TOKEN')=='fixture-value' and calls[-1]==(107,0,b'TOKEN\0',513)
assert module['env']('MISSING','fallback')=='fallback'
try:module['env']('LOCKED','fallback');raise AssertionError('Locked reference returned fallback')
except OSError as error:assert error.args[0]==13
module['setenv']('LANG','fr-FR');assert calls[-1]==(108,0,b'LANG\0fr-FR\0',0)
module['bindenv']('TOKEN','GIT_TOKEN');assert calls[-1]==(108,1,b'TOKEN\0GIT_TOKEN\0',0)
module['bindenv']('TOKEN');assert calls[-1]==(108,1,b'TOKEN\0TOKEN\0',0)
module['unsetenv']('TOKEN');assert calls[-1]==(109,0,b'TOKEN\0',0)
assert module['env_names']()==['TOKEN','PUBLIC']
for function,args in [('setenv',('TOKEN','bad\0value')),('env',('bad\0name',)),('bindenv',('TOKEN','bad\0name'))]:
 try:module[function](*args);raise AssertionError('NUL injection accepted')
 except ValueError:pass
python_source=(sketch/'Tab5Python.cpp').read_text(encoding='utf-8');elf_source=(sketch/'Tab5Executable.cpp').read_text(encoding='utf-8')
assert 'if(op>=107&&op<=109)return tab5_env_call(op,a,input,len,output,capacity);' in python_source
assert 'if(op>=107&&op<=109)return tab5_env_call(op,a,in,n,out,capacity);' in elf_source
assert before=={str(p.relative_to(release)):sha(p) for p in inputs},'Inputs changed during test'
report=dict(version='0.8.27',passed=True,physicalDeviceTested=False,sourcesStableDuringTest=True,sources=before,environment=reports[0],git=reports[1],python=dict(passed=True,checks=13,scope='Actual shipped tab5.py executed with broker boundary mocked; binary framing matches production C broker tested above. Python/ELF routing source contracts asserted; no claim of complete VM/ELF execution.'),scope='Actual C runtime environment, shell expansion/export/redirections, production Explorer .sh entrypoint, libgit2 credential callbacks and actual Git configuration operations. Vault boundary mock simulates lock/unlock/deletion; native crypto/SD durability tested separately. Hardware brokers source identity included; target syntax tested separately.')
(release/'tools/verification/runtime-env-0827.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'passed':True,'checks':sum(r['checks'] for r in reports)}))
