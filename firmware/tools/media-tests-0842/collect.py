#!/usr/bin/env python3
"""Collect the completed 0.8.42 media checks for the release verification index."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[2];audit=root/'tools/audit-0842';sketch=root/'Tab5OS_Arduino_v0.8.42_Ecriture';verification=root/'tools/verification'
production=['Tab5Audio.cpp','Tab5Audio.h','Tab5Video.cpp','Tab5Video.h','Tab5NetworkHttp.cpp','Tab5NetworkHttp.h','Tab5NetworkArgs.h','Tab5Compatibility.cpp','Tab5Compatibility.h','Tab5CompatibilityPolicy.h','Tab5NetworkGate.cpp','Tab5NetworkGate.h']
sourceHashes={str((sketch/n).relative_to(root)):hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in production}
testFiles=list((root/'tools/media-tests-0842').glob('*.py'))+list((root/'tools/media-tests-0842').glob('*.cpp'))
for folder in ['compatibility-tests-0832','network-gate-tests-0826']:
 testFiles.extend(p for p in (root/'tools'/folder).rglob('*') if p.is_file() and p.suffix in ['.py','.cpp','.h'])
testFiles.extend(root/'tools/network-tests'/n for n in ['http.cpp','HttpPlatform.h'])
testHashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in testFiles}
inputs=['audio-accessors-after','video-lifecycle','http-after','catalogue-sort','compatibility','network-gate','recording']
results={n:json.loads((audit/(n+'.json')).read_text()) for n in inputs}
assert all(d.get('passed',d.get('existingRegressionCasesPassed',False)) for d in results.values())
assert all(results['http-after'][n] for n in ['initialMetadataFaultProtected','finalMetadataFaultProtected','backupMetadataFaultProtected'])
report={'version':'0.8.42','passed':True,'physicalDeviceTested':False,'scope':'Collection of completed host ASan/UBSan media/network tests. Target object compilation is separately recorded in media-target-0842.json; full firmware/hardware is not claimed here.','sourceHashes':sourceHashes,'testSourceHashes':testHashes,'results':results,'baseline':{'archiveSha256':'01389322d10ae80b668ce369b2586e41831af03e3e407ecd3d60da62dd2667ff','accessors':json.loads((audit/'audio-accessors-before.json').read_text()),'http':json.loads((audit/'http-before.json').read_text())}}
for name,data in results.items():
 updated={**data,'version':'0.8.42','passed':True,'physicalDeviceTested':False,'sourceHashes':sourceHashes,'testSourceHashes':testHashes}
 (verification/('media-'+name+'-0842.json')).write_text(json.dumps(updated,ensure_ascii=False,indent=2)+'\n')
out=verification/'media-audit-0842.json';out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(out)
