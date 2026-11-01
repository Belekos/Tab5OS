"""Additional .25 evidence contracts; predecessor contracts remain untouched.

These checks validate the recorded executions, their exact source/harness
identities and their stated scope. They do not turn host tests into hardware
tests, or isolated compiler stack frames into whole-call-chain measurements.
"""
from pathlib import Path
import hashlib
import json
import re

NEW_REPORTS = (
    'credential-ui-0825.json', 'vault-0825.json', 'vault-target-0825.json',
    'runtime-env-0825.json', 'settings-layout-0825.json', 'sdr-dsp-0825.json',
)
LANGUAGES = {'ar', 'de-DE', 'en-US', 'es-ES', 'fr-FR', 'id-ID', 'it-IT',
             'ja-JP', 'ko-KR', 'nl-NL', 'pt-BR', 'pt-PT', 'ru-RU', 'zh-CN'}
VAULT_SOURCES = {name + suffix for name in
                 ('Tab5KeyStore', 'Tab5Vault', 'Tab5VaultCore', 'Tab5Secrets')
                 for suffix in ('.cpp', '.h')}
HISTORY_0824_SHA256 = '9d257a209c2289b691ce8ebdeafe420901bbe6faafe1d676af65ad09c3189261'
DSP_0824_SHA256 = 'af532a96ae11d2cf99d6d63e2264adea121e35653dbe540dcf8c6d991a40402a'
LVGL_ARCHIVE_SHA256 = '96475d6c42bdc86a9437f2330ecceb5c2bfab8dcba9951431a55d0d4cc83485a'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def hashes(base, values, required):
    require(isinstance(values, dict), 'Missing source/harness manifest')
    normalized = {str(k).replace('\\', '/'): v for k, v in values.items()}
    require(len(normalized) == len(values), 'Duplicate normalized manifest path')
    missing = set(required) - normalized.keys()
    require(not missing, 'Incomplete .25 manifest: ' + ', '.join(sorted(missing)))
    for name, expected in normalized.items():
        path = (base / name).resolve()
        require(path.is_relative_to(base.resolve()), 'Unsafe manifest path: ' + name)
        require(isinstance(expected, str) and re.fullmatch('[0-9a-f]{64}', expected),
                'Invalid SHA-256: ' + name)
        require(path.is_file() and digest(path) == expected, 'Hash mismatch: ' + name)


def closing(text, start):
    depth = 0
    quote = None
    escape = line = comment = False
    i = start
    while i < len(text):
        c, pair = text[i], text[i:i + 2]
        if line:
            if c == '\n':
                line = False
        elif comment:
            if pair == '*/':
                comment = False
                i += 1
        elif quote:
            if escape:
                escape = False
            elif c == '\\':
                escape = True
            elif c == quote:
                quote = None
        elif pair == '//':
            line = True
            i += 1
        elif pair == '/*':
            comment = True
            i += 1
        elif c in '\"\'':
            quote = c
        elif c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    raise ValueError('Unbalanced controller source')


def function(text, name, credential=False):
    if credential:
        pattern = r'^\s*(?:static )?(?:void|bool|unsigned|size_t|lv_obj_t\*)\s+' + re.escape(name) + r'\([^;]*?\)\s*\{'
    else:
        pattern = r'static [^\n]+\b' + re.escape(name) + r'\([^;]*?\)\s*\{'
    match = re.search(pattern, text, re.M)
    require(match is not None, 'Missing production controller: ' + name)
    result = text[match.start():closing(text, text.index('{', match.start()))]
    return result.strip() if credential else result


def functions(text, manifest, required, credential=False):
    require(isinstance(manifest, dict) and set(manifest) == set(required),
            'Missing or unexpected controller identities')
    for name, expected in manifest.items():
        actual = hashlib.sha256(function(text, name, credential).encode('utf-8')).hexdigest()
        require(actual == expected, 'Controller hash mismatch: ' + name)


def minimum_output(raw, minimum):
    output = raw.get('output', '')
    match = re.search(r'PASS (\d+) (?:checks|DSP checks)', output)
    require(match and int(match[1]) >= minimum, 'Missing successful execution/minimum checks')
    require('FAIL ' not in output, 'Execution contains failure')
    return output


def scoped(raw, text, required):
    require(raw.get('physicalDeviceTested', raw.get('hardwareTested')) is False,
            'Host/target evidence must not claim a physical device test')
    scope = raw.get(text, '').lower()
    require(all(part.lower() in scope for part in required), 'Evidence scope/limitations missing')


def language_paths(sketch):
    actual = {path.stem for path in (sketch / 'sdcard/System/languages').glob('*.json')}
    require(actual == LANGUAGES, 'Unexpected shipped language set')
    return {sketch.name + '/sdcard/System/languages/' + name + '.json' for name in LANGUAGES}


