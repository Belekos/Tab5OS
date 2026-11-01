"""Additional .26 contracts. Historical .25 assertions remain in force.

The per-feature contracts are filled when each actual execution is available;
no placeholder report, hardware result or earlier report is relabelled .26.
"""
from feature_contracts_0825 import *

NEW_REPORTS = ('git-focus-baseline-0826.json','git-tls-stream-0826.json',
               'maps-search-0826.json','maps-target-0826.json',
               'serial-backend-0826.json')
PREVIOUS_CONTRACT_PATHS = {
    'credential-ui-0825.json': {'tools/settings-tests': 'tools/credential-tests-0826'},
    'vault-0825.json': {'tools/vault-tests': 'tools/vault-tests-0826'},
    'vault-target-0825.json': {'vault-tests/run_target.py': 'vault-tests-0826/run_target.py'},
    'sdr-dsp-0825.json': {'tools/sdr-tests': 'tools/sdr-tests-0826'},
    'runtime-env-0825.json': {'tools/runtime-env-tests': 'tools/runtime-env-tests-0826'},
}


def checked_settings_extension(raw, release, sketch):
    """Add serial geometry identities while retaining every original assertion."""
    base={'makeButton','makeSettingsCard','makeSettingsText','styleSettingsSlider',
          'styleSettingsSwitch','styleSettingsDropdown','createSettingsPage','createGamepadSettings'}
    extra={'createSerialSettings','serialSettingsRefresh'}
    require(set(raw.get('mainFunctionHashes',{}))==base|extra,'Settings serial controllers missing')
    functions((sketch/'Tab5SerialSettings.inc').read_text(encoding='utf8'),
              {k:raw['mainFunctionHashes'][k] for k in extra},extra)
    hashes(release,raw['sources'],{sketch.name+'/Tab5SerialSettings.inc',sketch.name+'/Tab5SerialCore.h'})
    minimum_output(raw,34640)
    copy=dict(raw);copy['mainFunctionHashes']={k:raw['mainFunctionHashes'][k] for k in base}
    return copy


def checked_vault_extension(name,raw,release,sketch):
    if name=='vault-0825.json':
        hashes(sketch,raw.get('sources'),{'Tab5Environment.cpp','Tab5Environment.h'})
        for flag in ('mandatoryInitialPin','environmentCannotBypassPinOrCorruption',
                     'pinlessUpgradeKeepsMasterAndSecrets','currentPinRequiredForChange',
                     'asynchronousUnlock','recoveryPreservesCiphertext'):
            require(raw.get(flag) is True,'Missing current PIN/recovery regression: '+flag)
        require(raw.get('checks',0)>=1094 and raw.get('recoveryFaultInjectionPoints',0)>=40,
                'Current recovery fault coverage incomplete')
    else:
        minimum_output(raw,9069)
        require('mandatory PIN/current PIN reauthentication/deferred UI/recovery confirmation' in raw.get('output',''),
                'Current credential UI flow coverage incomplete')


def checked_explorer_extension(raw,release,sketch):
    """Validate the added image counterexample, then apply every old assertion."""
    rows=raw.get('results',[])
    require(len(rows)==3 and all(row.get('exitCode')==0 for row in rows),'Explorer three-way execution missing')
    images=[row for row in rows if row.get('imageBaseline') is True]
    current=[row for row in rows if row.get('baseline') is False and row.get('imageBaseline') is False]
    require(len(images)==1 and len(current)==1,'Explorer baseline/current distinction missing')
    baseline=json.loads((release/'tools/verification/history-0825/evidence/source-manifest-final.json').read_text(encoding='utf8'))
    old={row['path']:row['sha256'] for row in baseline['files']}
    require(images[0].get('imageBaselineSourceSha256')==old['Tab5OS_Arduino_v0.8.25_Ecriture.ino'],
            'Image counterexample is not bound to delivered .25')
    require('BASELINE .25 image activation: full viewer 1, previews 0' in images[0].get('output',''),
            'Original image activation was not reproduced')
    require('PASS 1209 Explorer production controllers/menu checks' in current[0].get('output',''),
            'Current image preview/navigation execution incomplete')
    copy=dict(raw);copy['results']=[row for row in rows if row.get('imageBaseline') is not True]
    return copy


