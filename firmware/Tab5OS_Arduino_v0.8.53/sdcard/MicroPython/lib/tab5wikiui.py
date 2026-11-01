"""Wikipedia touch reader using the native Tab5OS widgets."""
import gc
import tab5ui as ui
import tab5gui as gui
import tab5fs as fs
import tab5wiki as wiki
from time import sleep_ms
tr = ui.tr


LANGUAGES = (
    ('en', 'Anglais'), ('fr', 'Français'), ('de', 'Allemand'),
    ('es', 'Espagnol'), ('it', 'Italien'), ('pt', 'Portugais'),
    ('nl', 'Néerlandais'), ('pl', 'Polonais'), ('ca', 'Catalan'),
    ('sv', 'Suédois'), ('no', 'Norvégien'), ('da', 'Danois'),
    ('fi', 'Finnois'), ('cs', 'Tchèque'), ('uk', 'Ukrainien'),
    ('ru', 'Russe'), ('el', 'Grec'), ('tr', 'Turc'),
    ('ar', 'Arabe'), ('he', 'Hébreu'), ('hi', 'Hindi'),
    ('ja', 'Japonais'), ('zh', 'Chinois'), ('ko', 'Coréen'),
    ('vi', 'Vietnamien'), ('id', 'Indonésien'), ('eo', 'Espéranto'),
    ('simple', 'Anglais simplifié'))


def language_name(code):
    code = wiki.language(code)
    for key, name in LANGUAGES:
        if code == key:
            return tr(name) + ' (' + code + ')'
    return code


def choose_language(current, title='Langue de Wikipédia'):
    codes = [row[0] for row in LANGUAGES]
    if current in codes:
        codes.remove(current)
    codes.insert(0, current)
    labels = [language_name(code) for code in codes]
    codes.append('custom'); labels.append(tr('Autre langue…'))
    chosen = gui.choose(title, codes, labels, searchable=True)
    if chosen != 'custom':
        return chosen
    value = current
    while True:
        back = gui.start('Autre langue Wikipédia')
        ui.label(tr('Code de la langue Wikipédia'), 16, 90, 1064, 46)
        ui.label(tr('Par exemple : br, eu, be-tarask, zh-min-nan'), 16, 146, 1064, 54)
        entry = ui.entry(value, 16, 218, 1064, 56)
        apply = ui.button(tr('Choisir cette langue'), 580, 352, 500, 64)
        while True:
            key, unused = gui.event()
            if key == back.id:
                return None
            if key == apply.id:
                value = entry.text()
                try:
                    return wiki.language(value)
                except ValueError as error:
                    failure(error)
                break


def clip(text, size=470):
    text = str(text)
    while len(text.encode()) > size:
        text = text[:-1]
    return text


def progress(title):
    gui.start(title, False)
    return ui.label('', 16, 126, 1060, 170)


def failure(error):
    code = error.args[0] if isinstance(error, OSError) and error.args else None
    messages = {27: 'Article trop volumineux pour ce lecteur',
                116: 'Cette édition a changé. Recommencer son téléchargement.',
                5007: 'Ce serveur ne permet pas la reprise. Réessayez avec un autre miroir.',
                5008: 'La taille reçue ne correspond pas à cette édition. Actualisez le catalogue.',
                5009: 'Le miroir a changé sa copie. Réessayez pour utiliser un autre miroir.',
                5010: 'Réponse du miroir invalide. Réessayez avec un autre miroir.',
                5012: 'La vérification de l’archive a échoué. Recommencez son téléchargement.',
                110: 'Connexion interrompue. Le téléchargement peut être repris.'}
    if code in messages:
        gui.message('Wikipédia', tr(messages[code]))
    else:
        gui.failure(error)


def open_reader(filename, title, illustrated=False, rich=False, can_save=True, can_update=True):
    from _tab5 import call
    flags = (1 if illustrated else 0) | (2 if rich else 0) | (0 if can_save else 4) | (0 if can_update else 8)
    call(90, flags, 0, 0, fs.path(filename).encode() + b'\0' + clip(title, 190).encode())
    while True:
        status = call(91)
        if status <= 0: break
        sleep_ms(50)
    if status < 0: raise OSError(-status)
    return call(92, 0, 0, 0, None, 768).decode()


