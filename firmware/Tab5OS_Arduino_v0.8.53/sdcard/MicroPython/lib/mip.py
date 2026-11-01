"""Tab5OS package manager: mip source manifests, verified HTTPS, SD transactions.
install/update/uninstall/list. CPython wheels and native extensions are unsupported.
"""
import json
from builtins import list as _list
import tab5fs as fs

INDEX = 'https://micropython.org/pi/v2'
MANIFEST = '/System/python-packages.json'
TARGET = '/MicroPython/lib/'
CATALOG = '/System/python-catalog.json'
PROTECTED = ('machine', 'time', 'tab5', 'tab5ui', 'tab5fs', 'mip', 'tab5workshop', 'tab5library', 'tab5libraryui', 'tab5apps', 'tab5gui', 'tab5maps', '_tab5', 'builtins', 'json', 'sys', 'gc', 'struct', 'math')
MAX_PACKAGES = 32
MAX_FILES = 128
MAX_SOURCE = 262144
MAX_TOTAL = 4*1024*1024


def _token(value):
    if not value or not all(c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.' for c in value) or value in ('.', '..'):
        raise ValueError('Nom ou version de paquet invalide')
    return value


def _url(source, version=None):
    for prefix, host in (('github:', 'https://raw.githubusercontent.com/'), ('gitlab:', 'https://gitlab.com/')):
        if source.startswith(prefix):
            parts = source[len(prefix):].split('/')
            if len(parts) < 2:
                raise ValueError('Adresse de dépôt invalide')
            owner, repo = _token(parts[0]), _token(parts[1])
            branch = _token(version or 'HEAD')
            tail = '/'.join(parts[2:]) or 'package.json'
            fs.relative(tail)
            if not tail.endswith(('.json', '.py')):
                tail += '/package.json'
            if prefix == 'github:':
                return host + owner + '/' + repo + '/' + branch + '/' + tail
            return host + owner + '/' + repo + '/-/raw/' + branch + '/' + tail
    if not source.startswith('https://'):
        raise ValueError('Adresse HTTPS requise')
    if not source.endswith(('.json', '.py')):
        source = source.rstrip('/') + '/package.json'
    return source


def _join(base, value):
    if value.startswith(('https://', 'github:', 'gitlab:')):
        return _url(value)
    fs.relative(value)
    return base.rsplit('/', 1)[0] + '/' + value


def _state(manifest=MANIFEST):
    if not fs.exists(manifest):
        return {'version': 2, 'packages': {}}
    doc = fs.read_json(manifest)
    if not isinstance(doc, dict) or not isinstance(doc.get('packages'), dict):
        raise ValueError('Registre des bibliothèques invalide')
    # The PC 0.7.0 registry had different ownership semantics; never guess owners.
    if doc.get('version') != 2 and doc['packages']:
        raise ValueError('Ancien registre PC : conserver les bibliothèques et utiliser un nouveau registre')
    return doc


def list():
    """Return name/source/version/files/deps records, without changing the SD."""
    return sorted(_state()['packages'].values(), key=lambda p: p['name'].lower())


def _file(name, protected=True):
    fs.relative(name)
    if not name.endswith('.py'):
        raise ValueError('Seules les sources .py sont prises en charge')
    first = name.split('/')[0].split('.')[0].lower()
    if protected and first in PROTECTED:
        raise ValueError('Module Tab5OS réservé')
    return name


def _check_source(filename):
    # Decode before commit; never execute package code. Modules compile on import.
    with open(filename, 'rb') as f:
        f.read(MAX_SOURCE+1).decode('utf-8')


def install(package, version=None, target=None, mpy=False, progress=None, metadata=None):
    """Copy sources/dependencies to Tab5 lib or an isolated catalogue; never execute them."""
    target = fs.path(target or TARGET)
    local = target == TARGET.rstrip('/')
    manifest = MANIFEST
    if not local:
        # New catalogue writes use Libraries; an explicit legacy target stays valid.
        prefix = next((p for p in ('/MicroPython/Libraries/', '/MicroPython/Bibliotheques/') if target.startswith(p)), None)
        if prefix is None or not target.endswith('/lib'):
            raise ValueError('Destination : bibliothèque Tab5 ou catalogue isolé')
        fs.relative(target[len(prefix):])
        if len(target[len(prefix):].split('/')) != 4:
            raise ValueError('Destination du catalogue invalide')
        manifest = target.rsplit('/', 1)[0] + '/packages.json'
    elif metadata is not None:
        raise ValueError('Métadonnées réservées au catalogue')
    target += '/'
    if mpy:
        raise ValueError('Utiliser mpy=False pour les sources .py')
    state = _state(manifest)
    old_packages = state['packages']
    resolved, files, visiting, versions = {}, {}, set(), {}
    total = [0]
    with fs.Transaction() as tx:
        def fetch(url, filename, limit):
            fs.download(url, filename, limit, progress)

        def resolve(source, requested=None, depth=0):
            if depth > 12:
                raise ValueError('Dépendances trop profondes')
            key = source
            wanted = requested or 'latest'
            if key in visiting:
                raise ValueError('Dépendance circulaire')
            if key in resolved:
                if versions[key] != wanted:
                    raise ValueError('Versions de dépendances incompatibles')
                return key
            if len(resolved)+len(visiting) >= MAX_PACKAGES:
                raise ValueError('Trop de dépendances')
            visiting.add(key)
            versions[key] = wanted
            name = source
            if source.startswith(('https://', 'github:', 'gitlab:')):
                url = _url(source, requested)
                name = source.split('/')[-1] or source
            else:
                name = _token(source)
                url = INDEX + '/package/py/' + name + '/' + _token(wanted) + '.json'
            if progress:
                progress.text(name[-100:])
            temp = fs.STAGE + '/manifest' + str(depth)
            if fs.exists(temp):
                fs.remove(temp)
            if url.endswith('.py'):
                doc = {'urls': [[url.rsplit('/', 1)[-1], url]], 'version': requested or 'source'}
            else:
                fetch(url, temp, 65536)
                doc = fs.read_json(temp, 65536)
                fs.remove(temp)
            if not isinstance(doc, dict):
                raise ValueError('Manifest de paquet invalide')
            deps = []
            for item in doc.get('deps', []):
                if not isinstance(item, (tuple, _list)) or len(item) != 2:
                    raise ValueError('Dépendance invalide')
                deps.append(resolve(item[0], item[1], depth+1))
            own = {}
            items = []
            for destination, digest in doc.get('hashes', []):
                if not isinstance(digest, str) or not 8 <= len(digest) <= 64 or any(c not in '0123456789abcdef' for c in digest):
                    raise ValueError('Empreinte de paquet invalide')
                items.append((destination, INDEX + '/file/' + digest[:2] + '/' + digest, digest))
            for destination, link in doc.get('urls', []):
                items.append((destination, _join(url, link), None))
            if not items and not deps:
                raise ValueError('Paquet vide')
            for destination, link, expected in items:
                name_on_sd = _file(destination, local)
                normalized = name_on_sd.lower()
                if len(files) >= MAX_FILES and normalized not in files:
                    raise ValueError('Trop de fichiers')
                scratch = fs.STAGE + '/payload'
                fetch(link, scratch, MAX_SOURCE)
                digest = fs.sha256(scratch)
                if expected and not digest.startswith(expected):
                    raise ValueError('Empreinte du téléchargement incorrecte')
                _check_source(scratch)
                size = fs.stat(scratch)[1]
                total[0] += size
                if total[0] > MAX_TOTAL:
                    raise ValueError('Paquets trop volumineux')
                if normalized in files:
                    previous_name, previous_digest = files[normalized]
                    if previous_digest != digest or previous_name != name_on_sd:
                        raise ValueError('Deux paquets fournissent des fichiers différents au même endroit')
                    fs.remove(scratch)
                else:
                    dest = target + name_on_sd
                    owners = [record for record in old_packages.values() if name_on_sd in record.get('files', {})]
                    if fs.exists(dest):
                        actual = fs.sha256(dest)
                        if not owners or any(record['files'][name_on_sd] != actual for record in owners):
                            raise ValueError('Fichier personnel ou bibliothèque modifiée : ' + name_on_sd)
                    fs.rename(scratch, tx.allocate(dest))
                    files[normalized] = (name_on_sd, digest)
                own[name_on_sd] = digest
            record = {'name': name, 'source': source, 'version': str(doc.get('version', wanted)), 'requested': wanted, 'files': own, 'deps': deps}
            resolved[key] = record
            visiting.remove(key)
            return key

        resolve(package, version)
        new_packages = dict(old_packages)
        new_packages.update(resolved)
        # Shared ownership must remain consistent with packages outside this operation.
        for record in new_packages.values():
            for filename, digest in record.get('files', {}).items():
                planned = files.get(filename.lower())
                if planned and (planned[0] != filename or planned[1] != digest):
                    raise ValueError('Fichier partagé avec une autre bibliothèque')
        used = set()
        for record in new_packages.values():
            used.update(record.get('files', {}))
        for key in resolved:
            for filename, digest in old_packages.get(key, {}).get('files', {}).items():
                if filename not in used and fs.exists(target+filename):
                    if fs.sha256(target+filename) != digest:
                        raise ValueError('Ancien fichier modifié : ' + filename)
                    tx.allocate(target+filename, delete=True)
        state['version'] = 2
        state['packages'] = new_packages
        tx.json(manifest, state)
        if metadata is not None:
            tx.json(target.rstrip('/').rsplit('/', 1)[0] + '/metadata.json', metadata)
        tx.commit()
    return sorted(resolved.values(), key=lambda p: p['name'])


def update(package, version=None, progress=None):
    """Refresh an installed source; version can select a release/tag."""
    if package not in _state()['packages']:
        raise ValueError('Bibliothèque non installée')
    return install(package, version=version, progress=progress)


def uninstall(package):
    state = _state()
    packages = state['packages']
    if package not in packages:
        raise ValueError('Bibliothèque non installée')
    if any(package in p.get('deps', []) for key, p in packages.items() if key != package):
        raise ValueError('Bibliothèque utilisée par un autre paquet')
    removed = packages.pop(package)
    shared = set()
    for record in packages.values():
        shared.update(record.get('files', {}))
    with fs.Transaction() as tx:
        for filename, digest in removed['files'].items():
            _file(filename)
            full = TARGET+filename
            if filename not in shared and fs.exists(full):
                if fs.sha256(full) != digest:
                    raise ValueError('Fichier modifié : suppression interrompue')
                tx.allocate(full, delete=True)
        tx.json(MANIFEST, state)
        tx.commit()
    return removed


def catalog(refresh=False, progress=None):
    if refresh:
        with fs.Transaction() as tx:
            staged = tx.allocate(CATALOG)
            fs.download(INDEX+'/index.json', staged, 196608, progress)
            doc = fs.read_json(staged, 196608)
            if not isinstance(doc, dict) or not isinstance(doc.get('packages'), _list):
                raise ValueError('Catalogue invalide')
            tx.commit()
    if not fs.exists(CATALOG):
        return []
    return fs.read_json(CATALOG, 196608).get('packages', [])
