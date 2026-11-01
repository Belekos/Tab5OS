"""Verify .27 while preserving the exact .22/.23/.24/.25/.26 contracts and evidence.

Old reports retain their bytes and dates. Reuse requires source and harness
identities accepted by the original contract. Changed code needs a real rerun.
"""
from __future__ import annotations
import datetime,hashlib,importlib.util,inspect,json,re
from pathlib import Path
import feature_contracts_0825 as previous_additions
import feature_contracts_0826 as additions
import feature_contracts_0827 as current_additions

VERSION='0.8.27'
HISTORY_SHA256='cc0b47d4e1f7dba51866d50b3b6e43d604af5a136bea93fda8693f3eb134e9ab'
REQUIRE_CURRENT={'syntax-0822.json','resources-0822.json','resource-inventory-0822.json',
 'languages-0822.json','delta-languages-0822.json','ui-locale-calls-0822.json',
 'help-0825.json'}
NEW_REPORTS=(*previous_additions.NEW_REPORTS,'help-0825.json',*additions.NEW_REPORTS,*current_additions.NEW_REPORTS)

def require(ok,message):
    if not ok:raise ValueError(message)
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def read_json(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def safe_path(base,name):
    require(isinstance(name,str) and name,'Empty manifest path')
    path=(base/name.replace('\\','/')).resolve()
    require(path.is_relative_to(base.resolve()),'Unsafe manifest path: '+name)
    return path
def relocate_path(value):
    if not isinstance(value,str):return value
    value=re.sub(r'^Tab5OS_Arduino_v0\.8\.(?:19|20|21|22|23|24|25|26)_Ecriture(?=[/\\.]|$)',
                 'Tab5OS_Arduino_v0.8.27_Ecriture',value)
    if value.startswith('Tab5OS_Arduino_v0.8.27_Ecriture'):
        value=re.sub(r'([/\\])Tab5OS_Arduino_v0\.8\.(?:19|20|21|22|23|24|25|26)_Ecriture\.ino$',
                     r'\1Tab5OS_Arduino_v0.8.27_Ecriture.ino',value)
    if value.startswith(('Tab5OS_Arduino_v0.8.27_Ecriture/','Tab5OS_Arduino_v0.8.27_Ecriture\\','sdcard/','sdcard\\')):
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
    current=reports/re.sub(r'-08\d{2}(?=\.)','-0827',name)
    if current.is_file() or name in REQUIRE_CURRENT:return current
    for suffix in ('0826','0825','0824','0823'):
        previous=reports/re.sub(r'-08\d{2}(?=\.)','-'+suffix,name)
        if previous.is_file():return previous
    return reports/name
def read_selected(release,name):return relocated(read_json(selected_report_path(release,name)))
HARNESS_PATHS={
 'git-0822.json':{'run.py':'run_0825.py'},
 'git-journal-0822.json':{'run.py':'run_0825.py'},
 'git-https-0822.json':{'https_remote.py':'https_remote_0825.py','run.py':'run_0825.py'},
 'git-view-0822.json':{'git-view.cpp':'git-view-0825.cpp','run_git_view.py':'run_git_view_0825.py'},
 'git-close-baseline-0823.json':{'git-view.cpp':'git-view-0825.cpp','run_git_view.py':'run_git_view_0825.py'},
 'terminal-stack-0822.json':{'run_stack.py':'run_stack_0825.py'},
 'shell-review-0822.json':{'run_review.py':'run_review_0825.py'},
 'terminal-worker-0822.json':{'run_terminal_worker.py':'run_terminal_worker_0825.py'},
 'system-paths-0824.json':{'run.py':'run_0825.py'},
 'git-stack-0822.json':{'run_stack_audit.py':'run_stack_audit_0825.py'},
 'git-vendor-compile-0822.json':{'compile_vendor.py':'compile_vendor_0825.py'},
}
HARNESS_PATHS_0826={
 **{name:{before:after.replace('0825','0826') for before,after in mapping.items()}
    for name,mapping in HARNESS_PATHS.items() if name!='system-paths-0824.json'},
 **{name:{'tools/sdr-tests':'tools/sdr-tests-0826'} for name in
    ('sdr-worker-0824.json','sdr-ui-0824.json','sdr-usb-0824.json')},
 'git-tls-lifecycle-0822.json':{'run_tls_lifecycle.py':'run_tls_lifecycle_0826.py','tls_lifecycle.cpp':'tls_lifecycle_0826.cpp'},
 'git-init-oom-0822.json':{'run_init_oom.py':'run_init_oom_0826.py'},
 'git-config-oom-0822.json':{'run_config_oom.py':'run_config_oom_0826.py'},
 'git-no-thread-tls-0822.json':{'run_no_thread_tls.py':'run_no_thread_tls_0826.py'},
 'git-https-0822.json':{'https_remote.py':'https_remote_0826.py','run.py':'run_0826.py',
                        'libgit2-preflight-0822':'libgit2-preflight-0826'},
 'directory-picker-0822.json':{'run.py':'run_0826.py'},
 'reader-layout-0822.json':{'run_layout.py':'run_layout_0826.py'},
 'reader-seek-0822.json':{'run_seek.py':'run_seek_0826.py'},
 'map-session-0824.json':{'run_session.py':'run_session_0826.py'},
 'wifi-transitions-0822.json':{'tools/wifi-transition-tests':'tools/wifi-transition-tests-0826'},
 'calculator-0822.json':{'tools/calculator-tests':'tools/calculator-tests-0826'},
 'context-menu-0823.json':{'tools/context-menu-tests':'tools/context-menu-tests-0826'},
 'consoles-ui-0823.json':{'tools/context-menu-tests':'tools/context-menu-tests-0826'},
 'app-panel-ui-0823.json':{'tools/pentest-elf-tests':'tools/app-panel-ui-tests-0826'},
 'memory-target-stack-0822.json':{'tools/pentest-tests':'tools/memory-stack-tests-0826'},
 'network-gate-0822.json':{'tools/network-gate-tests':'tools/network-gate-tests-0826'},
 'task-diagnostics-0822.json':{'tools/pentest-tests':'tools/continuity-tests-0826'},
 'share-admission-0822.json':{'tools/pentest-tests':'tools/continuity-tests-0826'},
 'system-paths-0824.json':{'tools/system-path-tests':'tools/system-path-tests-0826','run.py':'run_0826.py'},
 'settings-recovery-0824.json':{'tools/system-migration-tests':'tools/system-migration-tests-0826'},
 'quota-reservation-0822.json':{'run_reservation.py':'run_reservation_0826.py'},
 'elf-lifecycle-0822.json':{'run_elf_lifecycle.py':'run_elf_lifecycle_0826.py'},
 'elf-resources-0823.json':{'run_elf_resources.py':'run_elf_resources_0826.py'},
 'elf-api5-0823.json':{'run_elf_api5.py':'run_elf_api5_0826.py'},
 'git-close-baseline-0823.json':{'run_git_view.py':'run_git_view_0826.py'},
 'explorer-0823.json':{},
}
def check_with_current_harness(function,name,*args):
    # Only explicitly versioned harness paths change. The complete original
    # function, assertions, minima and hashing operations execute unchanged.
    source=inspect.getsource(function).replace('M5Writer/','System/')
    source=source.replace('r"Tab5OS_(?:Arduino|Ecriture)_v(?!0\\.8\\.22)[0-9]"',
                          'r"Tab5OS_(?:Arduino|Ecriture)_v(?!"+re.escape(VERSION)+r")[0-9]"')
    origin=args[0].get('version',args[0].get('release'))
    current=origin in ('0.8.26',VERSION)
    mapping=dict(HARNESS_PATHS_0826.get(name,HARNESS_PATHS.get(name,{})) if current else HARNESS_PATHS.get(name,{}))
    if origin==VERSION:mapping.update(current_additions.HARNESS_PATHS.get(name,{}))
    for before,after in mapping.items():
        source=source.replace('"'+before+'"','"'+after+'"').replace("'"+before+"'","'"+after+"'")
        source=source.replace('/'+before+'"','/'+after+'"').replace('/'+before+"'",'/'+after+"'")
        source=source.replace(before+'/',after+'/')
    if current and name=='sdr-usb-0824.json':
        # The same scenarios remain mandatory. A new bounds assertion adds
        # one check; compare the recorded execution count, keeping >= 20080.
        source=source.replace("'PASS 20080 SDR USB adapter checks; 2000 complete lifecycles;'",
                              "('PASS '+str(raw['checks'])+' SDR USB adapter checks; 2000 complete lifecycles;')")
    if current and name=='git-view-0822.json':
        source=source.replace('{"git-view-0826.cpp", "run_git_view_0826.py"}',
                              '{"git-view-0826.cpp", "run_git_view_0826.py", "focus_os_0826.inc"}')
    if current and name=='git-https-0822.json':
        source=source.replace('build/libgit2-preflight-0822/', 'build/libgit2-preflight-0826/')
    namespace=dict(function.__globals__)
    exec(compile(source,'<explicit .25 harness path adaptation>','exec'),namespace)
    if current and name=='explorer-0823.json':
        args=(additions.checked_explorer_extension(args[0],args[1],args[2]),*args[1:])
    return namespace[function.__name__](name,*args)
class ReportDirectory:
    def __init__(self,release):self.release=release
    def __truediv__(self,name):return selected_report_path(self.release,name)

def load_contracts(release,history=None):
    history=read_json(release/'tools/verification/history-0824/index.json')
    path=release/'tools/verify_features_0824.py'
    require(digest(path)==history['verifierSha256'],'Historical .24 validation contracts changed')
    spec=importlib.util.spec_from_file_location('tab5_contracts_0824',path)
    old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
    old.VERSION=VERSION;old.relocate_path=relocate_path;old.relocated=relocated
    ledger=release/'tools/verification/history-0823/index.json'
    require(digest(ledger)==old.HISTORY_SHA256,'Historical .23 evidence index changed')
    previous=read_json(ledger)
    contracts,rules=old.load_contracts(release,previous)
    contracts.relocate_path=relocate_path;contracts.relocated=relocated
    rules.SYNTAX_UNITS.update({'Tab5Vault.cpp','Tab5KeyStore.cpp','Tab5Environment.cpp'})
    return old,contracts,rules

def checked_new(name,raw,release,sketch,contracts):
    if name in current_additions.NEW_REPORTS:
        return current_additions.checked_new(name,raw,release,sketch,contracts)
    if name in additions.NEW_REPORTS:
        return current_additions.checked_previous(name,raw,release,sketch,contracts)
    if name!='help-0825.json':
        # Run every original .25 assertion. Current reports adapt only their
        # explicit version and the names of separately versioned test files.
        source=inspect.getsource(previous_additions.checked_new)
        if raw.get('version') in ('0.8.26',VERSION):
            source=source.replace("'0.8.25'",repr(raw['version']))
            mapping=dict(additions.PREVIOUS_CONTRACT_PATHS.get(name,{}))
            if raw.get('version')==VERSION:mapping.update(current_additions.PREVIOUS_CONTRACT_PATHS.get(name,{}))
            for before,after in mapping.items():
                source=source.replace(before,after)
            if name=='settings-layout-0825.json':
                raw=additions.checked_settings_extension(raw,release,sketch)
            if name=='credential-ui-0825.json' and raw.get('version')==VERSION:
                raw=current_additions.checked_credential_extension(raw,release,sketch)
            if name in ('vault-0825.json','credential-ui-0825.json'):
                additions.checked_vault_extension(name,raw,release,sketch)
        namespace=dict(previous_additions.__dict__)
        exec(compile(source,'<preserved .25 assertions, explicit .26 paths>','exec'),namespace)
        return namespace['checked_new'](name,raw,release,sketch,contracts)
    if raw.get('version')==VERSION:
        return current_additions.checked_help(raw,release,sketch)
    if raw.get('version')=='0.8.26':
        return additions.checked_help(raw,release,sketch)
    require(raw.get('version')=='0.8.25' and raw.get('passed') is True and raw.get('physicalDeviceTested') is False,'Historical help evidence failed or scope missing')
    require(raw.get('sourceSha256')==digest(sketch/'Tab5Help.cpp'),'Embedded help changed after audit')
    folder=release/'tools/localization';snapshot=folder/'help-before-0825.json'
    require(raw.get('snapshotSha256')==digest(snapshot),'Pre-edit help snapshot changed')
    required={'help-west-0825.json','files-help-east-0825.json','sdr-quality-east-0825.json','vault-help-west-0825.json','vault-help-east-0825.json','delta-help-0825.json'}
    contracts.hashes(folder,raw['sources'],required)
    titles=['Fichiers et sauvegardes','Radio SDR','Coffre et récupération']
    require(raw.get('changedPages')==titles and raw.get('translatedLanguages')==14 and raw.get('newDictionaryKeys')==3,'Current help inventory incomplete')
    require(raw.get('unchangedParagraphsPreservedExactlyPerLanguage')==dict(zip(titles,[6,3,2])),'Help preservation counts changed')
    pattern=r'\{\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\}'
    actual={json.loads(title):json.loads(body) for title,body in re.findall(pattern,(sketch/'Tab5Help.cpp').read_text(encoding='utf8'))}
    delta=read_json(folder/'delta-help-0825.json');original=read_json(snapshot)
    require(set(delta)==previous_additions.LANGUAGES,'Help language coverage incomplete')
    for locale,values in delta.items():
        pack=read_json(sketch/'sdcard/System/languages'/(locale+'.json'))['translations']
        for title in titles:
            body=actual[title];require(values.get(body)==pack.get(body) and bool(values.get(body)),'Missing translated help: '+locale+'/'+title)
            before=original[title]['translations'][locale].split('\n\n');after=values[body].split('\n\n')
            if title==titles[0]:require(before[1:]==after[1:-1],'Unchanged file help paragraphs modified')
            elif title==titles[1]:require(before==after[:-1],'Unchanged SDR help paragraphs modified')
            else:require(before[1]==after[1] and before[3]==after[3],'Unchanged vault legacy/file help paragraphs modified')

def collect_evidence(release):
    release=release.resolve();sketch=release/f'Tab5OS_Arduino_v{VERSION}_Ecriture';reports=release/'tools/verification'
    require(release.name==f'Tab5OS_Ecriture_v{VERSION}' and sketch.is_dir(),'Wrong release root')
    ledger=reports/'history-0826/index.json'
    require(digest(ledger)==HISTORY_SHA256,'Historical .26 evidence index changed')
    history=read_json(ledger);require(history['baselineVersion']=='0.8.26','Wrong historical baseline')
    previous_ledger=reports/'history-0825/index.json'
    require(digest(previous_ledger)=='68e7ad2b73bfcf46a5968010f7f8151e841dd14df8578b5eeddccdc319324568',
            'Historical .25 evidence index changed')
    for name,expected in read_json(previous_ledger)['contractSha256'].items():
        require(digest(release/'tools'/name)==expected,'Historical .25 contract changed: '+name)
    for name,expected in history['contractSha256'].items():
        require(digest(release/'tools'/name)==expected,'Historical .26 contract changed: '+name)
    for name,row in history['files'].items():
        path=safe_path(reports,row['preservedAs'])
        require(path.is_file() and path.stat().st_size==row['bytes'] and digest(path)==row['sha256'],
                'Historical evidence bytes changed: '+name)
    old,contracts,rules=load_contracts(release,history)
    sources=rules.source_manifest(sketch);dependencies=rules.dependency_manifest(sketch);verifier=digest(Path(__file__))
    additional_rules=digest(release/'tools/feature_contracts_0827.py')
    selected=ReportDirectory(release);checks=[];errors=[]
    report_hashes={'history-0826/index.json':HISTORY_SHA256,
                  'history-0825/index.json':digest(reports/'history-0825/index.json'),
                  'history-0824/index.json':digest(reports/'history-0824/index.json'),
                  'history-0823/index.json':old.HISTORY_SHA256,'history-0822/index.json':contracts.HISTORY_SHA256}
    # Preserve baseline identities used by reproduced regressions. Build report
    # basenames are intentionally not globally pinned: actual .25 builds replace
    # those generic delivery reports after verification.
    for name in ('gamepad-0823.json','gamepad-pipeline-0823.json','wifi-transitions-0823.json'):
        expected=history['reports'][name]
        require(digest(reports/name)==expected,'Historical baseline identity changed: '+name)
        report_hashes[name]=expected
    base_names=[n for n in (*rules.CURRENT_REPORTS,*rules.HISTORICAL_REPORTS) if n not in contracts.REPLACED]
    def load(name):
        path=selected/name;key=path.name;sha=digest(path);raw=read_json(path);current=key.endswith('-0827.json')
        if current:
            require(raw.get('releaseVersion',raw.get('version',raw.get('release')))==VERSION,'Current report must identify .27: '+key)
            require(sha not in history['reports'].values(),'Old report copied as current: '+key)
        else:require(history['reports'].get(key)==sha,'Historical report differs from .26 archive: '+key)
        report_hashes[key]=sha
        return key,raw,current
    for name in (*base_names,*contracts.EXTRA_REPORTS,*old.NEW_REPORTS,*NEW_REPORTS):
        key=(selected/name).name;current=key.endswith('-0827.json')
        try:
            key,raw,current=load(name);origin=raw.get('releaseVersion',raw.get('version',raw.get('release','0.8.22')))
            old.VERSION=origin;contracts.VERSION=origin;rules.VERSION=origin
            original_hash=rules.HISTORICAL_REPORTS.pop(name,None) if key!=name else None
            try:
                if name in NEW_REPORTS:checked_new(name,relocated(raw),release,sketch,contracts)
                elif name in old.NEW_REPORTS:
                    if origin>='0.8.25' and name in (HARNESS_PATHS.keys()|HARNESS_PATHS_0826.keys()|current_additions.HARNESS_PATHS.keys()):check_with_current_harness(old.checked_new,name,relocated(raw),release,sketch,contracts)
                    else:old.checked_new(name,relocated(raw),release,sketch,contracts)
                elif name=='gamepad-pipeline-0823.json' and origin>='0.8.24':old.checked_pipeline(relocated(raw),release,sketch,contracts)
                elif name in contracts.EXTRA_REPORTS:
                    if origin>='0.8.25' and name in (HARNESS_PATHS.keys()|HARNESS_PATHS_0826.keys()|current_additions.HARNESS_PATHS.keys()):check_with_current_harness(contracts.checked_extra,name,relocated(raw),release,sketch)
                    else:contracts.checked_extra(name,relocated(raw),release,sketch)
                elif name=='gamepad-0822.json' and origin>='0.8.24':old.checked_gamepad(raw,release,sketch,contracts)
                elif name=='gamepad-0822.json' and key!=name:contracts.checked_gamepad(raw,release,sketch)
                elif name=='maps-0822.json' and key!=name:
                    if origin>='0.8.26':
                        source=inspect.getsource(contracts.checked_maps).replace('tools/maps-tests/run.py','tools/maps-tests/run_0826.py')
                        namespace=dict(contracts.__dict__);exec(compile(source,'<explicit .26 map runner path>','exec'),namespace)
                        namespace['checked_maps'](relocated(raw),release,sketch)
                    else:contracts.checked_maps(relocated(raw),release,sketch)
                elif name=='delta-languages-0822.json' and origin>='0.8.24':
                    translated=relocated(raw);translated['deltaFiles']=raw['sources'];translated['newSourceKeys']=raw['newKeys']
                    for row in translated['packs'].values():row['translatedNewKeys']=row['translatedKeys']
                    rules.checked_report(name,translated,release,sketch,selected)
                    if current:
                        inventory=read_json(release/'tools/localization/reference-final-delta-0827.json')
                        require(raw['newKeys']==len(inventory)>0,'Current translation delta does not match reviewed inventory')
                elif origin>='0.8.25' and name in (HARNESS_PATHS.keys()|HARNESS_PATHS_0826.keys()|current_additions.HARNESS_PATHS.keys()):check_with_current_harness(rules.checked_report,name,relocated(raw),release,sketch,selected)
                else:rules.checked_report(name,relocated(raw),release,sketch,selected)
                if name=='wifi-transitions-0822.json' and origin>='0.8.24':
                    folder='tools/wifi-transition-tests-0827/' if current else ('tools/wifi-transition-tests-0826/' if origin>='0.8.26' else 'tools/wifi-transition-tests/')
                    contracts.hashes(release,raw['testSources'],{folder+n for n in ('run.py','run_msvc.cmd','flow.cpp','Platform.h')}|
                                     {'tools/network-gate-tests/Arduino.h','tools/network-gate-tests/freertos/FreeRTOS.h','tools/network-gate-tests/freertos/semphr.h'})
                    previous=read_json(reports/'wifi-transitions-0823.json')
                    require(raw['baseline_0_8_19'].get('sources')==previous['baseline_0_8_19'].get('sources'),'Wi-Fi baseline changed')
                if name=='glyphs-0822.json' and origin>='0.8.24':
                    contracts.hashes(release,raw['testSources'],{'tools/localization/audit_glyphs.py','tools/audit_languages.py'})
                    require({sketch.name+'.ino','Tab5CjkFonts.cpp','Tab5CjkFonts.h','Tab5Localization.cpp','Tab5LanguageMetadata.h','Tab5KeyboardLayouts.cpp','lv_conf.h'}<=set(relocated(raw.get('sources',{}))), 'Glyph source identities missing')
                    tag=origin.replace('.','');names={n+'-'+tag+'.json' for n in ('languages','arabic','keyboard-shortcuts','ime-native','ime')}
                    require(names<=set(raw.get('proofs',[])),'Glyph current evidence references incomplete')
                    native=read_json(reports/('ime-native-'+tag+'.json'));ime=read_json(reports/('ime-'+tag+'.json'))
                    proof=raw.get('currentEvidence',{})
                    require(proof.get('ime-native-'+tag+'.json',{}).get('rerun') is True and proof['ime-native-'+tag+'.json'].get('sourceHashesVerified')==len(native['sources']) and
                      proof.get('ime-'+tag+'.json',{}).get('rerun') is True and proof['ime-'+tag+'.json'].get('assetHashesVerified')==sum(not n.startswith('System/languages/') for n in ime['assets']),'Glyph evidence accounting incomplete')
                if name=='task-diagnostics-0822.json' and current:current_additions.checked_diagnostics(raw,release,sketch)
                if name=='directory-picker-0822.json':rules.check_count(raw,581)
                if name=='git-view-0822.json' and origin>='0.8.26':
                    source=(sketch/(sketch.name+'.ino')).read_text(encoding='utf8')
                    start=source.index('static void setKeyboardVisible(bool visible, lv_obj_t* target) {')
                    keyboard=source[start:source.index('static void setTopActionVisible(',start)]
                    start=source.index('Tab5Focus.inputChanged(');observer=source[start:source.index('});',start)+3]
                    require(hashlib.sha256(keyboard.encode()).hexdigest()==raw.get('mainKeyboardFunctionSha256') and
                            hashlib.sha256(observer.encode()).hexdigest()==raw.get('mainFocusObserverSha256'),
                            'OS keyboard/focus controllers changed after Git UI execution')
                if name=='ui-locale-calls-0822.json' and origin>='0.8.26':
                    contracts.hashes(release/'tools/localization',raw.get('testSources',{}),{'audit_ui_calls.py','ui_producer_keys.py'})
                    require(raw.get('indirectUiProducerCallsites',0)>=100 and {'Ouvrir le dossier courant dans le terminal','Ctrl+Maj+N','Ctrl+Maj+H',"Masquer sur l'accueil",'HTTP : connexion non chiffrée.'}<=set(raw.get('indirectUiProducerKeys',[])),
                            'Indirect menu/shortcut/conditional translation audit incomplete')
                if name=='elf-lifecycle-0822.json' and key!='elf-lifecycle-0822.json':
                    require(raw.get('checks',0)>=25856 and raw.get('audioReleaseContention') is True and raw.get('baseline',{}).get('expectedFailure') is True and raw['baseline'].get('exitCode')==2 and raw['baseline'].get('version')=='0.8.22','ELF audio-release baseline missing')
            finally:
                if original_hash is not None:rules.HISTORICAL_REPORTS[name]=original_hash
            checks.append(dict(report=key,passed=True,historical=not current,originVersion=origin,reusedOnlyAfterSourceAndHarnessHashCheck=not current))
        except (OSError,ValueError,KeyError,TypeError,SyntaxError,AssertionError) as error:
            errors.append(key+': '+str(error));checks.append(dict(report=key,passed=False,historical=not current,error=str(error)))
    if rules.source_manifest(sketch)!=sources:errors.append('Sources changed during verification')
    if rules.dependency_manifest(sketch)!=dependencies:errors.append('Dependencies changed during verification')
    if digest(Path(__file__))!=verifier:errors.append('Verifier changed during verification')
    if digest(release/'tools/feature_contracts_0827.py')!=additional_rules:errors.append('Additional contracts changed during verification')
    for name,expected in report_hashes.items():
        if digest(safe_path(reports,name))!=expected:errors.append('Report changed during verification: '+name)
    return dict(version=VERSION,checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=not errors,
      physicalDeviceTested=False,sources=sources,dependencies=dependencies,verifierSha256=verifier,ruleSetSha256=history['verifierSha256'],additionalContractsSha256=additional_rules,reports=report_hashes,checks=checks,errors=errors,
      replacedEvidence=contracts.REPLACED,baselineArchiveSha256=history['baselineArchiveSha256'],scope='Actual .27 executions plus exact archived .22/.23/.24/.25/.26 reports with matching source and harness hashes. Predecessor contracts remain enforced. No physical Tab5 test or firmware flash.')
def main():
    release=Path(__file__).resolve().parents[1];result=collect_evidence(release)
    target=release/'tools/verification/features-0827.json';temporary=target.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8');temporary.replace(target)
    print(json.dumps({k:result[k] for k in ('passed','errors')},ensure_ascii=False,indent=2));return 0 if result['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
