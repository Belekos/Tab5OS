import builtins, hashlib, importlib.util, json, os, pathlib, sys, tempfile, time, types
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
    assert path(rec['path'] + '/lib/sensor/__init__.py').exists()
    assert not path(rec['path'] + '/lib/sensor/.git').exists()
    assert len(lib.catalogue('rp2040', 'capteurs')) == 1
    assert lib.catalogue('esp32') == []
    assert 'files' not in lib.catalogue()[0]
    folder = lib.create_project('Station Pico', 'rp2040')
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
        if destination.startswith('/MicroPython/Projets/rollback/') and '/new' in source:
            count[0] += 1
            if count[0] == 2:
                raise OSError(5)
        return old_rename(source, destination)
    fs.rename = fault_rename
    failure(lambda: lib.create_project('rollback', 'esp32'), 'SD interruption during commit rolls back')
    fs.rename = old_rename
    assert not path('/MicroPython/Projets/rollback/main.py').exists()
    assert not path(fs.STAGE).exists()
    assert lib.create_project('rollback', 'esp32').endswith('/rollback')
    write('/Deep/' + '/'.join('d' + str(i) for i in range(8)) + '/module.py', '# deep tree')
    lib.import_local('/Deep', 'Deep', 'portable')
    assert handles[0] == 0 and handles[1] <= 1
    print('PASS bounded directory handles and retry after rollback')
    print('PASS all catalogue regressions')
