"""Complete Kiwix archives, atomically checkpointed as FAT-safe 64 MiB parts."""
import gc
import json
import tab5fs as fs
import tab5wiki as wiki
from _tab5 import call
from time import sleep_ms

PART_SIZE = 67108864
CATALOG = 'https://opds.library.kiwix.org/catalog/v2/entries'
FORMAT = 'tab5-zim-parts-v1'
MAX_CATALOG = 98304


def _xml_text(value):
    if '<' in value or '<!DOCTYPE' in value:
        raise ValueError('Catalogue Wikipédia invalide')
    for source, target in (('&lt;', '<'), ('&gt;', '>'), ('&quot;', '"'), ('&apos;', "'"), ('&amp;', '&')):
        value = value.replace(source, target)
    return value.strip()


def _tag(xml, name, default=''):
    start = xml.find('<' + name + '>')
    if start < 0:
        return default
    start += len(name) + 2
    end = xml.find('</' + name + '>', start)
    if end < 0:
        raise ValueError('Catalogue Wikipédia invalide')
    return _xml_text(xml[start:end])


def _attributes(element):
    result = {}; p = element.find(' ') + 1
    while 0 < p < len(element):
        while p < len(element) and element[p].isspace():
            p += 1
        if p >= len(element) or element[p] in '/>':
            break
        end = element.find('=', p)
        if end < 0:
            raise ValueError('Catalogue Wikipédia invalide')
        name = element[p:end].strip(); p = end + 1
        while p < len(element) and element[p].isspace():
            p += 1
        if p >= len(element) or element[p] not in ('"', "'"):
            raise ValueError('Catalogue Wikipédia invalide')
        quote = element[p]; p += 1
        end = element.find(quote, p)
        if end < 0:
            raise ValueError('Catalogue Wikipédia invalide')
        if name in result:
            raise ValueError('Catalogue Wikipédia invalide')
        result[name] = _xml_text(element[p:end]); p = end + 1
    return result


def _source(url):
    if not isinstance(url, str) or not url.startswith('https://') or len(url) > 1024:
        raise ValueError('Adresse Kiwix invalide')
    host, sep, tail = url[8:].partition('/')
    if host not in ('download.kiwix.org', 'lb.download.kiwix.org') or not sep or not tail.startswith('zim/wikipedia/') or tail.count('/') != 2:
        raise ValueError('Adresse Kiwix invalide')
    if any(ord(c) <= 32 or c in '\\?#' for c in url):
        raise ValueError('Adresse Kiwix invalide')
    return url


def _mirror(url, filename):
    if not isinstance(url, str) or not url.startswith('https://') or len(url) > 1024:
        raise ValueError('Adresse Kiwix invalide')
    host, sep, tail = url[8:].partition('/')
    if not host or not sep or '@' in host or url.rsplit('/', 1)[-1] != filename:
        raise ValueError('Adresse Kiwix invalide')
    if any(ord(c) <= 32 or ord(c) == 127 or c in '\\?#' for c in url):
        raise ValueError('Adresse Kiwix invalide')
    return url


def _filename(name, code):
    prefix = 'wikipedia_' + wiki.language(code) + '_all_'
    if not isinstance(name, str) or not name.startswith(prefix) or not name.endswith('.zim') or len(name) > 100:
        raise ValueError('Archive Wikipédia invalide')
    rest = name[len(prefix):-4]
    variant, sep, date = rest.partition('_')
    if variant not in ('nopic', 'maxi') or not sep or len(date) != 7 or date[4] != '-' or not date[:4].isdigit() or not date[5:].isdigit() or not 1 <= int(date[5:]) <= 12:
        raise ValueError('Archive Wikipédia invalide')
    return name


