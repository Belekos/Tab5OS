"""Bounded IRC preferences. Message history stays only in the running VM."""
import tab5fs as fs
from time import ticks_ms, ticks_diff
from tab5irc import word

PATH = '/System/irc.json'
MAX_SERVERS = 8
MAX_CHANNELS = 16
SERVERS = (('Libera Chat', 'irc.libera.chat', 6697),
           ('OFTC', 'irc.oftc.net', 6697),
           ('hackint', 'irc.hackint.org', 6697))


def endpoint(host, port):
    if not isinstance(host, str):
        raise ValueError('Serveur IRC invalide')
    host = host.strip().lower()
    if not host or len(host) > 253 or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789.-' for c in host):
        raise ValueError('Serveur IRC invalide')
    try:
        port = int(port)
    except (ValueError, TypeError):
        raise ValueError('Port IRC invalide')
    if not 1 <= port <= 65535:
        raise ValueError('Port IRC invalide')
    return host, port


def channel_name(value):
    value = word(value)
    if value[0] not in '#&':
        raise ValueError('Le salon doit commencer par # ou &')
    return value


def same_channel(a, b):
    # Only collapse ASCII case here; server-specific CASEMAPPING remains in Client.
    return a.lower() == b.lower()


class Preferences:
    def __init__(self, path=PATH, clock=ticks_ms):
        self.path = path
        self.clock = clock
        self.host, self.port = SERVERS[0][1:]
        self.nickname = 'Tab5_' + str(clock() % 100000)
        self.channel = ''
        self.servers = []
        self.favorites = []
        self.recent = []
        self.dirty = False
        self.changed_at = 0
        self.error = ''
        self.observed = None
        self.load()

    def _channels(self, items):
        result = []
        if not isinstance(items, list):
            return result
        for item in items[:MAX_CHANNELS]:
            try:
                host, port = endpoint(item[0], item[1])
                name = channel_name(item[2])
                record = [host, port, name]
                if not any(r[:2] == record[:2] and same_channel(r[2], name) for r in result):
                    result.append(record)
            except (ValueError, TypeError, IndexError, KeyError):
                pass
        return result

    def load(self):
        try:
            fs.recover()
            if not fs.exists(self.path):
                return
            data = fs.read_json(self.path, 24576)
            if not isinstance(data, dict) or data.get('version') != 1:
                raise ValueError('Préférences IRC illisibles')
            self.host, self.port = endpoint(data.get('host'), data.get('port'))
            if not isinstance(data.get('nickname'), str):
                raise ValueError('Pseudo ou salon invalide')
            self.nickname = word(data['nickname'], True)
            self.channel = channel_name(data['channel']) if data.get('channel') else ''
            self.favorites = self._channels(data.get('favorites'))
            self.recent = self._channels(data.get('recent'))
            servers = data.get('servers', [])
            if isinstance(servers, list):
                for item in servers[:MAX_SERVERS]:
                    try:
                        value = list(endpoint(item[0], item[1]))
                        if value not in self.servers:
                            self.servers.append(value)
                    except (ValueError, TypeError, IndexError, KeyError):
                        pass
        except (OSError, ValueError, TypeError, KeyError):
            self.error = 'Préférences IRC non lues : vérifier la carte SD'

    def changed(self):
        self.dirty = True
        self.changed_at = self.clock()

    def configure(self, host, port, nickname, channel=''):
        host, port = endpoint(host, port)
        nickname = word(nickname, True)
        channel = channel_name(channel) if channel else ''
        new = (host, port, nickname, channel)
        if new != (self.host, self.port, self.nickname, self.channel):
            self.host, self.port, self.nickname, self.channel = new
            self.changed()

    def save(self, force=False):
        if not self.dirty:
            return not self.error
        if not force and ticks_diff(self.clock(), self.changed_at) < 2000:
            return False
        self.dirty = False
        try:
            data = {'version': 1, 'host': self.host, 'port': self.port,
                    'nickname': self.nickname, 'channel': self.channel,
                    'servers': self.servers, 'favorites': self.favorites,
                    'recent': self.recent}
            with fs.Transaction() as transaction:
                transaction.json(self.path, data)
                transaction.commit()
            self.error = ''
            return True
        except (OSError, ValueError):
            self.error = 'Préférences IRC non enregistrées : vérifier la carte SD'
            return False

    def channels(self, host, port, favorites=False):
        return [r[2] for r in (self.favorites if favorites else self.recent)
                if r[0] == host and r[1] == port]

    def favorite(self, host, port, channel, add=True):
        host, port = endpoint(host, port)
        channel = channel_name(channel)
        found = None
        for record in self.favorites:
            if record[:2] == [host, port] and same_channel(record[2], channel):
                found = record
                break
        if add and not found:
            if len(self.favorites) >= MAX_CHANNELS:
                raise ValueError('Seize salons favoris au maximum')
            self.favorites.append([host, port, channel])
            self.changed()
        elif not add and found:
            self.favorites.remove(found)
            self.changed()

    def remember_channel(self, host, port, channel):
        host, port = endpoint(host, port)
        channel = channel_name(channel)
        result = [[host, port, channel]] + [r for r in self.recent
                  if not (r[:2] == [host, port] and same_channel(r[2], channel))]
        result = result[:MAX_CHANNELS]
        if result != self.recent:
            self.recent = result
            self.changed()

    def sync(self, client):
        if client.registered:
            state = (client.host, client.port, client.nickname, tuple(client.joined))
            if state != self.observed:
                previous = self.observed
                self.observed = state
                self.configure(client.host, client.port, client.nickname, self.channel)
                server = [client.host, client.port]
                if not previous or previous[:2] != state[:2]:
                    self.servers = ([server] + [s for s in self.servers if s != server])[:MAX_SERVERS]
                    self.changed()
                old_joined = previous[3] if previous and previous[:2] == state[:2] else ()
                for channel in client.joined:
                    if channel not in old_joined:
                        self.remember_channel(client.host, client.port, channel)
        self.save()

    def clear_recent(self):
        if self.recent or self.servers:
            self.recent = []
            self.servers = []
            self.changed()
