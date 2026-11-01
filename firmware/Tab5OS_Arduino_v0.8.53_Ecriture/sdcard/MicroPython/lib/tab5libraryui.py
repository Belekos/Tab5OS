"""Touch forms for the isolated MicroPython catalogue and SD project folders."""
import tab5ui as ui
import tab5gui as gui
import tab5fs as fs
import tab5library as lib
tr = ui.tr

BOARD_LABELS = ('Portable / cible à vérifier', 'Tab5 ESP32-P4', 'ESP32', 'ESP32-S3',
                'ESP32-C3', 'ESP32-C6', 'ESP32-P4 (autre carte)', 'ESP8266',
                'RP2040 / Pico', 'RP2350 / Pico 2', 'STM32 / Pyboard', 'Autre carte')
CATEGORY_LABELS = ('Général', 'Capteurs', 'Écran', 'Réseau', 'Stockage', 'Moteurs', 'Audio')


def board_label(board):
    board = lib.BOARD_ALIASES.get(board, board)
    return tr(BOARD_LABELS[lib.BOARDS.index(board)]) if board in lib.BOARDS else board


def category_label(category):
    category = lib.CATEGORY_ALIASES.get(category, category)
    return tr(CATEGORY_LABELS[lib.CATEGORIES.index(category)]) if category in lib.CATEGORIES else category


def choose_board(all_boards=False):
    values = list(lib.BOARDS)
    labels = [tr(s) for s in BOARD_LABELS]
    if all_boards:
        values.insert(0, '*');labels.insert(0, tr('Toutes les cartes'))
    return gui.choose('Cible MicroPython', values, labels)


def choose_category(all_categories=False):
    values = list(lib.CATEGORIES)
    labels = [tr(s) for s in CATEGORY_LABELS]
    if all_categories:
        values.insert(0, '*');labels.insert(0, tr('Toutes les catégories'))
    return gui.choose('Usage de la bibliothèque', values, labels)


def text_form(title, label, initial=''):
    back = gui.start(title)
    ui.label(tr(label), 16, 80, 1060, 96)
    entry = ui.entry(initial, 16, 192, 1064, 52)
    ok = ui.button(tr('Continuer'), 700, 360, 380, 60)
    while True:
        key, value = gui.event()
        if key == back.id:
            return None
        if key == ok.id:
            return entry.text().strip()


def new_project():
    name = text_form('Nouveau projet MicroPython', 'Nom du projet')
    if name is None:
        return None
    board = choose_board()
    if board is None:
        return None
    folder = lib.create_project(name, board)
    gui.message('Projet créé', folder + '/main.py\n' + tr('Ouvrir main.py dans Écriture. Le dossier peut être ajouté aux projets de l’éditeur avec Documents → Importer un dossier.'))
    return folder


def pick_project():
    action = gui.choose('Projet destinataire', ['new', 'existing'],
                        [tr('Créer un projet'), tr('Choisir un dossier existant')])
    if action is None:
        return None
    if action == 'new':
        return new_project()
    start = lib.PROJECTS if fs.exists(lib.PROJECTS) else lib.LEGACY_PROJECTS if fs.exists(lib.LEGACY_PROJECTS) else '/MicroPython'
    folder = gui.pick(start, folder=True)
    if folder is None:
        return None
    data = lib.project(folder)
    if data is None:
        board = choose_board()
        if board is None:
            return None
        lib.project(folder, board)
    return folder


def import_local():
    kind = gui.choose('Importer depuis la SD / USB', ['module', 'folder'],
                      [tr('Un module .py ou .mpy'), tr('Un dossier de bibliothèque décompressé')])
    if kind is None:
        return
    source = gui.pick(folder=kind == 'folder')
    if source is None:
        return
    default = source.rsplit('/', 1)[-1]
    if kind == 'module':
        default = default.rsplit('.', 1)[0]
    name = text_form('Ranger la bibliothèque', 'Nom dans le catalogue', default)
    if name is None:
        return
    board = choose_board()
    if board is None:
        return
    category = choose_category()
    if category is None:
        return
    if not gui.confirm('Importer la bibliothèque', source + '\n' + board_label(board) + ' · ' + category_label(category) + '\n' + tr('Les fichiers seront copiés sans les exécuter. Cible déclarée, compatibilité non testée.')):
        return
    record = lib.import_local(source, name, board, category, gui.progress('Import de bibliothèque'))
    details(record)


def import_online():
    source = text_form('Obtenir les sources par Wi-Fi', 'Nom mip, adresse HTTPS ou github: du paquet')
    if not source:
        return
    version = text_form('Version du paquet', 'Version / tag (vide : version courante)', '')
    if version is None:
        return
    name = text_form('Ranger la bibliothèque', 'Nom dans le catalogue', source.rsplit('/', 1)[-1].replace('.json', '').replace('.py', ''))
    if name is None:
        return
    board = choose_board()
    if board is None:
        return
    category = choose_category()
    if category is None:
        return
    if not gui.confirm('Télécharger les sources', source + '\n' + tr('Les sources .py et dépendances mip seront copiées dans le catalogue SD, sans installation globale ni exécution.')):
        return
    record = lib.import_online(source, name, board, category, version or None, gui.progress('Téléchargement des sources'))
    details(record)