def checked_help(raw, release, sketch):
    require(raw.get('version') == '0.8.26' and raw.get('passed') is True and
            raw.get('physicalDeviceTested') is False, 'Current help evidence failed or scope missing')
    require(raw.get('sourceSha256') == digest(sketch / 'Tab5Help.cpp'), 'Help changed after audit')
    folder = release / 'tools/localization'
    hashes(folder, raw.get('sources'), {'help-before-0826.json', 'delta-help-0826.json', 'apply_delta_0826.py'})
    pattern = r'\{\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\}'
    actual = {json.loads(title): json.loads(body) for title, body in
              re.findall(pattern, (sketch / 'Tab5Help.cpp').read_text(encoding='utf8'))}
    delta = json.loads((folder / 'delta-help-0826.json').read_text(encoding='utf8'))
    require(set(delta) == LANGUAGES and raw.get('translatedLanguages') == 14, 'Help language coverage incomplete')
    titles = raw.get('changedPages', [])
    require(titles and len(titles) == len(set(titles)), 'Missing help change inventory')
    for locale, values in delta.items():
        pack = json.loads((sketch / 'sdcard/System/languages' / (locale + '.json')).read_text(encoding='utf8'))['translations']
        require(set(values) == {actual[title] for title in titles}, 'Help delta inventory differs')
        for title in titles:
            body = actual[title]
            require(values.get(body) == pack.get(body) and bool(values.get(body)), 'Missing help translation: ' + locale + '/' + title)


