#!/usr/bin/env python3
"""Compare selected GCC diagnostics against authenticated prior-release source.

Requires the immutable baseline ZIP and its materialized sketch. Comparing
a main .ino unit additionally requires its verified generated main; ordinary
C/C++ units do not use generated Arduino prototypes. No firmware is executed.
"""
from pathlib import Path
import argparse,collections,hashlib,json,re,subprocess,zipfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--environment',type=Path,required=True);p.add_argument('--database',type=Path,required=True)
p.add_argument('--baseline-zip',type=Path,required=True);p.add_argument('--baseline-release',type=Path,required=True);p.add_argument('--baseline-generated',type=Path)
a=p.parse_args();r=Path(__file__).resolve().parents[2];import sys;sys.path.insert(0,str(r/'tools'))
from release_config import VERSION,TAG,SKETCH,BASELINE_VERSION,BASELINE_ARCHIVE_SHA256
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();assert sha(a.baseline_zip)==BASELINE_ARCHIVE_SHA256
base=a.baseline_release.resolve();bs=base/f'Tab5OS_Arduino_v{BASELINE_VERSION}_Ecriture';e=a.environment.resolve();qa=e/'qa';out=e/'warning-baseline';out.mkdir(exist_ok=True);logs=qa/'warning-baseline';logs.mkdir(exist_ok=True)
with zipfile.ZipFile(a.baseline_zip) as z:
 prefix=f'Tab5OS_Ecriture_v{BASELINE_VERSION}/{bs.name}/'
 authenticated={n[len(prefix):]:hashlib.sha256(z.read(n)).hexdigest() for n in z.namelist() if n.startswith(prefix) and not n.endswith('/') and not n[len(prefix):].startswith('sdcard/')}
 for relative,h in authenticated.items():assert sha(bs/relative)==h,relative
 proof=json.loads(z.read(f'Tab5OS_Ecriture_v{BASELINE_VERSION}/tools/verification/firmware-verification-{BASELINE_VERSION.replace(".","")}.json'))
current=json.loads((qa/'compiler-warning-pass.json').read_text());assert current['version']==VERSION and current['passed'];db=json.loads(a.database.read_text());s=r/SKETCH
has_main=any(name.endswith('.ino') for name in current['units'])
isolated_generated=None
if has_main:
 assert a.baseline_generated is not None, 'Comparing .ino requires the authenticated generated baseline main'
 assert sha(a.baseline_generated)==proof['generatedSketchSha256']
 # Isolate quoted includes from any old build copies.
 generated_dir=out/'generated-main';generated_dir.mkdir(exist_ok=True)
 isolated_generated=generated_dir/a.baseline_generated.name
 assert not [p for p in generated_dir.iterdir() if p!=isolated_generated], 'Unexpected baseline include beside generated main'
 isolated_generated.write_bytes(a.baseline_generated.read_bytes())
results={};counter=lambda lines:dict(collections.Counter(re.search(r'\[(-W[^\]]+)\]',line).group(1) if re.search(r'\[(-W[^\]]+)\]',line) else 'unclassified' for line in lines))
for name,row in current['units'].items():
 template=name if not name.endswith('.ino') else SKETCH+'.ino.cpp';entry=next(v for v in db if Path(v['file']).name==template)
 source=bs/name if not name.endswith('.ino') else isolated_generated
 if not source.is_file():
  # New units have no historical source. Record their current diagnostics;
  # do not invent a baseline compile. An empty log identifies that scope.
  log=logs/(name+'.log');log.write_text('New source unit: no prior-release source to compile.\n')
  new=counter(row['warningLines']);results[name]={'baselineCategories':{},'currentCategories':new,'categoryDeltas':dict(new),'baselineUnavailable':True,'baselineWarningLines':[],'baselineLogSha256':sha(log),'baselineCommand':None}
  continue
 command=[v.replace(str(s),str(bs)) for v in entry['arguments'] if v!='-w'];command=[str(source) if v==entry['file'] or v==entry['file'].replace(str(s),str(bs)) else v for v in command]
 command.extend(current['addedFlags']);command[command.index('-o')+1]=str(out/(name+'.o'))
 run=subprocess.run(command,cwd=entry['directory'],capture_output=True,text=True);log=logs/(name+'.log');log.write_text(run.stdout+run.stderr)
 if run.returncode:print(run.stdout+run.stderr);raise SystemExit(run.returncode)
 lines=[v for v in (run.stdout+run.stderr).splitlines() if 'warning:' in v];old=counter(lines);new=counter(row['warningLines'])
 results[name]={'baselineCategories':old,'currentCategories':new,'categoryDeltas':{k:new.get(k,0)-old.get(k,0) for k in sorted(set(old)|set(new))},'baselineWarningLines':lines,'baselineLogSha256':sha(log),'baselineCommand':command}
report={'version':VERSION,'passed':True,'kind':'baseline-warning-comparison','baselineVersion':BASELINE_VERSION,'baselineArchiveSha256':BASELINE_ARCHIVE_SHA256,'baselineSourceFilesAuthenticated':len(authenticated),'baselineGeneratedMainSha256':sha(a.baseline_generated) if has_main else None,'units':results,'baselineWarningCount':sum(sum(v['baselineCategories'].values()) for v in results.values()),'currentWarningCount':sum(sum(v['currentCategories'].values()) for v in results.values()),'scriptSha256':sha(Path(__file__)),'limitations':['Diagnostic compilation only, no hardware or baseline firmware execution.','Baseline sources authenticated against prior delivered ZIP; .ino comparisons additionally require its verified generated main in an isolated directory. Ordinary C/C++ comparisons use no generated main. Same current SDK/ABI and dependency paths are used.','Diagnostic categories do not prove behavior; safety-relevant warnings require source review.']}
(qa/'compiler-warning-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ('passed','baselineVersion','baselineWarningCount','currentWarningCount')}))
