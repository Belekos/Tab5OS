"""Offline MicroPython source catalogue. No imported package is executed here.
Catalogue files never enter sys.path. Compatibility is a user declaration.
"""
import tab5fs as fs
from time import sleep_ms

ROOT = '/MicroPython/Libraries'
PROJECTS = '/MicroPython/Projects'
# Existing SD catalogues/projects are read in place, never moved or overwritten.
LEGACY_ROOT = '/MicroPython/Bibliotheques'
LEGACY_PROJECTS = '/MicroPython/Projets'
ROOTS = (ROOT, LEGACY_ROOT)
BOARDS = ('portable', 'tab5-p4', 'esp32', 'esp32-s3', 'esp32-c3', 'esp32-c6',
          'esp32-p4', 'esp8266', 'rp2040', 'rp2350', 'stm32', 'other')
CATEGORIES = ('general', 'sensors', 'display', 'network', 'storage', 'motors', 'audio')
BOARD_ALIASES = {'autre': 'other'}
CATEGORY_ALIASES = {'capteurs': 'sensors', 'ecran': 'display', 'reseau': 'network', 'stockage': 'storage', 'moteurs': 'motors'}
MAX_FILES = 160
MAX_TOTAL = 8 * 1024 * 1024
MAX_FILE = 1024 * 1024