def parse_catalog(xml, code):
    code = wiki.language(code)
    if len(xml.encode()) > MAX_CATALOG or '<!DOCTYPE' in xml or '<!ENTITY' in xml:
        raise ValueError('Catalogue Wikipédia invalide')
    total = int(_tag(xml, 'totalResults', '0'))
    if total < 0 or total > 1000:
        raise ValueError('Catalogue Wikipédia invalide')
    rows = []; position = 0
    while True:
        start = xml.find('<entry>', position)
        if start < 0:
            break
        end = xml.find('</entry>', start)
        if end < 0 or end - start > 16384 or len(rows) >= 20:
            raise ValueError('Catalogue Wikipédia invalide')
        entry = xml[start:end]; position = end + 8
        if _tag(entry, 'name') != 'wikipedia_' + code + '_all':
            continue
        variant = _tag(entry, 'flavour')
        if variant not in ('nopic', 'maxi'):
            continue
        linkpos = 0
        while True:
            linkpos = entry.find('<link ', linkpos)
            if linkpos < 0:
                break
            endlink = entry.find('>', linkpos)
            if endlink < 0:
                raise ValueError('Catalogue Wikipédia invalide')
            attrs = _attributes(entry[linkpos:endlink + 1]); linkpos = endlink + 1
            if attrs.get('type') != 'application/x-zim' or not attrs.get('rel', '').endswith('/acquisition/open-access'):
                continue
            url = attrs.get('href', '')
            if url.endswith('.meta4'):
                url = url[:-6]
            url = _source(url)
            filename = _filename(url.rsplit('/', 1)[-1], code)
            size = int(attrs.get('length', '0'))
            if not 80 <= size <= (1 << 50):
                raise ValueError('Catalogue Wikipédia invalide')
            if filename[len('wikipedia_' + code + '_all_'):].split('_')[0] != variant:
                raise ValueError('Catalogue Wikipédia invalide')
            rows.append({'filename': filename, 'language': code, 'title': _tag(entry, 'title')[:180],
                         'variant': variant, 'date': filename[-11:-4], 'size': size,
                         'source': url, 'catalog_id': _tag(entry, 'id')[:128],
                         'article_count': int(_tag(entry, 'articleCount', '0'))})
            break
    return rows, total


def catalog(code, progress=None):
    code = wiki.language(code)
    wiki.ensure(code)
    temporary = wiki.directory(code) + '/.kiwix-catalog.xml'
    selected = {}; start = 0
    try:
        while True:
            if fs.exists(temporary):
                fs.remove(temporary)
            url = CATALOG + '?name=wikipedia_' + code + '_all&count=20&start=' + str(start)
            fs.download(url, temporary, MAX_CATALOG, progress)
            with open(temporary) as stream:
                rows, total = parse_catalog(stream.read(), code)
            for row in rows:
                old = selected.get(row['variant'])
                if not old or old['date'] < row['date']:
                    selected[row['variant']] = row
            start += 20
            if start >= total:
                break
            gc.collect(); sleep_ms(1)
        return [selected[k] for k in ('nopic', 'maxi') if k in selected]
    finally:
        if fs.exists(temporary):
            fs.remove(temporary)


def parse_metalink(header, record):
    if '<!DOCTYPE' in header or '<!ENTITY' in header:
        raise ValueError('Catalogue Wikipédia invalide')
    start = header.find('<file '); end = header.find('>', start)
    if start < 0 or end < 0 or _attributes(header[start:end + 1]).get('name') != record['filename']:
        raise ValueError('Catalogue Wikipédia invalide')
    data = header[end + 1:]
    size = int(_tag(data, 'size', '0'))
    if not 80 <= size <= (1 << 50):
        raise ValueError('Catalogue Wikipédia invalide')
    digest = ''; position = 0
    while True:
        position = data.find('<hash ', position)
        if position < 0:
            break
        close = data.find('>', position)
        endhash = data.find('</hash>', close)
        if close < 0 or endhash < 0:
            break
        if _attributes(data[position:close + 1]).get('type') == 'sha-256':
            digest = data[close + 1:endhash].strip().lower(); break
        position = endhash + 7
    if len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
        raise ValueError('Empreinte de l’archive Wikipédia indisponible')
    result = dict(record)
    result['size'] = size; result['sha256'] = digest
    return result


def resolve(record, progress=None):
    # OPDS sizes are rounded to KiB. Metalink provides the exact byte count.
    temporary = wiki.directory(record['language']) + '/.kiwix-meta.xml'
    if fs.exists(temporary):
        fs.remove(temporary)
    try:
        fs.download(_source(record['source']) + '.meta4', temporary, 8 * 1024 * 1024, progress)
        with open(temporary) as stream:
            resolved = parse_metalink(stream.read(4096), record)
            stream.seek(0)
            mirrors = []
            while True:
                line = stream.readline(4097)
                if not line: break
                if len(line) > 4096: raise ValueError('Catalogue Wikipédia invalide')
                start = line.find('<url ')
                end = line.find('</url>', start)
                if start >= 0 and end >= 0:
                    value = _xml_text(line[line.find('>', start) + 1:end])
                    if value.startswith('https://'):
                        value = _mirror(value, record['filename'])
                        if value not in mirrors and len(mirrors) < 6:
                            mirrors.append(value)
            resolved['mirrors'] = mirrors
            return resolved
    finally:
        if fs.exists(temporary):
            fs.remove(temporary)


