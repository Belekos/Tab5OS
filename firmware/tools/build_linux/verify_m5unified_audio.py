"""Verify exact isolated Speaker/codec source and final firmware selection."""
from pathlib import Path
import hashlib
import json
import subprocess
from prepare_m5unified_audio import EXPECTED, patched_files


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check(environment, linked_map, elf):
    env = Path(environment).resolve()
    qa, build = env/'qa', env/'build'
    metadata_path = qa/'firmware/m5unified-audio-patch.json'
    metadata = json.loads(metadata_path.read_text())
    source = Path(metadata['source']).resolve()
    isolated = Path(metadata['isolatedCopy']).resolve()
    if source == isolated or metadata['version'] != '0.2.23':
        raise RuntimeError('Unexpected M5Unified source/copy identity')
    patches = patched_files(source)
    original = {p.relative_to(source).as_posix(): digest(p)
                for p in source.rglob('*') if p.is_file()}
    expected = dict(original)
    for name, data in patches.items():
        expected[name] = hashlib.sha256(data).hexdigest()
    current = {p.relative_to(isolated).as_posix(): digest(p)
               for p in isolated.rglob('*') if p.is_file()}
    if current != expected:
        raise RuntimeError('Isolated M5Unified contains unexpected changes')
    if {r['file']: (r['originalSha256'], r['patchedSha256'])
        for r in metadata['files']} != {n: (EXPECTED[n], expected[n]) for n in patches}:
        raise RuntimeError('M5Unified patch metadata does not match reviewed source')
    bindings, evidence = [], {}
    # M5Unified 0.2.23 compiles all utility implementations through one .cpp.
    for filename, included in [('M5Unified.cpp', ('src/M5Unified.inl',
                                                'src/utility/Speaker_Class.inl'))]:
        found = list(build.rglob(filename+'.d'))
        if len(found) != 1:
            raise RuntimeError('Expected one M5Unified dependency file: '+filename)
        dep = found[0]
        text = dep.read_text().replace('\\\n', ' ')
        actual_source = isolated/('src/' if filename == 'M5Unified.cpp' else 'src/utility/')/filename
        if str(actual_source) not in text or any(str(isolated/name) not in text for name in included):
            raise RuntimeError('M5Unified compiler input does not select isolated patch: '+filename)
        obj = dep.with_suffix('.o')
        if not obj.is_file() or 'libraries/M5Unified/objs.a('+filename+'.o)' not in linked_map:
            raise RuntimeError('Patched M5Unified object is not linked: '+filename)
        name = 'm5unified-'+filename+'.d'
        (qa/name).write_bytes(dep.read_bytes())
        evidence[name] = digest(qa/name)
        bindings.append({'source': filename, 'dependencySha256': digest(dep),
                         'objectSha256': digest(obj), 'patchedInclude': included})
    users, stale = 0, []
    for dep in build.rglob('*.d'):
        if dep.name.endswith('.libsdetect.d'):
            continue
        text = dep.read_text()
        if str(source/'src')+'/' in text:
            stale.append(str(dep.relative_to(build)))
        if str(isolated/'src')+'/' in text:
            users += 1
    if stale:
        raise RuntimeError('Unpatched M5Unified source/headers still compiled: '+repr(stale))
    nm = env/'data/packages/m5stack/tools/esp-rv32/2601/bin/riscv32-esp-elf-nm'
    symbols = subprocess.check_output([str(nm), '-C', '--defined-only', str(elf)], text=True)
    required = ['m5::Speaker_Class::_begin_raw()',
                'm5::Speaker_Class::stopAndWait(unsigned char)',
                'm5::M5Unified::_speaker_enabled_cb_tab5(void*, bool)']
    for name in required:
        if sum(line.endswith(' '+name) for line in symbols.splitlines()) != 1:
            raise RuntimeError('Missing or duplicated linked audio startup method: '+name)
    evidence['firmware/m5unified-audio-patch.json'] = digest(metadata_path)
    return ({'passed': True, 'patch': metadata, 'filesChecked': len(current),
             'compiledBindings': bindings, 'dependencyUsers': users,
             'unpatchedDependencyUsers': stale, 'linkedMethods': required,
             'scope': 'Pinned source/copy and compiler/link selection only; no physical pop measurement.'},
            evidence)
