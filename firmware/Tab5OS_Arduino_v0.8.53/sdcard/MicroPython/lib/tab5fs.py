"""Tab5OS SD services. Paths are volume-absolute (/USB is the removable USB volume); transfers are streamed in 4 KiB blocks."""
from _tab5 import call
import struct
import json
from time import sleep_ms


def path(value):
    if not isinstance(value, str) or not value.startswith('/') or len(value.encode()) > 260:
        raise ValueError('Chemin SD absolu requis')
    if any(part in ('.', '..') for part in value.split('/')) or any(c in value for c in ('\\', ':', '\x00')):
        raise ValueError('Chemin invalide')
    return value.rstrip('/') or '/'


def relative(value):
    if not isinstance(value, str) or not value or value.startswith('/') or '\\' in value:
        raise ValueError('Chemin relatif invalide')
    for part in value.split('/'):
        if not part or part in ('.', '..') or part.endswith((' ', '.')) or any(ord(c) < 32 or c in '<>:"|?*' for c in part):
            raise ValueError('Nom de fichier invalide')
    if len(value.encode()) > 200:
        raise ValueError('Chemin trop long')
    return value


def reconnect():
    """At a safe point only: native code refuses if any Python file/iterator is open."""
    return call(98)


def stat(name):
    usb = name == '/USB' or name.startswith('/USB/')
    name = path(name).encode()
    for attempt in range(3):
        try:
            return struct.unpack('<IQ', call(60, 0, 0, 0, name, 12))
        except OSError as error:
            if usb or error.args[0] not in (5, 6, 19, 110) or attempt == 2:
                raise
            try:
                reconnect()
            except OSError:
                raise error


def exists(name):
    try:
        stat(name)
        return True
    except OSError as e:
        if e.args[0] == 2:
            return False
        raise


def isdir(name):
    return (stat(name)[0] & 0xf000) == 0x4000


def mkdir(name):
    call(61, 0, 0, 0, path(name).encode())


def makedirs(name):
    current = ''
    for part in path(name).split('/'):
        if not part:
            continue
        current += '/' + part
        if not exists(current):
            mkdir(current)
        elif not isdir(current):
            raise ValueError('Un fichier remplace le dossier')


def remove(name):
    call(62, 0, 0, 0, path(name).encode())


def rmdir(name):
    call(62, 1, 0, 0, path(name).encode())


def rename(source, destination):
    call(63, 0, 0, 0, path(source).encode() + b'\0' + path(destination).encode())


def ilistdir(name):
    handle = call(64, 0, 0, 0, path(name).encode())
    try:
        while True:
            entry = call(65, handle, 0, 0, None, 262)
            if not entry:
                break
            yield entry.decode()
    finally:
        call(66, handle)


def rmtree(name, depth=0):
    # Only callers' staging directories, never invoked by a file chooser.
    if depth > 16:
        raise ValueError('Arborescence trop profonde')
    for entry in ilistdir(name):
        child = name + '/' + entry
        if isdir(child):
            rmtree(child, depth+1)
        else:
            remove(child)
    rmdir(name)


def sha256(name):
    return call(69, 0, 0, 0, path(name).encode(), 64).decode()


def copy(source, destination, progress=None):
    total = stat(source)[1]
    done = 0
    with open(source, 'rb') as src:
        with open(destination, 'xb') as dst:
            while True:
                data = src.read(4096)
                if not data:
                    break
                if dst.write(data) != len(data):
                    raise OSError(28)
                done += len(data)
                if progress:
                    progress(done, total)
            dst.flush()
    if done != total:
        raise OSError(5)
    return done


def read_json(name, limit=262144):
    if stat(name)[1] > limit:
        raise ValueError('Fichier trop volumineux')
    with open(name) as f:
        return json.load(f)


def write_json(name, data):
    with open(name, 'x') as f:
        json.dump(data, f)
        f.flush()


def download(url, destination, limit=262144, progress=None):
    if not url.startswith('https://'):
        raise ValueError('Adresse HTTPS requise')
    widget = getattr(progress, 'id', 0) if progress else 0
    for attempt in range(4):
        try:
            return call(80, limit, widget, 0, url.encode() + b'\0' + path(destination).encode())
        except OSError as error:
            code = error.args[0]
            if attempt == 3 or code not in (5, 6, 19, 110, 5002, 5003, 5004, 1429, 1500, 1502, 1503, 1504):
                raise
            if code in (5, 6, 19):
                if destination == '/USB' or destination.startswith('/USB/'):
                    raise
                try:
                    reconnect()
                except OSError:
                    raise error
            sleep_ms((400, 1200, 2500)[attempt])



STAGE = '/MicroPython/.atelier-transaction'


def _stage_names():
    names=list(ilistdir(STAGE))
    for leaf in names:
        numbered=(leaf.startswith('new') or leaf.startswith('old')) and leaf[3:].isdigit() and int(leaf[3:])<192
        scratch = leaf == 'payload' or (leaf.startswith('manifest') and leaf[8:].isdigit() and int(leaf[8:]) <= 12)
        if (leaf not in ('journal.json','journal.prepare','committed') and not numbered and not scratch) or isdir(STAGE+'/'+leaf):
            raise ValueError('Récupération interrompue : fichier inconnu dans le journal')
    return names