def archive_directory(code):
    return wiki.directory(code) + '/archives'


def location(state):
    return archive_directory(state['language']) + '/' + _filename(state['filename'], state['language']) + 'parts'


def part_path(state, index):
    return location(state) + '/part-%06d.bin' % index


def _validate(state):
    if not isinstance(state, dict) or state.get('format') != FORMAT or state.get('part_size') != PART_SIZE:
        raise ValueError('Archive Wikipédia invalide')
    code = wiki.language(state.get('language', ''))
    _filename(state.get('filename'), code)
    source = _source(state.get('source'))
    if source.rsplit('/', 1)[-1] != state['filename']:
        raise ValueError('Archive Wikipédia invalide')
    size = state.get('size', 0); done = state.get('done', -1)
    if not isinstance(size, int) or not isinstance(done, int) or not 80 <= size <= (1 << 50) or not 0 <= done <= size or (done != size and done % PART_SIZE) or (state.get('complete') != (done == size) and not (done == size and state.get('verify_required') and state.get('complete') is False)):
        raise ValueError('Archive Wikipédia invalide')
    for key in ('etag', 'last_modified'):
        if not isinstance(state.get(key, ''), str) or len(state.get(key, '').encode()) > 256 or any(ord(c) < 32 or ord(c) == 127 for c in state.get(key, '')):
            raise ValueError('Archive Wikipédia invalide')
    etag = state.get('etag', '')
    if etag and (len(etag) < 2 or not etag.startswith('"') or not etag.endswith('"') or any(ord(c) < 33 or c == '"' or ord(c) == 127 for c in etag[1:-1])):
        raise ValueError('Archive Wikipédia invalide')
    if state.get('transfer') == 'manual' or state.get('verify_required'):
        digest = state.get('sha256', '')
        if not isinstance(digest, str) or len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
            raise ValueError('Archive Wikipédia invalide')
    if done and not (etag or state.get('last_modified')) and state.get('transfer') != 'manual' and not state.get('verify_required'):
        raise ValueError('Archive Wikipédia invalide')
    if state.get('mirror'):
        _mirror(state['mirror'], state['filename'])
    if not isinstance(state.get('mirrors', []), list) or len(state.get('mirrors', [])) > 6:
        raise ValueError('Archive Wikipédia invalide')
    for url in state.get('mirrors', []):
        _mirror(url, state['filename'])
    return state


def save(state):
    _validate(state)
    fs.makedirs(location(state))
    wiki._json_replace(location(state) + '/manifest.json', state)


def load(directory, verify=False):
    directory = fs.path(directory)
    state = _validate(fs.read_json(directory + '/manifest.json', 16384))
    if directory != location(state):
        raise ValueError('Archive Wikipédia invalide')
    if verify:
        count = (state['done'] + PART_SIZE - 1) // PART_SIZE
        for index in range(count):
            expected = min(PART_SIZE, state['size'] - index * PART_SIZE)
            if fs.stat(part_path(state, index))[1] != expected:
                raise ValueError('Archive Wikipédia incomplète ou endommagée')
            if index % 16 == 0:
                sleep_ms(1)
    return state


def begin(record):
    state = dict(record)
    state.update({'format': FORMAT, 'part_size': PART_SIZE, 'done': 0, 'complete': False,
                  'etag': '', 'last_modified': '', 'mirror': '', 'verify_required': True})
    _validate(state)
    filename = location(state) + '/manifest.json'
    if fs.exists(filename):
        previous = load(location(state), True)
        if not previous['done']:
            save(state)
            return state
        if previous.get('transfer') == 'manual' and not previous['complete']:
            raise ValueError('Terminer la copie de cette archive avec l’outil utilisé sur le PC')
        if previous['size'] != state['size'] or previous['source'] != state['source'] or (state.get('sha256') and previous.get('sha256') != state['sha256']):
            raise ValueError('Cette édition a changé. Choisir une nouvelle archive.')
        return previous
    save(state)
    return state


