"""Cache metadata and optional-image cancellation tests with local fake services."""
import builtins
import io
import json
from pathlib import Path
import sys
import tempfile
import time
import types
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(next(Path(__file__).resolve().parents[2].glob('Tab5OS_Arduino_v*_Ecriture')) / 'sdcard/MicroPython/lib'))
time.sleep_ms = lambda unused: None
fs = types.ModuleType('tab5fs'); sys.modules['tab5fs'] = fs
native = types.ModuleType('_tab5'); native.call = lambda *args: None; sys.modules['_tab5'] = native
import tab5wiki as wiki
import tab5zim as zim
import tab5wikiimages as images

class CacheTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='wiki-cache-')
        self.root = Path(self.temporary.name)
        self.removed = []
        self.saved = []
        fs.makedirs = lambda path: self.path(path).mkdir(parents=True, exist_ok=True)
        fs.exists = lambda path: self.path(path).exists()
        fs.stat = lambda path: (0, self.path(path).stat().st_size)
        fs.remove = self.remove
        fs.read_json = self.read_json
        fs.download = lambda *args: (_ for _ in ()).throw(OSError(125))
        wiki._json_replace = lambda path, value: self.saved.append((path, value))
        zim.open = images.open = self.open
        zim.call = self.extract

    def tearDown(self):
        self.temporary.cleanup()

    def path(self, path):
        result = (self.root / path.lstrip('/')).resolve()
        assert result.is_relative_to(self.root)
        return result

    def open(self, path, mode='r'):
        return builtins.open(self.path(path), mode)

    def remove(self, path):
        self.removed.append(path); self.path(path).unlink()

    def read_json(self, path, limit):
        assert self.path(path).stat().st_size <= limit
        return json.loads(self.path(path).read_text())

    def extract(self, op, a, b, c, payload, capacity):
        self.assertEqual(op, 84)
        request = json.loads(payload)
        self.path(request['destination']).write_bytes(b'Article text\n')
        return json.dumps({'ok': True, 'path': request['destination'], 'id': 1})

    def cache_hint(self, value):
        path = self.path('/Wikipedia/.reader/reader-cache.json')
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value)

    def test_corrupt_and_wrong_type_cache_do_not_block_reading(self):
        for value in ('bad json', '[]', 'null', '{"path": 7}'):
            with self.subTest(value=value):
                self.cache_hint(value)
                self.assertTrue(zim.extract({'path': '/archive.zim'}, identity=1)['ok'])
        self.assertEqual(self.removed, [])

    def test_cleanup_is_limited_to_generated_file_names(self):
        self.cache_hint(json.dumps({'path': '/Wikipedia/.reader/article-personal.txt'}))
        self.path('/Wikipedia/.reader/article-personal.txt').write_text('personal')
        zim.extract({'path': '/archive.zim'}, identity=1)
        self.assertEqual(self.removed, [])
        old = '/Wikipedia/.reader/article-00000000-3.txt'
        self.path(old).write_text('old article')
        self.cache_hint(json.dumps({'path': old}))
        zim.extract({'path': '/archive.zim'}, identity=1)
        self.assertEqual(self.removed, [old])

    def test_optional_images_propagate_cancellation(self):
        base = '/Wikipedia/test.txt'
        self.path('/Wikipedia').mkdir()
        self.path(base + '.sources').write_text(json.dumps([{'offset': 0, 'url': 'https://upload.wikimedia.org/wikipedia/a/test.png'}]))
        original_paths = wiki.record_paths
        wiki.record_paths = lambda record: (base, base + '.json')
        try:
            with self.assertRaises(OSError) as caught:
                images.ensure({'rich': 2, 'bytes': 10}, fetch=True)
            self.assertEqual(caught.exception.args[0], 125)
            self.assertEqual(self.saved, [])
        finally:
            wiki.record_paths = original_paths

    def test_oversized_media_index_is_not_loaded(self):
        base = '/Wikipedia/test.txt'
        self.path('/Wikipedia').mkdir()
        self.path(base + '.media').write_text('x' * 100000)
        original_paths = wiki.record_paths
        wiki.record_paths = lambda record: (base, base + '.json')
        try:
            images.open = lambda *args: self.fail('Oversized index must not be opened')
            self.assertIsNone(images.cached({'rich': 2}))
        finally:
            wiki.record_paths = original_paths

if __name__ == '__main__':
    unittest.main(verbosity=2)
