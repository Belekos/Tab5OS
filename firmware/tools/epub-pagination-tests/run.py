"""Run the real EPUB index test, also emulating Arduino's two physical header copies."""
from pathlib import Path
import argparse, shutil, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('sketch',type=Path);p.add_argument('--cxx',default='cl');a=p.parse_args()
here=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='tab5-epub-index-') as name:
 d=Path(name);(d/'original').mkdir()
 for destination in [d/'EpubPageIndex.h',d/'original/EpubPageIndex.h']:shutil.copy2(a.sketch/'EpubPageIndex.h',destination)
 (d/'test.cpp').write_text('#include "original/EpubPageIndex.h"\n'+(here/'test_book_index.cpp').read_text(encoding='utf-8'),encoding='utf-8')
 subprocess.run([a.cxx,'/nologo','/std:c++17','/EHsc','/W4','/utf-8','test.cpp','/Fe:test.exe'],cwd=d,check=True)
 subprocess.run([str(d/'test.exe')],cwd=d,check=True)
 print('PASS: original and Arduino header copies included together.')
