"""Assemble verified 0.8.49 artifacts and bind changes to current evidence.

Does not compile, flash, alter a device, or relabel historical executions.
"""
from pathlib import Path
import argparse,difflib,hashlib,json,shutil,subprocess,sys,zipfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from release_config import VERSION,SKETCH,TAG,BASELINE_VERSION,BASELINE_ARCHIVE_SHA256
from check_preserved import preserved_path

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(d,indent=2)+'\n')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--environment',required=True,type=Path);ap.add_argument('--baseline',required=True,type=Path);ap.add_argument('--output',required=True,type=Path);a=ap.parse_args()
    env=a.environment.resolve();qa=env/'qa';s=ROOT/SKETCH;proofs=ROOT/'tools/verification'
    assert sha(a.baseline)==BASELINE_ARCHIVE_SHA256
    report=json.loads((qa/f'firmware-verification-{TAG}.json').read_text());assert report['passed'] and report['sourcesUnchanged']
    evidence=proofs/f'build-{TAG}';evidence.mkdir(exist_ok=True)
    for name,digest in report['evidenceSha256'].items():
        source=qa/name;assert sha(source)==digest,name
        target=evidence/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    shutil.copy2(qa/f'firmware-verification-{TAG}.json',proofs/f'firmware-verification-{TAG}.json')
    shutil.copy2(qa/'compiler-warning-pass.json',proofs/f'compiler-warning-current-{TAG}.json')
    firmware=ROOT/'firmware'
    if firmware.exists():shutil.rmtree(firmware)
    (firmware/'debug').mkdir(parents=True)
    for row in report['artifacts']:
        if row['suffix'] not in ('.bin','.elf','.map'):continue
        original=env/'build'/row['file'];assert sha(original)==row['sha256']
        target=firmware/f'Tab5OS_v{VERSION}.bin' if row['suffix']=='.bin' else firmware/'debug'/row['file']
        shutil.copy2(original,target)
    binary=firmware/f'Tab5OS_v{VERSION}.bin';(firmware/'SHA256.txt').write_text(sha(binary)+'  '+binary.name+'\n')
    names=['audio-contract-current','audio-target-current','cache-sync-current','cache-sync-target',
           'lvgl-memory-current','lvgl-memory-target','memory-admission-current','memory-layout',
           'media-hide','media-hide-native','network-gate-regression','radio-lifecycle','storage',
           'video-lifecycle','evolutions-localization','resources','compiler-warning-current','firmware-verification']
    rows=[];reports={}
    for name in names:
        filename=f'{name}-{TAG}.json';d=json.loads((proofs/filename).read_text())
        assert d.get('version',d.get('release'))==VERSION and (d.get('passed') is True or d.get('result')=='PASS'),filename
        assert d.get('sourceHashes'),filename
        for group in ('sourceHashes','testSourceHashes','testHashes','integrationSourceHashes','evidenceHashes','screenshots'):
            for path,h in d.get(group,{}).items():assert sha(ROOT/path)==h,(filename,path)
        reports[filename]=d;rows.append({'file':filename,'description':d.get('scope',d.get('kind',name))})
    baseline=json.loads((proofs/'baseline-0848-source.json').read_text())
    expected={p.replace('Tab5OS_Arduino_v0.8.48_Ecriture.ino',SKETCH+'.ino'):h for p,h in baseline['files'].items() if preserved_path(p)}
    actual={p.relative_to(s).as_posix():sha(p) for p in s.rglob('*') if p.is_file() and preserved_path(p.relative_to(s).as_posix())}
    changed=sorted(p for p in set(expected)|set(actual) if expected.get(p)!=actual.get(p));changes={}
    for path in changed:
        if path.startswith('Tab5Audio'):
            preferred='audio-contract-current';reason='Acquittement de la restitution PCM, séparation transition/fermeture et contrôle du démarrage de sortie.'
        elif path.startswith('Tab5Video'):
            preferred='video-lifecycle';reason='Restitution des frames de décodage sans invalider l’image empruntée par LVGL.'
        elif path.startswith('Tab5CacheSync'):
            preferred='cache-sync-current';reason='Découpage des synchronisations de cache PSRAM recevables, avec conservation des validations SDK.'
        elif path.startswith('Tab5Lvgl') or path=='lv_conf.h':
            preferred='lvgl-memory-current';reason='Pools PSRAM extensibles utilisant le TLSF natif LVGL, hors verrou critique du tas partagé.'
        elif path.endswith('.ino'):
            preferred='media-hide-native';reason='Version 0.8.49 et restitution des caches média inutilisés lors des transitions de visibilité.'
        else:
            preferred='memory-admission-current';reason='Admission bornée sans parcours du tas, contrats de budget réels et diagnostics mémoire exacts.'
        selected=f'{preferred}-{TAG}.json'
        candidates=[selected]+[row['file'] for row in rows if row['file']!=selected]
        selected=next(name for name in candidates if reports[name].get('sourceHashes',{}).get(SKETCH+'/'+path)==actual.get(path))
        changes[path]={'reason':reason,'report':selected}
    write(ROOT/'tools/release_scope.json',{'version':VERSION,'baselineVersion':BASELINE_VERSION,'finalized':True,'targetedReports':rows,'sourceChanges':changes})
    # A human-readable production/build diff accompanies the authenticated hashes.
    code_changes=[];chunks=[]
    with zipfile.ZipFile(a.baseline) as z:
        prefix=f'Tab5OS_Ecriture_v{BASELINE_VERSION}/';oldsketch=f'Tab5OS_Arduino_v{BASELINE_VERSION}_Ecriture'
        selected=[SKETCH+'/'+name for name in changed]+['Compiler_Tab5OS.ps1','tools/build_linux/compile_linux.py','tools/build_linux/prepare_m5unified_audio.py','tools/build_linux/verify_heap_probe.py']
        for relative in selected:
            oldrelative=relative.replace(SKETCH,oldsketch).replace(SKETCH+'.ino',oldsketch+'.ino')
            name=prefix+oldrelative;before=z.read(name) if name in z.namelist() else b'';after=(ROOT/relative).read_bytes()
            code_changes.append({'path':relative,'baselinePath':oldrelative,'beforeSha256':hashlib.sha256(before).hexdigest() if before else None,'afterSha256':hashlib.sha256(after).hexdigest(),'beforeBytes':len(before),'afterBytes':len(after)})
            chunks.extend(difflib.unified_diff(before.decode().splitlines(keepends=True),after.decode().splitlines(keepends=True),fromfile='0.8.48/'+oldrelative,tofile='0.8.49/'+relative))
    write(ROOT/'tools/audit-0849/code-delta.json',{'version':VERSION,'baselineArchiveSha256':BASELINE_ARCHIVE_SHA256,'files':code_changes,'scope':'Production changes plus directly affected build preparation; generated payloads separately checked by resource/build manifests.'})
    (ROOT/'tools/audit-0849/code-delta.patch').write_text(''.join(chunks))
    commands=[['check_preserved.py'],['audit_publication_current.py'],['package_release.py',str(a.output.resolve())]]
    for args in commands:
        result=subprocess.run([sys.executable,str(ROOT/'tools'/args[0]),*args[1:]],capture_output=True,text=True)
        (env/(args[0]+'.log')).write_text(result.stdout+result.stderr)
        if result.returncode:print(result.stdout[-3000:]+result.stderr[-5000:]);raise SystemExit(result.returncode)
    shutil.copy2(binary,a.output.parent/f'Tab5OS_{VERSION}.bin')
    shutil.copy2(ROOT/f'RAPPORT_INGENIERIE_{VERSION}.md',a.output.parent/f'RAPPORT_INGENIERIE_{VERSION}.md')
    print((env/'package_release.py.log').read_text())
if __name__=='__main__':main()
