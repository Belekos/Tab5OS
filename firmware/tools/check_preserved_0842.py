"""Compare .42 production sources with the authenticated delivered .41 archive.

Only explicitly reviewed changes with current test/source evidence are allowed.
Old reports are preserved as history, never relabelled as new executions.
"""
from pathlib import Path, PurePosixPath
import datetime,hashlib,json
VERSION='0.8.42'
SKETCH='Tab5OS_Arduino_v0.8.42_Ecriture'
BASELINE_VERSION='0.8.41'
ARCHIVE_SHA256='01389322d10ae80b668ce369b2586e41831af03e3e407ecd3d60da62dd2667ff'
BASELINE_MANIFEST_SHA256='83ad337d451167b0a75bb386a6bdd3c335cde302dea0ecbd2ab4d33c6e85d4a2'
CODE_SUFFIXES={'.c','.h','.cpp','.cc','.hpp','.inc','.inl','.s','.asm','.ino'}
GENERATED={'Tab5EnglishFallback.h','Tab5EnglishBitmap.h','Tab5SystemResourcesData.h'}

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):return json.loads(path.read_text(encoding='utf-8'))
def preserved_path(relative):
 p=PurePosixPath(relative)
 return relative=='sdcard/System/font.ttf' or (p.parts[0]!='sdcard' and p.suffix.lower() in CODE_SUFFIXES and not relative.startswith('tools/native-apps/image-editor/test-output/') and p.name not in GENERATED)
def safe(root,relative):
 p=(root/relative).resolve()
 if not p.is_relative_to(root.resolve()):raise ValueError('Path outside release: '+relative)
 return p

def validate(root):
 root=Path(root).resolve();sketch=root/SKETCH
 manifest=root/'tools/verification/baseline-0841-source.json';scope_path=root/'tools/release_scope_0842.json'
 if digest(manifest)!=BASELINE_MANIFEST_SHA256:raise ValueError('Authenticated baseline source manifest changed')
 baseline=read(manifest);scope=read(scope_path)
 if baseline['sourceArchiveSha256']!=ARCHIVE_SHA256 or baseline['baselineVersion']!=BASELINE_VERSION:raise ValueError('Unrecognized baseline')
 if scope.get('version')!=VERSION or scope.get('finalized') is not True:raise ValueError('Release source-change scope is not finalized')
 expected={p.replace('Tab5OS_Arduino_v0.8.41_Ecriture.ino',SKETCH+'.ino'):sha for p,sha in baseline['files'].items()}
 actual={p.relative_to(sketch).as_posix():digest(p) for p in sketch.rglob('*') if p.is_file() and preserved_path(p.relative_to(sketch).as_posix())}
 changed=sorted(p for p in set(expected)|set(actual) if expected.get(p)!=actual.get(p))
 if set(changed)!=set(scope['sourceChanges']):raise ValueError('Undeclared or obsolete changed sources: '+repr(sorted(set(changed)^set(scope['sourceChanges']))))
 current={row['file']:row for row in scope['targetedReports']}
 for path in changed:
  item=scope['sourceChanges'][path]
  if not item.get('reason') or not item.get('report') or item['report'] not in current:raise ValueError('Changed source lacks explicit rationale/current proof: '+path)
  report=read(root/'tools/verification'/item['report'])
  if report.get('version',report.get('release'))!=VERSION or not (report.get('passed') is True or report.get('result')=='PASS'):raise ValueError('Changed source proof not current/passed: '+path)
  hashes={**report.get('sourceHashes',{}),**report.get('integrationSourceHashes',{})}
  if path.startswith('tools/'):
   hashes.update(report.get('testSourceHashes',{}))
  if path in actual and hashes.get(SKETCH+'/'+path)!=actual[path]:raise ValueError('Changed source not fingerprinted by current proof: '+path)
  if path not in actual and path not in report.get('deletedSources',[]):raise ValueError('Deleted source not justified in current proof: '+path)
 for name,sha in baseline['historicalReports'].items():
  if digest(root/'tools/verification'/name)!=sha:raise ValueError('Historical report changed: '+name)
 return {'version':VERSION,'baselineVersion':BASELINE_VERSION,'kind':'byte-preservation','passed':True,'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'filesChecked':len(set(expected)&set(actual))-len(set(changed)&set(expected)),'baselineFiles':len(expected),'changedSources':changed,'sourceChangeScopeSha256':digest(scope_path),'baselineManifest':manifest.relative_to(root).as_posix(),'baselineManifestSha256':digest(manifest),'checkerSha256':digest(Path(__file__)),'historicalReportHashes':baseline['historicalReports'],'limitations':['No physical tablet test','Historical reports are not new executions','Unchanged sources do not establish absence of runtime interaction or timing on hardware']}

if __name__=='__main__':
 root=Path(__file__).resolve().parents[1];report=validate(root)
 (root/'tools/verification/preserved-components-0842.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report))