def online():
    from _tab5 import call
    try:
        return bool(call(99))
    except OSError:
        return False


def show_image(path):
    from _tab5 import call
    call(95, 0, 0, 0, fs.path(path).encode())
    while call(94) > 0:
        sleep_ms(50)


def image_actions(record, index):
    import tab5wikiimages as images
    text = wiki.record_paths(record)[0]
    rows = fs.read_json(text + '.images.json', 32768) if fs.exists(text + '.images.json') else []
    row = next((r for r in rows if r.get('index') == index), None)
    if not row: return
    source = row.get('source') or images.original(row['url'])
    source = images.source(source)
    directory = '/Picture/Wikipedia'; fs.makedirs(directory)
    name = source.rsplit('/', 1)[-1]
    from tab5wikimarkup import unquote
    name = unquote(name)
    name = ''.join(c for c in name if ord(c) >= 32 and c not in '/\\:*?"<>|')[:140]
    if not name or name.startswith('.'): name = 'image-' + str(index) + '.jpg'
    destination = directory + '/' + name
    while True:
        choice = gui.choose('Illustration', ['view', 'save', 'source'],
                            [tr('Ouvrir l’image'), tr('Enregistrer l’image source'), tr('Source et licence')])
        if choice is None: return
        if choice == 'source':
            gui.message('Source et licence', clip(row.get('artist', '') + '\n' + row.get('license', '') + '\n' + source))
            continue
        try:
            if not fs.exists(destination):
                temp = destination + '.download'
                if fs.exists(temp): fs.remove(temp)
                fs.download(source, temp, 16777216, progress('Chargement de l’image'))
                fs.rename(temp, destination)
            if choice == 'save':
                gui.message('Wikipédia', tr('Image enregistrée') + '\n' + destination)
            elif destination.lower().endswith(('.jpg', '.jpeg', '.png', '.webp', '.bmp')):
                show_image(destination)
            else:
                # Vector originals stay available on SD; display their raster rendition.
                path = text + '.img%d' % index
                with open(path, 'rb') as stream: signature = stream.read(12)
                extension = '.png' if signature.startswith(b'\x89PNG') else '.webp' if signature.startswith(b'RIFF') else '.jpg'
                preview = directory + '/.preview' + extension
                fs.copy(path, preview); show_image(preview)
        except (OSError, ValueError, MemoryError) as error:
            failure(error)


def reader(record, connected=False):
    # A legacy saved extract stays readable offline. Its next online read upgrades it.
    connected = connected or online()
    if connected and record.get('rich', 0) < 3 and not record.get('text_only'):
        try:
            record = wiki.enrich(record, progress('Ouverture de l’article'))
        except (OSError, ValueError, MemoryError):
            gui.message('Wikipédia', tr('Enrichissement indisponible ; la copie enregistrée reste accessible.'))
    record = wiki.ensure_credit(record)
    illustrated = False
    if wiki.settings(record['language'])['illustrations']:
        import tab5wikiimages as images
        record = images.ensure(record, connected, progress('Chargement des illustrations') if connected else None)
        illustrated = True
        if record.get('image_failures'):
            gui.message('Wikipédia', tr('Certaines illustrations sont indisponibles. Le texte reste accessible.'))
    title = record['title'] + (' · ' + tr('Texte abrégé') if record.get('abridged') else '')
    selected = open_reader(wiki.record_paths(record)[0], title, illustrated, bool(record.get('rich')), bool(record.get('temporary')), connected)
    return record, selected


def article_page(record, connected=False):
    return open_result(record['language'], {'kind': 'saved', 'record': record, 'title': record['title'], 'connected': connected})


def linked_result(code, target, current):
    import tab5wikilibrary as library
    import tab5wikimarkup as markup
    # Saved pages take priority; the current archive then resolves redirects locally.
    title = target
    if len(title) > 2 and title[1] == '/':
        title = title[2:]
    title = markup.unquote(title).replace('_', ' ').split('#', 1)[0]
    saved = wiki.exact_saved(code, title)
    if saved:
        return {'kind': 'saved', 'record': saved, 'title': saved['title']}
    if current['kind'] == 'zim':
        return {'kind': 'zim', 'archive': current['archive'], 'link': target,
                'base': current.get('article_link', ''), 'title': title}
    local = library.exact(code, title)
    if local:
        return local
    return {'kind': 'online', 'title': title}


