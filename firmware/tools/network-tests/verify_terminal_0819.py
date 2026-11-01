"""Run host regression suites and record the exact production-source hashes."""
from pathlib import Path
import hashlib,json,subprocess,sys,datetime
here=Path(__file__).resolve().parent
root=here.parent.parent
sketch=next(root.glob('Tab5OS_Arduino_v*_Ecriture'))
files=['Tab5Shell.cpp','Tab5Shell.h','Tab5ShellHelp.h','Tab5Terminal.cpp','Tab5Terminal.h','Tab5TerminalPath.h','Tab5NetworkTools.cpp','Tab5NetworkTools.h','Tab5NetworkToolsProtocol.h','Tab5NetworkArgs.h','Tab5NetworkHttp.cpp','Tab5NetworkHttp.h','Tab5SecureClient.h','Tab5CooperativeClient.h']
def hashes():return {file:hashlib.sha256((sketch/file).read_bytes()).hexdigest() for file in files}
before=hashes();checks=[]
for script in ['run_msvc.cmd','run_http.cmd','run_shell.cmd']:
 result=subprocess.run(['cmd','/c',str(here/script),str(sketch)],capture_output=True)
 output=result.stdout.decode('utf-8',errors='replace')+result.stderr.decode('utf-8',errors='replace')
 checks.append(dict(suite=script,exitCode=result.returncode,output=output))
 print(script,result.returncode,output,flush=True)
after=hashes();passed=before==after and all(c['exitCode']==0 for c in checks)
report={'version':'0.8.19','createdUTC':datetime.datetime.now(datetime.timezone.utc).isoformat(),'passed':passed,'sourceUnchangedDuringTests':before==after,'sourceSha256':after,'checks':checks,'scope':'Production parsers, shell, sockets/DNS and HTTP transfer executed on host. Platform transports are deterministic fixtures; real TLS validation and physical Wi-Fi/SD/USB require device checks.'}
dest=root/'tools/verification/terminal-network-0819.json';dest.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
sys.exit(not passed)
