#!/usr/bin/env python3
"""Bind heterogeneous 0.8.47 reports to current inputs; never re-label execution.

Original reports remain immutable. These wrappers validate their recorded hashes
and preserve their limits. The main integration wrapper distinguishes compilation,
executed callback fragments, and structural source inspection.
"""
import argparse
import ast
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from release_config import SKETCH, VERSION, TAG
S = ROOT/SKETCH
V = ROOT/'tools/verification'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
digest = lambda s: hashlib.sha256(s.encode()).hexdigest()


def read(name):
    p = V/(name+'-'+TAG+'.json')
    d = json.loads(p.read_text())
    assert d.get('passed') is True, p
    return p, d


def checked(hashes):
    for name, expected in hashes.items():
        p = (ROOT/name).resolve()
        assert p.is_relative_to(ROOT) and p.is_file() and sha(p) == expected, name
    return hashes


def leaf(hashes):
    return checked({SKETCH+'/'+n: h for n, h in hashes.items()})


def wrapper(name, sources, evidence=None, **details):
    p, d = read(name)
    tests = checked({**d.get('testHashes', {}), **d.get('testSourceHashes', {})})
    tests[Path(__file__).relative_to(ROOT).as_posix()] = sha(Path(__file__))
    evidence = checked(evidence or {})
    evidence[p.relative_to(ROOT).as_posix()] = sha(p)
    output = {'version': VERSION, 'passed': True, 'physicalDeviceTested': False,
              'normalizationOnly': True, 'originalReport': p.relative_to(ROOT).as_posix(),
              'sourceHashes': checked(sources), 'testSourceHashes': tests,
              'evidenceHashes': evidence,
              'normalizerSha256': sha(Path(__file__)), **details}
    out = V/('normalized-'+name+'-'+TAG+'.json')
    out.write_text(json.dumps(output, indent=2)+'\n')
    return out


def extract_function(source, name):
    # Use exactly the extraction function whose output the Media suite hashed.
    script = ROOT/'tools/media-ui-tests-0847/run.py'
    parsed = ast.parse(script.read_text())
    function = next(n for n in parsed.body if isinstance(n, ast.FunctionDef) and n.name == 'function')
    module = ast.Module(body=[function], type_ignores=[])
    context = {'re': re}
    exec(compile(ast.fix_missing_locations(module), str(script), 'exec'), context)
    return context['function'](source, name)


def media_fragments(main, hashes):
    actual = {}
    for name in hashes:
        if name == 'updateAudioUi-catalogue-prefix':
            fragment = extract_function(main, 'updateAudioUi').split('  static uint32_t seen = 0;')[0]+'\n}\n'
        elif name == 'tab-construction':
            create = extract_function(main, 'createAudioPage')
            blocks = []
            for tab in ('audioLocalTab', 'audioRadioTab', 'audioRecordTab', 'audioVideoTab'):
                start = create.index('  '+tab+' =')
                end = create.index('\n', create.index('lv_obj_add_event_cb('+tab, start))
                blocks.append(create[start:end])
            fragment = '\n'.join(blocks)
        else:
            fragment = extract_function(main, name)
        actual[name] = digest(fragment)
    assert actual == hashes, 'Media tested callback differs from final main'
    return actual


def block(source, start):
    i = source.index(start)
    begin = source.index('{', i)
    depth = 0
    for j in range(begin, len(source)):
        if source[j] == '{': depth += 1
        elif source[j] == '}':
            depth -= 1
            if not depth: return source[i:j+1]
    raise ValueError(start)