def slug(value):
    value = str(value).strip()
    if not value or len(value) > 80 or any(ord(c) < 32 for c in value):
        raise ValueError('Nom requis, 80 caractères maximum')
    key = ''.join(c.lower() if c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_' else '_' for c in value)[:40].strip('_-')
    if not key:
        raise ValueError('Le nom doit contenir au moins une lettre ou un chiffre ASCII')
    return key


def _board(board):
    board = BOARD_ALIASES.get(board, board)
    if board not in BOARDS:
        raise ValueError('Choisir une famille de carte')
    return board


def _category(category):
    category = CATEGORY_ALIASES.get(category, category)
    if category not in CATEGORIES:
        raise ValueError('Catégorie invalide')
    return category


def _location(name, board, category):
    return ROOT + '/' + _board(board) + '/' + _category(category) + '/' + slug(name)


def _note(progress, text):
    if progress:
        progress.text(text[-140:])
    sleep_ms(1)


def _copy(source, destination, progress=None):
    def tick(done, total):
        # Yield between every 4 KiB SD transfer; don't flood the UI message queue.
        if progress and (done == total or done % 65536 == 0):
            from tab5ui import tr
            progress.text(str(done) + ' / ' + str(total) + ' ' + tr('octets'))
        sleep_ms(1)
    return fs.copy(source, destination, tick)


def _vacant(folder):
    # A failed SD commit may leave only empty parent directories. Reusing them
    # is safe; any file, including a hidden one, prevents an accidental replace.
    if not fs.exists(folder):
        return True
    if not fs.isdir(folder):
        return False
    pending = [(folder, 0)]
    visited = 0
    while pending:
        current, depth = pending.pop()
        if depth > 8:
            return False
        entries = fs.ilistdir(current)
        try:
            for leaf in entries:
                visited += 1
                if visited > 2048 or len(pending) >= 256:
                    return False
                child = fs.path(current + '/' + fs.relative(leaf))
                if not fs.isdir(child):
                    return False
                pending.append((child, depth+1))
                sleep_ms(1)
        finally:
            close = getattr(entries, 'close', None)
            if close:
                close()

    return True


def _scan(directory, progress=None):
    """Bound traversal before copying; hold just one of eight native dir slots."""
    result = []
    seen = set()
    total = visited = 0
    pending = [(directory, '', 0)]
    while pending:
        folder, prefix, depth = pending.pop()
        if depth > 8:
            raise ValueError('Dossier trop profond : 8 niveaux maximum')
        entries = fs.ilistdir(folder)
        try:
            for leaf in entries:
                visited += 1
                if visited > 2048:
                    raise ValueError('Dossier trop grand : choisir le sous-dossier de la bibliothèque')
                if leaf.startswith('.') or leaf in ('__pycache__', 'node_modules'):
                    continue
                relative = fs.relative(prefix + leaf)
                full = fs.path(folder + '/' + leaf)
                if fs.isdir(full):
                    if len(pending) >= 256:
                        raise ValueError('Trop de sous-dossiers : choisir le dossier de la bibliothèque')
                    pending.append((full, relative + '/', depth+1))
                else:
                    size = fs.stat(full)[1]
                    if size > MAX_FILE:
                        raise ValueError('Fichier supérieur à 1 Mio : ' + relative)
                    if relative.lower() in seen:
                        raise ValueError('Noms incompatibles avec FAT : ' + relative)
                    total += size
                    if len(result) >= MAX_FILES or total > MAX_TOTAL:
                        raise ValueError('Limite : 160 fichiers et 8 Mio par bibliothèque')
                    seen.add(relative.lower())
                    result.append((relative, full, size))
                if visited % 16 == 0:
                    if progress:
                        from tab5ui import tr
                        _note(progress, tr('Analyse : ') + str(visited) + ' ' + tr('éléments'))
                    else:
                        sleep_ms(1)
        finally:
            close = getattr(entries, 'close', None)
            if close:
                close()

    return result


def _formats(files):
    py = mpy = 0
    headers = []
    for relative, source, size in files:
        if relative.lower().endswith('.py'):
            py += 1
        elif relative.lower().endswith('.mpy'):
            with open(source, 'rb') as f:
                head = f.read(4)
            if len(head) != 4 or head[0] != 77:
                raise ValueError('En-tête .mpy invalide : ' + relative)
            code = '-'.join(str(b) for b in head)
            if code not in headers:
                headers.append(code)
            mpy += 1
    if not py and not mpy:
        raise ValueError('Aucun module .py ou .mpy dans la sélection')
    return {'py': py, 'mpy': mpy, 'mpy_headers': headers}


def _metadata(name, board, category, source, formats):
    return {'schema': 1, 'name': str(name).strip(), 'board': _board(board),
            'category': _category(category), 'source': source, 'compatibility': 'declared',
            'formats': formats, 'dependencies': 'not-verified'}


def import_local(source, name, board='portable', category='general', progress=None):
    """Copy one module, a package folder, or the contents of a lib folder."""
    source = fs.path(source)
    dest = _location(name, board, category)
    if not _vacant(dest):
        raise ValueError('Nom déjà présent pour cette cible et catégorie : choisir un autre nom')
    if fs.isdir(source):
        if source == '/':
            raise ValueError('Choisir le dossier de la bibliothèque, pas la racine SD')
        files = _scan(source, progress)
        # A Python package folder retains its name; a lib bundle retains its contents.
        if fs.exists(source + '/__init__.py') or fs.exists(source + '/__init__.mpy'):
            prefix = source.rsplit('/', 1)[-1] + '/'
            files = [(fs.relative(prefix + r), s, n) for r, s, n in files]
    else:
        if not source.lower().endswith(('.py', '.mpy')):
            raise ValueError('Choisir un module .py, .mpy ou un dossier décompressé')
        size = fs.stat(source)[1]
        if size > MAX_FILE:
            raise ValueError('Fichier supérieur à 1 Mio')
        files = [(source.rsplit('/', 1)[-1], source, size)]
    formats = _formats(files)
    meta = _metadata(name, board, category, source, formats)
    digests = {}
    with fs.Transaction() as tx:
        for relative, full, size in files:
            _note(progress, relative)
            staged = tx.allocate(dest + '/lib/' + relative)
            _copy(full, staged, progress)
            digests[relative] = fs.sha256(staged)
        meta['files'] = digests
        tx.json(dest + '/metadata.json', meta)
        tx.commit()
    meta['path'] = dest
    return meta


def import_online(source, name, board, category, version=None, progress=None):
    """Source-only mip resolution into an isolated catalogue, including dependencies."""
    import mip
    dest = _location(name, board, category)
    if not _vacant(dest):
        raise ValueError('Nom déjà présent : choisir un autre nom')
    meta = _metadata(name, board, category, source, {'py': None, 'mpy': 0})
    meta['dependencies'] = 'mip-resolved'
    mip.install(source, version=version, target=dest + '/lib', progress=progress, metadata=meta)
    meta['path'] = dest
    return meta


def _catalogue_folders(parent, names):
    # Enumerate only existing categories; do not stat every new/legacy pair.
    # Materialize the bounded names before opening the next directory level.
    found = []
    entries = fs.ilistdir(parent)
    try:
        for leaf in entries:
            if leaf in names and fs.isdir(parent + '/' + leaf):
                found.append(leaf)
    finally:
        close = getattr(entries, 'close', None)
        if close:
            close()
    return found


def catalogue(board=None, category=None):
    """Read metadata in new and legacy roots without migrating payloads."""
    records = []
    board = _board(board) if board else None
    category = _category(category) if category else None
    for root in ROOTS:
        if not fs.exists(root):
            continue
        for b in _catalogue_folders(root, BOARDS + tuple(BOARD_ALIASES)):
            canonical_board = _board(b)
            if board and canonical_board != board:
                continue
            for cat in _catalogue_folders(root + '/' + b, CATEGORIES + tuple(CATEGORY_ALIASES)):
                canonical_category = _category(cat)
                if category and canonical_category != category:
                    continue
                folder = root + '/' + b + '/' + cat
                entries = fs.ilistdir(folder)
                try:
                    for leaf in entries:
                        fs.relative(leaf)
                        path = folder + '/' + leaf
                        if not fs.isdir(path) or not fs.exists(path + '/metadata.json'):
                            continue
                        meta = fs.read_json(path + '/metadata.json', 65536)
                        if (not isinstance(meta, dict) or meta.get('schema') != 1 or
                                _board(meta.get('board')) != canonical_board or
                                _category(meta.get('category')) != canonical_category):
                            raise ValueError('Métadonnées de bibliothèque invalides : ' + path)
                        meta.pop('files', None)
                        meta['board'] = canonical_board
                        meta['category'] = canonical_category
                        meta['path'] = path
                        records.append(meta)
                        if len(records) > 256:
                            raise ValueError('Filtrer par carte ou catégorie (plus de 256 bibliothèques)')
                        sleep_ms(1)
                finally:
                    close = getattr(entries, 'close', None)
                    if close:
                        close()

    return sorted(records, key=lambda r: str(r.get('name', '')).lower())


def create_project(name, board):
    folder = PROJECTS + '/' + slug(name)
    board = _board(board)
    if not _vacant(folder):
        raise ValueError('Un projet de ce nom existe déjà')
    data = {'schema': 1, 'name': name, 'board': board, 'libraries': []}
    code = '# Projet MicroPython : ' + str(name).replace('\n', ' ') + '\n# Cible déclarée : ' + board + '\n# Modifier ce fichier dans Écriture. Le transfert sur la carte reste distinct.\n\n'
    with fs.Transaction() as tx:
        tx.data(folder + '/main.py', code.encode())
        tx.json(folder + '/.tab5-project.json', data)
        tx.commit()
    return folder


def _project_path(folder):
    # All project writes use the SD journal. USB remains an import source only.
    folder = '/' + '/'.join(part for part in fs.path(folder).split('/') if part)
    low = folder.lower()
    if low == '/usb' or low.startswith('/usb/'):
        raise ValueError('Choisir un projet sur la SD. L’USB peut servir de source à importer.')
    return folder


def project(folder, board=None):
    folder = _project_path(folder)
    # A project must remain separate from OS modules and the catalogue.
    low = folder.lower()
    if low in ('/', '/micropython') or any(low == p or low.startswith(p + '/') for p in ('/micropython/lib', ROOT.lower(), LEGACY_ROOT.lower(), '/factory', '/m5writer')):
        raise ValueError('Choisir un dossier de projet distinct des fichiers système')
    if not fs.isdir(folder):
        raise ValueError('Dossier de projet absent')
    path = folder + '/.tab5-project.json'
    if fs.exists(path):
        data = fs.read_json(path, 32768)
        if not isinstance(data, dict) or data.get('schema') != 1 or not isinstance(data.get('libraries'), list):
            raise ValueError('Fiche projet invalide')
        data['board'] = _board(data.get('board'))
        return data
    if board is None:
        return None
    board = _board(board)
    data = {'schema': 1, 'name': folder.rsplit('/', 1)[-1], 'board': board, 'libraries': []}
    with fs.Transaction() as tx:
        tx.json(path, data)
        tx.commit()
    return data


def attach(record, folder, progress=None):
    """Add files to project/lib atomically. Never replace different existing bytes."""
    folder = _project_path(folder)
    data = project(folder)
    if data is None:
        raise ValueError('Choisir d’abord la cible du projet')
    source = fs.path(record['path'])
    if not any(source.startswith(root + '/') for root in ROOTS):
        raise ValueError('Bibliothèque hors catalogue')
    # Re-read trusted layout metadata; do not trust the stale UI object.
    meta = fs.read_json(source + '/metadata.json', 65536)
    expected = meta.get('files', {})
    if meta.get('dependencies') == 'mip-resolved':
        state = fs.read_json(source + '/packages.json')
        for package in state.get('packages', {}).values():
            for relative, digest in package.get('files', {}).items():
                if relative in expected and expected[relative] != digest:
                    raise ValueError('Registre de bibliothèque incohérent')
                expected[relative] = digest
    if not isinstance(expected, dict) or not expected:
        raise ValueError('Aucun fichier répertorié dans cette bibliothèque')
    files = _scan(source + '/lib', progress)
    if len(files) != len(expected):
        raise ValueError('Catalogue modifié : importer à nouveau sous un autre nom')
    planned = []
    for relative, full, size in files:
        digest = fs.sha256(full)
        if expected.get(relative) != digest:
            raise ValueError('Source du catalogue modifiée : ' + relative)
        dest = fs.path(folder + '/lib/' + relative)
        if fs.exists(dest):
            if fs.isdir(dest) or fs.sha256(dest) != digest:
                raise ValueError('Conflit : fichier du projet conservé : ' + relative)
        else:
            planned.append((full, dest, digest))
    libraries = data['libraries']
    if source not in libraries:
        if len(libraries) >= 64:
            raise ValueError('Limite : 64 bibliothèques par projet')
        libraries.append(source)
    with fs.Transaction() as tx:
        for full, dest, digest in planned:
            staged = tx.allocate(dest)
            _copy(full, staged, progress)
            if fs.sha256(staged) != digest:
                raise ValueError('La source a changé pendant la copie')
        tx.json(folder + '/.tab5-project.json', data)
        tx.commit()
    return len(planned)
