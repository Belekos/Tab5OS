"""Bind the reconstructed .50 release to executed tests and its final P4 build.

The authenticated .49 remains the baseline. No historical execution is relabelled.
Use --package only after the report and installation documents are final.
"""
from pathlib import Path
import argparse, difflib, hashlib, json, shutil, subprocess, sys, zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from release_config import VERSION, SKETCH, TAG, BASELINE_VERSION, BASELINE_TAG, BASELINE_ARCHIVE_SHA256
from check_preserved import preserved_path

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')

def reason(path):
    if path.startswith('tools/native-apps/image-editor/') or path == 'Tab5ImageCore.c':
        return 'Sélection flottante, presse-papiers et relief avec allocations bornées, erreurs atomiques et historique conservé.'
    if path.startswith('Tab5ImageCodec') or path == 'Tab5WebPPort.cpp':
        return 'Transfert explicite du presse-papiers lors du remplacement du document importé ; propriété des buffers conservée.'
    if path.startswith('Tab5Image'):
        return 'Commandes de sélection tactiles/clavier, texte dans la police OS et portée explicite des corrections.'
    if path.startswith('Tab5Alarm') or path in ('Tab5Icons.h', 'Tab5HomeUi.h'):
        return 'Icône réveil dédiée et quatre tonalités, sans modification du pilote audio ni du format de préférences.'
    if path.startswith('Tab5UiStyle'):
        return 'Couleurs des états sélectionnés cohérentes avec la palette OS.'
    if path == 'Tab5Help.cpp':
        return 'Documentation du chemin image canonique /Picture.'
    if path.endswith('.ino'):
        return 'Version 0.8.50, clavier Cartes explicite, chemin /Picture, icône Réveil et états sélectionnés partagés.'
    raise ValueError('Unreviewed production delta: ' + path)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--environment', type=Path, required=True)
    ap.add_argument('--baseline', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--package', action='store_true')
    args = ap.parse_args()
    env = args.environment.resolve()
    proofs = ROOT / 'tools/verification'
    qa = env / 'qa'
    sketch = ROOT / SKETCH
    assert sha(args.baseline) == BASELINE_ARCHIVE_SHA256
    build = json.loads((qa / f'firmware-verification-{TAG}.json').read_text())
    assert build['passed'] and build['sourcesUnchanged'] and build['buildExitCode'] == 0
    evidence = proofs / f'build-{TAG}'
    evidence.mkdir(exist_ok=True)
    for name, digest in build['evidenceSha256'].items():
        source = qa / name
        assert sha(source) == digest, name
        target = evidence / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    shutil.copy2(qa / f'firmware-verification-{TAG}.json', proofs / f'firmware-verification-{TAG}.json')
    firmware = ROOT / 'firmware'
    if firmware.exists():
        shutil.rmtree(firmware)
    (firmware / 'debug').mkdir(parents=True)
    for row in build['artifacts']:
        if row['suffix'] not in ('.bin', '.elf', '.map'):
            continue
        source = env / 'build' / row['file']
        assert sha(source) == row['sha256']
        target = firmware / f'Tab5OS_v{VERSION}.bin' if row['suffix'] == '.bin' else firmware / 'debug' / row['file']
        shutil.copy2(source, target)
    binary = firmware / f'Tab5OS_v{VERSION}.bin'
    (firmware / 'SHA256.txt').write_text(sha(binary) + '  ' + binary.name + '\n')

    # Every selected report must describe a current execution or an explicitly
    # bounded source check; all source and test hashes are rechecked here.
    required = {'firmware-verification', 'stability-preserved', 'map-input-paths',
                'wiki-picture-paths', 'alarm-theme', 'alarm-theme-target',
                'image-selection', 'relief', 'relief-target',
                'evolutions-localization', 'resources'}
    reports = {}
    for path in sorted(proofs.glob(f'*-{TAG}.json')):
        data = json.loads(path.read_text())
        if data.get('version', data.get('release')) != VERSION or not data.get('sourceHashes'):
            continue
        if not (data.get('passed') is True or data.get('result') == 'PASS'):
            raise ValueError('Current report did not pass: ' + path.name)
        for group in ('sourceHashes', 'testSourceHashes', 'testHashes', 'integrationSourceHashes', 'evidenceHashes', 'screenshots'):
            for relative, digest in data.get(group, {}).items():
                assert sha(ROOT / relative) == digest, (path.name, relative)
        reports[path.name] = data
    assert required <= {n.removesuffix(f'-{TAG}.json') for n in reports}, sorted(required - {n.removesuffix(f'-{TAG}.json') for n in reports})
    assert any('image' in name and 'native' in name for name in reports), 'Missing real image UI execution'
    baseline = json.loads((proofs / f'baseline-{BASELINE_TAG}-source.json').read_text())
    old_ino = f'Tab5OS_Arduino_v{BASELINE_VERSION}_Ecriture.ino'
    expected = {p.replace(old_ino, SKETCH + '.ino'): h for p, h in baseline['files'].items() if preserved_path(p)}
    actual = {p.relative_to(sketch).as_posix(): sha(p) for p in sketch.rglob('*') if p.is_file() and preserved_path(p.relative_to(sketch).as_posix())}
    changed = sorted(p for p in set(expected) | set(actual) if expected.get(p) != actual.get(p))
    changes = {}
    ordered = sorted(reports, key=lambda name: (name.startswith('firmware-verification'), name))
    for path in changed:
        candidates = [name for name in ordered if reports[name].get('sourceHashes', {}).get(SKETCH + '/' + path) == actual.get(path)]
        assert candidates, path
        changes[path] = {'reason': reason(path), 'report': candidates[0]}
    write(ROOT / 'tools/release_scope.json', {
        'version': VERSION, 'baselineVersion': BASELINE_VERSION, 'finalized': True,
        'targetedReports': [{'file': name, 'description': reports[name].get('scope', name)} for name in ordered],
        'sourceChanges': changes})
    chunks = []
    rows = []
    with zipfile.ZipFile(args.baseline) as archive:
        prefix = f'Tab5OS_Ecriture_v{BASELINE_VERSION}/Tab5OS_Arduino_v{BASELINE_VERSION}_Ecriture/'
        for path in changed:
            old_path = path.replace(SKETCH + '.ino', old_ino)
            try:
                before = archive.read(prefix + old_path)
            except KeyError:
                before = b''
            after = (sketch / path).read_bytes()
            rows.append({'path': path, 'beforeSha256': hashlib.sha256(before).hexdigest() if before else None,
                         'afterSha256': hashlib.sha256(after).hexdigest(), 'reason': changes[path]['reason']})
            chunks.extend(difflib.unified_diff(before.decode().splitlines(True), after.decode().splitlines(True),
                                            fromfile=BASELINE_VERSION + '/' + old_path, tofile=VERSION + '/' + path))
    write(ROOT / 'tools/audit-0850/code-delta.json', {'version': VERSION, 'baselineArchiveSha256': BASELINE_ARCHIVE_SHA256, 'files': rows})
    (ROOT / 'tools/audit-0850/code-delta.patch').write_text(''.join(chunks))
    subprocess.run([sys.executable, str(ROOT / 'tools/check_preserved.py')], check=True, stdout=(env / 'preserved-0850.log').open('w'))
    if args.package:
        subprocess.run([sys.executable, str(ROOT / 'tools/audit_publication_current.py')], check=True)
        subprocess.run([sys.executable, str(ROOT / 'tools/package_release.py'), str(args.output.resolve())], check=True)
        shutil.copy2(binary, args.output.parent / f'Tab5OS_{VERSION}.bin')
        shutil.copy2(ROOT / f'RAPPORT_FONCTIONNEL_{VERSION}.md', args.output.parent / f'RAPPORT_FONCTIONNEL_{VERSION}.md')
    print(json.dumps({'prepared': True, 'packaged': args.package, 'changedProductionSources': len(changes), 'currentReports': len(reports)}))

if __name__ == '__main__':
    main()