def article_actions(record, note=''):
    while True:
        options = wiki.settings(record['language'])
        back = gui.start('Wikipédia')
        ui.label(clip(record['title']), 16, 66, 1064, 100)
        ui.label(tr('Page temporaire') if record.get('temporary') else tr('Disponible hors ligne'), 16, 174, 1064, 46)
        read = ui.button(tr('Lire'), 16, 248, 500, 60)
        save = ui.button(tr('Enregistrer cette page'), 580, 248, 500, 60) if record.get('temporary') else None
        picture = ui.button(tr('Charger les illustrations'), 580, 248, 500, 60) if not save and options['illustrations'] else None
        update = ui.button(tr('Vérifier la mise à jour'), 16, 326, 1064, 60)
        ui.label(clip(note), 16, 408, 1064, 74)
        key, unused = gui.event()
        if key == back.id:
            return record, None
        if key == read.id:
            return record, False
        if save and key == save.id:
            record = wiki.save_article(record); note = tr('Page enregistrée sur la carte SD')
        elif picture and key == picture.id:
            return record, True
        elif key == update.id:
            try:
                revision = record.get('revision')
                record = wiki.refresh_article(record, progress('Vérification de l’article'))
                if not record.get('rich'):
                    record = wiki.enrich(record, progress('Ouverture de l’article'))
                note = tr('Article mis à jour') if record.get('revision') != revision else tr('Article à jour')
                return record, True
            except (OSError, ValueError, MemoryError) as error:
                failure(error)


def open_result(code, row):
    import tab5wikilibrary as library
    import tab5zim as zim
    current = dict(row); history = []; temporary = []; note = ''
    try:
        while True:
            options = wiki.settings(code); target = ''
            try:
                if current['kind'] == 'online':
                    local = library.exact(code, current['title']) if not current.get('skip_local') else None
                    if local:
                        current = dict(local)
                if current['kind'] == 'zim':
                    progress('Ouverture de l’article')
                    try:
                        article = zim.extract(current['archive'], current.get('id'), current.get('link'),
                                              current.get('base'), options['illustrations'])
                    except (OSError, ValueError):
                        # Try other installed editions/saved pages before any web request.
                        if not current.get('link'):
                            raise
                        fallback = library.exact(code, current['title'])
                        if fallback and fallback.get('archive') != current['archive']:
                            current = dict(fallback); continue
                        current = {'kind': 'online', 'title': current['title'], 'skip_local': True}; continue
                    current['id'] = article['id']; current.pop('link', None)
                    current['article_link'] = article.get('article_link', article.get('article_path', ''))
                    current['title'] = article['title']; wiki.remember(code, current['title'])
                    if article.get('image_failures'):
                        gui.message('Wikipédia', tr('Certaines illustrations sont indisponibles. Le texte reste accessible.'))
                    target = open_reader(article['path'], article['title'], options['illustrations'], article.get('rich', False), False, False)
                else:
                    connected = bool(current.get('connected'))
                    if current['kind'] == 'online':
                        widget = progress('Téléchargement Wikipédia')
                        record = (wiki.article(code, current['id'], widget, save=options['autosave']) if current.get('id') else
                                  wiki.by_title(code, current['title'], widget, save=options['autosave']))
                        current = {'kind': 'saved', 'record': record, 'title': record['title']}; connected = True
                        if record.get('temporary'):
                            temporary = [r for r in temporary if r['id'] != record['id']] + [record]
                    record = current['record']; wiki.remember(code, record['title'])
                    record, target = reader(record, connected); current['record'] = record; current['connected'] = False
                    if options['auto_update'] and not record.get('temporary') and wiki.update_due(record):
                        try:
                            record = wiki.refresh_article(record, progress('Vérification de l’article')); current['record'] = record
                        except (OSError, ValueError, MemoryError):
                            note = tr('Vérification indisponible ; la copie enregistrée reste accessible.')
                if target == '@images':
                    options['illustrations'] = not options['illustrations']
                    wiki.save_settings(code, options['query'], options)
                    current['connected'] = online(); continue
                if target == '@save':
                    if current['kind'] != 'zim':
                        current['record'] = wiki.save_article(current['record'])
                    gui.message('Wikipédia', tr('Page enregistrée sur la carte SD')); continue
                if target == '@update':
                    if current['kind'] != 'zim':
                        current['record'] = wiki.refresh_article(current['record'], progress('Vérification de l’article'))
                    else:
                        gui.message('Wikipédia', tr('Les éditions des archives se gèrent dans les options Wikipédia.'))
                    continue
                if target.startswith('@image:'):
                    if current['kind'] != 'zim':
                        image_actions(current['record'], int(target.split(':')[1]))
                    else:
                        path = article['path'] + '.img' + target.split(':')[1]
                        if fs.exists(path):
                            with open(path, 'rb') as stream: signature = stream.read(12)
                            extension = '.png' if signature.startswith(b'\x89PNG') else '.webp' if signature.startswith(b'RIFF') else '.jpg'
                            fs.makedirs('/Picture/Wikipedia')
                            destination = '/Picture/Wikipedia/zim-' + str(current['id']) + '-' + target.split(':')[1] + extension
                            fs.copy(path, destination); show_image(destination)
                    continue
                if target:
                    if len(history) >= 32:
                        evicted = history.pop(0)
                        old = evicted.get('record')
                        if old and old.get('temporary') and not any(h.get('record') == old for h in history):
                            wiki.discard_temporary(old)
                            temporary = [r for r in temporary if r != old]
                    history.append(dict(current)); current = linked_result(code, target, current); continue
                if history:
                    current = history.pop(); continue
                # Back returns directly to the originating result list.
                return
            except (OSError, ValueError, MemoryError) as error:
                failure(error)
                if history:
                    current = history.pop(); continue
                return
            finally:
                gc.collect()
    finally:
        for record in temporary:
            wiki.discard_temporary(record)
        if current.get('record'):
            wiki.discard_temporary(current['record'])
        gc.collect()


