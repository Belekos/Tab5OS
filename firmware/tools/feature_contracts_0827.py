"""Current .27 checks layered over the exact preserved .26 contracts."""
from feature_contracts_0826 import *
import feature_contracts_0826 as previous
import inspect

NEW_REPORTS = ('serial-text-0827.json', 'serial-syntax-0827.json', 'writing-serial-0827.json',
               'vault-reopen-0827.json')
HARNESS_PATHS = {
    'wifi-transitions-0822.json': {'tools/wifi-transition-tests': 'tools/wifi-transition-tests-0827'},
    'directory-picker-0822.json': {'run.py': 'run_0827.py'},
    'reader-layout-0822.json': {'run_layout.py': 'run_layout_0827.py'},
    'reader-seek-0822.json': {'run_seek.py': 'run_seek_0827.py'},
    'launch-radio-0822.json': {'run.py': 'run_0827.py'},
    'ui-controls-0822.json': {'run.py': 'run_0827.py'},
    'explorer-0823.json': {'run_explorer.py': 'run_explorer_0827.py'},
    'numeric-keyboard-0824.json': {'run_numeric.py': 'run_numeric_0827.py'},
    'task-diagnostics-0822.json': {'tools/pentest-tests': ''},
    'memory-target-stack-0822.json': {'tools/pentest-tests': 'tools/memory-stack-tests-0827'},
}


def checked_diagnostics(raw, release, sketch):
    hashes(sketch, raw.get('sources'), {'Tab5Diagnostics.cpp', 'Tab5Diagnostics.h',
           'Tab5Memory.cpp', 'Tab5Memory.h', 'Tab5Task.h', 'Tab5ResourceManager.cpp'})
    hashes(release, raw.get('tests'), {'tools/task-diagnostics-tests-0827/run.py',
           'tools/task-diagnostics-tests-0827/rtc.cpp', 'tools/continuity-tests-0826/tasks.cpp'})
    require(raw.get('rtcRecordBytes') == 968 and raw.get('rtcFormat') == 4 and
            raw.get('rtcSlots') == 7, 'Current RTC diagnostic format or slot inventory missing')
    rows = {row['name']: row for row in raw.get('results', [])}
    require(set(rows) == {'tasks', 'rtc'} and all(row.get('exitCode') == 0 for row in rows.values()),
            'RTC/task diagnostic executions missing')
    require('PASS 4118 task diagnostic checks; snapshots=2000; retained_transient_allocations=0' in rows['tasks']['output']
            and 'PASS 1999 RTC diagnostic checks;' in rows['rtc']['output']
            and 'corruptions=968; SD_writes=0; hardware_tested=false' in rows['rtc']['output'],
            'RTC corruption/no-SD-write and task lifecycle checks incomplete')
    scoped(raw, 'method', ['Actual production', 'Warm/cold reset', 'not a hardware reset reproduction'])


def checked_credential_extension(raw, release, sketch):
    base = {'note', 'label', 'button', 'erasePassword', 'field', 'dropdown',
            'refreshKeys', 'selectedKey', 'visible', 'enabled', 'close', 'open'}
    extra = {'hideKeyboard', 'inputKeyboard', 'captureLayout', 'layoutTools', 'resize'}
    functions((sketch / 'Tab5Tools.cpp').read_text(encoding='utf8'),
              raw.get('toolsFunctionHashes'), base | extra, True)
    keyboard = function((sketch / (sketch.name + '.ino')).read_text(encoding='utf8'),
                        'setKeyboardVisible', True)
    require(raw.get('osKeyboardSha256') == hashlib.sha256(keyboard.encode('utf8')).hexdigest(),
            'OS keyboard observer controller changed after credential lifecycle execution')
    require(raw.get('addressSanitizer') is True, 'Credential lifecycle was not instrumented with ASan')
    output = minimum_output(raw, 13335)
    require('96production unlock/' in output and 'DELETE callback' in output and
            'asynchronous Retry' in output, 'Credential load/close/keyboard retry coverage incomplete')
    copy = dict(raw)
    copy['toolsFunctionHashes'] = {name: raw['toolsFunctionHashes'][name] for name in base}
    return copy