def _journal():
    entries=read_json(STAGE+'/journal.json')
    if not isinstance(entries,list) or len(entries)>192:
        raise ValueError('Journal de récupération invalide')
    targets=[]
    for i,e in enumerate(entries):
        if not isinstance(e,list) or len(e) not in (5,6):
            raise ValueError('Journal de récupération invalide')
        target,staged,backup,old,digest=e[:5]
        path(target)
        if target.lower().startswith(STAGE.lower()+'/') or target.lower()==STAGE.lower() or target.lower() in targets:
            raise ValueError('Destination de récupération invalide')
        targets.append(target.lower())
        if backup!=STAGE+'/old'+str(i) or staged not in (None,STAGE+'/new'+str(i)) or type(old) is not bool:
            raise ValueError('Journal de récupération invalide')
        for h in (digest, e[5] if len(e)==6 else None):
            if h is not None and (not isinstance(h,str) or len(h)!=64 or any(c not in '0123456789abcdef' for c in h)):
                raise ValueError('Empreinte de récupération invalide')
        if old!=(digest is not None) or (exists(backup) and (not old or sha256(backup)!=digest)):
            raise ValueError('Sauvegarde de récupération invalide')
    return entries


def _clean_stage():
    names=_stage_names()
    for leaf in names:
        if leaf.startswith(('new','old','manifest')) or leaf == 'payload':
            remove(STAGE+'/'+leaf)
    for leaf in ('journal.prepare','journal.json'):
        if leaf in names:
            remove(STAGE+'/'+leaf)
    if 'committed' in names:
        remove(STAGE+'/committed')
    rmdir(STAGE)


def recover():
    """Validate the entire private journal before any rollback or cleanup."""
    if not exists(STAGE):
        return False
    names=_stage_names()
    if 'journal.json' not in names:
        if any(n.startswith('old') for n in names):
            raise ValueError('Journal absent : sauvegardes conservées')
        _clean_stage()
        return True
    entries=_journal()
    if 'committed' not in names:
        actions=[]
        for e in reversed(entries):
            target,staged,backup,old,digest=e[:5]
            new_digest=e[5] if len(e)==6 else None
            if exists(backup):
                if exists(target) and (not new_digest or (staged and exists(staged)) or sha256(target)!=new_digest):
                    raise ValueError('Fichier modifié : récupération manuelle nécessaire')
                actions.append((target,backup))
            elif not old and staged and not exists(staged) and exists(target):
                if not new_digest or sha256(target)!=new_digest:
                    raise ValueError('Fichier modifié : récupération manuelle nécessaire')
                actions.append((target,None))
            elif old and (not exists(target) or sha256(target)!=digest):
                raise ValueError('Sauvegarde absente : récupération manuelle nécessaire')
        for target,backup in actions:
            if exists(target):
                remove(target)
            if backup:
                rename(backup,target)
    _clean_stage()
    return True


class Transaction:
    """Prepare files, then commit with a write-ahead journal and rollback.
    One Python worker owns the journal. No installed code runs while staging.
    """
    def __init__(self):
        recover()
        makedirs(STAGE)
        self.entries = []
        self.done = False

    def allocate(self, target, delete=False):
        target = path(target)
        if target.lower()==STAGE.lower() or target.lower().startswith(STAGE.lower()+'/'):
            raise ValueError('Destination réservée au journal')
        if any(e[0].lower() == target.lower() for e in self.entries):
            raise ValueError('Destination en double')
        if len(self.entries) >= 192:
            raise ValueError('Trop de fichiers dans cette opération')
        old = exists(target)
        if old and isdir(target):
            raise ValueError('La destination est un dossier')
        index = str(len(self.entries))
        staged = None if delete else STAGE + '/new' + index
        self.entries.append([target, staged, STAGE + '/old' + index, old, sha256(target) if old else None])
        return staged

    def data(self, target, data):
        staged = self.allocate(target)
        with open(staged, 'xb') as f:
            if f.write(data) != len(data):
                raise OSError(28)
            f.flush()

    def json(self, target, data):
        write_json(self.allocate(target), data)

    def commit(self):
        for target, staged, backup, old, digest in self.entries:
            if exists(target) != old or (old and sha256(target) != digest):
                raise ValueError('Fichier modifié pendant la préparation')
            if staged and not exists(staged):
                raise ValueError('Préparation incomplète')
            makedirs(target.rsplit('/', 1)[0] or '/')
        for entry in self.entries:
            entry.append(sha256(entry[1]) if entry[1] else None)
        write_json(STAGE + '/journal.prepare', self.entries)
        rename(STAGE + '/journal.prepare', STAGE + '/journal.json')
        try:
            for target, staged, backup, old, digest, new_digest in self.entries:
                if old:
                    rename(target, backup)
                if staged:
                    rename(staged, target)
            with open(STAGE + '/committed', 'x') as f:
                f.write('1')
                f.flush()
            self.done = True
        except BaseException:
            recover()
            raise
        # Once the marker exists, a cleanup failure must never roll back new files.
        _clean_stage()

    def abort(self):
        if exists(STAGE):
            recover()

    def __enter__(self):
        return self

    def __exit__(self, kind, error, trace):
        if not self.done:
            self.abort()