def search_articles(code, query):
    import tab5wikilibrary as library
    cursor = None; history = []; page = 1
    while True:
        widget = progress('Recherche Wikipédia')
        issues = []
        rows, following = library.search_page(code, query, cursor, online(), widget, issues)
        while True:
            back = gui.start('Recherche Wikipédia')
            buttons = {}; actions = {}
            for index, row in enumerate(rows):
                annotation = tr('En ligne') if row['kind'] == 'online' else tr('Hors ligne')
                button = ui.button(clip(row['title'] + ' · ' + annotation), 16, 62 + index * 55, 960 if row['kind'] != 'online' else 1064, 49)
                buttons[button.id] = row
                if row['kind'] != 'online':
                    more = ui.button('…', 992, 62 + index * 55, 88, 49)
                    actions[more.id] = row
            if not rows: ui.label(tr('Aucun article trouvé'), 16, 130, 1064, 100)
            if issues:
                ui.label(tr('Connexion indisponible') if issues[0] == 'Connexion indisponible' else tr('Archive indisponible'), 214, 394, 660, 32)
            ui.label(str(page), 430, 436, 240, 42)
            previous = ui.button('<', 16, 426, 180, 54) if history else None
            next_page = ui.button('>', 900, 426, 180, 54) if following is not None else None
            if previous: ui.shortcut(previous, 'previous')
            if next_page: ui.shortcut(next_page, 'next')
            key, value = gui.event()
            if key == back.id: return
            if key in buttons:
                open_result(code, buttons[key]); gc.collect()
            elif key in actions:
                row = actions[key]
                title = 'Supprimer la copie hors ligne' if row['kind'] == 'saved' else 'Supprimer cette archive'
                if gui.choose(row['title'], ['delete'], [tr(title)]) == 'delete':
                    detail = row['title'] if row['kind'] == 'saved' else (row['archive'].get('path') or row['archive'].get('filename', ''))
                    if gui.confirm(title, detail):
                        if row['kind'] == 'saved': wiki.remove_article(row['record'])
                        else: library.remove_archive(row['archive'])
                        cursor = None; history = []; page = 1; break
            elif next_page and key == next_page.id:
                history.append(cursor); cursor = following; page += 1
                if len(history) > 32: history.pop(0)
                break
            elif previous and key == previous.id:
                cursor = history.pop(); page -= 1; break


