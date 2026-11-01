"""Optional article thumbnails, cached with source/credit information on SD."""
import tab5fs as fs
import tab5wiki as wiki

MAX_BYTES = 131072
MAX_IMAGES = 8


def source(url):
    if (not isinstance(url, str) or not url.startswith(('https://upload.wikimedia.org/wikipedia/', 'https://thumb.wikimedia.org/wikipedia/'))
            or len(url.encode()) > 1024 or any(ord(c) < 33 or c in '\\?#' for c in url)):
        raise ValueError('Illustration Wikipédia indisponible')
    return url


def plain(value, limit=240):
    from tab5wikimarkup import unescape
    if not isinstance(value, str):
        return ''
    result = ''; tag = False
    for character in value[:4096]:
        if character == '<':
            tag = True
        elif character == '>':
            tag = False
        elif not tag and (ord(character) >= 32 or character in '\n\t'):
            result += character
            if len(result) >= limit:
                break
    result = ' '.join(unescape(result).split())
    while len(result.encode()) > limit:
        result = result[:-1]
    return result


def header(path):
    with open(path, 'rb') as stream:
        data = stream.read(24)
    return len(data) == 24 and (data[:2] == b'\xff\xd8' or data[:8] == b'\x89PNG\r\n\x1a\n' or
                              data[:4] == b'RIFF' and data[8:12] == b'WEBP')


def cached(record):
    text = wiki.record_paths(record)[0]
    if not record.get('rich'):
        return True if fs.exists(text + '.image') and header(text + '.image') else None
    path = text + '.media'
    if not fs.exists(path) or fs.stat(path)[1] > 4096:
        return None
    with open(path) as stream:
        for unused in range(MAX_IMAGES):
            line = stream.readline(353)
            if not line or len(line) > 352:
                break
            values = line.rstrip('\n').split('\t')
            if len(values) == 2 and fs.exists(values[1]):
                return True
    return None


def thumbnail(url):
    url = source(url.split('?', 1)[0].split('#', 1)[0])
    # Wikimedia supplies rendered raster thumbnails, including SVG originals.
    parts = url.rsplit('/', 1)
    if '/thumb/' in url and parts[-1].split('px-', 1)[0].isdigit():
        width = int(parts[-1].split('px-', 1)[0])
        if width > 330 or width not in (20, 40, 60, 120, 250, 330):
            url = parts[0] + '/330px-' + parts[-1].split('px-', 1)[1]
    return url.replace('https://upload.wikimedia.org/wikipedia/', 'https://thumb.wikimedia.org/wikipedia/') if '/thumb/' in url else url


def original(url):
    url = source(url.split('?', 1)[0].split('#', 1)[0])
    if '/thumb/' in url:
        url = url.rsplit('/', 1)[0].replace('/thumb/', '/', 1)
    return url.replace('https://thumb.wikimedia.org/', 'https://upload.wikimedia.org/')


def credits(code, row, progress=None):
    from tab5wikimarkup import unquote
    description = row.get('description', '')
    if description.startswith('//'):
        description = 'https:' + description
    elif description.startswith('/'):
        description = 'https://' + code + '.wikipedia.org' + description
    elif description.startswith('./'):
        description = 'https://' + code + '.wikipedia.org/wiki/' + description[2:]
    result = {'source': original(row['url']), 'description': description or row['url'], 'artist': '', 'license': '', 'license_url': ''}
    if '/wiki/File:' not in description:
        return result
    name = unquote(description.split('/wiki/', 1)[1])
    data = wiki.api(code, [('prop', 'imageinfo'), ('titles', name), ('iiprop', 'url|extmetadata'),
                         ('iiextmetadatafilter', 'Artist|LicenseShortName|LicenseUrl')], 16384, progress)
    pages = data.get('query', {}).get('pages', [])
    infos = pages[0].get('imageinfo', []) if pages else []
    if infos:
        info = infos[0]; metadata = info.get('extmetadata', {})
        result['description'] = info.get('descriptionurl', result['description'])
        if info.get('url'): result['source'] = source(info['url'])
        for key, name in (('artist', 'Artist'), ('license', 'LicenseShortName'), ('license_url', 'LicenseUrl')):
            result[key] = plain(metadata.get(name, {}).get('value', ''))
    return result


