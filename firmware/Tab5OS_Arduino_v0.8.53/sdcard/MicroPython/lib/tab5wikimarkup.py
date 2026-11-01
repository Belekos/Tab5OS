"""Bounded HTML-to-reader stream. Sidecar offsets always count UTF-8 bytes.

No DOM, CSS, scripts or network access; at most one 4 KiB token is retained.
Article links are local Wikipedia titles, never arbitrary URLs.
"""
MAX_TEXT = 262144
MAX_LINKS = 2048
MAX_IMAGES = 8
ENTITIES = {'amp': '&', 'lt': '<', 'gt': '>', 'quot': '"', 'apos': "'",
            'nbsp': ' ', 'ndash': '–', 'mdash': '—', 'hellip': '…',
            'eacute': 'é', 'Eacute': 'É', 'agrave': 'à', 'egrave': 'è',
            'rsquo': '’', 'lsquo': '‘', 'rdquo': '”', 'ldquo': '“',
            'times': '×', 'minus': '−', 'middot': '·', 'bull': '•'}


def unescape(value):
    result = []; start = 0
    while True:
        pos = value.find('&', start)
        if pos < 0:
            result.append(value[start:]); break
        result.append(value[start:pos]); end = value.find(';', pos + 1, pos + 18)
        if end < 0:
            result.append('&'); start = pos + 1; continue
        name = value[pos + 1:end]; decoded = ENTITIES.get(name)
        if name.startswith('#'):
            try:
                number = int(name[2:], 16) if name[1:2].lower() == 'x' else int(name[1:])
                if number >= 32 and number <= 0x10ffff and not 0xd800 <= number <= 0xdfff:
                    decoded = chr(number)
            except ValueError:
                pass
        result.append(decoded if decoded is not None else value[pos:end + 1]); start = end + 1
    return ''.join(result)


def unquote(value):
    raw = bytearray(); value = value.encode(); i = 0
    while i < len(value):
        if value[i] == 37 and i + 2 < len(value):
            try:
                raw.append(int(value[i + 1:i + 3], 16)); i += 3; continue
            except ValueError:
                pass
        raw.append(value[i]); i += 1
    try:
        return raw.decode()
    except UnicodeError:
        return ''


def target(href, language):
    href = unescape(href)
    for prefix in ('https://' + language + '.wikipedia.org', '//' + language + '.wikipedia.org'):
        if href.startswith(prefix + '/'):
            href = href[len(prefix):]; break
    if href.startswith('/wiki/'):
        href = href[6:]
    elif href.startswith('./'):
        href = href[2:]
    else:
        return ''
    href = unquote(href.split('#', 1)[0]).replace('_', ' ').strip()
    if (not href or len(href.encode()) > 700 or ':' in href or '?' in href or
            any(ord(c) < 32 or c in '\\<>\t' for c in href)):
        return ''
    return href


def attributes(token):
    # Handles quoted > and unquoted HTML attributes; bounded by token size.
    result = {}; i = 0; size = len(token)
    while i < size and not token[i].isspace():
        i += 1
    while i < size:
        while i < size and (token[i].isspace() or token[i] == '/'):
            i += 1
        start = i
        while i < size and not token[i].isspace() and token[i] not in '=/':
            i += 1
        name = token[start:i].lower()
        while i < size and token[i].isspace():
            i += 1
        if not name:
            i += 1; continue
        value = ''
        if i < size and token[i] == '=':
            i += 1
            while i < size and token[i].isspace():
                i += 1
            if i < size and token[i] in ('"', "'"):
                quote = token[i]; i += 1; start = i
                while i < size and token[i] != quote:
                    i += 1
                value = token[start:i]; i += 1
            else:
                start = i
                while i < size and not token[i].isspace():
                    i += 1
                value = token[start:i]
        if name in ('href', 'src', 'alt', 'width', 'height', 'class', 'style'):
            result[name] = unescape(value)
    return result


