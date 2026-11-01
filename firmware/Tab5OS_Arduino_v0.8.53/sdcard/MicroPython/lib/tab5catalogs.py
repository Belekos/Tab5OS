"""Radio Browser catalogue, downloaded to SD and read in bounded pages."""
import json
import tab5fs as fs
import tab5ui as ui
import tab5gui as gui
from time import sleep_ms
tr = ui.tr
TEMP = '/MicroPython/.catalog-response.json'
_servers = None


def quote(value):
    return ''.join(chr(c) if 48 <= c <= 57 or 65 <= c <= 90 or 97 <= c <= 122 or c in (45,46,95,126) else '%%%02X' % c for c in str(value).encode())


def clean(value, limit):
    value = ''.join(c for c in str(value) if ord(c) >= 32).strip()
    while len(value.encode()) > limit:
        value = value[:-1]
    return value


def fetch_json(url, limit, progress=None):
    if fs.exists(TEMP):
        fs.remove(TEMP)
    try:
        fs.download(url, TEMP, limit, progress)
        return fs.read_json(TEMP, limit)
    finally:
        if fs.exists(TEMP):
            fs.remove(TEMP)


def radio_servers(progress=None):
    global _servers
    if _servers:
        return _servers
    # /json/servers is the API's documented discovery fallback for clients without DNS APIs.
    data = fetch_json('https://all.api.radio-browser.info/json/servers', 8192, progress)
    if not isinstance(data, list):
        raise ValueError('Catalogue radio invalide')
    names = []
    for row in data[:32]:
        host = row.get('name', '') if isinstance(row, dict) else ''
        if isinstance(host, str) and host.endswith('.api.radio-browser.info') and len(host) < 100 and all(c in 'abcdefghijklmnopqrstuvwxyz0123456789.-' for c in host) and host not in names:
            names.append(host)
    if not names:
        raise ValueError('Aucun serveur Radio Browser disponible')
    _servers = names
    return names


def radio_search(name='', country='', tag='', codec='', offset=0, progress=None):
    global _servers
    country = country.strip().upper()
    if country and (len(country) != 2 or not all('A' <= c <= 'Z' for c in country)):
        raise ValueError('Pays : code de deux lettres, par exemple FR ou GB')
    if codec not in ('', 'MP3', 'AAC'):
        raise ValueError('Format audio incompatible')
    params = [('name', clean(name, 96)), ('countrycode', country), ('tag', clean(tag, 64)), ('codec', codec), ('hidebroken', 'true'), ('order', 'clickcount'), ('reverse', 'true'), ('limit', '24'), ('offset', max(0, int(offset)))]
    suffix = '/json/stations/search?' + '&'.join(k + '=' + quote(v) for k,v in params if str(v))
    error = None
    for host in radio_servers(progress)[:4]:
        try:
            data = fetch_json('https://' + host + suffix, 98304, progress)
            if not isinstance(data, list) or len(data) > 24:
                raise ValueError('Catalogue radio invalide')
            results = []
            for row in data:
                if not isinstance(row, dict) or row.get('hls') or not row.get('lastcheckok'):
                    continue
                audio = str(row.get('codec', '')).upper()
                if audio not in ('MP3', 'AAC', 'AAC+'):
                    continue
                url = str(row.get('url_resolved') or row.get('url') or '')
                if not url.startswith(('http://', 'https://')) or len(url.encode()) > 512 or any(ord(c) < 33 for c in url) or '.m3u8' in url.lower():
                    continue
                title = clean(row.get('name', ''), 128)
                if title:
                    results.append({'name': title, 'url': url, 'style': clean(row.get('tags', ''), 160) or 'Radio Browser', 'enabled': True, 'country': clean(row.get('countrycode', ''), 2), 'codec': audio, 'bitrate': int(row.get('bitrate') or 0)})
            return results, len(data) == 24
        except OSError as e:
            if not e.args or e.args[0] in (12, 28, 125, 5001, 5002, 5003, 5006):
                raise
            error = e
    _servers = None
    if error:
        raise error
    raise ValueError('Aucun serveur Radio Browser disponible')


def import_station(station):
    import tab5workshop as work
    if fs.exists(TEMP):
        fs.remove(TEMP)
    try:
        fs.write_json(TEMP, {'radios': [station]})
        return work.import_radios(TEMP)
    finally:
        if fs.exists(TEMP):
            fs.remove(TEMP)


def radios(manual):
    name = ''; country = ''; tag = ''; codec = 0
    while True:
        back = gui.start('Radios en ligne')
        ui.label('Radio Browser', 16, 66, 1060, 40)
        ui.label(tr('Nom de la radio'), 16, 116, 700, 36)
        q = ui.entry(name, 16, 158, 700, 48)
        ui.label(tr('Pays (code, facultatif)'), 750, 116, 330, 36)
        c = ui.entry(country, 750, 158, 330, 48)
        ui.label(tr('Genre musical (facultatif)'), 16, 228, 700, 36)
        t = ui.entry(tag, 16, 270, 700, 48)
        ui.label(tr('Format'), 750, 228, 330, 36)
        f = ui.dropdown([tr('Tous'), 'MP3', 'AAC'], 750, 270, 330, 48, codec)
        search = ui.button(tr('Rechercher'), 16, 360, 510, 62)
        local = ui.button(tr('Import manuel'), 570, 360, 510, 62)
        while True:
            key, value = gui.event()
            if key == f.id:
                codec = value
                continue
            break
        name = q.text().strip(); country = c.text().strip(); tag = t.text().strip()
        if key == back.id:
            return
        try:
            if key == local.id:
                manual()
            elif key == search.id:
                offset = 0
                while True:
                    items, more = radio_search(name, country, tag, ('','MP3','AAC')[codec], offset, gui.progress('Recherche de radios'))
                    choices = list(items)
                    labels = [r['name'] + ' · ' + r['country'] + ' · ' + r['codec'] + ' ' + str(r['bitrate']) for r in items]
                    if offset:
                        choices.append('previous'); labels.append(tr('Résultats précédents'))
                    if more:
                        choices.append('next'); labels.append(tr('Résultats suivants'))
                    if not choices:
                        gui.message('Radios', tr('Aucune radio compatible trouvée.')); break
                    chosen = gui.choose('Radio Browser', choices, labels)
                    if chosen is None:
                        break
                    if chosen == 'previous':
                        offset = max(0, offset-24); continue
                    if chosen == 'next':
                        offset += 24; continue
                    if gui.confirm('Ajouter la radio', chosen['name'] + '\n' + chosen['style']):
                        import_station(chosen)
                        gui.message('Radios', tr('Radio ajoutée. Recharger Musique pour actualiser la liste.'))
        except (OSError, ValueError, KeyError, TypeError, MemoryError) as e:
            gui.failure(e)



def maps(manual):
    import tab5regions
    tab5regions.main(manual)
