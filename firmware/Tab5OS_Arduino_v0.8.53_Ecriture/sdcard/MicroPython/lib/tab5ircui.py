"""Touch/USB IRC app. Polling continues when the OS hides this app."""
import tab5ui as ui
from tab5irc import Client
from tab5ircprefs import Preferences, SERVERS, channel_name
from time import sleep_ms, ticks_ms, ticks_diff

tr = ui.tr

def clip(text, limit=480):
    while len(text.encode()) > limit:
        text = text[:-1]
    return text


def error_text(error):
    if isinstance(error, OSError) and error.args:
        code = error.args[0]
        known = {12: 'Mémoire insuffisante',
                 5001: 'Régler la date et l’heure pour vérifier les certificats',
                 5002: 'Réseau occupé : arrêter la radio puis réessayer',
                 5003: 'Connecter le Wi-Fi dans Réglages',
                 5007: 'Connexion IRC TLS impossible : vérifier le serveur, le port, la date et les certificats',
                 107: 'IRC déconnecté', 128: 'IRC déconnecté',
                 110: 'Le serveur IRC ne répond plus', 116: 'Le serveur IRC ne répond plus',
                 125: 'Connexion IRC annulée', 140: 'Connexion IRC annulée', 32: 'Envoi IRC interrompu',
                 5: 'Réception IRC interrompue'}
        return tr(known.get(code, 'Erreur IRC')) + (' (' + str(code) + ')' if code not in known else '')
    return tr(str(error))


def short(text, limit=38):
    return clip(text, limit) + ('…' if len(text.encode()) > limit else '')


def server_choices(prefs):
    choices = [(label, host, port) for label, host, port in SERVERS]
    for host, port in [[prefs.host, prefs.port]] + prefs.servers:
        if not any((host, port) == item[1:] for item in choices):
            choices.append((short(host, 25), host, port))
    return choices


def options(prefs):
    ui.open(tr('Options IRC'))
    ui.label(tr('Serveur TLS'), 16, 0, 214, 40)
    host = ui.entry(prefs.host, 240, 0, 620, 48)
    port = ui.entry(str(prefs.port), 880, 0, 196, 48)
    ui.label(tr('Pseudo enregistré'), 16, 72, 214, 42)
    nickname = ui.entry(prefs.nickname, 240, 72, 836, 48)
    ui.label(tr('La connexion reste manuelle. Les messages ne sont pas enregistrés.'), 16, 148, 1060, 76)
    status = ui.label(tr(prefs.error), 16, 252, 1060, 76)
    clear = ui.button(tr('Effacer les récents'), 16, 346, 350, 52)
    back = ui.button(tr('Retour'), 16, 418, 230, 54)
    save = ui.button(tr('Enregistrer'), 730, 418, 346, 54)
    while ui.is_open():
        event = ui.poll()
        if event:
            if event[0] == back.id:
                return
            try:
                if event[0] == clear.id:
                    prefs.clear_recent()
                    prefs.changed()
                    status.text(tr('Récents effacés') if prefs.save(True) else tr(prefs.error))
                elif event[0] == save.id:
                    prefs.configure(host.text().strip(), port.text().strip(), nickname.text().strip(), prefs.channel)
                    prefs.changed()
                    if prefs.save(True):
                        return
                    status.text(tr(prefs.error))
            except Exception as error:
                status.text(clip(error_text(error)))
        sleep_ms(40)


def channel_choices(prefs):
    result = []
    for favorite in (True, False):
        for name in prefs.channels(prefs.host, prefs.port, favorite):
            if name not in result:
                result.append(name)
    return result[:16]