class BufferedWriter:
    def __init__(self, target):
        self.target = target; self.buffer = bytearray()
    def write(self, data):
        self.buffer.extend(data)
        if len(self.buffer) >= 2048: self.flush()
        return len(data)
    def flush(self):
        if self.buffer:
            if self.target.write(self.buffer) != len(self.buffer): raise OSError(5)
            self.buffer = bytearray()


class Parser:
    def __init__(self, text, links, language):
        self.text = BufferedWriter(text); self.links = BufferedWriter(links); self.language = language
        self.offset = 0; self.link_count = 0; self.images = []
        self.pending = ''; self.skipped = ''; self.skip_depth = 0
        self.anchor = None; self.picture_link = ''; self.space = ''; self.last = '\n'
        self.saw_content = False; self.table_depth = 0; self.cells = []
        self.long_tag = False; self.tag_quote = ''

    def write(self, value):
        if not value:
            return
        encoded = value.encode()
        if self.offset + len(encoded) > MAX_TEXT:
            raise ValueError('Article trop volumineux pour ce lecteur')
        if self.text.write(encoded) != len(encoded):
            raise OSError(28)
        self.offset += len(encoded); self.last = value[-1]

    def flush_space(self):
        if self.space and self.offset and self.last != '\n':
            self.write(self.space)
        self.space = ''

    def words(self, value):
        if self.skipped:
            return
        value = unescape(value)
        # Whitespace is normalized independently of stream chunk boundaries.
        start = 0
        for index, character in enumerate(value):
            if character.isspace():
                if index > start:
                    self.flush_space(); self.write(value[start:index]); self.saw_content = True
                if self.space != '\n\n':
                    self.space = ' '
                start = index + 1
        if start < len(value):
            self.flush_space(); self.write(value[start:]); self.saw_content = True

    def finish_anchor(self):
        if self.anchor:
            start, destination = self.anchor
            if self.offset > start and self.link_count < MAX_LINKS:
                row = ('%d\t%d\t%s\n' % (start, self.offset, destination)).encode()
                if self.links.write(row) != len(row):
                    raise OSError(28)
                self.link_count += 1
        self.anchor = None; self.picture_link = ''

    def flush(self):
        self.text.flush(); self.links.flush()

    def tag(self, token):
        token = token.strip()
        if not token or token.startswith(('!', '?')):
            return
        closing = token.startswith('/'); body = token[1:] if closing else token
        name = body.split(None, 1)[0].rstrip('/').lower()
        if self.skipped:
            if name == self.skipped:
                self.skip_depth += -1 if closing else 1
                if self.skip_depth <= 0:
                    self.skipped = ''
            return
        if name in ('script', 'style', 'noscript', 'head', 'svg', 'math') and not closing:
            self.skipped = name; self.skip_depth = 1; return
        attrs = attributes(body) if not closing else {}
        classes = attrs.get('class', '').split()
        if not closing and any(c in classes for c in ('mw-editsection', 'navbox', 'metadata', 'noprint', 'sistersitebox')):
            self.skipped = name; self.skip_depth = 1; return
        if name == 'a':
            self.finish_anchor()
            if not closing:
                self.flush_space()
                href = attrs.get('href', ''); dest = target(href, self.language)
                if dest:
                    self.anchor = (self.offset, dest)
                if '/wiki/File:' in href or href.startswith('./File:'):
                    self.picture_link = href
        if name == 'img' and not closing and len(self.images) < MAX_IMAGES:
            source = attrs.get('src', '')
            if source.startswith('//'):
                source = 'https:' + source
            source = source.split('?', 1)[0].split('#', 1)[0]
            try:
                width = int(attrs.get('width', '80')); height = int(attrs.get('height', '80'))
            except ValueError:
                width = height = 80
            if (source.startswith(('https://upload.wikimedia.org/wikipedia/', 'https://thumb.wikimedia.org/wikipedia/')) and
                    len(source.encode()) <= 1024 and width >= 48 and height >= 40 and
                    not any(c in source for c in '\t\r\n\\?#') and
                    not any(row['url'] == source for row in self.images)):
                self.space = '\n\n'; self.flush_space()
                self.images.append({'offset': self.offset, 'url': source,
                                    'description': self.picture_link, 'alt': attrs.get('alt', '')[:180]})
        if name == 'table':
            if closing:
                if self.cells: self.cells.pop()
                self.table_depth = max(0, self.table_depth - 1)
            else:
                self.table_depth += 1; self.cells.append(0)
        if name == 'tr' and not closing and self.cells:
            self.cells[-1] = 0
        if name in ('td', 'th') and not closing and self.cells:
            if self.cells[-1]:
                self.space = ''; self.write('  |  ')
            self.cells[-1] += 1
        if name in ('p', 'div', 'section', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'table', 'tr', 'figure', 'figcaption', 'blockquote', 'ul', 'ol'):
            self.space = '\n\n'
        elif name == 'br' or name == 'li':
            self.space = '\n'
        elif name in ('td', 'th') and closing:
            self.space = ' '

    def feed(self, chunk, final=False):
        self.pending += chunk
        while self.pending:
            if self.long_tag:
                end = -1
                for i, char in enumerate(self.pending):
                    if self.tag_quote:
                        if char == self.tag_quote: self.tag_quote = ''
                    elif char in ('"', "'"): self.tag_quote = char
                    elif char == '>': end = i; break
                if end < 0: self.pending = ''; return
                self.pending = self.pending[end + 1:]; self.long_tag = False
                continue
            if self.pending.startswith('<!--'):
                end = self.pending.find('-->')
                if end < 0:
                    if len(self.pending) > 4096:
                        self.pending = '<!--' + self.pending[-3:]
                    return
                self.pending = self.pending[end + 3:]; continue
            if self.pending[0] == '<':
                quote = ''; end = -1
                for i in range(1, len(self.pending)):
                    char = self.pending[i]
                    if quote:
                        if char == quote:
                            quote = ''
                    elif char in ('"', "'"):
                        quote = char
                    elif char == '>':
                        end = i; break
                if end < 0:
                    if len(self.pending) > 4096:
                        # Parsoid metadata attributes can dwarf the displayed content.
                        self.tag(self.pending[1:].split(None, 1)[0])
                        self.long_tag = True; self.tag_quote = quote; self.pending = ''
                        return
                    if final:
                        self.pending = ''
                    return
                self.tag(self.pending[1:end]); self.pending = self.pending[end + 1:]; continue
            end = self.pending.find('<')
            if end < 0:
                end = len(self.pending)
                if not final:
                    entity = self.pending.rfind('&', max(0, end - 18))
                    if entity >= 0 and ';' not in self.pending[entity:]:
                        end = entity
            if not end:
                return
            self.words(self.pending[:end]); self.pending = self.pending[end:]
        if final:
            self.finish_anchor(); self.flush()


def convert(source, destination, links, language):
    parser = Parser(destination, links, language)
    carry = b''
    while True:
        block = source.read(2048)
        if not block:
            break
        block = carry + block; end = len(block)
        while end and block[end - 1] & 0xc0 == 0x80:
            end -= 1
        if end:
            lead = block[end - 1]
            need = 1 if lead < 128 else 2 if lead < 224 else 3 if lead < 240 else 4
            end = end - 1 if len(block) - end + 1 < need else len(block)
        carry = block[end:]
        try:
            parser.feed(block[:end].decode())
        except UnicodeError:
            raise ValueError('Réponse Wikipédia invalide')
    if carry:
        raise ValueError('Réponse Wikipédia invalide')
    parser.feed('', True); parser.finish_anchor(); parser.flush()
    if not parser.saw_content:
        raise ValueError('Aucun texte disponible pour cet article')
    return parser.offset, parser.images
