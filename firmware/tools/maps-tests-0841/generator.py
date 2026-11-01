#!/usr/bin/env python3
"""Generator/runtime normalization equivalence, deterministic fixture and data provenance."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,random,struct,subprocess,tempfile,zipfile
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.40_Ecriture';here=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--build',type=Path);p.add_argument('--source',type=Path);p.add_argument('--final',action='store_true');a=p.parse_args();build=a.build or Path(tempfile.mkdtemp(prefix='maps-generator-'));build.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('gazetteer',root/'tools/maps-gazetteer/build.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
values=['a'+chr(cp)+'b' for cp in range(1,0x2100)]+[' A\u3000B ','Évry','São Paulo','Москва','東京','  L’Haÿ-les-Roses  ','x'*160,'\x01']
random.seed(840)
for _ in range(10000):values.append(''.join(chr(random.choice([*range(32,0x530),*range(0x1e00,0x2070),0x3000])) for _ in range(random.randrange(1,30))))
payload=''.join(s.encode().hex()+':'+(g.normalize(s) if len(g.normalize(s).encode())<160 else '').encode().hex()+'\n' for s in values)
cmd=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(sketch),str(here/'normalize.cpp'),'-o',str(build/'normalize')];subprocess.run(cmd,check=True,capture_output=True,text=True)
r=subprocess.run([str(build/'normalize')],input=payload,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),check=False);print(r.stdout+r.stderr);r.check_returncode()
# A small official-schema fixture exercises filtering, alternate names and UTF-8 limits.
rows=[]
for gid,name,ascii,aliases,lat,lon,pop in [(1,'Évry','Evry','ÉVRY,Evry','48.63','2.45',50000),(2,'New Town','New Town','Town','-30','150',1234),(3,'bad','bad','','nan','3',1234),(4,'polar','polar','','89','3',1234)]:
 rows.append('\t'.join(map(str,[gid,name,ascii,aliases,lat,lon,'P','PPL','FR','','11','91','','',pop,'','', 'Europe/Paris','2026-01-01'])))
source=build/'cities1000.zip'
with zipfile.ZipFile(source,'w') as z:z.writestr('cities1000.txt','\n'.join(rows))
one=g.generate(source,build/'one.t5geo',build/'fold.h');two=g.generate(source,build/'two.t5geo')
assert one['places']==2 and one['keys']>=3 and one['sha256']==two['sha256']
assert sha(build/'fold.h')==sha(sketch/'Tab5GeoFold.h')
data=sketch/'sdcard/System/maps/gazetteer/cities.t5geo';meta=json.loads(data.with_suffix('.json').read_text());assert meta['sha256']==sha(data) and meta['bytes']==data.stat().st_size and meta['generatorSha256']==sha(root/'tools/maps-gazetteer/build.py') and meta['foldHeaderSha256']==sha(sketch/'Tab5GeoFold.h')
checks=len(values)+6
if a.source:
 assert sha(a.source)==meta['sourceSha256'];rebuilt=g.generate(a.source,build/'rebuilt.t5geo');assert rebuilt['sha256']==meta['sha256'];checks+=2
report={'version':'0.8.40','passed':a.final,'draft':not a.final,'checks':checks,'sourceHashes':{str(p.relative_to(root)):sha(p) for p in [root/'tools/maps-gazetteer/build.py',sketch/'Tab5GeoFold.h',sketch/'Tab5GeoIndex.h',data,data.with_suffix('.json')]},'testSourceHashes':{str(p.relative_to(root)):sha(p) for p in [here/'generator.py',here/'normalize.cpp']},'normalizationCases':len(values),'recreatedOfficialIndex':bool(a.source),'sourceSha256':meta['sourceSha256'],'indexSha256':meta['sha256'],'physicalDeviceTested':False}
(root/'tools/verification/maps-generator-0840.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
