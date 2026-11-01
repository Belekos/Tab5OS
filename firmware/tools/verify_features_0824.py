"""Validate .24 against preserved .22/.23 contracts and exact historical evidence.

No report or result is relabelled. Reuse requires its original archive digest,
all source/harness hashes required by its existing contract, and the same test
thresholds. Only path components (sketch version and System directory) move.
"""
from __future__ import annotations
import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import re

VERSION='0.8.24'
HISTORY_SHA256='6954d5f0ec0a1d55c80c214501e09941b8415c4c08827d274a319f415071048d'
REQUIRE_CURRENT={'directory-picker-0822.json','maps-0822.json','gamepad-0822.json',
                 'syntax-0822.json','resources-0822.json','resource-inventory-0822.json',
                 'languages-0822.json','delta-languages-0822.json','ui-locale-calls-0822.json',
                 'explorer-0823.json','gamepad-pipeline-0823.json','gamepad-settings-0823.json'}
NEW_REPORTS=('folder-preview-0824.json','map-session-0824.json','system-migration-0824.json',
             'sdr-worker-0824.json','sdr-ui-0824.json','sdr-usb-0824.json','media-0824.json',
             'gamepad-navigation-0824.json','gamepad-history-0824.json',
             'numeric-keyboard-0824.json','system-paths-0824.json','settings-recovery-0824.json')

def require(ok,message):
    if not ok:raise ValueError(message)
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def controller_block(source,signature):
    begin=source.index(signature);end=source.index('{',begin)+1;depth=1
    while depth:
        if source[end]=='{':depth+=1
        elif source[end]=='}':depth-=1
        end+=1
    return source[begin:end]
