"""Online Wikipedia articles and their local text cache."""
import gc
import tab5fs as fs
from time import sleep_ms

ROOT = '/Wikipedia'
MAX_RESPONSE = 262144
SEARCH_SIZE = 10


def language(code):
    if not isinstance(code, str):
        raise ValueError('Code de langue Wikipédia invalide')
    code = code.strip().lower()
    if not 2 <= len(code) <= 20 or code[0] == '-' or code[-1] == '-':
        raise ValueError('Code de langue Wikipédia invalide')
    if any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-' for c in code):
        raise ValueError('Code de langue Wikipédia invalide')
    return code


def quote(value):
    out = []
    for byte in str(value).encode():
        out.append(chr(byte) if (65 <= byte <= 90 or 97 <= byte <= 122 or
                   48 <= byte <= 57 or byte in (45, 46, 95, 126)) else '%%%02X' % byte)
    return ''.join(out)


def directory(code):
    return ROOT + '/' + language(code)


def ensure(code):
    fs.makedirs(directory(code) + '/articles')


def _json_replace(path, value):
    with fs.Transaction() as transaction:
        transaction.json(path, value)
        transaction.commit()


def settings(default='en'):
    result = {'language': language(default), 'query': '', 'autosave': False,
              'illustrations': False, 'auto_update': False, 'auto_archives': False, 'archive_checks': {}, 'archives': [], 'recent': []}
    path = ROOT + '/settings.json'
    if fs.exists(path):
        try:
            value = fs.read_json(path, 32768)
            result['language'] = language(value.get('language', default))
            result['query'] = str(value.get('query', ''))[:180]
            for key in ('autosave', 'auto_update', 'auto_archives', 'illustrations'):
                result[key] = value.get(key) is True
            checks = value.get('archive_checks', {})
            if isinstance(checks, dict):
                result['archive_checks'] = {language(k): v for k, v in list(checks.items())[:8] if isinstance(v, int)}
            result['archives'] = [{'path': fs.path(v['path']), 'language': language(v['language'])}
                                  for v in value.get('archives', [])[:32] if isinstance(v, dict)]
            result['recent'] = [r for r in value.get('recent', [])[:12]
                                if isinstance(r, dict) and isinstance(r.get('title'), str)
                                and len(r['title'].encode()) <= 512 and isinstance(r.get('language'), str)]
        except (OSError, ValueError, TypeError, KeyError):
            pass
    return result


def save_settings(code, query, options=None):
    value = settings(code) if options is None else dict(options)
    value['language'] = language(code); value['query'] = query[:180]
    value['archives'] = value.get('archives', [])[:32]
    value['recent'] = value.get('recent', [])[:12]
    fs.makedirs(ROOT)
    _json_replace(ROOT + '/settings.json', value)


def remember(code, title):
    options = settings(code)
    rows = [r for r in options['recent'] if r['language'] != code or r['title'] != title]
    while len(title.encode()) > 240:
        title = title[:-1]
    options['recent'] = [{'language': code, 'title': title}] + rows[:11]
    save_settings(options['language'], options['query'], options)


def api(code, parameters, limit, progress=None):
    ensure(code)
    path = directory(code) + '/.response.json'
    if fs.exists(path):
        fs.remove(path)
    url = 'https://' + language(code) + '.wikipedia.org/w/api.php?action=query&format=json&formatversion=2&utf8=1&maxlag=5'
    for key, value in parameters:
        url += '&' + key + '=' + quote(value)
    if len(url.encode()) > 1024:
        raise ValueError('Recherche Wikipédia trop longue')
    try:
        fs.download(url, path, limit, progress)
        data = fs.read_json(path, limit)
        if not isinstance(data, dict):
            raise ValueError('Réponse Wikipédia invalide')
        if 'error' in data:
            error = data['error'].get('code', '')
            if error in ('maxlag', 'ratelimited', 'readonly'):
                raise ValueError('Wikipédia est occupé. Réessayer plus tard ; la progression est conservée.')
            raise ValueError('Wikipédia ne peut pas répondre à cette recherche')
        return data
    finally:
        if fs.exists(path):
            fs.remove(path)


