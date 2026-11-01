"""Exact six-unit P4 serial/editor/recovery compilation proof for release .28."""
from feature_contracts_0827 import require, hashes, digest, scoped

def checked_serial_target(raw,release,sketch):
    require(raw.get('version')=='0.8.28' and raw.get('passed') is True and
            raw.get('physicalDeviceTested') is False and raw.get('sourceStableDuringTests') is True,
            'Serial target execution failed, changed sources or scope missing')
    hashes(release,raw.get('sourceHashes'),{'tools/serial-tests-0828/run_target.py'})
    rows=raw.get('files',[])
    required={'Tab5Serial.cpp','Tab5SerialCommand.cpp','Tab5Writing.cpp','WriterStorage.cpp',
              'Tab5Terminal.cpp',sketch.name+'.ino'}
    require(len(rows)==len(required) and {row.get('name') for row in rows}==required,
            'Serial/editor/recovery target units missing or duplicated')
    for row in rows:
        require(row.get('exitCode')==0 and row.get('sourceStable') is True and
                row.get('stackUsage') and row.get('sha256')==digest(sketch/row['name']),
                'Serial/editor/recovery target compilation failed or source changed')
    hashes(sketch,raw.get('headers'),{'Tab5Serial.h','Tab5SerialCore.h','Tab5SerialSettings.inc',
           'Tab5SerialText.h','Tab5WritingSerial.inc','Tab5Writing.h','WriterStorage.h'})
    scoped(raw,'method',['Real ESP32-P4','stack usage','installed SDK','recovery SaveJob'])
