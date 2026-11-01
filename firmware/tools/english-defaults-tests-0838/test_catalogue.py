import builtins, hashlib, importlib.util, json, os, pathlib, sys, tempfile, time, types
sys.dont_write_bytecode = True
BASE = pathlib.Path(sys.argv[1])
for n in ('tab5library.py', 'tab5libraryui.py', 'tab5apps.py', 'tab5fs.py', 'mip.py'):
    compile((BASE / n).read_text(encoding='utf8'), n, 'exec')
time.sleep_ms = lambda milliseconds: None
sys.modules['_tab5'] = types.SimpleNamespace(call=lambda *a: None)
def load(name):
    spec = importlib.util.spec_from_file_location(name, BASE / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module
fs = load('tab5fs')
with tempfile.TemporaryDirectory() as temp:
    base = pathlib.Path(temp)
    def path(value):
        assert value.startswith('/')
        return base / value.lstrip('/')
    def vopen(value, *args, **kwargs):
        return builtins.open(path(value), *args, **kwargs)
    fs.open = vopen
    fs.stat = lambda value: (path(value).stat().st_mode, path(value).stat().st_size)
    fs.mkdir = lambda value: path(value).mkdir()
    fs.remove = lambda value: path(value).unlink()
    fs.rmdir = lambda value: path(value).rmdir()
    fs.rename = lambda source, target: path(source).rename(path(target))
    handles = [0, 0]
    def entries(value):
        handles[0] += 1
        handles[1] = max(handles[1], handles[0])
        assert handles[0] <= 8
        try:
            yield from os.listdir(path(value))
        finally:
            handles[0] -= 1
    fs.ilistdir = entries
    fs.sha256 = lambda value: hashlib.sha256(path(value).read_bytes()).hexdigest()
    lib = load('tab5library'); lib.open = vopen
    mip = load('mip'); mip.open = vopen
    def write(value, data):
        path(value).parent.mkdir(parents=True, exist_ok=True)
        path(value).write_bytes(data if isinstance(data, bytes) else data.encode())
    def failure(call, message):
        try:
            call()
        except (ValueError, OSError) as error:
            print('PASS', message, ':', error)
        else:
            raise AssertionError(message)
    write('/Inbox/sensor/__init__.py', 'raise RuntimeError("Do not execute during import")\n')
    write('/Inbox/sensor/i2c.py', '# driver for another card\n')
    write('/Inbox/sensor/README.md', '# Wiring details\n')
    write('/Inbox/sensor/.git/ignore', 'ignored')
    rec = lib.import_local('/Inbox/sensor', 'Temperature', 'rp2040', 'capteurs')
    assert rec['path'] == '/MicroPython/Libraries/rp2040/sensors/temperature'
    assert rec['category'] == 'sensors'
    assert path(rec['path'] + '/lib/sensor/__init__.py').exists()
    assert not path(rec['path'] + '/lib/sensor/.git').exists()
    assert len(lib.catalogue('rp2040', 'capteurs')) == 1
    assert lib.catalogue('esp32') == []
    assert 'files' not in lib.catalogue()[0]
    folder = lib.create_project('Station Pico', 'rp2040')
    assert folder == '/MicroPython/Projects/station_pico'
    assert not path('/MicroPython/Projets').exists()
    assert not path('/MicroPython/Bibliotheques').exists()
    assert lib.attach(rec, folder) == 3
    assert lib.attach(rec, folder) == 0
    assert not path('/MicroPython/lib/sensor').exists()
    print('PASS isolated package import, project creation, idempotent attach; no execution')
    write(folder + '/lib/sensor/i2c.py', '# user modified file')
    failure(lambda: lib.attach(rec, folder), 'existing project source kept')
    assert path(folder + '/lib/sensor/i2c.py').read_text() == '# user modified file'
    write(rec['path'] + '/lib/sensor/i2c.py', '# modified catalogue')
    failure(lambda: lib.attach(rec, folder), 'changed catalogue rejected')
    write('/Inbox/native.mpy', bytes([77, 6, 40, 31, 0]))
    mpyrec = lib.import_local('/Inbox/native.mpy', 'Precompiled', 'esp32', 'general')
    assert mpyrec['formats']['mpy'] == 1 and mpyrec['compatibility'] == 'declared'
    target = lib.create_project('Precompiled firmware project', 'esp32')
    assert lib.attach(mpyrec, target) == 1
    print('PASS .mpy preserved without execution or compatibility claim')
    failure(lambda: lib.import_local('/Inbox/native.mpy', 'Precompiled', 'esp32', 'general'), 'duplicate does not replace previous entry')
    write('/Inbox/bad.mpy', b'BAD')
    failure(lambda: lib.import_local('/Inbox/bad.mpy', 'Bad', 'esp32'), 'invalid .mpy header')
    failure(lambda: lib.project('/MicroPython/lib', 'tab5-p4'), 'system folders cannot become project')
    failure(lambda: lib.create_project('../', 'esp32'), 'invalid project name')
    failure(lambda: lib.project('//MicroPython//lib/', 'tab5-p4'), 'repeated separators cannot bypass system folder protection')
    write('/USB/project/source.py', '# existing USB file')
    failure(lambda: lib.project('//USB//project/', 'rp2040'), 'USB target rejected before SD transaction')
    failure(lambda: lib.attach(rec, '/USB/project'), 'USB attach rejected before any copy')
    assert not path('/USB/project/.tab5-project.json').exists()
    assert not path('/USB/project/lib').exists()
    assert not path(fs.STAGE).exists()
    usb = lib.import_local('/USB/project/source.py', 'USB source', 'rp2040')
    assert path(usb['path'] + '/lib/source.py').exists()
    assert lib.project('//' + target.strip('/').replace('/', '//'))['board'] == 'esp32'
    print('PASS USB import retained, SD-only projects, normalized project paths')
    # Network resolver includes dependency sources, but never touches P4 global lib.
    urls = {
        mip.INDEX + '/package/py/test/latest.json': {'urls': [['sensor.py', 'https://test.invalid/sensor.py']], 'deps': [['helper', None]], 'version': '1'},
        mip.INDEX + '/package/py/helper/latest.json': {'urls': [['helper.py', 'https://test.invalid/helper.py']], 'version': '2'},
        'https://test.invalid/sensor.py': 'raise RuntimeError("not executed")\n',
        'https://test.invalid/helper.py': 'answer = 42\n',
    }
    def download(url, destination, limit=262144, progress=None):
        data = urls[url]
        raw = json.dumps(data).encode() if isinstance(data, dict) else data.encode()
        assert len(raw) <= limit
        with vopen(destination, 'xb') as f:
            f.write(raw)
        return len(raw)
    fs.download = download
    online = lib.import_online('test', 'Online test', 'esp32-c3', 'general')
    assert online['path'] == '/MicroPython/Libraries/esp32-c3/general/online_test'
    assert path(online['path'] + '/lib/helper.py').exists()
    assert not path('/MicroPython/lib/helper.py').exists()
    assert not path(mip.MANIFEST).exists()
    target = lib.create_project('ESP32 network', 'esp32-c3')
    assert lib.attach(online, target) == 2
    print('PASS mip dependency graph isolated and copied offline into project')
    # Protected module is allowed as foreign source but cannot replace system module.
    urls['https://test.invalid/machine.py'] = 'foreign machine support = 1'
    foreign = lib.import_online('https://test.invalid/machine.py', 'Foreign machine', 'stm32', 'general')
    assert path(foreign['path'] + '/lib/machine.py').exists()
    failure(lambda: mip.install('https://test.invalid/machine.py'), 'native module protection retained')
    assert not path(fs.STAGE).exists()
    # Failure writes a partial manifest, simulating network and SD interruption.
    def broken(url, destination, *args):
        with vopen(destination, 'xb') as f:
            f.write(b'partial')
        raise OSError(5004)
    fs.download = broken
    failure(lambda: lib.import_online('broken', 'Broken', 'esp32', 'general'), 'download failure rolls back private staging')
    assert not path(fs.STAGE).exists()
    assert not path(lib.ROOT + '/esp32/general/broken').exists()
    for scratch in ('manifest0', 'manifest12', 'payload'):
        write(fs.STAGE + '/' + scratch, 'interrupted')
    fs.recover()
    assert not path(fs.STAGE).exists()
    print('PASS stale mip temporary files no longer block Atelier recovery')
    # Interrupted commit rollback restores all prior state.
    old_rename = fs.rename
    count = [0]
    def fault_rename(source, destination):
        if destination.startswith('/MicroPython/Projects/rollback/') and '/new' in source:
            count[0] += 1
            if count[0] == 2:
                raise OSError(5)
        return old_rename(source, destination)
    fs.rename = fault_rename
    failure(lambda: lib.create_project('rollback', 'esp32'), 'SD interruption during commit rolls back')
    fs.rename = old_rename
    assert not path('/MicroPython/Projects/rollback/main.py').exists()
    assert not path(fs.STAGE).exists()
    assert lib.create_project('rollback', 'esp32').endswith('/rollback')
    write('/Deep/' + '/'.join('d' + str(i) for i in range(8)) + '/module.py', '# deep tree')
    lib.import_local('/Deep', 'Deep', 'portable')
    assert handles[0] == 0 and handles[1] <= 1
    print('PASS bounded directory handles and retry after rollback')
    # Old SD contents remain readable at their exact paths. Reads do not migrate
    # or rewrite metadata, including a prior French board/category value.
    legacy = '/MicroPython/Bibliotheques/autre/capteurs/legacy'
    legacy_project = '/MicroPython/Projets/Existing user folder'
    payload = b'# preserved legacy payload\n'
    write(legacy + '/lib/legacy.py', payload)
    write(legacy + '/metadata.json', json.dumps({
        'schema': 1, 'name': 'Legacy', 'board': 'autre', 'category': 'capteurs',
        'dependencies': 'not-verified', 'files': {'legacy.py': hashlib.sha256(payload).hexdigest()}}))
    write(legacy_project + '/main.py', '# existing user project\n')
    write(legacy_project + '/.tab5-project.json', json.dumps({
        'schema': 1, 'name': 'Existing user folder', 'board': 'autre', 'libraries': []}))
    def snapshot():
        return {str(p.relative_to(base)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in base.rglob('*') if p.is_file()}
    before = snapshot()
    record = lib.catalogue('other', 'sensors')[0]
    assert record['path'] == legacy and record['board'] == 'other' and record['category'] == 'sensors'
    assert lib.catalogue('autre', 'capteurs') == [record]
    assert lib.project(legacy_project)['board'] == 'other'
    assert snapshot() == before, 'Legacy reads must leave every byte unchanged'
    failure(lambda: lib.project(legacy, 'portable'), 'legacy system catalogue protected')
    assert lib.attach(record, legacy_project) == 1
    assert path(legacy_project + '/lib/legacy.py').read_bytes() == payload
    assert path(legacy_project + '/main.py').read_text() == '# existing user project\n'
    assert path(legacy).exists() and not path('/MicroPython/Projects/Existing user folder').exists()
    print('PASS legacy metadata/catalogue reads are byte-preserving; explicit attach keeps existing path')
    # Callers using former category identifiers still create English default paths.
    for old, new in lib.CATEGORY_ALIASES.items():
        assert lib._location('user name', 'autre', old) == '/MicroPython/Libraries/other/' + new + '/user_name'
    new_project = lib.create_project('New user project', 'autre')
    assert new_project == '/MicroPython/Projects/new_user_project'
    assert lib.project(new_project)['board'] == 'other'
    # An explicitly supplied legacy mip destination remains supported without
    # becoming the destination chosen by new imports.
    fs.download = download
    mip.install('test', target='/MicroPython/Bibliotheques/esp32/general/explicit/lib')
    assert path('/MicroPython/Bibliotheques/esp32/general/explicit/lib/helper.py').exists()
    assert handles[0] == 0 and handles[1] <= 1
    print('PASS canonical English defaults, explicit legacy destinations, bounded directory handles')
    # Localized labels remain separate from storage IDs, including old metadata.
    starts = []
    sys.modules['tab5ui'] = types.SimpleNamespace(tr=lambda value: 'translated:' + value)
    sys.modules['tab5gui'] = types.SimpleNamespace(choose=lambda *args: 'existing',
        pick=lambda start, folder=False: starts.append(start) or legacy_project)
    ui = load('tab5libraryui')
    assert ui.board_label('autre') == ui.board_label('other') == 'translated:Autre carte'
    assert ui.category_label('capteurs') == ui.category_label('sensors') == 'translated:Capteurs'
    exists = fs.exists
    for available, expected_start in (({lib.PROJECTS, lib.LEGACY_PROJECTS}, lib.PROJECTS),
                                      ({lib.LEGACY_PROJECTS}, lib.LEGACY_PROJECTS),
                                      (set(), '/MicroPython')):
        fs.exists = lambda value: value in available if value in (lib.PROJECTS, lib.LEGACY_PROJECTS) else exists(value)
        assert ui.pick_project() == legacy_project
        assert starts[-1] == expected_start
    fs.exists = exists
    print('PASS localized labels and project picker defaults preserve selected legacy paths')
    print('PASS all catalogue and English-default regressions')