def read_json(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def safe_path(base,name):
    require(isinstance(name,str) and name,'Empty manifest path')
    path=(base/name.replace('\\','/')).resolve()
    require(path.is_relative_to(base.resolve()),'Unsafe manifest path: '+name)
    return path
def relocate_path(value):
    if not isinstance(value,str):return value
    # Never rewrite captured compiler flags, stdout, or absolute machine paths.
    # They are immutable execution evidence, not manifests to relocate.
    value=re.sub(r'^Tab5OS_Arduino_v0\.8\.(?:19|20|21|22|23)_Ecriture(?=[/\\.]|$)',
                 'Tab5OS_Arduino_v0.8.24_Ecriture',value)
    if value.startswith('Tab5OS_Arduino_v0.8.24_Ecriture'):
        value=re.sub(r'([/\\])Tab5OS_Arduino_v0\.8\.(?:19|20|21|22|23)_Ecriture\.ino$',
                     r'\1Tab5OS_Arduino_v0.8.24_Ecriture.ino',value)
    if value.startswith(('Tab5OS_Arduino_v0.8.24_Ecriture/','Tab5OS_Arduino_v0.8.24_Ecriture\\','sdcard/','sdcard\\')):
        value=value.replace('sdcard/M5Writer/','sdcard/System/').replace('sdcard\\M5Writer\\','sdcard\\System\\')
    return re.sub(r'^M5Writer([/\\])',r'System\1',value)
def relocated(data):
    if isinstance(data,dict):
        result={relocate_path(k):relocated(v) for k,v in data.items()}
        require(len(result)==len(data),'Duplicate relocated manifest paths');return result
    if isinstance(data,list):return [relocated(v) for v in data]
    return relocate_path(data)
def selected_report_path(release,name):
    reports=release/'tools/verification'
    current=reports/re.sub(r'-08\d{2}(?=\.)','-0824',name)
    if current.is_file() or name in REQUIRE_CURRENT:return current
    previous=reports/re.sub(r'-08\d{2}(?=\.)','-0823',name)
    if previous.is_file():return previous
    return reports/name
def read_selected(release,name):return relocated(read_json(selected_report_path(release,name)))
class ReportDirectory:
    def __init__(self,release):self.release=release
    def __truediv__(self,name):return selected_report_path(self.release,name)

def load_contracts(release,history):
    path=release/'tools/verify_features_0823.py'
    require(digest(path)==history['verifierSha256'],'Historical .23 validation contracts changed')
    spec=importlib.util.spec_from_file_location('tab5_contracts_0823',path)
    old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
    old.relocate_path=relocate_path;old.relocated=relocated
    history22_path=release/'tools/verification/history-0822/index.json'
    require(digest(history22_path)==old.HISTORY_SHA256,'Historical .22 evidence index changed')
    history22=read_json(history22_path)
    for name,expected in history22['reports'].items():
        require(digest(safe_path(release/'tools/verification',name))==expected,'Historical .22 evidence changed: '+name)
    # Load the exact historical file, with explicit path-only adaptations in
    # memory. No result thresholds, required scenarios or hash checks change.
    rule_path=release/'tools/verify_features_0822.py'
    require(digest(rule_path)==history22['verifierSha256'],'Historical .22 validation rules changed')
    import types
    rules=types.ModuleType('tab5_contracts_0822_system');rules.__file__=str(rule_path)
    code=rule_path.read_text(encoding='utf8').replace('M5Writer/','System/')
    code=code.replace('r"Tab5OS_(?:Arduino|Ecriture)_v(?!0\\.8\\.22)[0-9]"',
                      'r"Tab5OS_(?:Arduino|Ecriture)_v(?!"+re.escape(VERSION)+r")[0-9]"')
    code=code.replace('report.get("nativeProof") == "ime-native-0819.json"',
                      'report.get("nativeProof") == (reports / "ime-native-0819.json").name')
    exec(compile(code,str(rule_path),'exec'),rules.__dict__)
    rules.read_json=lambda path:relocated(read_json(path))
    rules.SYNTAX_UNITS={relocate_path(n) for n in rules.SYNTAX_UNITS}
    rules.SYNTAX_UNITS.discard('Tab5Pentest.cpp')
    rules.SYNTAX_UNITS.update({'Tab5ContextMenu.cpp','Tab5AppPanel.cpp','Tab5ElfValidate.cpp','Tab5NativeView.cpp'})
    rules.SYNTAX_UNITS.update({'Tab5Sdr.cpp','Tab5SdrUi.cpp','Tab5DirectoryView.cpp','Tab5FilePicker.cpp',
                             'Tab5Writing.cpp','Tab5MediaList.cpp','Tab5MediaSession.cpp','Tab5SystemMigration.cpp',
                             'WriterGamepadNavigation.cpp'})
    return old,rules

def checked_gamepad(raw,release,sketch,contracts):
    require(raw.get('version')==VERSION and raw.get('passed') is True and raw.get('physicalDeviceTested') is False,
            'USB execution or hardware scope missing')
    contracts.hashes(sketch,raw['files'],{'src/usb_hid/hid_host.c','src/usb_hid/input_interface.h','src/usb_hid/gamepad_transport.h',
                                        'WriterUsb.cpp','WriterGamepad.cpp','WriterHid.cpp','WriterMouse.cpp'})
    contracts.hashes(release/'tools/usb-gamepad-tests',raw['harness'],{'host_stub.h','run.py','run_host.py','run_msvc.cmd',
                                                                    'test_gamepad.cpp','test_host.c','test_interfaces.cpp'})
    current=raw['current'];baseline=raw['baseline'];old=read_json(release/'tools/verification/gamepad-0823.json')
    match=re.search(r'PASS: (\d+) USB lifecycle assertions; 0 failures; retained allocations 0; claims 0',current.get('result',''))
    require(current.get('exitCode')==0 and match and int(match[1])>=46095 and
            'hid_host_xinput_initialize' in current.get('extractedFunctions',[]) and
            current.get('sourceSha256')==digest(sketch/'src/usb_hid/hid_host.c') and
            baseline.get('exitCode')==1 and '5 failures' in baseline.get('result','') and
            baseline.get('sourceSha256')==old['current']['sourceSha256'] and
            'PASS: 4069' in raw.get('enumeration','') and 'PASS:' in raw.get('packets',''),
            'USB initialization/lifecycle .24 or reproduced .23 regression incomplete')

def checked_pipeline(raw,release,sketch,contracts):
    require(raw.get('version')==VERSION and raw.get('passed') is True and raw.get('physicalDeviceTested') is False and raw.get('exitCode')==0,
            'USB callback pipeline execution missing')
    require(set(raw.get('extractedFunctions',[]))=={'releasePad','interfaceEvent','usbGamepadButtons','usbGameButtons','usbGamepadInfo','serviceRecovery'},
            'USB callback/input/recovery coverage differs')
    match=re.search(r'PASS: (\d+) production callback/API assertions; HID and XInput without F1, (\d+) press/release cycles',raw.get('output',''))
    require(match and int(match[1])>=20048 and int(match[2])>=4000,'USB pipeline regression coverage incomplete')
    prefix=sketch.name+'/'
    contracts.hashes(release,raw['sources'],{prefix+n for n in ('WriterUsb.cpp','WriterGamepad.cpp','WriterGamepad.h','WriterHid.cpp','WriterHid.h','WriterMouse.cpp','WriterMouse.h')}|
                     {'tools/usb-gamepad-tests/run_pipeline.py','tools/usb-gamepad-tests/test_pipeline.cpp'})

def checked_new(name,raw,release,sketch,contracts):
    require(raw.get('version')==VERSION and raw.get('passed') is True and raw.get('physicalDeviceTested') is False,
            'Current regression failed or scope missing: '+name)
    if name=='folder-preview-0824.json':
        require(raw.get('exitCode')==0 and raw.get('sanitizer')=='AddressSanitizer' and
                'PASS: production Tab5FolderPreview.h' in raw.get('output',''),
                'Sorted folder preview lifecycle/paging execution missing')
        contracts.hashes(sketch,raw['sources'],{'Tab5FolderPreview.h','Tab5DirectoryModel.h','Tab5FileSort.h'})
        contracts.hashes(release/'tools/folder-preview-tests',raw['tests'],
                         {'run.py','test_folder_preview.cpp','Tab5VolumeFS.h','Tab5HiddenFiles.h'})
    elif name=='map-session-0824.json':
        result=raw.get('result',{});match=re.search(r'PASS (\d+) production Cartes persistence, corruption and LVGL UI checks',result.get('output',''))
        require(result.get('exitCode')==0 and match and int(match[1])>=14750,'Cartes persistence and actual UI coverage incomplete')
        contracts.hashes(release,raw['sources'],{sketch.name+'/'+n for n in ('Tab5Map.cpp','Tab5Map.h','Tab5MapViewState.h','Tab5MapViewPolicy.h')}|
                         {'tools/maps-tests/session.cpp','tools/maps-tests/run_session.py'})
        corpus=[(sketch/(sketch.name+'.ino')).read_text(encoding='utf8'),(sketch/'Tab5Map.cpp').read_text(encoding='utf8')]
        require(len(raw.get('controllers',{}))>=15,'Map persistence controller identities missing')
        for signature,expected in raw['controllers'].items():
            source=next((s for s in corpus if signature in s),None);require(source is not None,'Missing map controller: '+signature)
            begin=source.index(signature);end=source.index('{',begin)+1;depth=1
            while depth:
                if source[end]=='{':depth+=1
                elif source[end]=='}':depth-=1
                end+=1
            require(hashlib.sha256(source[begin:end].encode()).hexdigest()==expected,'Map controller changed: '+signature)
    elif name=='system-migration-0824.json':
        result=raw.get('result',{});match=re.search(r'PASS (\d+) production migration checks; (\d+) injected I/O boundaries, (\d+) mutation cut points; 10000-tile subtree optimization',result.get('output',''))
        require(result.get('exitCode')==0 and match and int(match[1])>=3570 and int(match[2])>=179 and int(match[3])>=20,
                'Migration resumability/fault-injection coverage incomplete')
        contracts.hashes(release,raw['sources'],{sketch.name+'/'+n for n in ('Tab5SystemMigration.cpp','Tab5SystemMigration.h')}|
                         {'tools/system-migration-tests/'+n for n in ('migration.cpp','Arduino.h','FS.h','run.py')})
    elif name in ('sdr-worker-0824.json','sdr-ui-0824.json'):
        ui=name.startswith('sdr-ui');match=re.search(r'PASS (\d+) SDR (?:UI|model/DSP/worker) checks',raw.get('output',''))
        require(raw.get('sourceStableDuringTests') is True and raw.get('limitations') and raw.get('method'),
                'SDR source stability or limitations missing')
        require(match and int(match[1])>=(549 if ui else 18624),'SDR lifecycle/functional execution count incomplete')
        require(('100 reopen cycles' if ui else '2000 lifecycle cycles') in raw['output'],'SDR repeat lifecycle coverage missing')
        required={'Tab5SdrUi.cpp','Tab5SdrUi.h','Tab5SdrModel.h','Tab5Sdr.h','Tab5TextInput.cpp','Tab5Focus.cpp','Tab5UiStyle.cpp'} if ui else {'Tab5Sdr.cpp','Tab5Sdr.h','Tab5SdrModel.h','Tab5SdrDsp.h'}
        contracts.hashes(sketch,raw['sources'],required)
        contracts.hashes(release/'tools/sdr-tests',raw['tests'],{'ui.cpp','run_ui.py'} if ui else {'worker.cpp','run.py'})
        require(raw.get('checks')==int(match[1]) and raw.get('lifecycleCycles')==(100 if ui else 2000),
                'SDR structured execution metrics differ from output')
    elif name=='sdr-usb-0824.json':
        require(raw.get('sourceStableDuringTests') is True and raw.get('limitations') and raw.get('method') and
                raw.get('checks',0)>=20080 and raw.get('lifecycleCycles',0)>=2000,
                'SDR USB adapter execution/stability/limitations missing')
        require('PASS 20080 SDR USB adapter checks; 2000 complete lifecycles;' in raw.get('output','') and
                'cancellation, unplug, EP0 completion, DMA OOM and bounds' in raw['output'],
                'SDR USB cancellation/unplug/allocation scenarios missing')
        contracts.hashes(sketch,raw['sources'],{'src/rtl_sdr/'+n for n in ('usb_adapter.cpp','libusb.h','librtlsdr.c','rtl-sdr.h')})
        contracts.hashes(release/'tools/sdr-tests',raw['tests'],{'usb.cpp','run_usb.py'})
    elif name=='media-0824.json':
        tests={row['name']:row for row in raw.get('tests',[])};require(set(tests)>={'session','list','catalogue','ui'} and all(v['exitCode']==0 for v in tests.values()),'Media session/list/catalogue/UI tests missing')
        for label,count in [('session',26),('list',1019),('catalogue',9),('ui',21)]:
            match=re.search(r'PASS (\d+) checks',tests[label]['output']);require(match and int(match[1])>=count,'Media coverage incomplete: '+label)
        require('4000 redraws' in tests['session']['output'] and '800 records, pool24' in tests['list']['output'] and 'actual600pack' in tests['catalogue']['output'],
                'Media redraw/deferred persistence/bounded-list/catalogue cases missing')
        require('actual main Media callbacks/page layout' in tests['ui']['output'] and 'noautoplay' in tests['ui']['output'],
                'Media actual page/callback/no-autoplay coverage missing')
        contracts.hashes(sketch,raw['sources'],{'Tab5MediaList.cpp','Tab5MediaList.h','Tab5MediaSession.cpp','Tab5MediaSession.h','Tab5Audio.cpp','Tab5Audio.h','Tab5FileSort.h',
                                              'Tab5Focus.cpp','WriterDocument.cpp','sdcard/Packs/radios-internet.json'})
        contracts.hashes(release,raw['testSources'],{'tools/media-tests/'+n for n in ('catalogue.cpp','list.cpp','session.cpp','ui.cpp','Preferences.h','Platform.h','run.py')})
        required={'audioRememberPlayback','audioRestoreBrowser','audioRememberBrowser','audioRememberScroll','audioRememberItem','setAudioTabState','audioGroupBackEvent',
                  'audioFavoriteId','audioMediaMetadata','audioMediaFormat','audioMediaAction','audioChildGroup','refreshAudioList','audioLocalTabEvent','audioRadioTabEvent',
                  'audioRecordTabEvent','audioVideoTabEvent','audioReloadEvent','audioPlayEvent','audioVolumeEvent','createAudioPage'}
        require(required<=set(raw.get('mainFunctionHashes',{})),'Media page/callback identities missing')
        source=(sketch/(sketch.name+'.ino')).read_text(encoding='utf8')
        for name,expected in raw['mainFunctionHashes'].items():
            match=re.search(r'static [^\n]+\b'+re.escape(name)+r'\([^\n]*\)\s*\{',source)
            require(match is not None,'Missing media controller: '+name)
            require(hashlib.sha256(controller_block(source,match[0]).encode()).hexdigest()==expected,'Media controller changed: '+name)
        begin=source.index('static AudioView audioView =');end=source.index('static uint32_t lastTopAudioUpdateMs = 0;',begin)+len('static uint32_t lastTopAudioUpdateMs = 0;')
        require(hashlib.sha256(source[begin:end].encode()).hexdigest()==raw.get('mainGlobalsSha256'),'Media UI globals changed or not identified')
    elif name=='gamepad-navigation-0824.json':
        require(raw.get('exitCode')==0 and 'PASS: OS navigation, 12 controls' in raw.get('output','') and '4000 non-consuming snapshots' in raw['output'],
                'OS/Python gamepad input coverage incomplete')
        require(len(raw.get('pythonCases',[]))>=4 and 'PASS: actual gamepad.py bridge decoding, constants and capture calls.' in raw.get('pythonOutput',''),
                'Actual MicroPython gamepad bridge checks missing')
        contracts.hashes(release,raw['sources'],{sketch.name+'/'+n for n in ('WriterGamepadNavigation.cpp','WriterGamepadNavigation.h','sdcard/MicroPython/lib/gamepad.py')}|
                         {'tools/usb-gamepad-tests/run_navigation.py','tools/usb-gamepad-tests/test_navigation.cpp'})
    elif name=='gamepad-history-0824.json':
        versions=raw.get('versions',{});require(set(versions)=={'15','16','17','18','23','24'},'Historical HID comparisons incomplete')
        for version,row in versions.items():
            require(row.get('exitCode')==(1 if version=='23' else 0),'Unexpected historical HID result: '+version)
            require(('buttons=260 expected=1, directions=6 expected=0' if version=='23' else 'buttons=1 expected=1, directions=0 expected=0') in row.get('output',''),'HID historical fixture result missing')
        old=read_json(release/'tools/verification/gamepad-pipeline-0823.json')
        require(versions['23']['callbackSourceSha256']==old['sources']['Tab5OS_Arduino_v0.8.23_Ecriture/WriterUsb.cpp'] and
                versions['24']['callbackSourceSha256']==digest(sketch/'WriterUsb.cpp'),'HID current/historical callback identity missing')
        require(versions['24']['decoderSourceSha256']==digest(sketch/'WriterGamepad.cpp') and
                raw.get('fixture',{}).get('packet')=='04 01 00 00 00 00 00 00 00',
                'HID decoder/fixture identity missing')
        contracts.hashes(sketch,raw['sources'],{'WriterUsb.cpp','WriterGamepad.cpp','WriterGamepad.h','WriterHid.cpp','WriterHid.h','WriterMouse.cpp','WriterMouse.h'})
        contracts.hashes(release/'tools/usb-gamepad-tests',raw['tests'],{'run_hid_history.py'})
    elif name=='numeric-keyboard-0824.json':
        require(raw.get('exitCode')==0 and raw.get('sourceStableDuringTests') is True and
                'PASS 1672 numeric keyboard checks; 64 unsigned/signed/return cycles; actual LVGL and production keyPressedEvent' in raw.get('output',''),
                'Numeric keyboard unsigned/signed/return execution missing')
        contracts.hashes(release,raw['sources'],{sketch.name+'/'+n for n in ('Tab5NumericKeyboard.h','Tab5ImeUi.inc',sketch.name+'.ino')}|
                         {'tools/ui-tests/numeric.cpp','tools/ui-tests/run_numeric.py'})
        require(re.fullmatch(r'[0-9a-f]{64}',raw.get('lvglArchiveSha256','')) is not None and raw.get('scope'),
                'Numeric keyboard LVGL identity or scope missing')
    elif name=='system-paths-0824.json':
        match=re.search(r'PASS: (\d+) actual path-helper, service, archive, native import assertions;',raw.get('output',''))
        require(raw.get('exitCode')==0 and match and int(match[1])>=106 and
                'PASS: production System helpers compile and execute as C17.' in raw['output'] and
                'PASS: production VolumeFS open/read/write/exists/rename/remove/mkdir/rmdir;' in raw['output'] and
                'independent rename buffers, deep paths through 1023 bytes and zero references.' in raw['output'],
                'System alias C/C++/VFS route execution missing')
        contracts.hashes(sketch,raw['sources'],{'Tab5SystemPath.h','Tab5VolumeFS.cpp','Tab5VolumeFS.h','Tab5Storage.cpp','Tab5Storage.h',
                         'Tab5FilePolicy.h','Tab5Python.cpp','Tab5PythonServices.cpp','Tab5PythonArchives.cpp','src/micropython/tab5_module.c','src/micropython/py/reader.c'})
        contracts.hashes(release/'tools/system-path-tests',raw['tests'],{'run.py','test_header.c','test_paths.cpp','test_volume.cpp','vfs_api.h'})
    elif name=='settings-recovery-0824.json':
        result=raw.get('result',{});match=re.search(r'PASS (\d+) production settings/migration/recovery controller checks; every settings byte interrupted then resumed',result.get('output',''))
        require(result.get('exitCode')==0 and match and int(match[1])>=985 and 'AddressSanitizer' in raw.get('method',''),
                'Transactional settings/recovery fault injection missing')
        contracts.hashes(release,raw['sources'],{sketch.name+'/'+n for n in ('Tab5SystemMigration.cpp','Tab5SystemMigration.h')}|
                         {'tools/system-migration-tests/'+n for n in ('run_settings.py','settings.cpp','Arduino.h','FS.h')})
        required={'static bool migrateSystemDirectory()','static bool saveSettings()','static void loadSettings()',
                  'static bool loadSettingsSources()','static bool reloadSettingsAfterMigration()','struct AppSettings',
                  'static void serviceSdRecovery(','void setup() {'}
        require(required<=set(raw.get('controllers',{})),'Settings boot/recovery controller identities missing')
        source=(sketch/(sketch.name+'.ino')).read_text(encoding='utf8')
        for signature,expected in raw['controllers'].items():
            require(hashlib.sha256(controller_block(source,signature).encode()).hexdigest()==expected,
                    'Settings controller changed: '+signature)
    else:raise ValueError('New regression has no reviewed contract: '+name)

def collect_evidence(release):
    release=release.resolve();sketch=release/f'Tab5OS_Arduino_v{VERSION}_Ecriture';reports=release/'tools/verification'
    require(release.name==f'Tab5OS_Ecriture_v{VERSION}' and sketch.is_dir(),'Wrong release root')
    ledger=reports/'history-0823/index.json'
    require(digest(ledger)==HISTORY_SHA256,'Historical .23 evidence index changed')
    history=read_json(ledger);require(history['baselineVersion']=='0.8.23','Wrong historical baseline')
    contracts,rules=load_contracts(release,history)
    sources=rules.source_manifest(sketch);dependencies=rules.dependency_manifest(sketch);verifier=digest(Path(__file__))
    selected=ReportDirectory(release);checks=[];errors=[]
    report_hashes={'history-0823/index.json':HISTORY_SHA256,'history-0822/index.json':contracts.HISTORY_SHA256}
    # These unchanged .23 reports bind reproduced baseline identities inside
    # current .24 regressions, even though their current tests run again.
    for name in ('gamepad-0823.json','gamepad-pipeline-0823.json','wifi-transitions-0823.json'):
        expected=history['reports'][name]
        require(digest(reports/name)==expected,'Historical baseline identity report changed: '+name)
        report_hashes[name]=expected
    names=[n for n in (*rules.CURRENT_REPORTS,*rules.HISTORICAL_REPORTS) if n not in contracts.REPLACED]
    def load(name):
        path=selected/name;key=path.name;sha=digest(path);raw=read_json(path)
        current=key.endswith('-0824.json')
        if current:
            require(raw.get('version',raw.get('release'))==VERSION,'Current report must identify .24: '+key)
            require(sha not in history['reports'].values(),'Old report copied as current: '+key)
        else:
            require(history['reports'].get(key)==sha,'Historical report differs from original .23 archive: '+key)
        report_hashes[key]=sha
        return key,raw,current
    for name in names:
        key=(selected/name).name;current=key.endswith('-0824.json')
        try:
            key,raw,current=load(name);origin=raw.get('version',raw.get('release','0.8.22'))
            rules.VERSION=origin;contracts.VERSION=origin
            old_hash=rules.HISTORICAL_REPORTS.pop(name,None) if key!=name else None
            try:
                if name=='gamepad-0822.json' and current:checked_gamepad(raw,release,sketch,contracts)
                elif name=='gamepad-0822.json' and key!='gamepad-0822.json':contracts.checked_gamepad(raw,release,sketch)
                elif name=='maps-0822.json' and key!='maps-0822.json':contracts.checked_maps(relocated(raw),release,sketch)
                elif name=='delta-languages-0822.json' and current:
                    # The .24 writer uses clearer field names; preserve the
                    # predecessor's complete-pack/delta/hash requirements.
                    translated=relocated(raw)
                    translated['deltaFiles']=raw['sources'];translated['newSourceKeys']=raw['newKeys']
                    require(raw['newKeys']>=83,'Missing .24 translation delta')
                    for row in translated['packs'].values():row['translatedNewKeys']=row['translatedKeys']
                    rules.checked_report(name,translated,release,sketch,selected)
                else:rules.checked_report(name,relocated(raw),release,sketch,selected)
                if name=='wifi-transitions-0822.json' and current:
                    contracts.hashes(release,raw['testSources'],{'tools/wifi-transition-tests/'+n for n in ('run.py','run_msvc.cmd','flow.cpp','Platform.h')}|
                                     {'tools/network-gate-tests/Arduino.h','tools/network-gate-tests/freertos/FreeRTOS.h','tools/network-gate-tests/freertos/semphr.h'})
                    previous=read_json(reports/'wifi-transitions-0823.json')
                    require(raw['baseline_0_8_19'].get('sources')==previous['baseline_0_8_19'].get('sources'),
                            'Wi-Fi regression baseline source differs from archived .19 identity')
                if name=='glyphs-0822.json' and current:
                    require({sketch.name+'.ino','Tab5CjkFonts.cpp','Tab5CjkFonts.h','Tab5Localization.cpp',
                             'Tab5LanguageMetadata.h','Tab5KeyboardLayouts.cpp','lv_conf.h'}<=set(raw.get('sources',{})),
                            'Glyph fallback source identity incomplete')
                    contracts.hashes(release,raw['testSources'],{'tools/localization/audit_glyphs.py','tools/audit_languages.py'})
                    require({'languages-0824.json','arabic-0824.json','keyboard-shortcuts-0824.json','ime-native-0824.json','ime-0824.json'}<=set(raw.get('proofs',[])),
                            'Glyph audit does not reference current native/IME/keyboard evidence')
                    native=read_json(reports/'ime-native-0824.json');ime=read_json(reports/'ime-0824.json')
                    current_proofs=raw.get('currentEvidence',{})
                    require(current_proofs.get('ime-native-0824.json',{}).get('rerun') is True and
                            current_proofs['ime-native-0824.json'].get('sourceHashesVerified')==len(native['sources']) and
                            current_proofs.get('ime-0824.json',{}).get('rerun') is True and
                            current_proofs['ime-0824.json'].get('assetHashesVerified')==sum(not n.startswith('System/languages/') for n in ime['assets']),
                            'Glyph current evidence source/asset accounting incomplete')
                if name=='directory-picker-0822.json':
                    require(current,'Current sorting/selection tests required')
                    rules.check_count(raw,581)
                if name=='elf-lifecycle-0822.json' and key!='elf-lifecycle-0822.json':
                    require(raw.get('checks',0)>=25856 and raw.get('audioReleaseContention') is True and
                            raw.get('baseline',{}).get('expectedFailure') is True and
                            raw['baseline'].get('exitCode')==2 and raw['baseline'].get('version')=='0.8.22',
                            'ELF audio-release regression or .22 baseline missing')
            finally:
                if old_hash is not None:rules.HISTORICAL_REPORTS[name]=old_hash
            checks.append(dict(report=key,passed=True,historical=not current,originVersion=origin,
                               reusedOnlyAfterSourceAndHarnessHashCheck=not current))
        except (OSError,ValueError,KeyError,TypeError,SyntaxError) as error:
            errors.append(key+': '+str(error));checks.append(dict(report=key,passed=False,historical=not current,error=str(error)))
    for name in contracts.EXTRA_REPORTS:
        key=(selected/name).name;current=key.endswith('-0824.json')
        try:
            key,raw,current=load(name);origin=raw.get('version');contracts.VERSION=origin
            # Contract labels remain .23, even when the new execution is .24.
            if name=='gamepad-pipeline-0823.json' and current:checked_pipeline(raw,release,sketch,contracts)
            else:contracts.checked_extra(name,relocated(raw),release,sketch)
            checks.append(dict(report=key,passed=True,historical=not current,originVersion=origin,
                               reusedOnlyAfterSourceAndHarnessHashCheck=not current))
        except (OSError,ValueError,KeyError,TypeError,SyntaxError) as error:
            errors.append(key+': '+str(error));checks.append(dict(report=key,passed=False,historical=not current,error=str(error)))
    for name in NEW_REPORTS:
        try:
            raw=read_json(reports/name);report_hashes[name]=digest(reports/name)
            checked_new(name,raw,release,sketch,contracts)
            checks.append(dict(report=name,passed=True,historical=False,originVersion=VERSION))
        except (OSError,ValueError,KeyError,TypeError,SyntaxError) as error:
            errors.append(name+': '+str(error));checks.append(dict(report=name,passed=False,historical=False,error=str(error)))
    if rules.source_manifest(sketch)!=sources:errors.append('Sources changed during verification')
    if rules.dependency_manifest(sketch)!=dependencies:errors.append('Dependencies changed during verification')
    if digest(Path(__file__))!=verifier:errors.append('Verifier changed during verification')
    for name,expected in report_hashes.items():
        if digest(safe_path(reports,name))!=expected:errors.append('Report changed during verification: '+name)
    return dict(version=VERSION,checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=not errors,
                physicalDeviceTested=False,sources=sources,dependencies=dependencies,verifierSha256=verifier,
                ruleSetSha256=history['verifierSha256'],reports=report_hashes,checks=checks,errors=errors,
                replacedEvidence=contracts.REPLACED,baselineArchiveSha256=history['baselineArchiveSha256'],
                scope='Current .24 executions plus exact archived .22/.23 reports whose source and harness hashes still match. All predecessor contracts remain enforced; no physical Tab5 test or firmware flash.')
def main():
    release=Path(__file__).resolve().parents[1];result=collect_evidence(release)
    target=release/'tools/verification/features-0824.json';temporary=target.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8');temporary.replace(target)
    print(json.dumps({k:result[k] for k in ('passed','errors')},ensure_ascii=False,indent=2))
    return 0 if result['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