def search(code, query, offset=0, count=SEARCH_SIZE, progress=None):
    query = query.strip()
    if not query:
        raise ValueError('Saisir une recherche Wikipédia')
    if len(query.encode()) > 240:
        raise ValueError('Recherche Wikipédia trop longue')
    count = max(1, min(SEARCH_SIZE, int(count)))
    data = api(code, [('list', 'search'), ('srsearch', query), ('srnamespace', 0),
                     ('srprop', ''), ('srlimit', count), ('sroffset', max(0, int(offset)))],
               24576, progress)
    rows = data.get('query', {}).get('search', [])
    if not isinstance(rows, list) or len(rows) > SEARCH_SIZE:
        raise ValueError('Réponse Wikipédia invalide')
    result = []
    for row in rows:
        identity = int(row.get('pageid', 0))
        title = row.get('title', '')
        if identity <= 0 or not isinstance(title, str) or len(title.encode()) > 512:
            raise ValueError('Réponse Wikipédia invalide')
        result.append({'id': identity, 'title': title})
    continuation = data.get('continue', {}).get('sroffset')
    if continuation is not None:
        continuation = int(continuation)
        if continuation <= offset or continuation > 10000:
            raise ValueError('Réponse Wikipédia invalide')
    return result, continuation


def paths(code, identity):
    identity = int(identity)
    if identity <= 0:
        raise ValueError('Article Wikipédia invalide')
    base = directory(code) + '/articles/' + str(identity)
    return base + '.txt', base + '.json'


def cached(code, identity):
    text, meta = paths(code, identity)
    if not fs.exists(text) or not fs.exists(meta):
        return None
    try:
        record = fs.read_json(meta, 2048)
        if (record.get('id') == int(identity) and record.get('language') == language(code)
                and 0 < record.get('bytes', 0) <= MAX_RESPONSE + 16384
                and fs.stat(text)[1] == record['bytes']
                and all(isinstance(record.get(key), str) and 0 < len(record[key].encode()) <= 512
                        for key in ('title', 'license', 'source', 'history'))):
            return record
    except (OSError, ValueError, TypeError, KeyError):
        pass
    return None


def sidecars():
    return ('.links', '.media', '.sources', '.images.json', '.image', '.image.json') + tuple('.img%d' % i for i in range(8))


def enrich(record, progress=None):
    """Stream revision HTML to SD; a failed conversion leaves the old page intact."""
    code = language(record['language']); filename, metadata = record_paths(record)
    temporary = directory(code) + '/.response.html'
    if fs.exists(temporary):
        fs.remove(temporary)
    url = 'https://' + code + '.wikipedia.org/w/index.php?action=render&' + (
        'oldid=' + str(record['revision']) if record.get('revision') else 'curid=' + str(record.get('pageid', record['id'])))
    try:
        fs.download(url, temporary, 2097152, progress)
        updated = dict(record)
        with fs.Transaction() as transaction:
            from _tab5 import call
            text_path = transaction.allocate(filename)
            links_path = transaction.allocate(filename + '.links')
            sources_path = transaction.allocate(filename + '.sources')
            payload = ('\0'.join((temporary, text_path, links_path, sources_path, code))).encode()
            size = call(103, 0, 0, 0, payload)
            updated['bytes'] = size; updated['rich'] = 3
            updated.pop('attribution', None); updated.pop('image_credit', None); updated.pop('image_credits', None)
            transaction.json(metadata, updated)
            for suffix in sidecars():
                if suffix not in ('.links', '.sources') and fs.exists(filename + suffix):
                    transaction.allocate(filename + suffix, delete=True)
            transaction.commit()
        return updated
    finally:
        if fs.exists(temporary):
            fs.remove(temporary)
        gc.collect()