def run(environment, include_main):
    _, d = read('media-persistence')
    assert d['sourcesUnchanged'] and d['byteCompatibleVersion1']
    wrapper('media-persistence', leaf(d['sources']),
            scope='Original actual production MediaSession host execution, controlled NVS/scheduler boundaries; wrapper only checks source identity.')
    _, d = read('media-ui')
    mainpath = S/(SKETCH+'.ino')
    main = mainpath.read_text()
    fragments = media_fragments(main, d['verifiedMainFragmentSha256'])
    wrapper('media-ui', leaf(d['sourceSha256']), verifiedMainFragmentSha256=fragments,
            scope='Original real-LVGL host execution and selected main callbacks; no whole-main execution claim.')
    _, d = read('media-ui-size')
    wrapper('media-ui-size', leaf({'Tab5MediaList.h': d['headerSha256']['current']}),
            scope='Original target sizeof probe, not runtime heap measurement.')
    _, d = read('display-blit-independent')
    evidence = {'tools/audit-0847/review_display_blit.cpp': d['fixtureSha256']}
    wrapper('display-blit-independent', leaf(d['sourceSha256']), evidence,
            scope=d['scope'])
    _, d = read('resource-race-independent')
    wrapper('resource-race-independent', checked(d['sourceHashes']), scope=d['scope'])

    helper = ROOT/'tools/build_linux/prepare_m5unified_audio.py'
    spec = importlib.util.spec_from_file_location('speaker_patch', helper)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    original = environment/'sketchbook/libraries/M5Unified'
    patched = {n: hashlib.sha256(b).hexdigest() for n, b in module.patched_files(original).items()}
    recipe = {helper.relative_to(ROOT).as_posix(): sha(helper)}
    for name in ('audio-startup', 'audio-independent-review'):
        _, d = read(name)
        assert d['patchedHashes'] == patched, 'Stale patched methods: '+name
        if 'upstreamHashes' in d: assert d['upstreamHashes'] == module.EXPECTED
        evidence = {}
        if name == 'audio-independent-review':
            assert d['sourceMethodsMatch']
            for filename, expected in d['fixtureHashes'].items():
                p = V/'audio-independent-review-0847'/filename
                assert p.is_file() and sha(p) == expected, p
                evidence[p.relative_to(ROOT).as_posix()] = expected
        wrapper(name, recipe, evidence, patchedMethodHashes=patched,
                scope=d['scope'], limitation='Host adapters cannot prove acoustic output, interrupt scheduling or physical codec behavior.')
    _, d = read('audio-sdk-target')
    assert d['before']['compilerExit'] == d['after']['compilerExit'] == 0
    assert {n: d['after']['hashes'][n] for n in patched} == patched
    assert {n: d['before']['hashes'][n] for n in module.EXPECTED} == module.EXPECTED
    assert sha(original/'src/M5Unified.cpp') == d['before']['hashes']['src/M5Unified.cpp'] == d['after']['hashes']['src/M5Unified.cpp']
    evidence = {d[k]['log']: d[k]['logSha256'] for k in ('before', 'after')}
    wrapper('audio-sdk-target', recipe, evidence, scope=d['scope'], patchedMethodHashes=patched)

    if not include_main: return
    qa = environment/'qa'
    fwpath = qa/('firmware-verification-'+TAG+'.json')
    if not fwpath.exists(): fwpath = qa/'firmware-verification.json'
    fw = json.loads(fwpath.read_text())
    assert fw['passed'] and fw['version'] == VERSION and fw['sourcesUnchanged']
    manifest = json.loads((qa/'source-manifest-final.json').read_text())
    hashes = {f['path']: f['sha256'] for f in manifest['files']}
    assert hashes[mainpath.name] == sha(mainpath), 'Main differs from compiled source'
    # Final deliverable copies are also made by prepare_delivery; copying now
    # lets scope validation bind main to the exact complete firmware proof.
    finalfw = V/('firmware-verification-'+TAG+'.json')
    shutil.copyfile(fwpath, finalfw)
    compiledmanifest = V/('compiled-source-manifest-'+TAG+'.json')
    shutil.copyfile(qa/'source-manifest-final.json', compiledmanifest)
    displaypath, display = read('display')
    flush = block(main, 'static void displayFlush(')
    assert digest(flush) == display['productionFlushSha256']
    # Structural inspection: these assertions are NOT behavioral tests.
    setup = main[main.index('void setup() {'):]
    assert setup.index('Tab5MemoryManager.begin();') < setup.index('Tab5DebugSerial.begin(')
    assert 'if(!Tab5MediaSession::flush(millis()))return Tab5MediaSession::saving()' in main
    assert 'return Tab5Writing.reclaimHiddenIndexes();' in block(main, 'static void registerResourceCaches()')
    assert '[MEDIA PREFS] started=' in main and 'Tab5MediaSession::snapshot()' in main
    references = [displaypath, V/'media-ui-0847.json', V/'memory-0847.json',
                  V/'media-persistence-0847.json', V/'writing-cache-0847.json', finalfw, compiledmanifest]
    for name in ('memory', 'media-persistence', 'writing-cache'): read(name)
    report = {'version': VERSION, 'passed': True, 'physicalDeviceTested': False,
              'sourceHashes': {mainpath.relative_to(ROOT).as_posix(): sha(mainpath)},
              'testSourceHashes': {Path(__file__).relative_to(ROOT).as_posix(): sha(Path(__file__))},
              'evidenceHashes': {p.relative_to(ROOT).as_posix(): sha(p) for p in references},
              'compiledMainMatches': True, 'productionDisplayFlushSha256': digest(flush),
              'testedMediaFragmentSha256': fragments,
              'structuralSourceInspection': ['Memory admission initialized before device setup',
                 'Deep sleep waits for preference worker completion through existing retry UI',
                 'Hidden syntax indexes registered as PSRAM-reclaimable cache',
                 'Media persistence diagnostics exposed without unconditional sampling loop'],
              'scope': 'Full firmware compile/link of exact main; selected Display/Media callbacks executed by referenced host suites. Other integration sites are structural source checks and parent manual review, not whole-main execution.',
              'limitations': ['No physical board tested; boot ordering, live FreeRTOS scheduling, peripheral timing and end-to-end UI remain hardware validation.']}
    (V/('main-integration-'+TAG+'.json')).write_text(json.dumps(report, indent=2)+'\n')
    flashpath, flash = read('flash-context')
    assert flash['linkedVerificationExecuted']
    elf = next(a for a in fw['artifacts'] if a['suffix'] == '.elf')
    assert flash['elfSha256'] == elf['sha256']
    wrapper('flash-context', {'tools/audit-0847/verify_flash_context.py': sha(ROOT/'tools/audit-0847/verify_flash_context.py')},
            {finalfw.relative_to(ROOT).as_posix(): sha(finalfw)},
            scope='Actual linked ISR placement and pinned SDK configuration inspection; no hardware execution.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', type=Path, required=True)
    parser.add_argument('--include-main', action='store_true')
    args = parser.parse_args()
    run(args.environment.resolve(), args.include_main)
    print(json.dumps({'passed': True, 'normalizationOnly': True, 'mainIntegration': args.include_main}))