def restart(state, progress=None, resolved=None):
    updated = dict(resolved) if resolved is not None else resolve(state, progress)
    updated.update({'format': FORMAT, 'part_size': PART_SIZE, 'done': 0, 'complete': False,
                    'etag': '', 'last_modified': '', 'mirror': '', 'verify_required': True})
    updated.pop('transfer', None)
    updated.pop('verified', None)
    _validate(updated)
    # Publish the incomplete checkpoint first: a power loss must not expose
    # a complete archive after some of its parts have been removed.
    save(updated)
    directory = location(updated)
    for name in fs.ilistdir(directory):
        base = name[:-5] if name.endswith('.part') else name
        if base.startswith('part-') and base.endswith('.bin') and len(base) == 15 and base[5:11].isdigit():
            fs.remove(directory + '/' + name)
        sleep_ms(1)
    return updated


def archives(code, complete=None):
    root = archive_directory(wiki.language(code))
    if not fs.exists(root):
        return []
    result = []
    for filename in fs.ilistdir(root):
        if filename.endswith('.zimparts'):
            try:
                state = load(root + '/' + filename)
                if (complete is None or state['complete'] == complete) and not (complete is False and state.get('transfer') == 'manual'):
                    result.append(state)
            except (OSError, ValueError, TypeError, KeyError):
                continue
        if len(result) >= 100:
            break
        sleep_ms(1)
    return result


def storage():
    result = json.loads(call(85, 0, 0, 0, None, 256))
    return int(result['free'])


def step(state, progress=None, pause=None):
    _validate(state)
    if state['complete']:
        return state
    if state.get('transfer') == 'manual':
        raise ValueError('Terminer la copie de cette archive avec l’outil utilisé sur le PC')
    if not state.get('verify_required'):
        resolved = resolve(state, progress)
        if resolved['size'] != state['size'] and state['done']:
            raise ValueError('Cette édition a changé. Choisir une nouvelle archive.')
        state.update({'size': resolved['size'], 'sha256': resolved['sha256'],
                      'mirrors': resolved['mirrors'], 'verify_required': True})
        save(state)
    if state['done'] < state['size']:
        index = state['done'] // PART_SIZE
        destination = part_path(state, index)
        for partial in (destination, destination + '.part'):
            if fs.exists(partial): fs.remove(partial)
        count = min(PART_SIZE, state['size'] - state['done'])
        if storage() < count + 65536:
            raise OSError(28)
        sources = []
        for url in [state.get('mirror', '')] + state.get('mirrors', []) + [state['source']]:
            if url and url not in sources: sources.append(url)
        payload = {'path': destination, 'offset': state['done'], 'count': count,
                   'expected_size': state['size'], 'sha256': state['sha256']}
        attempt = 0; source = 0
        while True:
            url = sources[source]
            payload.update({'url': url, 'etag': '', 'last_modified': ''})
            if url == state.get('mirror'):
                payload.update({'etag': state.get('etag', ''), 'last_modified': state.get('last_modified', '')})
            try:
                result = json.loads(call(82, 0, getattr(progress, 'id', 0), getattr(pause, 'id', 0),
                                         json.dumps(payload).encode(), 2048))
                break
            except OSError as error:
                number = error.args[0]
                retry = number in (5, 6, 19, 110, 116, 5002, 5003, 5004, 5007, 5008, 5009, 5010,
                                   1403, 1404, 1429, 1500, 1502, 1503, 1504)
                if not retry or attempt >= 7: raise
                if number in (5, 6, 19): fs.reconnect()
                elif number != 5002 and source + 1 < len(sources): source += 1
                elif attempt >= 3: raise
                for partial in (destination, destination + '.part'):
                    if fs.exists(partial): fs.remove(partial)
                sleep_ms((400, 1200, 2500)[min(attempt, 2)])
                attempt += 1
        if (result.get('size') != state['size'] or result.get('offset') != state['done'] or
                result.get('count') != count or fs.stat(destination)[1] != count):
            if fs.exists(destination): fs.remove(destination)
            raise ValueError('Réponse de téléchargement invalide')
        updated = dict(state)
        updated.update({'etag': result.get('etag', ''), 'last_modified': result.get('last_modified', ''),
                        'mirror': _mirror(result.get('mirror', payload['url']), state['filename']),
                        'done': state['done'] + count, 'complete': False})
        save(updated)
        state.clear(); state.update(updated)
        gc.collect()
    if state['done'] == state['size']:
        from tab5ui import tr
        payload = {'path': location(state), 'size': state['size'], 'sha256': state['sha256'],
                   'label': tr('Vérification de l’archive')}
        result = json.loads(call(104, 0, getattr(progress, 'id', 0), getattr(pause, 'id', 0),
                                 json.dumps(payload).encode(), 256))
        if result.get('verified') is not True:
            raise ValueError('Archive Wikipédia incomplète ou endommagée')
        updated = dict(state); updated['complete'] = True; updated['verified'] = True
        save(updated); state.clear(); state.update(updated)
    return state


