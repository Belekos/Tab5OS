"""Bounded IRC protocol on the OS verified TLS transport; one Python worker."""
from _tab5 import call
from time import ticks_ms, ticks_diff

MAX_TARGETS = 8
MAX_HISTORY = 128
MAX_PENDING = 8
MAX_LINE = 2048


def clean(text):
    # Server text is untrusted: remove IRC colours and terminal/control escapes.
    text = str(text)
    out = []
    i = 0
    while i < len(text):
        char = text[i]
        if char == '\x03':
            i += 1
            for part in range(2):
                digits = 0
                while i < len(text) and text[i].isdigit() and digits < 2:
                    i += 1
                    digits += 1
                if part == 0 and i < len(text) and text[i] == ',':
                    i += 1
                else:
                    break
            continue
        if ord(char) >= 32 and ord(char) != 127:
            out.append(char)
        i += 1
    return ''.join(out)


def word(text, nickname=False):
    text = str(text).strip()
    if not text or len(text.encode()) > (30 if nickname else 48):
        raise ValueError('Pseudo ou salon invalide')
    if any(ord(c) < 33 or c in ',:\x7f' for c in text):
        raise ValueError('Pseudo ou salon invalide')
    if nickname and (text[0].isdigit() or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-[]\\`_^{}|' for c in text)):
        raise ValueError('Pseudo ou salon invalide')
    return text


def parse(line):
    if line.startswith('@'):
        _, _, line = line.partition(' ')
    source = ''
    if line.startswith(':'):
        source, _, line = line[1:].partition(' ')
    line = line.lstrip(' ')
    if not line:
        return source, '', []
    prefix, marker, trailing = line.partition(' :')
    parts = prefix.split()
    command = parts.pop(0).upper() if parts else ''
    if marker:
        parts.append(trailing)
    return source, command, parts[:15]


class Client:
    def __init__(self, transport=call, clock=ticks_ms):
        self.call = transport
        self.clock = clock
        self.connected = False
        self.registered = False
        self.nickname = ''
        self.host = ''
        self.port = 6697
        self.joined = []
        self.current = '*'
        self.targets = ['*']
        self.history = []
        self.pending = []
        self.incoming = b''
        self.discard = False
        self.revision = 0
        self.mapping = 'rfc1459'
        self.last_send = 0
        self.last_received = 0
        self.started = 0
        self.ping_at = None
        self.initial_channel = ''

    def fold(self, text):
        text = text.lower()
        if self.mapping != 'ascii':
            text = text.replace('[', '{').replace(']', '}').replace('\\', '|')
            if self.mapping != 'strict-rfc1459':
                text = text.replace('^', '~')
        return text

    def add_target(self, target):
        try:
            target = word(target)
        except ValueError:
            return '*'
        for existing in self.targets:
            if self.fold(existing) == self.fold(target):
                return existing
        if len(self.targets) > MAX_TARGETS:
            return '*'
        self.targets.append(target)
        self.revision += 1
        return target

    def log(self, target, text):
        self.history.append((target, clean(text)[:500]))
        if len(self.history) > MAX_HISTORY:
            del self.history[0]
        self.revision += 1

    def _wire(self, line):
        if not isinstance(line, str) or any(c in line for c in ('\r', '\n', '\0')):
            raise ValueError('Commande IRC invalide')
        wire = line.encode() + b'\r\n'
        if len(wire) > 512:
            raise ValueError('Message IRC trop long')
        return wire

    def send_now(self, line):
        wire = self._wire(line)
        try:
            if self.call(87, 0, 0, 0, wire) != len(wire):
                raise OSError(32)
        except Exception:
            self.close()
            raise

    def queue(self, line, echo=None):
        self._wire(line)
        if len(self.pending) >= MAX_PENDING:
            raise ValueError('Attendre la fin des envois IRC')
        self.pending.append((line, echo))

    def connect(self, host, port, nickname, channel=''):
        if self.connected:
            raise ValueError('Déjà connecté')
        host = host.strip()
        if not host or len(host) > 253 or any(ch not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-' for ch in host):
            raise ValueError('Serveur IRC invalide')
        nickname = word(nickname, True)
        channel = word(channel) if channel else ''
        if channel and channel[0] not in '#&':
            raise ValueError('Le salon doit commencer par # ou &')
        if not isinstance(port, int) or not 1 <= port <= 65535:
            raise ValueError('Port IRC invalide')
        self.call(86, port, 1, 0, host.strip().encode())
        self.connected = True
        self.registered = False
        self.nickname = nickname
        self.host = host.strip()
        self.port = port
        self.joined = []
        self.initial_channel = channel
        self.mapping = 'rfc1459'
        self.incoming = b''
        self.discard = False
        self.pending = []
        self.targets = ['*']
        self.current = '*'
        self.history = []
        self.started = self.last_received = self.last_send = self.clock()
        self.ping_at = None
        self.send_now('NICK ' + nickname)
        self.send_now('USER tab5 0 * :Tab5 IRC')
        self.revision += 1

    def close(self):
        try:
            self.call(89)
        finally:
            self.connected = self.registered = False
            self.pending = []
            self.incoming = b''
            self.joined = []
            self.ping_at = None
            self.revision += 1

    def disconnect(self):
        try:
            if self.connected:
                self.send_now('QUIT :Leaving')
        except OSError:
            pass
        finally:
            self.close()

    def receive(self, line):
        try:
            text = line.decode()
        except UnicodeError:
            self.log('*', 'Texte IRC UTF-8 invalide ignoré')
            return
        source, command, params = parse(text)
        sender = source.split('!', 1)[0]
        if command == 'PING' and params:
            # PONG bypasses the user's rate limited queue, including while hidden.
            token = params[-1]
            if token and len(token.encode()) <= 504 and not any(c in token for c in ('\r', '\n', '\0')):
                self.send_now('PONG :' + token)
            return
        if command == 'PONG':
            return
        if command == '001':
            self.registered = True
            if params:
                self.nickname = params[0]
            if self.initial_channel:
                self.queue('JOIN ' + self.initial_channel)
                self.current = self.add_target(self.initial_channel)
                self.initial_channel = ''
            self.revision += 1
        elif command == '005':
            for item in params:
                if item.startswith('CASEMAPPING=') and item[12:] in ('ascii', 'rfc1459', 'strict-rfc1459'):
                    self.mapping = item[12:]
        elif command in ('PRIVMSG', 'NOTICE') and len(params) >= 2:
            destination, message = params[0], params[-1]
            target = sender if self.fold(destination) == self.fold(self.nickname) else destination
            target = self.add_target(target)
            if message.startswith('\x01'):
                if message.startswith('\x01ACTION ') and message.endswith('\x01'):
                    self.log(target, '* ' + sender + ' ' + message[8:-1])
                return
            self.log(target, ('-' + sender + '- ' if command == 'NOTICE' else '<' + sender + '> ') + message)
            return
        elif command == 'NICK' and params:
            if self.fold(sender) == self.fold(self.nickname):
                self.nickname = params[-1]
            self.log('*', sender + ' → ' + params[-1])
            return
        elif command in ('JOIN', 'PART', 'KICK') and params:
            channel = params[0]
            if command == 'JOIN' and self.fold(sender) == self.fold(self.nickname):
                channel = self.add_target(channel)
                if channel != '*' and not any(self.fold(c) == self.fold(channel) for c in self.joined):
                    self.joined.append(channel)
                self.current = channel
            elif ((command == 'PART' and self.fold(sender) == self.fold(self.nickname)) or
                  (command == 'KICK' and len(params) > 1 and self.fold(params[1]) == self.fold(self.nickname))):
                self.drop_target(channel)
                self.log('*', command + ' · ' + channel + ' · ' + ' '.join(params[1:]))
                return
            self.log(channel, command + ' · ' + sender + ' · ' + ' '.join(params[1:]))
            return
        elif command == 'ERROR':
            self.log('*', 'ERROR · ' + ' '.join(params))
            self.close()
            return
        if command:
            summary = command + ' · ' + ' '.join(params[1:] if command.isdigit() else params)
            self.log('*', summary)
            if command.isdigit() and int(command) >= 400 and self.current != '*':
                self.log(self.current, summary)

    def feed(self, data):
        # No growing readline(): both unterminated and excessive server lines are bounded.
        for piece in data.split(b'\n')[:-1]:
            if not self.discard and len(self.incoming) + len(piece) <= MAX_LINE:
                self.receive((self.incoming + piece).rstrip(b'\r'))
            self.incoming = b''
            self.discard = False
            if not self.connected:
                return
        tail = data.rsplit(b'\n', 1)[-1]
        if self.discard:
            return
        if len(self.incoming) + len(tail) > MAX_LINE:
            self.incoming = b''
            self.discard = True
            self.log('*', 'Ligne IRC trop longue ignorée')
        else:
            self.incoming += tail

    def poll(self):
        if not self.connected:
            return
        try:
            for _ in range(4):
                data = self.call(88, 0, 0, 0, None, 1024)
                if not data:
                    break
                self.last_received = self.clock()
                self.ping_at = None
                self.feed(data)
                if not self.connected:
                    return
            now = self.clock()
            if not self.registered and ticks_diff(now, self.started) > 90000:
                raise OSError(110)
            if self.ping_at is not None and ticks_diff(now, self.ping_at) > 30000:
                raise OSError(110)
            if ticks_diff(now, self.last_received) > 180000 and self.ping_at is None:
                self.send_now('PING :tab5-' + str(now))
                self.ping_at = now
            if self.pending and ticks_diff(now, self.last_send) >= 500:
                line, echo = self.pending.pop(0)
                self.send_now(line)
                if echo is not None:
                    self.log(echo[0], echo[1])
                self.last_send = now
        except Exception:
            self.close()
            raise

    def message(self, target, message, action=False):
        target = word(target)
        if target == '*':
            raise ValueError('Choisir un salon ou un destinataire')
        if not self.registered:
            raise ValueError('Connexion IRC en cours')
        message = message.strip()
        if not message:
            return
        if any(c in message for c in ('\r', '\n', '\0', '\x01')):
            raise ValueError('Commande IRC invalide')
        prefix = 'PRIVMSG ' + target + ' :'
        body = '\x01ACTION ' + message + '\x01' if action else message
        view_target = self.add_target(target)
        self.queue(prefix + body, (view_target, ('* ' + self.nickname + ' ' if action else '<' + self.nickname + '> ') + message))
        self.current = view_target

    def drop_target(self, target):
        same = lambda value: self.fold(value) == self.fold(target)
        self.joined = [name for name in self.joined if not same(name)]
        self.targets = [name for name in self.targets if name == '*' or not same(name)]
        self.history = [(name, line) for name, line in self.history if not same(name)]
        if same(self.current):
            self.current = '*'
        self.revision += 1

    def leave(self, target):
        target = word(target)
        if target == '*':
            return
        if target[0] in '#&' and self.connected:
            self.queue('PART ' + target)
        self.drop_target(target)

    def command(self, text):
        if not self.connected:
            raise ValueError('IRC déconnecté')
        if not text.startswith('/'):
            return self.message(self.current, text)
        name, _, rest = text[1:].partition(' ')
        name = name.lower()
        rest = rest.strip()
        if name == 'join':
            channel = word(rest)
            if channel[0] not in '#&':
                raise ValueError('Le salon doit commencer par # ou &')
            if len(self.targets) > MAX_TARGETS and not any(self.fold(t) == self.fold(channel) for t in self.targets):
                raise ValueError('Huit conversations au maximum')
            self.queue('JOIN ' + channel)
            self.current = self.add_target(channel)
        elif name == 'msg':
            target, _, message = rest.partition(' ')
            self.message(target, message)
        elif name == 'me':
            self.message(self.current, rest, True)
        elif name == 'nick':
            self.queue('NICK ' + word(rest, True))
        elif name == 'part':
            channel = word(rest or self.current)
            if channel[0] not in '#&':
                raise ValueError('Choisir un salon ou un destinataire')
            self.leave(channel)
        elif name == 'close':
            self.leave(self.current)
        elif name == 'quit':
            self.disconnect()
        elif name == 'help':
            self.log(self.current, '/join #salon · /msg pseudo texte · /me texte · /nick pseudo · /part · /close · /quit')
        else:
            raise ValueError('Commande IRC inconnue')
