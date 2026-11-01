"""Bind the small main-loop integrations to a successful exact firmware build.

This is a source/build integration check, not execution of the whole main loop
on a host. Runtime behavior belongs to the referenced module suites and hardware.
"""
from pathlib import Path
import argparse, hashlib, json

ROOT = Path(__file__).resolve().parents[2]
SKETCH = 'Tab5OS_Arduino_v0.8.48_Ecriture'
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path): return json.loads(path.read_text())

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--environment', required=True, type=Path)
    args = parser.parse_args()
    qa = args.environment.resolve() / 'qa'
    firmware = read(qa / 'firmware-verification-0848.json')
    assert firmware['passed'] and firmware['buildExitCode'] == 0 and firmware['sourcesUnchanged']
    manifests = [read(qa / ('source-manifest-' + suffix + '.json'))['files']
                 for suffix in ('start', 'end', 'final')]
    assert manifests[0] == manifests[1] == manifests[2]
    source = ROOT / SKETCH / (SKETCH + '.ino')
    fingerprint = sha(source)
    assert next(row for row in manifests[0] if row['path'] == source.name)['sha256'] == fingerprint
    text = source.read_text()
    assert 'static constexpr char APP_VERSION[] = "0.8.48";' in text
    assert 'static void refreshWifiSettingsUi(bool force) {\n  if (force) Tab5Audio.requestWifiSignal();' in text
    assert text.count('tab5DisplayTakeUnderrunEvent(') == 1
    display_service = text.index('tab5DisplayTakeUnderrunEvent(')
    assert text.rfind('void loop()', 0, display_service) >= 0
    assert 'signals.allocatedFlags' in text
    evidence = ['audio-housekeeping-0848.json', 'display-signals-0848.json',
                'display-target-0848.json', 'display-signals-target-0848.json']
    proofs = ROOT / 'tools/verification'
    for name in evidence:
        report = read(proofs / name)
        assert report['passed'] is True
    report = {
        'version': '0.8.48', 'passed': True, 'physicalDeviceTested': False,
        'sourceHashes': {str(source.relative_to(ROOT)): fingerprint},
        'testSourceHashes': {str(Path(__file__).relative_to(ROOT)): sha(Path(__file__))},
        'evidenceHashes': {str((proofs / name).relative_to(ROOT)): sha(proofs / name) for name in evidence},
        'scope': [
            'Full target build/link of the exact main sketch; source manifests agree.',
            'Explicit Wi-Fi settings action requests an atomic RSSI sample; periodic settings refresh does not.',
            'One task-context consumer logs aggregated native DSI underruns; no reuse of the video RTC activity slot.',
            'IRQ allocation flags are exposed in the existing on-demand serial diagnostics.'
        ],
        'limitations': ['Structural integration checks plus referenced module executions; not host execution of the entire Arduino loop.',
                        'No panel, audio, RF, boot latency or physical FreeRTOS scheduling measured.']
    }
    (proofs / 'main-integration-0848.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'passed': True, 'mainSha256': fingerprint}))

if __name__ == '__main__': main()
