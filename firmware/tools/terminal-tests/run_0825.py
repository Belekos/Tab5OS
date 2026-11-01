from pathlib import Path
import argparse,hashlib,json,subprocess,tempfile,shutil
p=argparse.ArgumentParser();p.add_argument('sketch',type=Path);a=p.parse_args();sketch=a.sketch.resolve();here=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='tab5-terminal-',dir=here) as tmp:
 d=Path(tmp)
 shutil.copy2(here.parent/'network-tests/ShellPlatform.h',d/'ShellPlatform.h')
 shutil.copy2(here/'terminal.cpp',d/'terminal.cpp')
 for header in ['unistd.h','dirent.h','fnmatch.h']:(d/header).write_text('#include "ShellPlatform.h"\n',encoding='utf-8')
 subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/MDd','/D_DEBUG','/W3','/wd4996','/wd4267','/utf-8','/DARDUINO','/DTAB5_SHELL_HOST','/I'+str(d),'/I'+str(sketch),'terminal.cpp','/Fe:terminal.exe'],cwd=d,check=True)
 result=subprocess.run([str(d/'terminal.exe'),str(d/'volume')],cwd=d,capture_output=True,text=True,encoding='utf-8');print(result.stdout);print(result.stderr);result.check_returncode()
 report=json.loads(result.stdout.strip().splitlines()[-1]);report['version']='0.8.25';report['sources']={x:hashlib.sha256((sketch/x).read_bytes()).hexdigest() for x in ['Tab5Shell.cpp','Tab5Shell.h','Tab5ShellHelp.h','Tab5ShellBuffer.h','Tab5TerminalBuffer.h','Tab5Terminal.cpp','Tab5Terminal.h','Tab5NetworkTools.cpp','Tab5NetworkTools.h','Tab5NetworkHttp.cpp','Tab5NetworkHttp.h']}
 report['scope']='Production shell engine, PSRAM-buffer host allocator, console ring and yield cadence; MSVC debug CRT tracks live host allocations, not physical SRAM. UI and FreeRTOS lifecycle require separate target review.'
 report['harness_sha256']=hashlib.sha256((here/'terminal.cpp').read_bytes()).hexdigest()
 legacy=here.parent/'network-tests/run_shell.py'
 result=subprocess.run(['python','-X','utf8',str(legacy),str(sketch)],capture_output=True,text=True,encoding='utf-8');print(result.stdout);print(result.stderr);result.check_returncode()
 report['existing_commands_regression']={'passed':result.returncode==0,'exit_code':result.returncode,'runner':'tools/network-tests/run_shell.py','runner_sha256':hashlib.sha256(legacy.read_bytes()).hexdigest(),'harness_sha256':hashlib.sha256((legacy.parent/'shell.cpp').read_bytes()).hexdigest(),'platform_sha256':hashlib.sha256((legacy.parent/'ShellPlatform.h').read_bytes()).hexdigest(),'output':result.stdout,'scope':'Production shell existing help/options, SD/USB paths, script/redirection/file commands and network argument forwarding; no live network.'}
 for key,runner,names in [('network_regression','run.py',['session.cpp','protocol.cpp','arguments.cpp','Platform.h']),('http_regression','run_http.py',['http.cpp','HttpPlatform.h'])]:
  script=legacy.parent/runner;result=subprocess.run(['python','-X','utf8',str(script),str(sketch)],capture_output=True,text=True,encoding='utf-8',errors='replace');print(result.stdout);print(result.stderr);result.check_returncode()
  report[key]={'passed':True,'exit_code':result.returncode,'runner':'tools/network-tests/'+runner,'sources':{name:hashlib.sha256((legacy.parent/name).read_bytes()).hexdigest() for name in [runner]+names},'output':result.stdout,'scope':'Production network implementation, simulated sockets/HTTP; includes optional stderr and historical callback behavior; no live network.'}
 (here.parent/'verification/terminal-0825.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