def search(state, prefix='', cursor=0):
    if not state['complete']:
        raise ValueError('Terminer le téléchargement avant de lire cette archive')
    payload = {'path': state.get('path') or location(state), 'prefix': prefix.strip(), 'limit': 6, 'cursor': int(cursor)}
    result = json.loads(call(83, 0, 0, 0, json.dumps(payload).encode(), 4096))
    if not result.get('ok'):
        raise ValueError(result.get('error', 'Archive Wikipédia invalide'))
    return result


def extract(state, identity=None, link=None, base=None, images=True):
    # One extracted article per archive. Stable path retains the reader position.
    archive = state.get('path') or location(state)
    directory = '/Wikipedia/.reader' if state.get('path') else location(state)
    fs.makedirs(directory)
    # Single-file archives use a content-independent path key, without retaining HTML.
    key = 2166136261
    for byte in archive.encode():
        key = ((key ^ byte) * 16777619) & 0xffffffff
    article_key = int(identity) if identity is not None else 2166136261
    if identity is None:
        for byte in (link or '').encode():
            article_key = ((article_key ^ byte) * 16777619) & 0xffffffff
    destination = directory + '/article-%08x-%d.txt' % (key, article_key)
    payload = {'path': archive, 'destination': destination, 'images': bool(images)}
    if link is not None:
        payload['link'] = link
        if base:
            payload['base'] = base
    else:
        payload['id'] = int(identity)
    gc.collect()
    result = json.loads(call(84, 0, 0, 0, json.dumps(payload).encode(), 4096))
    if not result.get('ok'):
        if result.get('requested'):
            from tab5gui import tr
            message = tr('Insufficient PSRAM for ZIM decoder') + '\n'
            message += tr('Requested: %d KiB; free: %d KiB; largest block: %d KiB') % (
                (result['requested'] + 1023) // 1024, result.get('free', 0) // 1024,
                result.get('largest_block', 0) // 1024)
            raise ValueError(message)
        raise ValueError(result.get('error', 'Article Wikipédia indisponible'))
    code = state.get('language', '')
    if not code and archive.rsplit('/', 1)[-1].startswith('wikipedia_'):
        code = archive.rsplit('/', 1)[-1].split('_')[1]
    article_path = result.get('article_path', '')
    source = ('https://' + wiki.language(code) + '.wikipedia.org/wiki/' + wiki.quote(article_path)) if code and article_path else archive
    with open(destination, 'ab') as stream:
        footer = (wiki.credit(source) if code else '\n\n' + archive + '\n').encode()
        if stream.write(footer) != len(footer):
            raise OSError(28)
        stream.flush()
    result['bytes'] = fs.stat(destination)[1]
    previous = directory + '/reader-cache.json'
    # A damaged cache hint is optional; it must not make a valid archive unreadable.
    try:
        old = fs.read_json(previous, 1024) if fs.exists(previous) else {}
    except (OSError, ValueError, TypeError):
        old = {}
    oldpath = old.get('path', '') if isinstance(old, dict) else ''
    if not isinstance(oldpath, str):
        oldpath = ''
    name = oldpath[len(directory) + 1:] if oldpath.startswith(directory + '/') else ''
    parts = name[:-4].split('-') if name.endswith('.txt') else []
    generated = (len(parts) == 3 and parts[0] == 'article' and len(parts[1]) == 8
                 and all(c in '0123456789abcdef' for c in parts[1]) and parts[2].isdigit())
    if generated and oldpath != destination and fs.exists(oldpath):
        for path in (oldpath,) + tuple(oldpath + suffix for suffix in wiki.sidecars()):
            if fs.exists(path):
                fs.remove(path)
    wiki._json_replace(previous, {'path': destination})
    return result
