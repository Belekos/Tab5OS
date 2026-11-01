"""Freeze current changed-source coverage after all test owners have finished."""
from pathlib import Path
import hashlib,json
from release_config import ROOT,VERSION,TAG,SKETCH,BASELINE_VERSION,BASELINE_TAG
from check_preserved import preserved_path
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
baseline=json.loads((ROOT/('tools/verification/baseline-'+BASELINE_TAG+'-source.json')).read_text())
expected={p.replace('Tab5OS_Arduino_v'+BASELINE_VERSION+'_Ecriture.ino',SKETCH+'.ino'):h for p,h in baseline['files'].items() if preserved_path(p)}
s=ROOT/SKETCH;actual={p.relative_to(s).as_posix():sha(p) for p in s.rglob('*') if p.is_file() and preserved_path(p.relative_to(s).as_posix())}
changed=sorted(p for p in set(expected)|set(actual) if expected.get(p)!=actual.get(p))
reports=[];covers={};stale=[]
for p in sorted((ROOT/'tools/verification').glob('*'+TAG+'*.json')):
 if any(x in p.name for x in ('firmware-verification','preserved-components','publication','evolutions-localization','languages-','resources-')):continue
 d=json.loads(p.read_text())
 if d.get('version',d.get('release'))!=VERSION or not(d.get('passed') is True or d.get('result')=='PASS') or not d.get('sourceHashes'):continue
 for group in ('sourceHashes','testSourceHashes','testHashes','integrationSourceHashes','evidenceHashes','screenshots'):
  for name,h in d.get(group,{}).items():
   q=(ROOT/name).resolve();assert q.is_relative_to(ROOT.resolve()),name
   if not q.is_file() or sha(q)!=h:stale.append((p.name,name))
 reports.append({'file':p.name,'description':p.stem.replace('-'+TAG,'').replace('-',' ')})
 for name,h in {**d.get('sourceHashes',{}),**d.get('integrationSourceHashes',{}),**d.get('testSourceHashes',{})}.items():
  if name.startswith(SKETCH+'/') and actual.get(name[len(SKETCH)+1:])==h:covers[name[len(SKETCH)+1:]]=p.name
assert not stale,('Stale reports need owner verification',stale)
missing=[p for p in changed if p not in covers];assert not missing,('Uncovered changed source',missing)
changes={p:{'reason':'Reviewed targeted stability/cleanup change; see the named current report and module audit.','report':covers[p]} for p in changed}
out={'version':VERSION,'baselineVersion':BASELINE_VERSION,'finalized':True,'targetedReports':reports,'sourceChanges':changes,'physicalDeviceTested':False}
(ROOT/'tools/release_scope.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'changedSources':len(changed),'currentReports':len(reports),'finalized':True}))
