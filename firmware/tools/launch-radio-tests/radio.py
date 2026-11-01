"""Real catalogue module, deterministic API/FAT/UI boundaries."""
import importlib.util, json, pathlib, sys, time, types
from urllib.parse import parse_qs, urlsplit

sketch = pathlib.Path(sys.argv[1])
checks = 0
def check(condition, label):
    global checks
    assert condition, label
    checks += 1

class Widget:
    def __init__(self, identity, value=''): self.id = identity; self.value = value
    def text(self): return self.value

store = {}
requests = []
payload = []
def download(url, destination, limit, progress=None):
    requests.append(url)
    check(limit == 98304, 'bounded radio response')
    store[destination] = json.dumps(payload)
fs = types.SimpleNamespace(exists=lambda p: p in store, remove=lambda p: store.pop(p), download=download, read_json=lambda p, n: json.loads(store[p]), write_json=lambda p, data: store.update({p: json.dumps(data)}))
dropdowns = []
events = []
def dropdown(choices, *args):
    dropdowns.append((choices, args[-1]))
    return Widget(10)
ui = types.SimpleNamespace(tr=lambda s: s, label=lambda *a: None, entry=lambda s, *a: Widget(0, s), dropdown=dropdown, button=lambda s, *a: Widget(20 if s == 'Rechercher' else 30))
gui = types.SimpleNamespace(start=lambda s: Widget(99), event=lambda: events.pop(0), progress=lambda s: None, choose=lambda *a: None, message=lambda *a: None, failure=lambda e: (_ for _ in ()).throw(e))
sys.modules.update({'tab5fs': fs, 'tab5ui': ui, 'tab5gui': gui})
time.sleep_ms = lambda n: None
spec = importlib.util.spec_from_file_location('tab5catalogs', sketch / 'sdcard/MicroPython/lib/tab5catalogs.py')
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)
catalog._servers = ['one.api.radio-browser.info']

def station(codec='MP3', **values):
    return {'codec': codec, 'name': 'Test é', 'url': 'https://stream.example/audio', 'lastcheckok': 1, 'bitrate': 128, **values}

payload[:] = [station('MP3'), station('AAC'), station('AAC+'), station('FLAC'), station('OGG'), station('OPUS'), station('MP3', hls=1), station('MP3', lastcheckok=0), station('MP3', url='https://stream.example/radio.m3u8'), station('MP3', url='file:///bad'), station('MP3', url='https://bad\nheader'), station('MP3', name='')]
items, more = catalog.radio_search('café', 'fr', 'jazz')
query = parse_qs(urlsplit(requests[-1]).query)
check('codec' not in query, 'default search omits format filter')
check([s['codec'] for s in items] == ['MP3', 'AAC', 'AAC+'], 'mixed compatible codecs returned; unsupported/HLS/broken rejected')
check(query['name'] == ['café'] and query['countrycode'] == ['FR'] and query['tag'] == ['jazz'], 'search facets and UTF8 escaping preserved')
check(not more and not store, 'final page and response cleanup')
for codec in ('MP3', 'AAC'):
    catalog.radio_search(codec=codec)
    check(parse_qs(urlsplit(requests[-1]).query)['codec'] == [codec], 'explicit format filter preserved')
before = len(requests)
for codec in ('FLAC', 'OPUS', None):
    try: catalog.radio_search(codec=codec)
    except ValueError: check(True, 'invalid format rejected before request')
    else: check(False, 'invalid format accepted')
check(len(requests) == before, 'invalid format never sends request')
payload[:] = [station('OPUS') for _ in range(24)]
items, more = catalog.radio_search(offset=24)
check(items == [] and more, 'incompatible page retains next-page navigation')
check(parse_qs(urlsplit(requests[-1]).query)['offset'] == ['24'], 'pagination offset unchanged')

# Execute actual radios screen loop: all-format default, then explicit codec.
payload[:] = [station('AAC')]
events[:] = [(20, 1), (99, 1)]
catalog.radios(lambda: None)
check(dropdowns[-1] == (['Tous', 'MP3', 'AAC'], 0), 'translated all-formats default displayed')
check('codec' not in parse_qs(urlsplit(requests[-1]).query), 'screen uses unfiltered default')
events[:] = [(10, 2), (20, 1), (99, 1)]
catalog.radios(lambda: None)
check(parse_qs(urlsplit(requests[-1]).query)['codec'] == ['AAC'], 'screen selection maps to AAC')
check(dropdowns[-1][1] == 2, 'screen retains selected codec after search')
calls = []
events[:] = [(30, 1), (99, 1)]
catalog.radios(lambda: calls.append('manual'))
check(calls == ['manual'], 'manual import remains accessible')
check(not store, 'no response file leaked after searches')
print('PASS', checks, 'radio search/import checks; production module; no live network')
