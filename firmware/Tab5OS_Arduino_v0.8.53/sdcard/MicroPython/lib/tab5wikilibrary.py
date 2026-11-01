"""One bounded search route for saved pages, complete archives and web results."""
import tab5fs as fs
import tab5wiki as wiki
import tab5zim as zim


def archives(code, issues=None):
    # Search newest complete editions first; older copies remain available.
    available = zim.archives(code, True)
    available.sort(key=lambda r: (r.get('date', ''), r.get('variant') == 'maxi'), reverse=True)
    result = available
    for item in wiki.settings(code)['archives']:
        if item['language'] != code:
            continue
        if not fs.exists(item['path']):
            if issues is not None:
                issues.append(item['path'] + ' : ' + 'Archive introuvable')
            continue
        if item['language'] == code:
            if item['path'].endswith('/manifest.json'):
                try:
                    state = zim.load(item['path'].rsplit('/', 1)[0], True)
                    if state['complete'] and all(zim.location(r) != zim.location(state) for r in result if not r.get('path')):
                        result.append(state)
                except (OSError, ValueError, KeyError, TypeError):
                    continue
            else:
                result.append({'complete': True, 'path': item['path'], 'language': code})
    return result


def local(code, query, cursor=None, issues=None):
    wiki.ensure(code)
    cursor = dict(cursor) if cursor else {'source': 0, 'page': 0}
    sources = archives(code, issues)
    if cursor['source'] == 0:
        rows, total = wiki.offline(code, cursor['page'], query, 6)
        following = ({'source': 0, 'page': cursor['page'] + 1} if (cursor['page'] + 1) * 6 < total
                     else {'source': 1, 'page': 0} if sources else None)
        if rows:
            return [{'kind': 'saved', 'record': r, 'title': r['title']} for r in rows], following
        cursor = {'source': 1, 'page': 0}
    while cursor['source'] <= len(sources):
        state = sources[cursor['source'] - 1]
        try:
            prefix = cursor.get('prefix', query)
            data = zim.search(state, prefix, cursor['page'])
            capitalized = query[:1].upper() + query[1:]
            if not data.get('entries') and cursor['page'] == 0 and capitalized != prefix:
                prefix = capitalized
                data = zim.search(state, prefix, 0)
            entries = data.get('entries', [])
            next_page = data.get('next_cursor')
            following = ({'source': cursor['source'], 'page': next_page, 'prefix': prefix} if next_page is not None
                         else {'source': cursor['source'] + 1, 'page': 0} if cursor['source'] < len(sources) else None)
            if entries:
                return [{'kind': 'zim', 'archive': state, 'id': r['id'], 'title': r['title']} for r in entries], following
        except (OSError, ValueError, KeyError, TypeError) as error:
            if issues is not None:
                issues.append((state.get('path') or zim.location(state)) + ' : ' + str(error))
        cursor = {'source': cursor['source'] + 1, 'page': 0}
    return [], None


def exact(code, title):
    wiki.ensure(code)
    saved = wiki.exact_saved(code, title)
    if saved:
        return {'kind': 'saved', 'record': saved, 'title': saved['title']}
    normalized = title.replace('_', ' ').strip().lower()
    for state in archives(code):
        try:
            result = zim.search(state, title, 0)
            for row in result.get('entries', []):
                if row['title'].replace('_', ' ').strip().lower() == normalized:
                    return {'kind': 'zim', 'archive': state, 'id': row['id'], 'title': row['title']}
        except (OSError, ValueError, KeyError, TypeError):
            pass
    return None