def ensure(record, fetch=False, progress=None):
    text, metadata = wiki.record_paths(record)
    if not record.get('rich') or not fs.exists(text + '.sources'):
        return record
    rows = fs.read_json(text + '.sources', 32768)
    if not isinstance(rows, list) or len(rows) > MAX_IMAGES:
        raise ValueError('Illustration Wikipédia indisponible')
    known = fs.read_json(text + '.images.json', 32768) if fs.exists(text + '.images.json') else []
    if not isinstance(known, list) or len(known) > MAX_IMAGES:
        known = []
    known = [value for value in known if isinstance(value, dict)]
    values = []; failures = 0
    for index, row in enumerate(rows):
        if (not isinstance(row, dict) or not isinstance(row.get('offset'), int)
                or not 0 <= row['offset'] <= record.get('bytes', 0)):
            raise ValueError('Illustration Wikipédia indisponible')
        path = text + '.img%d' % index
        existing = None
        for value in known:
            if value.get('index') == index and value.get('url') == row.get('url'):
                existing = value; break
        if existing and fs.exists(path) and fs.stat(path)[1] == existing.get('bytes'):
            values.append(existing); continue
        if not fetch:
            continue
        temporary = path + '.download'
        try:
            if fs.exists(temporary):
                fs.remove(temporary)
            url = thumbnail(row['url'])
            fs.download(url, temporary, MAX_BYTES, progress)
            if not header(temporary):
                raise ValueError('Format d’illustration non pris en charge')
            value = {'index': index, 'offset': int(row['offset']), 'url': row['url'],
                     'bytes': fs.stat(temporary)[1], 'description': row.get('description') or row['url'],
                     'artist': '', 'license': '', 'license_url': '', 'source': original(row['url'])}
            # Missing metadata must not hide a valid image. Its source remains visible.
            try:
                value.update(credits(record['language'], row, progress))
            except (OSError, ValueError, KeyError, TypeError) as error:
                if isinstance(error, OSError) and error.args and error.args[0] == 125:
                    raise
                value['credit_unavailable'] = True
            with fs.Transaction() as transaction:
                fs.copy(temporary, transaction.allocate(path)); transaction.commit()
            values.append(value)
        except (OSError, ValueError, KeyError, TypeError, MemoryError) as error:
            if isinstance(error, OSError) and error.args and error.args[0] == 125:
                raise
            if isinstance(error, MemoryError):
                failures += len(rows) - index
                break
            failures += 1
        finally:
            if fs.exists(temporary):
                fs.remove(temporary)
    updated = dict(record); updated['image_failures'] = failures
    from tab5ui import tr
    with fs.Transaction() as transaction:
        staged = transaction.allocate(text + '.media')
        with open(staged, 'w') as destination:
            for value in values:
                destination.write('%d\t%s.img%d\n' % (value['offset'], text, value['index']))
        # Append attribution once per successfully downloaded image.
        credited = list(record.get('image_credits', [])); additions = [v for v in values if v['index'] not in credited]
        if additions:
            staged = transaction.allocate(text); fs.copy(text, staged)
            with open(staged, 'ab') as destination:
                for value in additions:
                    footer = ('\n\n' + tr('Illustration') + ' : ' + value.get('artist', '') + '\n' +
                              value.get('license', '') + '\n' + value.get('license_url', '') + '\n' +
                              value['description'] + '\n').encode()
                    if destination.write(footer) != len(footer):
                        raise OSError(28)
                    credited.append(value['index'])
            updated['bytes'] = fs.stat(staged)[1]; updated['image_credits'] = credited
        transaction.json(text + '.images.json', values); transaction.json(metadata, updated); transaction.commit()
    return updated