def article(code, identity, progress=None, refresh=False, save=True):
    identity = int(identity)
    previous = cached(code, identity)
    if previous and not refresh:
        return previous if previous.get('rich', 0) >= 3 else enrich(previous, progress)
    data = api(code, [('prop', 'info'), ('pageids', identity), ('redirects', 1), ('inprop', 'url')], 16384, progress)
    pages = data.get('query', {}).get('pages', [])
    if not isinstance(pages, list) or len(pages) != 1 or 'missing' in pages[0]:
        raise ValueError('Article Wikipédia indisponible')
    page = pages[0]; title = page.get('title', '')
    if not isinstance(title, str) or not title or len(title.encode()) > 512:
        raise ValueError('Réponse Wikipédia invalide')
    real_id = int(page.get('pageid', 0)); revision = int(page.get('lastrevid', 0))
    if real_id <= 0:
        raise ValueError('Article Wikipédia invalide')
    record = {'id': identity, 'pageid': real_id, 'title': title, 'language': language(code),
              'revision': revision, 'license': 'CC BY-SA 4.0',
              'source': 'https://' + language(code) + '.wikipedia.org/w/index.php?' +
                        ('oldid=' + str(revision) if revision else 'curid=' + str(real_id)),
              'history': 'https://' + language(code) + '.wikipedia.org/w/index.php?curid=' +
                         str(real_id) + '&action=history'}
    if not save and previous is None:
        record['temporary'] = True
    del data, pages, page
    gc.collect()
    try:
        return enrich(record, progress)
    except (MemoryError, ValueError, OSError) as error:
        if isinstance(error, OSError) and (not error.args or error.args[0] != 27):
            raise
        if previous:
            raise
        gc.collect()
        data = api(code, [('prop', 'extracts'), ('pageids', real_id), ('explaintext', 1), ('exchars', 1200)], 16384, progress)
        pages = data.get('query', {}).get('pages', [])
        text = pages[0].get('extract', '') if pages else ''
        if not isinstance(text, str) or not text: raise error
        payload = text.encode()
        record['bytes'] = len(payload); record['text_only'] = True; record['abridged'] = True
        filename, metadata = record_paths(record)
        with fs.Transaction() as transaction:
            with open(transaction.allocate(filename), 'xb') as destination:
                if destination.write(payload) != len(payload): raise OSError(28)
                destination.flush()
            for suffix in sidecars():
                if fs.exists(filename + suffix):
                    transaction.allocate(filename + suffix, delete=True)
            transaction.json(metadata, record); transaction.commit()
        return record


def by_title(code, title, progress=None, save=True):
    data = api(code, [('prop', 'info'), ('titles', title), ('redirects', 1)], 16384, progress)
    pages = data.get('query', {}).get('pages', [])
    if len(pages) != 1 or 'missing' in pages[0] or int(pages[0].get('pageid', 0)) <= 0:
        raise ValueError('Article Wikipédia indisponible')
    return article(code, pages[0]['pageid'], progress, save=save)


def offline(code, page=0, query='', size=6):
    ensure(code)
    found = []; total = 0
    query = query.lower()
    for name in fs.ilistdir(directory(code) + '/articles'):
        if not name.endswith('.json') or not name[:-5].isdigit():
            continue
        record = cached(code, int(name[:-5]))
        if record and query in record['title'].lower():
            if page * size <= total < (page + 1) * size:
                found.append(record)
            total += 1
        if total % 16 == 0:
            sleep_ms(1)
    return found, total


def credit(source, history=''):
    from tab5ui import tr
    return ('\n\n' + tr('Texte de Wikipédia et de ses contributeurs') + '\nCC BY-SA 4.0\n' +
            'https://creativecommons.org/licenses/by-sa/4.0/\n' + tr('Source') + ' : ' + source +
            ('\n' + tr('Historique') + ' : ' + history if history else '') + '\n')


