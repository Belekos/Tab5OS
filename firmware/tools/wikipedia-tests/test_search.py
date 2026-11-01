"""The production cursor merge and deletion paths, with deterministic sources."""
from pathlib import Path
import sys
import time
import types
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(next(Path(__file__).resolve().parents[2].glob('Tab5OS_Arduino_v*_Ecriture')) / 'sdcard/MicroPython/lib'))
time.sleep_ms = lambda unused: None
fs = types.ModuleType('tab5fs'); sys.modules['tab5fs'] = fs
native = types.ModuleType('_tab5'); native.call = lambda *args: None; sys.modules['_tab5'] = native
import tab5wiki as wiki
import tab5wikilibrary as library

class SearchTests(unittest.TestCase):
    def setUp(self):
        self.original_local = library.local
        self.original_web = wiki.search
        self.original_forget = library.forget
        self.local_pages = []
        self.web = []
        self.calls = []
        library.local = self.local
        wiki.search = self.search

    def tearDown(self):
        library.local = self.original_local
        wiki.search = self.original_web
        library.forget = self.original_forget

    def local(self, code, query, cursor=None, issues=None):
        index = cursor or 0
        rows = self.local_pages[index] if index < len(self.local_pages) else []
        return rows, index + 1 if index + 1 < len(self.local_pages) else None

    def search(self, code, query, offset=0, count=6, progress=None):
        self.calls.append(offset)
        return self.web[offset:offset + count], offset + count if offset + count < len(self.web) else None

    def row(self, title, kind='saved'):
        return {'title': title, 'kind': kind, 'id': len(title)}

    def titles(self, rows):
        return [r['title'] for r in rows]

    def test_one_saved_result_does_not_get_its_own_page(self):
        self.local_pages = [[self.row('Local')], []]
        self.web = [self.row('Web ' + str(n), 'online') for n in range(10)]
        first, cursor = library.search_page('fr', 'q', connected=True)
        self.assertEqual(self.titles(first), ['Local'] + ['Web ' + str(n) for n in range(5)])
        second, end = library.search_page('fr', 'q', cursor, True)
        self.assertEqual(self.titles(second), ['Web ' + str(n) for n in range(5, 10)])
        self.assertIsNone(end)
        again, end = library.search_page('fr', 'q', cursor, True)
        self.assertEqual(second, again)  # A history cursor is immutable.

    def test_saved_and_multiple_archives_fill_the_same_offline_page(self):
        self.local_pages = [[self.row('Saved')], [], [self.row('A ' + str(n), 'zim') for n in range(3)], [self.row('B ' + str(n), 'zim') for n in range(3)]]
        first, cursor = library.search_page('fr', 'q')
        self.assertEqual(len(first), 6)
        second, end = library.search_page('fr', 'q', cursor)
        self.assertEqual(self.titles(second), ['B 2'])
        self.assertIsNone(end)
        self.assertEqual(self.calls, [])

    def test_duplicate_titles_do_not_consume_visible_slots(self):
        self.local_pages = [[self.row('École de Paris')]]
        self.web = [self.row('école_de_Paris', 'online')] + [self.row(str(n), 'online') for n in range(12)]
        all_rows = []; state = None
        while True:
            rows, state = library.search_page('fr', 'q', state, True)
            all_rows.extend(rows)
            if state is None: break
            self.assertEqual(len(rows), 6)
        self.assertEqual(self.titles(all_rows), ['École de Paris'] + [str(n) for n in range(12)])

    def test_exact_page_has_no_empty_following_page(self):
        self.local_pages = [[self.row(str(n)) for n in range(6)], []]
        rows, cursor = library.search_page('fr', '')
        self.assertEqual(len(rows), 6); self.assertIsNone(cursor)

    def test_network_failure_keeps_local_results(self):
        self.local_pages = [[self.row('Saved')]]
        wiki.search = lambda *args: (_ for _ in ()).throw(OSError(110))
        issues = []
        rows, cursor = library.search_page('fr', 'q', connected=True, issues=issues)
        self.assertEqual(self.titles(rows), ['Saved']); self.assertIsNone(cursor)
        self.assertTrue(issues)

    def test_split_archive_deletion_is_limited_to_exact_stem(self):
        removed = []; forgotten = []
        present = {'/Books/wiki.zimaa', '/Books/wiki.zimab', '/Books/other.zim', '/Books/wiki-notes.txt'}
        fs.path = lambda value: value
        fs.exists = lambda value: value in present
        fs.isdir = lambda value: False
        fs.remove = lambda value: (removed.append(value), present.remove(value))
        library.forget = forgotten.append
        library.remove_archive({'path': '/Books/wiki.zimaa'})
        self.assertEqual(removed, ['/Books/wiki.zimaa', '/Books/wiki.zimab'])
        self.assertEqual(forgotten, ['/Books/wiki.zimaa'])
        self.assertEqual(present, {'/Books/other.zim', '/Books/wiki-notes.txt'})

    def test_saved_page_deletion_uses_transaction_for_all_sidecars(self):
        staged = []; committed = []
        class Transaction:
            def __enter__(self): return self
            def __exit__(self, *args): pass
            def allocate(self, path, delete=False): staged.append((path, delete))
            def commit(self): committed.append(True)
        fs.Transaction = Transaction
        fs.exists = lambda path: path in {'/Wikipedia/fr/articles/3.txt', '/Wikipedia/fr/articles/3.json', '/Wikipedia/fr/articles/3.txt.links', '/Wikipedia/fr/articles/3.txt.img0'}
        wiki.remove_article({'language': 'fr', 'id': 3})
        self.assertEqual(len(staged), 4)
        self.assertTrue(all(delete and path.startswith('/Wikipedia/fr/articles/3.') for path, delete in staged))
        self.assertEqual(committed, [True])

if __name__ == '__main__':
    unittest.main(verbosity=2)
