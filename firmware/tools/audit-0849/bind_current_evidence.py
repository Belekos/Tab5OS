"""Bind current 0.8.49 execution reports to the release gate without editing raw reports.

These envelopes do not run tests or claim a new execution. They preserve the
original reports by path/hash and check their source fingerprints before binding.
"""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[2]
SKETCH=ROOT/'Tab5OS_Arduino_v0.8.49_Ecriture'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def bind(original,target):
    path=ROOT/original;data=json.loads(path.read_text());assert data.get('passed') is True
    result=dict(data)
    if not result.get('sourceHashes'):
        hashes={}
        for row in result.get('results',[]):
            source=SKETCH/row['source']
            if source.is_file():
                assert sha(source)==row['sourceSha256'],source
                hashes[source.relative_to(ROOT).as_posix()]=row['sourceSha256']
        assert hashes,original
        result['sourceHashes']=hashes
    for group in ('sourceHashes','testSourceHashes','testHashes','integrationSourceHashes','evidenceHashes'):
        for name,digest in result.get(group,{}).items():assert sha(ROOT/name)==digest,name
    result['version']='0.8.49'
    result['originalExecutionReport']={'path':original,'sha256':sha(path)}
    result.setdefault('evidenceHashes',{})[original]=sha(path)
    result['bindingMethod']='Release metadata envelope of the unchanged current execution; no test rerun or physical-device claim.'
    result.setdefault('testSourceHashes',{})[Path(__file__).relative_to(ROOT).as_posix()]=sha(Path(__file__))
    (ROOT/'tools/verification'/target).write_text(json.dumps(result,indent=2)+'\n')

def main():
    for source,target in [
        ('tools/audit-0849/audio-contract-tests.json','audio-contract-current-0849.json'),
        ('tools/audit-0849/audio-target-tests.json','audio-target-current-0849.json'),
        ('tools/verification/cache-sync-0849.json','cache-sync-current-0849.json'),
        ('tools/verification/lvgl-memory-native-0849.json','lvgl-memory-current-0849.json'),
        ('tools/memory-admission-tests-0849/results.json','memory-admission-current-0849.json')]:
        bind(source,target)
    print('Bound five current execution reports; raw reports remain unchanged.')
if __name__=='__main__':main()