def ensure_credit(record):
    if record.get('attribution') == 1:
        return record
    text, metadata = record_paths(record)
    updated = dict(record)
    with fs.Transaction() as transaction:
        staged = transaction.allocate(text)
        fs.copy(text, staged)
        with open(staged, 'ab') as destination:
            footer = credit(record['source'], record['history']).encode()
            if destination.write(footer) != len(footer):
                raise OSError(28)
            destination.flush()
        updated['bytes'] = fs.stat(staged)[1]
        updated['attribution'] = 1
        transaction.json(metadata, updated)
        transaction.commit()
    return updated


def record_paths(record):
    if record.get('temporary'):
        base = directory(record['language']) + '/.reading-' + str(int(record['id']))
        return base + '.txt', base + '.json'
    return paths(record['language'], record['id'])


def save_article(record):
    record = ensure_credit(record)
    if not record.get('temporary'):
        return record
    text, unused = record_paths(record)
    target, metadata = paths(record['language'], record['id'])
    updated = dict(record); updated.pop('temporary', None)
    with fs.Transaction() as transaction:
        fs.copy(text, transaction.allocate(target))
        transaction.json(metadata, updated)
        for suffix in sidecars():
            if fs.exists(text + suffix):
                if suffix == '.media':
                    staged = transaction.allocate(target + suffix)
                    with open(text + suffix) as source, open(staged, 'w') as destination:
                        for line in source:
                            destination.write(line.replace(text + '.img', target + '.img'))
                else:
                    fs.copy(text + suffix, transaction.allocate(target + suffix))
        transaction.commit()
    discard_temporary(record)
    return updated


def discard_temporary(record):
    if record.get('temporary'):
        text, metadata = record_paths(record)
        for path in (text, metadata, text + '.image.download') + tuple(text + suffix for suffix in sidecars()):
            if fs.exists(path):
                fs.remove(path)


def remove_article(record):
    """Remove one saved copy atomically, including its illustrations and links."""
    text, metadata = record_paths(record)
    with fs.Transaction() as transaction:
        for path in (text, metadata) + tuple(text + suffix for suffix in sidecars()):
            if fs.exists(path):
                transaction.allocate(path, delete=True)
        transaction.commit()


def exact_saved(code, title):
    title = title.replace('_', ' ').strip().lower()
    for name in fs.ilistdir(directory(code) + '/articles'):
        if name.endswith('.json') and name[:-5].isdigit():
            record = cached(code, int(name[:-5]))
            if record and record['title'].replace('_', ' ').strip().lower() == title:
                return record
        sleep_ms(1)
    return None


def check_update(record, progress=None):
    data = api(record['language'], [('prop', 'info'), ('pageids', record.get('pageid', record['id']))],
               8192, progress)
    pages = data.get('query', {}).get('pages', [])
    if not isinstance(pages, list) or len(pages) != 1 or 'missing' in pages[0]:
        raise ValueError('Article Wikipédia indisponible')
    revision = int(pages[0].get('lastrevid', 0))
    if revision <= 0:
        raise ValueError('Réponse Wikipédia invalide')
    return revision != record.get('revision', 0)


def now():
    from _tab5 import call
    import struct
    try:
        return struct.unpack('<Q', call(8, 0, 0, 0, None, 8))[0]
    except OSError as error:
        if error.args and error.args[0] == 38:
            return 0
        raise


def mark_checked(record):
    updated = dict(record); updated['checked_at'] = now()
    _json_replace(record_paths(updated)[1], updated)
    return updated


def update_due(record):
    current = now(); checked = record.get('checked_at', 0)
    return current > 100000000 and (not isinstance(checked, int) or current - checked >= 86400)


def refresh_article(record, progress=None):
    if check_update(record, progress):
        record = article(record['language'], record['id'], progress, True,
                         not record.get('temporary', False))
        record = ensure_credit(record)
    return mark_checked(record)
