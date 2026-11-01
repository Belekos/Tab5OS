"""Build and verify Git 0.8.33 with real repositories and loopback smart HTTPS."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys
here=Path(__file__).resolve().parent;release=here.parent.parent
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--collect-existing',action='store_true');a=p.parse_args();build=a.build.resolve()
def run(*args):subprocess.run([sys.executable,*map(str,args)],check=True)
if not a.collect_existing:
 run(here/'build.py','--build',build)
 run(here/'build.py','--build',build,'--baseline')
 run(here/'local.py','--client',build/'git_cli','--baseline-client',build/'git_cli_baseline_0832')
 run(here/'network.py','--client',build/'git_cli')
 run(here/'network.py','--client',build/'git_cli_baseline_0832','--baseline')
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest=json.loads((build/'git_cli.json').read_text())
assert digest(build/'git_cli')==manifest['clientSha256']
for name,sha in manifest['sourceHashes'].items():assert digest(release/name)==sha,('Compiled source changed',name)
reports={};sources=dict(manifest['sourceHashes'])
for name in ['git-local-0833.json','git-https-0833.json','git-network-baseline-0833.json','git-vendor-patch-0833.json']:
 path=release/'tools/verification'/name;data=json.loads(path.read_text());assert data['passed'] and data['version']=='0.8.33';reports[name]={'sha256':digest(path),'checks':len(data.get('checks',[]))}
 for source,sha in data.get('sourceHashes',{}).items():assert digest(release/source)==sha,('Test source changed',source)
 sources.update(data.get('sourceHashes',{}))
for path in here.rglob('*'):
 if path.is_file() and '__pycache__' not in path.parts:sources[path.relative_to(release).as_posix()]=digest(path)
for name,sha in sources.items():assert digest(release/name)==sha,('Source changed after build or test',name)
report={'version':'0.8.33','passed':True,'physicalDeviceTested':False,'method':__doc__,'sourceHashes':sources,'evidence':reports,'clientSha256':manifest['clientSha256'],'vendorTranslationUnits':manifest['vendorTranslationUnits'],'hostFeatureChanges':manifest['hostFeatureChanges'],'hostSha1Adaptation':manifest['hostSha1Adaptation'],'sanitizers':['address','undefined'],'checks':sum(item['checks'] for item in reports.values()),'limitations':['Loopback smart HTTPS uses OpenSSL on Linux, not the ESP32 mbedTLS backend or C6 network gate.','The original .32 engine baseline uses the corrected socket so branch routing can be exercised after the independent failed-connect issue.','Timestamp tests reproduce coarse stat cache behavior on POSIX; physical FAT, storage removal and tablet stack pressure still require device validation.']}
output=release/'tools/verification/git-engine-0833.json';output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps({'passed':True,'checks':report['checks'],'report':str(output)}))
