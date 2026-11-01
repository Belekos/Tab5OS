#!/usr/bin/env python3
"""Exercise real EPUB ownership/lifecycle and compare JPEG/tuner outputs with .52."""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
SKETCH = ROOT / 'Tab5OS_Arduino_v0.8.53_Ecriture'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def extract(source, marker):
    start = source.index(marker)
    pos = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[start:pos]

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    baseline = args.baseline.resolve() / 'Tab5OS_Arduino_v0.8.52_Ecriture'
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    production = ['Tab5EpubPageIndex.h', 'Tab5EpubBookIndex.inc', 'EpubPageIndex.h',
                  'Tab5EpubSeek.inc', 'src/jpeg/jpge.cpp', 'src/jpeg/jpge.h',
                  'src/rtl_sdr/tuner_fc0013.c', 'src/rtl_sdr/tuner_fc0013.h', 'src/rtl_sdr/rtlsdr_i2c.h']
    hashes = {p: sha(SKETCH / p) for p in production}
    assert sha(SKETCH / 'EpubPageIndex.h') == sha(baseline / 'EpubPageIndex.h')
    assert sha(SKETCH / 'Tab5EpubSeek.inc') == sha(baseline / 'Tab5EpubSeek.inc')
    source = (SKETCH / 'Tab5EpubBookIndex.inc').read_text()
    markers = ['static void readerBookIndexStop(', 'static void readerBookIndexStart(',
               'static String readerBookIndexStatusText(']
    (out / 'lifecycle.inc').write_text('\n\n'.join(extract(source, m) for m in markers) + '\n')
    # The gain array has no header declaration or firmware export; only the
    # driver's own implementation references it. No alias is handed to callers.
    references = []
    for path in SKETCH.rglob('*'):
        if path.suffix.lower() in {'.h','.c','.cpp','.inc','.ino'} and 'fc0013_lna_gains' in path.read_text(errors='replace'):
            references.append(path.relative_to(SKETCH).as_posix())
    assert references == ['src/rtl_sdr/tuner_fc0013.c'], references
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    commands = []
    def run(name, command, execute=False):
        r = subprocess.run([str(x) for x in command], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
        (out / (name + '.log')).write_text(r.stdout)
        commands.append({'name':name,'argv':[str(x) for x in command],'exitCode':r.returncode})
        if r.returncode: raise RuntimeError(name + ': ' + r.stdout[-4000:])
        return json.loads(r.stdout.strip()) if execute else None
    flags = ['-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
    run('index-compile', ['g++','-std=c++17',*flags,'-I'+str(HERE/'stubs'),'-I'+str(SKETCH),'-I'+str(out),HERE/'index.cpp','-o',out/'index'])
    index = run('index-run',[out/'index'],True)
    legacy = []
    for label, sketch in [('baseline',baseline),('current',SKETCH)]:
        run('original-index-'+label+'-compile',['g++','-std=c++17',*flags,'-I'+str(sketch),ROOT/'tools/epub-pagination-tests/test_book_index.cpp','-o',out/('original-index-'+label)])
        r = subprocess.run([str(out/('original-index-'+label))], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
        (out/('original-index-'+label+'-run.log')).write_text(r.stdout)
        known = 'g.width == 720 && g.height == 1080 && g.x == 0 && g.y == 111' in r.stdout
        assert r.returncode != 0 and known, 'Unexpected legacy fixture outcome: ' + r.stdout
        legacy.append({'mode':label,'exitCode':r.returncode,'sameObsoleteGeometryAssertion':known})
    results = {}
    for label, sketch in [('baseline',baseline),('current',SKETCH)]:
        run('jpeg-'+label+'-compile',['g++','-std=c++17',*flags,'-I'+str(sketch/'src/jpeg'),HERE/'jpeg.cpp',sketch/'src/jpeg/jpge.cpp','-o',out/('jpeg-'+label)])
        jpeg = run('jpeg-'+label+'-run',[out/('jpeg-'+label),out/('jpeg-'+label+'.bin')],True)
        run('tuner-'+label+'-compile',['gcc','-std=c11',*flags,'-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(sketch/'src/rtl_sdr'),HERE/'tuner.c',sketch/'src/rtl_sdr/tuner_fc0013.c','-o',out/('tuner-'+label)])
        tuner = run('tuner-'+label+'-run',[out/('tuner-'+label)],True)
        results[label] = {'jpeg':jpeg,'jpegBytes':(out/('jpeg-'+label+'.bin')).stat().st_size,
                          'jpegSha256':sha(out/('jpeg-'+label+'.bin')),'tuner':tuner,
                          'sourceHashes':{p:sha(sketch/p) for p in production if (sketch/p).exists()}}
    assert results['baseline']['jpegSha256'] == results['current']['jpegSha256']
    assert results['baseline']['tuner'] == results['current']['tuner']
    assert hashes == {p:sha(SKETCH/p) for p in production}, 'Production changed during tests'
    proof = ROOT / 'tools/verification/sram-0853';proof.mkdir(exist_ok=True)
    for p in [*out.glob('*.log'),out/'lifecycle.inc']:
        shutil.copy2(p,proof/p.name)
    (proof/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    testfiles = [*HERE.rglob('*.h'),*HERE.glob('*.cpp'),*HERE.glob('*.c'),Path(__file__),ROOT/'tools/epub-pagination-tests/test_book_index.cpp']
    report = {
      'version':'0.8.53','passed':True,'physicalDeviceTested':False,
      'scope':'Real EPUB index and extracted UI lifecycle; JPEG byte-for-byte equivalence and tuner I2C traces compared with authenticated delivered .52 sources.',
      'sourceHashes':{(SKETCH/p).relative_to(ROOT).as_posix():h for p,h in hashes.items()},
      'testSourceHashes':{p.relative_to(ROOT).as_posix():sha(p) for p in testfiles},
      'evidenceHashes':{p.relative_to(ROOT).as_posix():sha(p) for p in proof.iterdir() if p.is_file()},
      'sanitizers':{'address':True,'undefined':True,'leakSanitizer':False,
                    'leakSanitizerLimitation':'LSan cannot inspect /proc threads in this execution environment; EPUB heap calls are independently counted and paired.'},
      'index':index,'comparison':results,'tunerTableReferenceFiles':references,
      'historicalFixture':{'file':'tools/epub-pagination-tests/test_book_index.cpp','results':legacy,
        'explanation':'Unmodified historical test expects y=111 for 720x1280, while the byte-identical .52/.53 geometry function returns y=94. Both executions fail at that same stale assertion; no geometry change was made to satisfy it.'},
      'expectedGrossInternalSavingsBytes':2624,
      'expectedPlacement':{'readerBookPages':{'beforeBytes':2052,'afterBytes':4,'region':'internal, pointer only'},
        'jpge::s_std_lum_quant':{'bytes':128,'region':'read-only flash'},
        'jpge::s_std_croma_quant':{'bytes':128,'region':'read-only flash'},
        'jpge::s_alt_quant':{'bytes':128,'region':'read-only flash'},
        'fc0013_lna_gains':{'bytes':192,'region':'read-only flash'}},
      'limitations':['Expected savings must be checked in the final P4 ELF, and other .53 changes can offset the net gain.',
        'No physical SRAM free/minimum/largest-block measurement, no RF tuner or SD hardware test.',
        'EPUB renderer, cache I/O and LVGL deletion are stub boundaries in the lifecycle fixture; the index and three lifecycle functions are production code.',
        'The index falls back to a bounded internal allocation only if PSRAM allocation fails; both regions unavailable leaves reading open and reports pagination memory failure, retrying only on explicit reopen/relayout.']}
    target = ROOT/'tools/verification/sram-0853.json'
    target.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'passed':True,'index':index,'jpegCases':results['current']['jpeg']['cases'],'jpegIdentical':True,'tunerCases':results['current']['tuner']['cases'],'tunerIdentical':True}))

if __name__ == '__main__':
    main()