PREVIOUS_CONTRACT_PATHS = {
    'settings-layout-0825.json': {'tools/settings-tests/run_layout.py': 'tools/settings-tests/run_layout_0827.py'},
    'credential-ui-0825.json': {'tools/settings-tests': 'tools/credential-tests-0827'},
    'vault-0825.json': {'tools/vault-tests': 'tools/vault-tests-0827'},
    'vault-target-0825.json': {'vault-tests/run_target.py': 'vault-tests-0827/run_target.py'},
    'runtime-env-0825.json': {'tools/runtime-env-tests': 'tools/runtime-env-tests-0827'},
    'serial-backend-0826.json': {'tools/serial-tests-0826': 'tools/serial-tests-0827'},
}


def checked_previous(name, raw, release, sketch, contracts=None):
    if raw.get('version') == '0.8.26':
        return previous.checked_new(name, raw, release, sketch, contracts)
    source = inspect.getsource(previous.checked_new).replace("'0.8.26'", "'0.8.27'")
    for before, after in PREVIOUS_CONTRACT_PATHS.get(name, {}).items():
        source = source.replace(before, after)
    namespace = dict(previous.__dict__)
    exec(compile(source, '<preserved .26 assertions; explicit current harness paths>', 'exec'), namespace)
    namespace['checked_new'](name, raw, release, sketch, contracts)
    if name == 'serial-backend-0826.json':
        match = re.search(r'PASS serial backend (\d+) checks', raw.get('output', ''))
        require(match and int(match[1]) >= 588575, 'Serial reader ownership/whole-document checks incomplete')


def checked_help(raw, release, sketch):
    source = inspect.getsource(previous.checked_help).replace('0.8.26', '0.8.27').replace('0826', '0827')
    namespace = dict(previous.__dict__)
    exec(compile(source, '<current translated help identities>', 'exec'), namespace)
    return namespace['checked_help'](raw, release, sketch)