def configure(prefs):
    while ui.is_open() or not getattr(prefs, '_opened', False):
        prefs._opened = True
        ui.open(tr('Discussion IRC'))
        choices = server_choices(prefs)
        selected = next(i for i, item in enumerate(choices) if item[1:] == (prefs.host, prefs.port))
        ui.label(tr('Serveur TLS'), 16, 0, 214, 42)
        servers = ui.dropdown([short(item[0], 34) for item in choices] + [tr('Personnalisé…')],
                              240, 0, 836, 46, selected)
        ui.label(tr('Pseudo'), 16, 66, 214, 42)
        nickname = ui.entry(prefs.nickname, 240, 66, 836, 46)
        ui.label(tr('Salon facultatif'), 16, 132, 214, 42)
        channel = ui.entry(prefs.channel, 240, 132, 836, 46)
        saved = channel_choices(prefs)
        choose = ui.dropdown([tr('Favoris et récents')] + [short(name, 25) for name in saved],
                             240, 196, 836, 46)
        status = ui.label(tr(prefs.error), 16, 270, 1064, 100)
        settings = ui.button(tr('Options'), 16, 418, 260, 54)
        close = ui.button(tr('Fermer'), 306, 418, 260, 54)
        connect = ui.button(tr('Se connecter'), 730, 418, 346, 54)
        redraw = False
        while ui.is_open() and not redraw:
            event = ui.poll()
            if event:
                widget, value = event
                if widget == close.id:
                    return None
                try:
                    if widget == choose.id and 0 < value <= len(saved):
                        channel.text(saved[value - 1])
                    elif widget == servers.id:
                        if value == len(choices):
                            options(prefs)
                        elif value < len(choices):
                            _, host, port = choices[value]
                            prefs.configure(host, port, nickname.text().strip())
                        redraw = True
                    elif widget == settings.id:
                        prefs.configure(prefs.host, prefs.port, nickname.text().strip(), channel.text().strip())
                        options(prefs)
                        redraw = True
                    elif widget == connect.id:
                        prefs.configure(prefs.host, prefs.port, nickname.text().strip(), channel.text().strip())
                        prefs.save(True)
                        client = Client()
                        status.text(tr('Connexion IRC en cours'))
                        client.connect(prefs.host, prefs.port, prefs.nickname, prefs.channel)
                        prefs.observed = None
                        return client
                except Exception as error:
                    status.text(clip(error_text(error)))
            sleep_ms(40)
    return None


def channels(client, prefs):
    ui.open(tr('Salons IRC'))
    ui.label(short(client.host, 90), 16, 0, 1060, 38)
    group = ui.dropdown([tr('Salons actifs'), tr('Favoris'), tr('Récents')], 16, 52, 350, 46)
    choices = ui.dropdown([''] * 17, 382, 52, 694, 46)
    ui.label(tr('Salon'), 16, 120, 180, 42)
    field = ui.entry(client.current if client.current.startswith(('#', '&')) else '', 212, 120, 864, 48)
    join = ui.button(tr('Rejoindre'), 16, 190, 336, 52)
    add = ui.button(tr('Ajouter aux favoris'), 376, 190, 336, 52)
    remove = ui.button(tr('Retirer des favoris'), 736, 190, 340, 52)
    leave = ui.button(tr('Quitter le salon'), 16, 264, 336, 52)
    status = ui.label('', 16, 338, 1060, 64)
    back = ui.button(tr('Retour'), 16, 418, 230, 54)
    selected_group = 0
    known = None
    names = []
    error = ''
    notice = ''
    while ui.is_open():
        try:
            client.poll()
        except Exception as failure:
            error = error_text(failure)
        prefs.sync(client)
        current = (client.joined[:] if selected_group == 0 else
                   prefs.channels(client.host, client.port, selected_group == 1))
        if current != known:
            known = current[:]
            names = current
            choices.text('\n'.join([tr('Choisir un salon')] + [short(name, 25) for name in names]))
            choices.value(0)
        message = error or tr(prefs.error) or notice
        if getattr(status, '_irc_text', None) != message:
            status.text(clip(message))
            status._irc_text = message
        event = ui.poll()
        if event:
            widget, value = event
            try:
                if widget == back.id:
                    prefs.save(True)
                    return
                if widget == group.id:
                    selected_group = value if value < 3 else 0
                    known = None
                elif widget == choices.id and 0 < value <= len(names):
                    field.text(names[value - 1])
                elif widget in (join.id, add.id, remove.id, leave.id):
                    name = channel_name(field.text().strip())
                    error = ''
                    notice = ''
                    if widget == join.id:
                        client.command('/join ' + name)
                        return
                    if widget == leave.id:
                        client.leave(name)
                    else:
                        prefs.favorite(client.host, client.port, name, widget == add.id)
                        if prefs.save(True):
                            notice = tr('Favori enregistré' if widget == add.id else 'Favori retiré')
                    known = None
            except Exception as failure:
                error = error_text(failure)
        sleep_ms(40)


