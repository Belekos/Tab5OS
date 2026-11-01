#!/usr/bin/env python3
"""Actual packaging loop on a temporary fixture; firmware-validation boundary mocked."""
from pathlib import Path
import tempfile,sys,json,hashlib,zipfile,contextlib,io
here=Path(__file__).resolve().parent;release=here.parents[1];sys.path.insert(0,str(release/'tools'))
import release_artifacts as artifacts
import package_release as package
from release_config import SKETCH,VERSION
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
kept={'.gitignore':'*.o\n',SKETCH+'/source.cpp':'// source\n',SKETCH+'/tools/core/.gitlab-ci.yml':'image: build\n','tools/verification/build-0830/build.log':'complete log\n','tools/verification/ui/frame.png':'PNG fixture','tools/verification/report.json':'{}',SKETCH+'/sdcard/System/maps/gazetteer/cities.t5geo':'index',SKETCH+'/sdcard/Documents/.report.ppm.ABC123':'user document',SKETCH+'/sdcard/Applications/Image-editor.app.elf':'elf1',SKETCH+'/sdcard/Applications/_Custom.elf':'elf2',SKETCH+'/sdcard/Factory/Tab5OS-'+VERSION+'/manifest.json':'{}','firmware/Tab5OS_v'+VERSION+'.bin':'bin','firmware/SHA256.txt':'sha','firmware/debug/'+SKETCH+'.ino.elf':'elf','firmware/debug/'+SKETCH+'.ino.map':'map'}
omitted={'tools/verification/build-0830/.build.log.a1B2c3':'copy','tools/verification/ui/.frame.ppm.a1B2c3.D4e5F6':'copy',SKETCH+'/sdcard/System/maps/gazetteer/.cities.t5geo.a1B2c3':'copy',SKETCH+'/sdcard/Factory/Tab5OS-0.8.47/manifest.json':'old','firmware/Tab5OS_v0.8.47.bin':'old','firmware/debug/.old.ino.map.a1B2c3':'copy'}
checks=0
with tempfile.TemporaryDirectory(prefix='release-filter-0848-') as work:
 root=Path(work)/'Tab5OS_Ecriture_v0.8.48';(root/'tools').mkdir(parents=True)
 for name,content in {**kept,**omitted}.items():p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(content)
 validations=[]
 def fixture_validation(path):
  # The original full firmware validator is deliberately not exercised here.
  # The fixture contains no valid firmware; this only tests archive selection.
  validations.append(str(path))
  if path!=root:assert {p.relative_to(path).as_posix() for p in path.rglob('*') if p.is_file()}==set(kept)
 old_file,old_validate,old_argv=package.__file__,package.validate,sys.argv
 try:
  package.__file__=str(root/'tools/package_release.py');package.validate=fixture_validation;sys.argv=['package_release.py',str(Path(work)/'test.zip')]
  with contextlib.redirect_stdout(io.StringIO()):package.main()
 finally:package.__file__,package.validate,sys.argv=old_file,old_validate,old_argv
 with zipfile.ZipFile(Path(work)/'test.zip') as z:
  names={n[len(root.name)+1:] for n in z.namelist()};assert names==set(kept);assert not names.intersection(omitted);assert z.testzip() is None
 assert len(validations)==2;checks+=len(kept)+len(omitted)+3
# Authenticate every explicitly omitted historical copy against the unmodified manifest.
baseline=json.loads((release/'tools/verification/baseline-0847-source.json').read_text())
for name,row in artifacts.HISTORICAL_COPY_ARTIFACTS.items():
 assert baseline['historicalReports'][name]==row['sha256'];assert artifacts.copy_temporary('tools/verification/'+name);checks+=2
 path=release/'tools/verification'/name
 if path.exists():assert sha(path)==row['sha256'] and path.stat().st_size==row['bytes'];checks+=1
for name in kept:assert not artifacts.copy_temporary(name);checks+=1
assert not artifacts.copy_temporary(SKETCH+'/sdcard/System/maps/gazetteer/.cities.t5geo.abc')
assert not artifacts.copy_temporary('tools/verification/.build.log.not-temporary')
assert not artifacts.copy_temporary('tools/verification/.custom.json.a1B2c3');checks+=3
inputs=[release/'tools'/n for n in ('release_artifacts.py','package_release.py','audit_publication_current.py','check_preserved.py')]
report={'version':'0.8.48','passed':True,'checks':checks,'method':__doc__,'physicalDeviceTested':False,'sourceHashes':{str(p.relative_to(release)):sha(p) for p in inputs},'testSourceHashes':{str(Path(__file__).resolve().relative_to(release)):sha(Path(__file__))},'historicalCopyArtifacts':artifacts.HISTORICAL_COPY_ARTIFACTS,'historicalCopyBytes':sum(x['bytes'] for x in artifacts.HISTORICAL_COPY_ARTIFACTS.values()),'keptFixturePaths':sorted(kept),'excludedFixturePaths':sorted(omitted),'limitations':['Packaging writer, CRC and fresh extraction exercised with the firmware validator replaced by a fixture assertion. This is not final firmware/release validation.','No files removed from the source tree or user device. Final release validation must separately check build and source hashes.']}
(release/'tools/verification/release-copy-filter-0848.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':True,'checks':checks,'historicalCopyBytes':report['historicalCopyBytes']}))
