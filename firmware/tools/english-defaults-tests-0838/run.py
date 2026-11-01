#!/usr/bin/env python3
"""Host verification of English storage defaults and read-only legacy support."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SKETCH = ROOT / 'Tab5OS_Arduino_v0.8.38_Ecriture'
HERE = Path(__file__).resolve().parent
OUT = ROOT / 'tools/verification/english-defaults-0838'
RENAMES = {
    'MicroPython/bibliotheques.py': 'MicroPython/libraries.py',
    'MicroPython/exemple.py': 'MicroPython/example.py',
    'MicroPython/importer_cartes.py': 'MicroPython/import_maps.py',
    'MicroPython/importer_radios.py': 'MicroPython/import_radios.py',
    'MicroPython/mettre_a_jour_sd.py': 'MicroPython/update_sd.py',
    'MicroPython/creer_theme.py': 'MicroPython/create_theme.py',
    'Games/GB/LISEZ-MOI.txt': 'Games/GB/README.txt',
    'Images/LISEZ-MOI.txt': 'Images/README.txt',
    'System/maps/offline/LISEZ-MOI.txt': 'System/maps/offline/README.txt',
    'System/CLAVIERS.txt': 'System/KEYBOARDS.txt',
    'Packs/Radios/LISEZ_MOI.txt': 'Packs/Radios/README.txt',
}
CPP = ('Tab5Writing.cpp', 'Tab5Tools.cpp', 'Tab5AppCatalogue.cpp', 'Tab5PythonServices.cpp')
DEFAULTS = {
    'explorerNewFolder': 'New folder', 'explorerCopySuffix': ' - copy',
    'writerSaveAs': 'Novel.md', 'workspaceFallbackTitle': 'Project',
    'diagnosticsBasename': 'report-', 'pythonLibraries': '/MicroPython/Libraries',
    'pythonProjects': '/MicroPython/Projects', 'pythonOtherBoard': 'other',
    'pythonCategories': ['general', 'sensors', 'display', 'network', 'storage', 'motors', 'audio'],
}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', type=Path, help='Optional 0.8.37 release root for byte-preserving rename checks')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    log = []
    def note(value):
        log.append(value)
        print(value, flush=True)
    def run(argv, env=None):
        result = subprocess.run([str(a) for a in argv], cwd=ROOT, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
        note(result.stdout.rstrip())
        if result.returncode:
            raise RuntimeError('Command failed: ' + str(argv[0]))
    report = {'version': '0.8.38', 'passed': False, 'scope': 'English default storage names; localized UI and legacy user paths preserved',
              'physicalDeviceTested': False, 'defaultNames': DEFAULTS, 'shippedRenames': RENAMES,
              'limitations': ['Host filesystem and network adapters are mocked; real production Python transaction code is executed.',
                              'C++ app scan uses production source with host FS/LVGL adapters.',
                              'AddressSanitizer and UBSan enabled; leak sanitizer disabled because container process tracing is unavailable.',
                              'Main sketch integration checks are static; firmware compilation is reported separately.',
                              'Generated Factory and embedded resources are verified by the release resource gate, not hashed here.']}
    try:
        run([sys.executable, HERE / 'test_catalogue.py', SKETCH / 'sdcard/MicroPython/lib'])
        with tempfile.TemporaryDirectory(prefix='tab5-english-0838-') as tmp:
            exe = Path(tmp) / 'appscan'
            run(['g++', '-std=c++17', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
                 '-I' + str(HERE / 'mocks'), '-I' + str(SKETCH), HERE / 'test_app_scan.cpp', '-o', exe])
            env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
            run([exe], env)
            python_scan = Path(tmp) / 'python-scan'
            run(['g++', '-std=c++17', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
                 '-DTAB5_PYTHON_HOST_TEST', '-Wno-deprecated-declarations', '-I' + str(SKETCH),
                 HERE / 'test_python_scan.cpp', SKETCH / 'Tab5PythonServices.cpp', '-lcrypto', '-lz',
                 '-Wl,--wrap=opendir', '-Wl,--wrap=stat', '-o', python_scan])
            run([python_scan, Path(tmp) / 'sd'], env)
        py = sorted(p for p in (SKETCH / 'sdcard/MicroPython').rglob('*.py') if p.name != 'tab5factory.py')
        for p in py:
            compile(p.read_text(encoding='utf-8'), str(p.relative_to(ROOT)), 'exec')
        note('PASS Python syntax: ' + str(len(py)) + ' shipped scripts/modules (generated factory excluded)')
        for old, new in RENAMES.items():
            assert not (SKETCH / 'sdcard' / old).exists(), old
            assert (SKETCH / 'sdcard' / new).is_file(), new
            if args.baseline:
                prior = args.baseline / 'Tab5OS_Arduino_v0.8.37_Ecriture/sdcard' / old
                assert sha(prior) == sha(SKETCH / 'sdcard' / new), new
        note('PASS 11 shipped English filenames' + ('; all renamed payloads byte-identical to 0.8.37' if args.baseline else ''))
        writer = (SKETCH / 'Tab5Writing.cpp').read_text()
        tools = (SKETCH / 'Tab5Tools.cpp').read_text()
        ino = (SKETCH / 'Tab5OS_Arduino_v0.8.38_Ecriture.ino').read_text()
        assert '"Novel.md"' in writer and '"Roman.md"' not in writer
        assert '"Project"' in writer
        assert '"report-"' in tools and '"rapport-"' not in tools
        assert 'String suffix = " - copy";' in ino and 'String suffix = T(' not in ino
        assert '"Nouveau dossier", "New folder"' in ino
        assert 'tab5FS.exists("/MicroPython/libraries.py")?"/MicroPython/libraries.py":"/MicroPython/bibliotheques.py"' in ino
        services = (SKETCH / 'Tab5PythonServices.cpp').read_text()
        for old, new in RENAMES.items():
            if old.startswith('MicroPython/'):
                assert '"' + Path(old).name + '"' in services
                assert '"' + Path(new).name + '"' in services
        note('PASS current main sketch and app default-name integration; legacy script aliases retained')
        source = [SKETCH / f for f in CPP]
        source += [SKETCH / f for f in ('Tab5AppCatalogue.h', 'Tab5AppCataloguePolicy.h', 'Tab5Ico.h', 'Tab5PythonServices.h', 'Tab5Storage.h', 'Tab5VolumeFS.h', 'Tab5UsbStorage.h', 'Tab5SystemPath.h')]
        source += py + [SKETCH / 'sdcard' / name for name in RENAMES.values()]
        source += [p for p in HERE.rglob('*') if p.is_file() and '__pycache__' not in p.parts]
        source += [ROOT / 'ENGLISH_DEFAULTS_0.8.38.md', SKETCH / 'sdcard/Packs/README.txt']
        report['sourceHashes'] = {str(p.relative_to(ROOT)): sha(p) for p in sorted(set(source))}
        report['mainIntegrationFragmentsChecked'] = ['English folder/copy defaults', 'English libraries hook with legacy fallback']
        main_source = SKETCH / 'Tab5OS_Arduino_v0.8.38_Ecriture.ino'
        report['integrationSourceHashes'] = {str(main_source.relative_to(ROOT)): sha(main_source)}
        report['baselineRenameContentVerified'] = bool(args.baseline)
        report['passed'] = True
    except Exception as error:
        note('FAIL ' + str(error))
        raise
    finally:
        logpath = OUT / 'run.log'
        logpath.write_text('\n'.join(log) + '\n', encoding='utf-8')
        report['evidenceHashes'] = {str(logpath.relative_to(ROOT)): sha(logpath)}
        (OUT.parent / 'english-defaults-0838.json').write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')

if __name__ == '__main__':
    main()