def wrapped(client):
    lines = []
    for target, text in client.history:
        if client.fold(target) != client.fold(client.current):
            continue
        text = tr(text)
        if not text:
            lines.append('')
        while text:
            cut = min(76, len(text))
            if len(text) > cut:
                space = text.rfind(' ', 0, cut + 1)
                if space > 45:
                    cut = space
            lines.append(text[:cut])
            text = text[cut:].lstrip()
    return lines


def conversation(client, prefs):
    ui.open(tr('Discussion IRC'))
    status = ui.label('', 16, 0, 650, 38)
    manage = ui.button(tr('Salons'), 682, 0, 174, 44)
    disconnect = ui.button(tr('Déconnecter'), 872, 0, 204, 44)
    targets = ui.dropdown([tr('Serveur')] + [''] * 8, 16, 46, 1060, 44)
    rows = [ui.label('', 16, 100 + row * 31, 1064, 31) for row in range(8)]
    entry = ui.entry('', 16, 357, 810, 48, submit=True)
    send = ui.button(tr('Envoyer'), 842, 357, 234, 48)
    older = ui.button('↑', 16, 423, 100, 48)
    newer = ui.button('↓', 132, 423, 100, 48)
    position = ui.label('', 250, 423, 546, 44)
    help_button = ui.button(tr('Commandes'), 830, 423, 246, 48)
    offset = 0
    revision = -1
    last_view = None
    known_targets = []
    last_status = None
    error = ''
    last_draw = ticks_ms() - 200
    while ui.is_open():
        try:
            client.poll()
        except Exception as failure:
            error = error_text(failure)
        prefs.sync(client)
        event = ui.poll()
        if event:
            widget, value = event
            if widget == disconnect.id:
                return 'disconnect'
            if widget == manage.id:
                return 'channels'
            try:
                if widget == send.id or (widget == entry.id and value == 1):
                    text = entry.text()
                    if text.strip():
                        client.command(text)
                        entry.text('')
                        error = ''
                        offset = 0
                elif widget == targets.id and value < len(client.targets):
                    client.current = client.targets[value]
                    offset = 0
                elif widget == older.id:
                    offset += 8
                elif widget == newer.id:
                    offset = max(0, offset - 8)
                elif widget == help_button.id:
                    client.log(client.current, '/join #salon · /msg pseudo texte · /me texte · /nick pseudo · /part · /close · /quit')
            except Exception as failure:
                error = error_text(failure)
        # State publication is capped at 5 Hz and only changed labels draw.
        if ticks_diff(ticks_ms(), last_draw) >= 200:
            last_draw = ticks_ms()
            state = error or tr(prefs.error) or (tr('Connecté') if client.registered else tr('Connexion IRC en cours') if client.connected else tr('IRC déconnecté'))
            if client.connected:
                state += ' · ' + client.nickname + ' @ ' + client.host
            if client.pending:
                state += ' · ' + tr('En attente') + ' ' + str(len(client.pending))
            if state != last_status:
                status.text(clip(state))
                last_status = state
            if known_targets != client.targets:
                known_targets = client.targets[:]
                targets.text('\n'.join(tr('Serveur') if target == '*' else target for target in known_targets))
            target_index = client.targets.index(client.current)
            view = (client.current, offset)
            if revision != client.revision or last_view != view:
                lines = wrapped(client)
                offset = min(offset, max(0, len(lines)-8))
                end = max(0, len(lines)-offset)
                shown = lines[max(0, end-8):end]
                for i, row in enumerate(rows):
                    value = shown[i] if i < len(shown) else ''
                    # Keep a local copy: native reads would copy all widget text.
                    if getattr(row, '_irc_text', None) != value:
                        row.text(value)
                        row._irc_text = value
                position.text(tr('Messages récents') if offset == 0 else str(max(1, end-7)) + '–' + str(end) + ' / ' + str(len(lines)))
                targets.value(target_index)
                revision = client.revision
                last_view = (client.current, offset)
        sleep_ms(40)

def main():
    prefs = Preferences()
    try:
        while True:
            client = configure(prefs)
            if client is None:
                return
            try:
                while ui.is_open():
                    action = conversation(client, prefs)
                    if action != 'channels':
                        break
                    channels(client, prefs)
            finally:
                prefs.sync(client)
                prefs.save(True)
                client.disconnect()
    finally:
        prefs.save(True)
        ui.close()