def size_text(size):
    if size >= 1073741824:
        return '%.2f ' % (size / 1073741824) + tr('Gio')
    return '%.1f ' % (size / 1048576) + tr('Mio')


def archive_label(state):
    edition = tr('Tous les articles, sans images') if state.get('variant') == 'nopic' else tr('Tous les articles, avec images')
    return language_name(state['language']) + ' · ' + edition + ' · ' + state.get('date', '') + ' · ' + size_text(state['size'])


def download_archive(state):
    import tab5zim as zim
    state = zim.load(zim.location(state), True)
    if state['complete']:
        archive_reader(state); return
    gui.start('Téléchargement Wikipédia', False)
    ui.label(clip(language_name(state['language']) + '\n' + state['filename']), 16, 16, 1064, 68)
    summary = ui.label('', 16, 104, 1064, 66)
    current = ui.label('', 16, 190, 1064, 80)
    pause = ui.button(tr('Pause'), 16, 338, 500, 64)
    ui.shortcut(pause, 'back')
    ui.label(tr('Vous pouvez quitter cet écran et reprendre le téléchargement plus tard.'), 16, 414, 1064, 72)
    try:
        while not state['complete']:
            if not ui.is_open():
                raise KeyboardInterrupt
            event = ui.poll()
            if event and event[0] == pause.id:
                return
            summary.text(size_text(state['done']) + ' / ' + size_text(state['size']) + ' · %.1f%%' % (100 * state['done'] / state['size']))
            zim.step(state, current, pause)
            sleep_ms(1)
    except OSError as error:
        if error.args and error.args[0] in (125, 5006):
            return
        raise
    gui.message('Wikipédia hors ligne', tr('Archive complète disponible hors ligne'))


