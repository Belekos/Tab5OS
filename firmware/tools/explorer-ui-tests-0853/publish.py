"""Publish source-bound Explorer UI and viewer test evidence after both runs."""
from pathlib import Path
import argparse,hashlib,json,shutil

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build-root',type=Path,required=True)
p.add_argument('--baseline-sketch',type=Path,required=True)
a=p.parse_args();build=a.build_root.resolve();baselineSketch=a.baseline_sketch.resolve()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def load(name):
    result=json.loads((build/name/'result.json').read_text())
    assert result['passed']
    for field in ('sourceHashes','testSourceHashes'):
        for path,digest in result[field].items():assert sha(Path(path))==digest,'Changed '+path
    return result
def relative(hashes,root):return {str(Path(path).relative_to(root)):digest for path,digest in hashes.items()}
def copyEvidence(name,destination,prefix=''):
    result={}
    for path in sorted((build/name).iterdir()):
        if path.suffix not in ('.log','.png','.inc'):continue
        target=destination/(prefix+path.name);shutil.copyfile(path,target)
        result[str(target.relative_to(ROOT))]=sha(target)
    return result
current=load('explorer-ui-current');baseline=load('explorer-ui-baseline');viewer=load('explorer-viewer')
proof=ROOT/'tools/verification/explorer-ui-0853';proof.mkdir(parents=True,exist_ok=True)
evidence=copyEvidence('explorer-ui-current',proof,'current-')
evidence.update(copyEvidence('explorer-ui-baseline',proof,'baseline-'))
report={
    'version':'0.8.53','passed':True,'physicalDeviceTested':False,'sourceStableDuringTests':True,
    'scope':'Production Explorer constructor, action state, actual image command branches, real LVGL and Tab5Mouse controller.',
    'sourceHashes':relative(current['sourceHashes'],ROOT),
    'testSourceHashes':relative(current['testSourceHashes'],ROOT),
    'evidenceHashes':evidence,'result':current['result'],
    'extractedHashes':current['extractedHashes'],'lvglArchiveSha256':current['lvglArchiveSha256'],
    'baseline':{'version':'0.8.52','result':baseline['result'],
        'sourceHashes':relative(baseline['sourceHashes'],baselineSketch),
        'originalArchiveSha256':'d603cf2c465a5ffecffbde90ba90bc158bdab52d770a0cc4337184298c26a696',
        'extractedHashes':baseline['extractedHashes']},
    'sanitizers':current['sanitizers'],
    'observations':['Baseline reproduces absent Drawing quick action, shared image symbol and zero gap before the first content row.',
        'Current page has eye Viewer, brush Drawing and Open with; minimum vertical/horizontal gaps are 8 px, Drawing touch target is 58 by 46 px.',
        'PNG, JPG, JPEG, BMP, WebP and T5I select the corresponding real ViewImage and EditImage command branches with unchanged paths.',
        'Busy image actions do not launch; multiselection, directories and unsupported files do not advertise image actions; original text open callback remains wired.',
        'Light and dark captures inspected.'],
    'limits':current['limits']+['Geometry is tested at the normal 1216-pixel workspace; Tab5Shell width registration is replaced by a no-op.']}
report['testSourceHashes'][str(Path(__file__).relative_to(ROOT))]=sha(Path(__file__))
(proof.parent/'explorer-ui-0853.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
proof=ROOT/'tools/verification/viewer-0853';proof.mkdir(parents=True,exist_ok=True)
report={
    'version':'0.8.53','passed':True,'physicalDeviceTested':False,'sourceStableDuringTests':True,
    'scope':viewer['method'],'sourceHashes':relative(viewer['sourceHashes'],ROOT),
    'testSourceHashes':relative(viewer['testSourceHashes'],ROOT),
    'evidenceHashes':copyEvidence('explorer-viewer',proof),
    'result':viewer['result'],'extractedHashes':viewer['extractedHashes'],
    'sanitizers':viewer['sanitizers'],'limits':viewer['limits'],
    'memoryMeasurement':'Host allocation accounting, not SRAM/PSRAM measurements on ESP32-P4.'}
report['testSourceHashes'][str(Path(__file__).relative_to(ROOT))]=sha(Path(__file__))
(proof.parent/'viewer-0853.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
print(json.dumps({'passed':True,'ui':current['result'],'baseline':baseline['result'],'viewer':viewer['result']}))