def details(record):
    while True:
        back = gui.start('Bibliothèque du catalogue')
        formats = record.get('formats', {})
        mpy = formats.get('mpy', 0)
        ui.label(str(record.get('name', ''))[:100], 16, 60, 1060, 50)
        ui.label(board_label(record['board']) + ' · ' + category_label(record['category']) + '\n' + tr('Cible déclarée — compatibilité non testée'), 16, 120, 1060, 74)
        status = tr('Sources .py conservées sur SD, sans exécution.')
        if mpy:
            status = str(mpy) + ' ' + tr('fichier(s) .mpy : version du firmware et architecture à vérifier sur la carte cible.')
        if record.get('dependencies') != 'mip-resolved':
            status += '\n' + tr('Dépendances locales à compléter si nécessaire.')
        ui.label(status, 16, 204, 1060, 116)
        ui.label(record['path'], 16, 332, 1060, 60)
        add = ui.button(tr('Ajouter à un projet'), 16, 420, 520, 58)
        info = ui.button(tr('Voir les fichiers'), 564, 420, 516, 58)
        key, value = gui.event()
        if key == back.id:
            return
        if key == info.id:
            selected = gui.pick(record['path'] + '/lib')
            if selected:
                gui.message('Fichier de bibliothèque', selected + '\n' + str(fs.stat(selected)[1]) + ' ' + tr('octets') + '\n' + tr('Ouvrir ce chemin dans Écriture pour consulter sa source.'))
        elif key == add.id:
            folder = pick_project()
            if folder is None:
                continue
            data = lib.project(folder)
            text = board_label(record['board']) + ' → ' + board_label(data['board']) + '\n' + folder + '/lib\n'
            text += tr('Copie de fichiers uniquement. Les fichiers différents déjà présents bloquent la copie. Compatibilité à vérifier sur la carte cible.')
            if mpy:
                text += '\n' + tr('Les fichiers .mpy ne sont pas convertis ni validés pour ce firmware.')
            if gui.confirm('Ajouter la bibliothèque', text):
                count = lib.attach(record, folder, gui.progress('Copie vers le projet'))
                gui.message('Bibliothèque ajoutée', str(count) + ' ' + tr('nouveaux fichiers') + '\n' + folder + '/lib\n' + tr('Le transfert vers la carte et le lancement du programme sont des opérations distinctes.'))


def browse():
    board = category = None
    while True:
        back = gui.start('Catalogue MicroPython sur SD')
        by_board = ui.button(tr('Carte : ') + (board_label(board) if board else tr('Toutes')), 16, 88, 1064, 62)
        by_category = ui.button(tr('Usage : ') + (category_label(category) if category else tr('Tous')), 16, 174, 1064, 62)
        show = ui.button(tr('Afficher les bibliothèques'), 16, 282, 1064, 72)
        ui.label(tr('Filtres par cible déclarée et usage. Le catalogue reste disponible hors ligne.'), 16, 382, 1060, 80)
        key, value = gui.event()
        if key == back.id:
            return
        if key == by_board.id:
            selected = choose_board(True)
            if selected is not None:
                board = None if selected == '*' else selected
        elif key == by_category.id:
            selected = choose_category(True)
            if selected is not None:
                category = None if selected == '*' else selected
        elif key == show.id:
            records = lib.catalogue(board, category)
            if not records:
                gui.message('Catalogue', tr('Aucune bibliothèque pour ces filtres. Utiliser Importer SD / USB ou Obtenir par Wi-Fi.'))
                continue
            labels = [str(r.get('name', '')) + ' · ' + board_label(r['board']) + ' · ' + category_label(r['category']) for r in records]
            record = gui.choose('Bibliothèques disponibles', records, labels, True)
            if record:
                details(record)


def main(tab5_manager=None):
    fs.recover()
    while True:
        choices = ['catalogue', 'local', 'online', 'project']
        labels = [tr('Catalogue sur SD'), tr('Importer SD / USB'), tr('Obtenir les sources par Wi-Fi'), tr('Nouveau projet MicroPython')]
        if tab5_manager:
            choices.append('tab5');labels.append(tr('Modules installés pour la Tab5'))
        action = gui.choose('Bibliothèques MicroPython · toutes cartes', choices, labels)
        if action is None:
            return
        try:
            if action == 'catalogue':
                browse()
            elif action == 'local':
                import_local()
            elif action == 'online':
                import_online()
            elif action == 'project':
                new_project()
            elif action == 'tab5':
                tab5_manager()
        except (OSError, ValueError, KeyError, TypeError, MemoryError) as error:
            gui.failure(error)