def checked_new(name, raw, release, sketch, contracts=None):
    require(name in NEW_REPORTS and raw.get('version') == '0.8.27' and raw.get('passed') is True
            and raw.get('physicalDeviceTested') is False, 'Current regression evidence failed or scope missing: ' + name)
    prefix = sketch.name + '/'
    if name == 'serial-text-0827.json':
        hashes(release, raw.get('sourceHashes'), {prefix + 'Tab5SerialText.h', prefix + 'WriterDocument.cpp',
               'tools/serial-tests-0827/text.cpp', 'tools/serial-tests-0827/run_text.py'})
        match = re.search(r'PASS serial text (\d+) checks', raw.get('output', ''))
        require(match and int(match[1]) >= 160154, 'Serial UTF-8/bounded capture checks incomplete')
        scoped(raw, 'method', ['Production UTF-8 decoder', 'independent WriterDocument UTF-8 validator', 'ASan'])
    elif name == 'serial-syntax-0827.json':
        hashes(release, raw.get('sourceHashes'), {'tools/serial-tests-0827/run_target.py'})
        rows = raw.get('files', [])
        require({row.get('name') for row in rows} == {'Tab5Serial.cpp', 'Tab5SerialCommand.cpp',
                'Tab5Writing.cpp', 'Tab5Terminal.cpp', sketch.name + '.ino'}, 'Serial/editor target units missing')
        for row in rows:
            require(row.get('exitCode') == 0 and row.get('sourceStable') is True and row.get('stackUsage')
                    and row.get('sha256') == digest(sketch / row['name']), 'Serial target compilation failed or source changed')
        hashes(sketch, raw.get('headers'), {'Tab5Serial.h', 'Tab5SerialCore.h', 'Tab5SerialSettings.inc',
               'Tab5SerialText.h', 'Tab5WritingSerial.inc'})
        scoped(raw, 'method', ['Real ESP32-P4', 'stack usage', 'installed SDK'])
    elif name == 'vault-reopen-0827.json':
        require(raw.get('addressSanitizer') is True, 'Vault comparison was not instrumented with ASan')
        hashes(release / 'tools/vault-tests-0827', raw.get('testSources'),
               {'Platform.h', 'reopen_regression.cpp', 'run_reopen.py'})
        rows = {row['version']: row for row in raw.get('rows', [])}
        require(set(rows) == {'0.8.26', '0.8.27'}, 'Vault delivered-baseline/current comparison missing')
        baseline = json.loads((release / 'tools/verification/history-0826/evidence/source-manifest-final.json').read_text(encoding='utf8'))
        old = {row['path']: row['sha256'] for row in baseline['files']}
        required = VAULT_SOURCES | {'Tab5Environment.cpp', 'Tab5Environment.h'}
        require(set(rows['0.8.26'].get('sources', {})) == required
                and all(old.get(name) == value for name, value in rows['0.8.26']['sources'].items()),
                'Vault counterexample sources differ from delivered .26')
        hashes(sketch, rows['0.8.27'].get('sources'), required)
        require(rows['0.8.26'].get('exitCode') == 1 and rows['0.8.27'].get('exitCode') == 0,
                'Vault failed-baseline/passing-current distinction missing')
        for version, early, operations in [('0.8.26', 1408, 33792), ('0.8.27', 0, 0)]:
            observed = rows[version].get('observations', {})
            require(observed.get('checks', 0) >= 675 and observed.get('failedThenRepairedSessions') == 32
                    and observed.get('earlySuccessDuringPreload') == early
                    and observed.get('uiFileOperationsAfterFailedPreload') == operations,
                    'Vault preload/retry observations missing: ' + version)
        scoped(raw, 'method', ['Actual complete KeyStore', 'actual mbedTLS3.6.5', 'NVS/FS/RTOS mocked',
               'not the physical reset itself'])
    elif name == 'writing-serial-0827.json':
        require(raw.get('sourceStableDuringTests') is True, 'Writing sources changed during UI execution')
        hashes(sketch, raw.get('sources'), {'Tab5Writing.cpp', 'Tab5Writing.h', 'Tab5WritingSerial.inc',
               'Tab5Serial.h', 'Tab5SerialCore.h', 'Tab5SerialText.h', 'WriterDocument.h',
               'WriterRich.h', 'WriterView.h', 'WriterIndex.h', 'WriterDocument.cpp', 'WriterRich.cpp',
               'WriterView.cpp', 'WriterSyntax.cpp', 'WriterIndex.cpp', 'Tab5Focus.cpp',
               'Tab5TextInput.cpp', 'Tab5ContextMenu.cpp', 'Tab5UiStyle.cpp'})
        hashes(release / 'tools/writing-serial-tests-0827', raw.get('tests'), {'run.py', 'test.cpp'})
        require(raw.get('lvglArchiveSha256') == '1a84414b953acdda6d2b5c49d94578d80099aca6e36d40b129c2f38c074b05a9',
                'Unexpected Writing LVGL archive')
        rows = {row['mode']: row for row in raw.get('results', [])}
        require(set(rows) == {'baseline', 'current'} and all(row.get('exitCode') == 0 for row in rows.values()),
                'Writing old-toolbar/current-controller executions missing')
        baseline = json.loads((release / 'tools/verification/history-0826/evidence/source-manifest-final.json').read_text(encoding='utf8'))
        old = {row['path']: row['sha256'] for row in baseline['files']}
        require(rows['baseline'].get('baselineSourceSha256') == old['Tab5Writing.cpp']
                and 'BASELINE .26 active editor serial entry absent: 2 checks, expected regression reproduced' in rows['baseline'].get('output', ''),
                'Missing counterexample from the delivered .26 active editor')
        match = re.search(r'CURRENT writing serial: (\d+) checks', rows['current'].get('output', ''))
        require(match and int(match[1]) >= 613, 'Writing selection/document/session lifecycle execution incomplete')
        current = rows['current'].get('controllerHashes', {})
        required = {'enum Action {', 'const char* tr(', 'uint32_t color(', 'writing::ViewTheme viewTheme(',
                    'static void setLabel(', 'void caption(', 'const lv_font_t* font(', 'void panel(',
                    'lv_obj_t* button(', 'lv_obj_t* label(', 'void build()', 'void layout()',
                    'bool changedRich()', 'bool replaceSelection(', 'void Tab5WritingApp::show(',
                    'void Tab5WritingApp::resize(', 'serial', 'serialDispatch'}
        require(set(current) == required, 'Writing production controller inventory incomplete')
        source = (sketch / 'Tab5Writing.cpp').read_text(encoding='utf8')
        for signature, expected in current.items():
            if signature == 'serial':
                value = (sketch / 'Tab5WritingSerial.inc').read_text(encoding='utf8')
            elif signature == 'serialDispatch':
                start = source.index('case SerialToggle:')
                value = source[start:source.index('\n', source.index('serialAction(action); break;', start))]
            else:
                start = source.index(signature)
                value = source[start:closing(source, source.index('{', start))]
                if signature == 'enum Action {':
                    value += ';'
            require(hashlib.sha256(value.encode('utf8')).hexdigest() == expected,
                    'Writing executed controller changed: ' + signature)
        scoped(raw, 'method', ['production active Tab5Writing', 'boundary mocks', 'MSVC AddressSanitizer',
               'LVGL archive is prebuilt and uninstrumented'])