def checked_new(name, raw, release, sketch, contracts=None):
    require(name in NEW_REPORTS, 'Unknown .25 evidence contract: ' + name)
    require(raw.get('version') == '0.8.25' and raw.get('passed') is True,
            'New evidence must identify a passing .25 execution')
    prefix = sketch.name + '/'

    if name == 'credential-ui-0825.json':
        required = {prefix + n for n in ('Tab5CredentialUi.inc', 'Tab5TextInput.cpp',
                    'Tab5TextInput.h', 'Tab5UiStyle.cpp', 'Tab5Focus.cpp', 'WriterKeys.h')}
        required |= {'tools/settings-tests/test_credentials.cpp', 'tools/settings-tests/run_credentials.py'}
        hashes(release, raw.get('sources'), required | language_paths(sketch))
        functions((sketch / 'Tab5Tools.cpp').read_text(encoding='utf-8'),
                  raw.get('toolsFunctionHashes'), {'note', 'label', 'button', 'erasePassword',
                  'field', 'dropdown', 'refreshKeys', 'selectedKey', 'visible', 'enabled', 'close', 'open'}, True)
        functions((sketch / 'WriterDocument.cpp').read_text(encoding='utf-8'),
                  raw.get('utf8FunctionHashes'), {'byteOffset', 'charCount'}, True)
        output = minimum_output(raw, 8957)
        wipes = re.search(r'\b(\d+) wipe-before-free observations', output)
        require(raw.get('exitCode') == 0 and '200backupclose-reopen cycles' in output and
                '30s clipboard' in output and wipes and int(wipes[1]) >= 7,
                'Credential lifecycle/clipboard/wipe evidence incomplete')
        require(raw.get('lvglArchiveSha256') == LVGL_ARCHIVE_SHA256, 'Unexpected tested LVGL archive')
        scoped(raw, 'method', ['actual', 'mocked', 'not crypto or hardware'])

    elif name == 'settings-layout-0825.json':
        required = {prefix + n for n in ('Tab5SettingsLayout.h', 'Tab5UiStyle.cpp',
                    'Tab5ShellLayout.h', 'Tab5ShellPolicy.h', 'sdcard/System/font.ttf')}
        required |= {'tools/settings-tests/test_layout.cpp', 'tools/settings-tests/run_layout.py'}
        hashes(release, raw.get('sources'), required | language_paths(sketch))
        mapping = raw.get('mainFunctionHashes', {})
        expected = {'makeButton', 'makeSettingsCard', 'makeSettingsText', 'styleSettingsSlider',
                    'styleSettingsSwitch', 'styleSettingsDropdown', 'createSettingsPage', 'createGamepadSettings'}
        require(set(mapping) == expected, 'Settings controller manifest incomplete')
        functions((sketch / (sketch.name + '.ino')).read_text(encoding='utf-8'),
                  {k: v for k, v in mapping.items() if k != 'createGamepadSettings'}, expected - {'createGamepadSettings'})
        functions((sketch / 'Tab5GamepadSettings.inc').read_text(encoding='utf-8'),
                  {'createGamepadSettings': mapping['createGamepadSettings']}, {'createGamepadSettings'})
        output = minimum_output(raw, 29432)
        require(raw.get('exitCode') == 0 and raw.get('languages') == 14 and
                '1216/1280px and live resize' in output, 'Settings layout coverage incomplete')
        require(raw.get('lvglArchiveSha256') == LVGL_ARCHIVE_SHA256, 'Unexpected tested LVGL archive')
        scoped(raw, 'method', ['actual', 'geometry/style only', 'mocked'])

    elif name == 'vault-0825.json':
        hashes(sketch, raw.get('sources'), VAULT_SOURCES)
        hashes(release / 'tools/vault-tests', raw.get('testSources'),
               {'Platform.h', 'README.md', 'run.py', 'run_target.py', 'test.cpp'})
        require(raw.get('checks', 0) >= 709 and raw.get('faultInjectionPoints', 0) >= 95 and
                raw.get('backupOpenCycles', 0) >= 50, 'Vault state/fault/cycle coverage incomplete')
        require(raw.get('pbkdf2Iterations') == 600000 and raw.get('cryptoVersion') == '3.6.5',
                'Portable derivation or real crypto version changed')
        for flag in ('keyDeletionIncludingInitial', 'secretsSurviveAllKeysDeleted', 'plaintextNeverWritten',
                     'portableBackupCompatible', 'deviceBackupRestore', 'pinGateAndRetry',
                     'corruptionFailsClosed', 'legacyMigrationAndPasswordChange', 'sessionRevocation'):
            require(raw.get(flag) is True, 'Missing vault regression evidence: ' + flag)
        objects = raw.get('cryptoObjectSha256', {})
        require({'aes.obj', 'gcm.obj', 'sha256.obj', 'pkcs5.obj', 'md.obj', 'platform_util.obj'} <= set(objects),
                'Native cryptographic objects missing')
        require(all(re.fullmatch('[0-9a-f]{64}', value) for value in objects.values()), 'Invalid crypto object identity')
        scoped(raw, 'method', ['entire production', 'actual mbedtls', 'in-memory', 'not measured'])

    elif name == 'vault-target-0825.json':
        hashes(sketch, raw.get('sources'), VAULT_SOURCES)
        hashes(release / 'tools', raw.get('tests'), {'vault-tests/run_target.py'})
        expected = {n for n in VAULT_SOURCES if n.endswith('.cpp')}
        rows = raw.get('rows', [])
        require(len(rows) == 4 and {r.get('source') for r in rows} == expected, 'Target vault units incomplete')
        for row in rows:
            require(row.get('exitCode') == 0 and row.get('frames'), 'Target unit failed or no stack evidence')
            require(all(type(frame.get('bytes')) is int and 0 <= frame['bytes'] <= 2048 and
                        frame.get('kind') == 'static' for frame in row['frames']),
                    'Vault target frame exceeds 2048 bytes or is unbounded: ' + row['source'])
        scoped(raw, 'method', ['esp32-p4', 'per-function frames only', 'not measured'])

    elif name == 'runtime-env-0825.json':
        production = {'Tab5Environment.cpp', 'Tab5Environment.h', 'Tab5Secrets.h',
            'Tab5Shell.cpp', 'Tab5Shell.h', 'Tab5ShellHelp.h', 'Tab5Terminal.cpp', 'Tab5Terminal.h',
            'Tab5Git.cpp', 'Tab5Git.h', 'Tab5GitPort.cpp', 'Tab5GitPort.h', 'Tab5GitPull.cpp',
            'Tab5GitPull.h', 'Tab5Python.cpp', 'Tab5Executable.cpp', 'Tab5AppApi.h', 'sdcard/MicroPython/lib/tab5.py'}
        required = {prefix + n for n in production} | {'tools/network-tests/ShellPlatform.h'}
        required |= {'tools/runtime-env-tests/' + n for n in ('environment.cpp', 'git-secret.cpp', 'run.py')}
        hashes(release, raw.get('sources'), required)
        require(raw.get('sourcesStableDuringTest') is True, 'Runtime source stability missing')
        for key, count in (('environment', 3049), ('git', 16), ('python', 13)):
            require(raw.get(key, {}).get('passed') is True and raw[key].get('checks', 0) >= count,
                    'Environment regression coverage incomplete: ' + key)
        require(raw['environment'].get('lockCycles', 0) >= 1000 and raw['git'].get('secretReads', 0) >= 4,
                'Runtime lock/Git secret resolution coverage incomplete')
        require('mocked' in raw['python'].get('scope', '') and 'no claim of complete VM/ELF execution' in raw['python']['scope'],
                'Python/ELF broker scope missing')
        scoped(raw, 'scope', ['actual', 'vault boundary mock', 'target syntax tested separately'])

    elif name == 'sdr-dsp-0825.json':
        hashes(sketch, raw.get('sources'), {'Tab5SdrDsp.h'})
        hashes(release / 'tools/sdr-tests', raw.get('tests'), {'dsp.cpp', 'run_dsp.py'})
        require(raw.get('sourceStableDuringTests') is True and raw.get('checks', 0) >= 2699,
                'DSP source stability or coverage incomplete')
        require(raw.get('baseline') == {'version': '0.8.24', 'sha256': DSP_0824_SHA256},
                'DSP baseline identity changed')
        fixture = release / 'tools/sdr-tests/fixtures/dsp-0824.h'
        require(digest(fixture) == DSP_0824_SHA256, 'Immutable DSP baseline fixture changed')
        reports = release / 'tools/verification'
        ledger = reports / 'history-0824/index.json'
        require(digest(ledger) == HISTORY_0824_SHA256, 'Historical .24 index changed')
        history = json.loads(ledger.read_text(encoding='utf-8'))
        previous = reports / 'sdr-worker-0824.json'
        require(digest(previous) == history['reports']['sdr-worker-0824.json'], 'Historical SDR report changed')
        require(json.loads(previous.read_text(encoding='utf-8'))['sources']['Tab5SdrDsp.h'] == DSP_0824_SHA256,
                'DSP fixture does not match the delivered .24 source')
        output = minimum_output(raw, 2699)
        clean = re.search(r'minimum clean FM SNR ([\d.]+) dB', output)
        alias = re.search(r'Stereo/RDS aliases old (-?[\d.]+) dBc, new (-?[\d.]+) dBc; improvement ([\d.]+) dB', output)
        quiet = re.search(r'Quiet centred carrier old (-?[\d.]+) dB SNR, new (-?[\d.]+) dB SNR', output)
        require(clean and float(clean[1]) > 29 and alias and float(alias[2]) < -45 and float(alias[3]) > 20,
                'FM signal quality or alias rejection regression')
        require(quiet and float(quiet[2]) > 40 and float(quiet[2]) - float(quiet[1]) > 20,
                'Centred low-deviation FM carrier regression')
        scoped(raw, 'method', ['production dsp', 'synthetic', 'original 0.8.24 dsp replayed'])
        require(len(raw.get('limitations', [])) >= 2 and any('No physical' in s for s in raw['limitations']),
                'Synthetic DSP evidence limitations missing')