def checked_new(name, raw, release, sketch, contracts=None):
    require(name in NEW_REPORTS and raw.get('version')=='0.8.26' and raw.get('passed') is True
            and raw.get('physicalDeviceTested') is False,'Missing successful .26 execution/scope: '+name)
    prefix=sketch.name+'/'
    baseline=json.loads((release/'tools/verification/history-0825/evidence/source-manifest-final.json').read_text(encoding='utf8'))
    old={row['path']:row['sha256'] for row in baseline['files']}
    if name=='git-focus-baseline-0826.json':
        require(raw.get('baselineSha256')==old['Tab5GitView.cpp'] and
                raw.get('baselineMainSha256')==old['Tab5OS_Arduino_v0.8.25_Ecriture.ino'],
                'Git focus baseline differs from delivered .25')
        hashes(release/'tools/maps-tests',raw.get('tests'),{'git-view-0826.cpp','focus_os_0826.inc','run_git_view_0826.py'})
        require(raw.get('exitCode')==1 and 'FAIL touch supersedes last Tab focus globally' in raw.get('output',''),
                'Original Tab/touch regression not reproduced')
        scoped(raw,'method',['Original .25','actual LVGL','No physical reset'])
    elif name=='git-tls-stream-0826.json':
        path='src/tab5git/src/libgit2/streams/mbedtls.c'
        hashes(sketch,raw.get('sources'),{path})
        hashes(release/'tools/git-tests',raw.get('tests'),{'run_tls_stream_0826.py','tls_stream_0826.cpp'})
        rows={r['name']:r for r in raw.get('results',[])}
        require(set(rows)=={'current','baseline'},'TLS ownership comparison missing')
        require(rows['baseline'].get('sourceSha256')==old[path] and rows['baseline'].get('exitCode')==2
                and 'FAIL: socket close after free' in rows['baseline'].get('output',''),'TLS .25 failure missing')
        require(rows['current'].get('sourceSha256')==digest(sketch/path) and rows['current'].get('exitCode')==0
                and 'PASS 33600 checks: 1400 TLS constructor paths, 1400 healthy retries;' in rows['current'].get('output',''),
                'TLS ownership/retry execution missing')
        scoped(raw,'method',['production','boundaries instrumented','Compiled and run with MSVC AddressSanitizer','No physical'])
    elif name=='maps-search-0826.json':
        hashes(release,raw.get('sources'),{prefix+n for n in ('Tab5Map.cpp','Tab5Map.h','Tab5MapSearchParser.h',
               'Tab5Focus.cpp','Tab5Focus.h','Tab5TextInput.cpp','WriterDocument.cpp')}|
               {'tools/maps-tests-0826/'+n for n in ('parser.cpp','info.cpp','run.py')})
        results=raw.get('results',{})
        for key,count,phrase in [('parser',9615,'map parser checks'),('info',1202,'map info checks')]:
            row=results.get(key,{})
            match=re.search(r'PASS (\d+) '+phrase,row.get('output',''))
            require(row.get('exitCode')==0 and match and int(match[1])>=count,'Map execution incomplete: '+key)
        require('24 allocation-fault positions; 300 cycles' in results['parser']['output'] and
                '200 open/action/close cycles' in results['info']['output'],'Map OOM/lifecycle cases missing')
        require(raw.get('baseline',{}).get('sha256')==old['Tab5OS_Arduino_v0.8.25_Ecriture.ino'] and
                results.get('info-baseline',{}).get('exitCode')==1 and
                'FAIL I button opens point information immediately' in results['info-baseline'].get('output',''),
                'Map I-button .25 regression missing')
        source=(sketch/(sketch.name+'.ino')).read_text(encoding='utf8')
        expected={'static lv_obj_t* makeButton(', 'static lv_obj_t* createMapOverlay(', 'static void closeMapStylesEvent(',
                  'static double mapWorldX(', 'static double mapWorldY(', 'static void mapCoordinatesFromWorld(',
                  'static void mapPointInfo(int x, int y) {', 'static void mapTouchEvent(', 'static void mapInfoEvent(', 'info button binding'}
        require(set(raw.get('controllers',{}))==expected,'Map controller identities incomplete')
        for signature,value in raw['controllers'].items():
            if signature=='info button binding':
                start=source.index('  mapInfoButton = makeIconButton(');end=source.index('  mapInfoLabel = lv_label_create(toolbar);',start)
            else:
                start=source.index(signature);end=closing(source,source.index('{',start))
            require(hashlib.sha256(source[start:end].encode()).hexdigest()==value,'Map controller changed: '+signature)
        scoped(raw,'method',['actual ArduinoJson','real LVGL','No physical reset reproduction'])
    elif name=='maps-target-0826.json':
        hashes(release,raw.get('sources'),{prefix+n for n in ('Tab5Map.cpp','Tab5Map.h','Tab5MapSearchParser.h')})
        hashes(release,raw.get('tests'),{'tools/maps-tests-0826/target.py'})
        require(raw.get('result',{}).get('exitCode')==0 and raw['result'].get('sourceUnchanged') is True,'Map target compilation failed')
        frames=raw.get('frames',[])
        require(len(frames)>=3 and all(len(row.split('\t'))==3 and row.split('\t')[2]=='static'
                and 0<=int(row.split('\t')[1])<=2048 for row in frames),'Map target frame exceeds budget')
        scoped(raw,'method',['Actual ESP32-P4','individual functions','not complete'])
    elif name=='serial-backend-0826.json':
        hashes(release,raw.get('sourceHashes'),{prefix+n for n in ('Tab5Serial.cpp','Tab5Serial.h','Tab5SerialCore.h','Tab5SerialCommand.cpp')}|
               {'tools/serial-tests-0826/backend.cpp','tools/serial-tests-0826/run.py'})
        match=re.search(r'PASS serial backend (\d+) checks',raw.get('output',''))
        require(match and int(match[1])>=211881,'Serial bounded queues/command execution incomplete')
        scoped(raw,'method',['Exact production','fake UART/USB/NVS/FreeRTOS','real command parser'])