def search_page(code, query, state=None, connected=False, progress=None, issues=None):
    """Fill a visible page across source boundaries; keep a one-result lookahead.

    A cursor describes both streams, so going back never drops a web remainder.
    Saved pages and ZIM results are preferred; title deduplication spans 32 pages.
    """
    state = dict(state or {})
    pending = list(state.get('pending', []))
    seen = list(state.get('seen', []))
    rows = []
    while len(rows) < 7:
        if pending:
            row = pending.pop(0)
            title = row['title'].replace('_', ' ').strip().lower()
            if title in seen:
                continue
            rows.append(row); seen.append(title); seen = seen[-192:]
            continue
        if not state.get('local_done'):
            pending, following = local(code, query, state.get('local_cursor'), issues)
            state['local_cursor'] = following
            state['local_done'] = following is None
            continue
        if query and connected and not state.get('web_done'):
            try:
                found, following = wiki.search(code, query, state.get('web_cursor', 0), 6, progress)
                pending = [dict(row, kind='online') for row in found]
                state['web_cursor'] = following
                state['web_done'] = following is None
            except (OSError, ValueError, MemoryError):
                # Keep already available local matches when the network fails.
                state['web_done'] = True
                if issues is not None:
                    issues.append('Connexion indisponible')
            continue
        break
    following = None
    if len(rows) > 6:
        lookahead = rows.pop()
        pending.insert(0, lookahead)
        seen.pop()  # It has not yet appeared on a visible page.
        following = state
    state['pending'] = pending
    state['seen'] = seen
    return rows, following


def remove_archive(state):
    """Delete only the selected archive's validated files, never its parent tree."""
    filename = state.get('path')
    if filename:
        filename = fs.path(filename)
        if filename.lower().endswith('.zimaa'):
            stem = filename[:-2]
            for index in range(676):
                part = stem + chr(97 + index // 26) + chr(97 + index % 26)
                if not fs.exists(part):
                    break
                if fs.isdir(part):
                    raise ValueError('Archive Wikipédia invalide')
                fs.remove(part)
        elif filename.lower().endswith('.zim') and not fs.isdir(filename):
            fs.remove(filename)
        else:
            raise ValueError('Archive Wikipédia invalide')
        forget(filename)
        return
    directory = zim.location(zim._validate(state))
    # Mark it unavailable before removing the first segment.
    checkpoint = dict(state, done=0, complete=False)
    zim.save(checkpoint)
    for name in fs.ilistdir(directory):
        base = name[:-5] if name.endswith('.part') else name
        if base.startswith('part-') and base.endswith('.bin') and len(base) == 15 and base[5:11].isdigit():
            fs.remove(directory + '/' + name)
    fs.remove(directory + '/manifest.json')
    forget(directory + '/manifest.json')


def register(code, filename):
    filename = fs.path(filename)
    if filename.endswith('/manifest.json'):
        state = zim.load(filename.rsplit('/', 1)[0], True)
        if not state['complete']:
            raise ValueError('Terminer le téléchargement avant de lire cette archive')
        code = state['language']
    elif filename.lower().endswith(('.zim', '.zimaa')):
        if not fs.exists(filename):
            raise ValueError('Archive Wikipédia invalide')
        result = zim.search({'complete': True, 'path': filename}, '', 0)
        if not result.get('entries'):
            raise ValueError('Archive sans article lisible dans son index de titres')
    else:
        raise ValueError('Choisir un fichier ZIM ou le manifeste de ses segments')
    options = wiki.settings(code)
    options['archives'] = [{'path': filename, 'language': code}] + [r for r in options['archives'] if r['path'] != filename][:31]
    wiki.save_settings(code, options['query'], options)
    if not any(r['path'] == filename and r['language'] == code for r in wiki.settings(code)['archives']):
        raise OSError(5)
    return code


def registered():
    rows = [dict(r) for r in wiki.settings()['archives']]
    for row in rows:
        row['title'] = row['path'].rsplit('/', 1)[-1]
        row['missing'] = not fs.exists(row['path'])
    return rows


def forget(filename):
    options = wiki.settings()
    options['archives'] = [r for r in options['archives'] if r['path'] != filename]
    wiki.save_settings(options['language'], options['query'], options)