def archive_choice(title, items, labels, refreshable):
    page = 0
    while True:
        back = gui.start(title)
        page = max(0, min(page, max(0, (len(items) - 1) // 6)))
        buttons = {}
        for position, index in enumerate(range(page * 6, min(len(items), page * 6 + 6))):
            can_refresh = refreshable(items[index])
            y = 74 + position * 56
            row = ui.button(clip(labels[index], 132), 16, y, 952 if can_refresh else 1064, 50)
            buttons[row.id] = (items[index], False)
            if can_refresh:
                refresh = ui.button('↻', 980, y, 100, 50)
                buttons[refresh.id] = (items[index], True)
        ui.label(str(page + 1) + ' / ' + str(max(1, (len(items) + 5) // 6)), 350, 432, 380, 48)
        prev = ui.button('<', 16, 428, 180, 50) if page else None
        nxt = ui.button('>', 900, 428, 180, 50) if (page + 1) * 6 < len(items) else None
        if prev: ui.shortcut(prev, 'previous')
        if nxt: ui.shortcut(nxt, 'next')
        while True:
            key, value = gui.event()
            if key == back.id: return None, False
            if key in buttons: return buttons[key]
            if prev and key == prev.id: page -= 1; break
            if nxt and key == nxt.id: page += 1; break


def choose_archive(code):
    import tab5zim as zim
    rows = zim.catalog(code, progress('Catalogue Wikipédia'))
    if not rows:
        gui.message('Wikipédia', tr('Aucune archive complète disponible dans cette langue')); return
    chosen, refresh = archive_choice(tr('Wikipédia complet') + ' · ' + language_name(code), rows, [archive_label(row) for row in rows], lambda row: fs.exists(zim.location(row) + '/manifest.json'))
    if chosen is None:
        return
    install_archive(chosen, refresh)


def install_archive(chosen, force=False):
    import tab5zim as zim
    chosen = zim.resolve(chosen, progress('Préparation de l’archive'))
    free = zim.storage()
    manifest = zim.location(chosen) + '/manifest.json'
    existing = zim.load(zim.location(chosen), not force) if fs.exists(manifest) else dict(chosen, done=0, complete=False)
    if existing['complete'] and not force:
        archive_reader(existing); return
    remaining = max(0, chosen['size'] - existing['done']) if force else existing['size'] - existing['done']
    text = archive_label(chosen) + '\n' + tr('Espace libre') + ' : ' + size_text(free)
    if free < remaining + 65536:
        gui.message('Wikipédia complet', text + '\n' + tr('Espace SD insuffisant pour cette archive')); return
    text += '\n' + tr('Lecture du texte sur la tablette. Pause et reprise disponibles.')
    if force:
        text += '\n' + tr('L’archive téléchargée sera remplacée. Vos articles enregistrés seront conservés.')
    if gui.confirm('Recommencer le téléchargement' if force else 'Télécharger Wikipédia complet', text):
        download_archive(zim.restart(chosen, resolved=chosen) if force else zim.begin(chosen))


def archive_reader(state):
    import tab5zim as zim
    query = ''; cursor = 0; history = []
    while True:
        back = gui.start('Wikipédia hors ligne')
        entry = ui.entry(query, 156, 0, 650, 46, submit=True)
        search = ui.button(tr('Rechercher'), 820, 0, 260, 46)
        ui.shortcut(search, 'enter')
        ui.label(tr('Début du titre de l’article'), 16, 56, 1064, 38)
        result = zim.search(state, query, cursor)
        rows = {}; following = result.get('next_cursor')
        for index, record in enumerate(result.get('entries', [])):
            button = ui.button(clip(record['title']), 16, 102 + index * 51, 1064, 46)
            rows[button.id] = record
        if not rows:
            ui.label(tr('Aucun article trouvé. Vérifier les majuscules du titre.'), 16, 148, 1064, 110)
        previous = ui.button('<', 16, 436, 180, 46) if history else None
        next_page = ui.button('>', 900, 436, 180, 46) if following is not None else None
        if previous: ui.shortcut(previous, 'previous')
        if next_page: ui.shortcut(next_page, 'next')
        ui.label(str(len(history) + 1), 460, 438, 200, 42)
        while True:
            key, value = gui.event()
            if key == back.id:
                return
            if key == search.id or (key == entry.id and value == 1):
                query = entry.text().strip(); cursor = 0; history = []; break
            if previous and key == previous.id:
                cursor = history.pop(); break
            if next_page and key == next_page.id:
                history.append(cursor); cursor = following; break
            if key in rows:
                open_result(state['language'], {'kind': 'zim', 'archive': state, 'id': rows[key]['id'], 'title': rows[key]['title']})
                gc.collect(); break


def resume_archive(code):
    import tab5zim as zim
    rows = zim.archives(code, False)
    if not rows:
        gui.message('Wikipédia', tr('Aucun téléchargement en attente dans cette langue')); return
    chosen = gui.choose('Reprendre un téléchargement', rows,
                        [archive_label(row) + ' · %.1f%%' % (100 * row['done'] / row['size']) for row in rows])
    if chosen:
        action = gui.choose('Téléchargement Wikipédia', ['resume', 'restart'], [tr('Reprendre'), tr('Recommencer')])
        if action == 'restart':
            if not gui.confirm('Recommencer le téléchargement', tr('Les données déjà téléchargées pour cette archive seront remplacées.')):
                return
            chosen = zim.restart(chosen, progress('Préparation de l’archive'))
        if action is not None:
            download_archive(chosen)


def check_archives(code, silent=False):
    import tab5zim as zim
    installed = zim.archives(code, True)
    if not installed:
        if not silent:
            gui.message('Wikipédia', tr('Aucune archive complète enregistrée dans cette langue'))
        return
    latest = zim.catalog(code, progress('Vérification des archives'))
    if not latest:
        raise ValueError('Catalogue Wikipédia indisponible pour cette langue')
    options = wiki.settings(code)
    checks = options['archive_checks']
    if code not in checks and len(checks) >= 8:
        checks = {}
    checks[code] = wiki.now(); options['archive_checks'] = checks
    wiki.save_settings(options['language'], options['query'], options)
    newer = [r for r in latest if any(old.get('variant') == r['variant'] and old.get('date', '') < r['date'] for old in installed)]
    if not newer:
        if not silent:
            gui.message('Wikipédia', tr('Les archives enregistrées sont à jour'))
        return
    chosen = gui.choose('Nouvelle édition Wikipédia', newer, [archive_label(r) for r in newer])
    if chosen:
        install_archive(chosen)


def preferences(code):
    while True:
        options = wiki.settings(code)
        on = tr('Activé'); off = tr('Désactivé')
        items = ['illustrations', 'autosave', 'auto_update', 'auto_archives']
        labels = [tr('Illustrations dans les articles') + ' · ' + (on if options['illustrations'] else off),
                  tr('Enregistrer automatiquement les pages lues') + ' · ' + (on if options['autosave'] else off),
                  tr('Actualiser les articles enregistrés') + ' · ' + (on if options['auto_update'] else off),
                  tr('Vérifier les éditions au démarrage') + ' · ' + (on if options['auto_archives'] else off)]
        selected = gui.choose('Préférences Wikipédia', items, labels)
        if selected is None:
            return
        if not options[selected] and selected in ('auto_update', 'auto_archives'):
            explanation = ('Vérifier les articles après lecture, au plus une fois par jour. Les nouvelles versions remplacent leur copie enregistrée. Les archives complètes restent inchangées.' if selected == 'auto_update' else
                           'Vérifier le catalogue une fois par semaine à l’ouverture. Une nouvelle édition sera proposée ; son téléchargement complet demandera votre confirmation.')
            if not gui.confirm('Mises à jour Wikipédia', tr(explanation)):
                continue
        options[selected] = not options[selected]
        wiki.save_settings(code, options['query'], options)


def automatic_archives(code):
    options = wiki.settings(code)
    if not options['auto_archives']:
        return
    now = wiki.now(); previous = options['archive_checks'].get(code, 0)
    if now > 100000000 and now - previous >= 604800:
        try:
            check_archives(code, True)
        except (OSError, ValueError, KeyError, TypeError, MemoryError):
            pass
        finally:
            gc.collect()


def options_menu(code):
    while True:
        options = wiki.settings(code)
        items = ['preferences', 'complete', 'resume', 'update', 'import']
        labels = [tr('Préférences de lecture'), tr('Télécharger Wikipédia complet'),
                  tr('Reprendre un téléchargement'), tr('Vérifier les éditions des archives'),
                  tr('Ajouter une archive ZIM de la carte SD')]
        selected = gui.choose('Options Wikipédia', items, labels)
        if selected is None:
            return code
        if selected == 'preferences':
            preferences(code)
        elif selected == 'complete':
            chosen = choose_language(code, 'Langue de l’encyclopédie à télécharger')
            if chosen:
                code = chosen; wiki.save_settings(code, options['query']); choose_archive(code)
        elif selected == 'resume':
            resume_archive(code)
        elif selected == 'update':
            check_archives(code)
        elif selected == 'import':
            filename = gui.pick('/')
            if filename:
                import tab5wikilibrary as library
                chosen = choose_language(code, 'Langue de l’archive ZIM')
                if not chosen:
                    continue
                progress('Vérification de l’index ZIM')
                code = library.register(chosen, filename)
                wiki.save_settings(code, options['query'])
                gui.message('Wikipédia', tr('Archive ajoutée sans copie du fichier') + '\n' + filename + '\n' + language_name(code))
                registered_archives(code)


def refresh_record(row):
    import tab5zim as zim
    if row['path'].endswith('/manifest.json'):
        return zim.load(row['path'].rsplit('/', 1)[0])
    name = row['path'].rsplit('/', 1)[-1]
    try:
        zim._filename(name, row['language'])
    except ValueError:
        return None
    return {'filename': name, 'language': row['language'], 'title': row['title'],
            'variant': name.split('_all_', 1)[1].split('_')[0], 'date': name[-11:-4],
            'source': 'https://download.kiwix.org/zim/wikipedia/' + name, 'size': 80}


def registered_archives(code):
    import tab5wikilibrary as library
    import tab5zim as zim
    while True:
        rows = library.registered()
        # Complete downloads live in their own manifests, but open through the same reader.
        for state in zim.archives(code, True):
            path = zim.location(state) + '/manifest.json'
            if not any(r['path'] == path for r in rows):
                rows.append({'path': path, 'language': code, 'title': state['filename'], 'missing': False})
        if not rows:
            gui.message('Wikipédia', tr('Aucune archive ajoutée. Options → Ajouter une archive ZIM de la carte SD.'))
            return
        labels = [r['language'] + ' · ' + r['title'] + (' · ' + tr('Introuvable') if r['missing'] else '') for r in rows]
        chosen, refresh = archive_choice('Archives disponibles', rows, labels,
                                        lambda row: refresh_record(row) is not None)
        if chosen is None:
            return
        if refresh:
            state = refresh_record(chosen)
            install_archive(state, True)
            continue
        actions = ['open']; labels = [tr('Lire')]
        if not chosen['path'].endswith('/manifest.json'):
            actions += ['language', 'remove']
            labels += [tr('Changer la langue'), tr('Retirer de la liste')]
        action = gui.choose(chosen['title'], actions, labels)
        if action == 'remove':
            if gui.confirm('Retirer de la liste', chosen['path'] + '\n' + tr('Le fichier sur la carte SD sera conservé.')):
                library.forget(chosen['path'])
        elif action == 'language':
            language = choose_language(chosen['language'], 'Langue de l’archive ZIM')
            if language:
                library.register(language, chosen['path'])
        elif action == 'open':
            filename = chosen['path']
            state = zim.load(filename.rsplit('/', 1)[0], True) if filename.endswith('/manifest.json') else {'complete': True, 'path': filename, 'language': chosen['language']}
            try:
                archive_reader(state)
            except (OSError, ValueError, TypeError, KeyError, MemoryError) as error:
                failure(error)


def main():
    fs.recover()
    defaults = wiki.settings('en')
    code = defaults['language']; query = defaults['query']
    automatic_archives(code)
    while True:
        back = gui.start('Wikipédia')
        lang = ui.button(language_name(code), 156, 0, 630, 46)
        options = ui.button(tr('Options'), 804, 0, 276, 46)
        ui.label(tr('Sujet ou titre d’article'), 16, 92, 1064, 42)
        entry = ui.entry(query, 16, 144, 1064, 54, submit=True)
        search = ui.button(tr('Rechercher'), 16, 224, 1064, 64)
        recent = ui.button(tr('Lectures récentes'), 16, 328, 500, 60)
        browse = ui.button(tr('Articles disponibles'), 580, 328, 500, 60)
        archives = ui.button(tr('Archives disponibles'), 16, 408, 1064, 54)
        ui.label('Wikipedia · CC BY-SA 4.0', 16, 468, 1064, 32)
        key, value = gui.event()
        if key == back.id:
            return
        if key not in (lang.id, search.id, recent.id, options.id, browse.id, entry.id, archives.id):
            continue
        if key == entry.id and value != 1:
            continue
        try:
            query = entry.text().strip()
            wiki.save_settings(code, query)
            if key == lang.id:
                chosen = choose_language(code)
                if chosen:
                    code = chosen; wiki.save_settings(code, query)
            elif key in (search.id, entry.id):
                search_articles(code, query)
            elif key == browse.id:
                search_articles(code, '')
            elif key == archives.id:
                registered_archives(code)
            elif key == options.id:
                code = options_menu(code)
            elif key == recent.id:
                rows = wiki.settings(code)['recent']
                chosen = gui.choose('Lectures récentes', rows, [r['language'] + ' · ' + r['title'] for r in rows])
                if chosen:
                    code = wiki.language(chosen['language']); query = chosen['title']
                    wiki.save_settings(code, query)
                    import tab5wikilibrary as library
                    local = library.exact(code, query)
                    if local:
                        open_result(code, local)
                    else:
                        search_articles(code, query)
        except (OSError, ValueError, TypeError, KeyError, MemoryError) as error:
            failure(error)
        gc.collect()


def run():
    try:
        main()
    except KeyboardInterrupt:
        pass
    finally:
        ui.close()
