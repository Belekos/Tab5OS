"""Compile and exercise the production x86 controls and bounded audio sink."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    cc = shutil.which(args.cc)
    if not cc:
        parser.error('Compiler not found: ' + args.cc)
    here = Path(__file__).resolve().parent
    app = (here.parent / 'app.c').read_text()
    msvc = Path(cc).name.lower() in ('cl', 'cl.exe')
    with tempfile.TemporaryDirectory(prefix='tab5-x86-front-') as scratch:
        work = Path(scratch)
        for region in ('PRIMITIVES', 'ACTIONS', 'RENDER'):
            begin = f'/* DOS_UI_{region}_BEGIN'
            text = app.split(begin, 1)[1].split('*/', 1)[1]
            text = text.split(f'/* DOS_UI_{region}_END */', 1)[0]
            (work / ('ui_' + region.lower() + '.inc')).write_text(text)
        # Font data are unchanged; omit the unused old machine header for this
        # host-only translation unit so no 286 implementation is pulled in.
        font = (here.parents[1] / 'dos/core/ibmpc/font.c').read_text()
        (work / 'font.c').write_text(font.replace('#include "pc_internal.h"', ''))
        for name, sources in (
            ('audio', [here / 'test_audio_frontend.c']),
            ('ui', [here / 'test_ui.c', work / 'font.c']),
        ):
            exe = work / (name + ('.exe' if msvc else ''))
            if msvc:
                command = [cc, '/nologo', '/O2', '/std:c11', '/W4', '/I' + str(work)]
                command += [str(s) for s in sources] + ['/Fe:' + str(exe)]
            else:
                command = [cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-I', str(work)]
                command += [str(s) for s in sources] + ['-o', str(exe)]
            subprocess.run(command, cwd=work, check=True)
            run = [str(exe)]
            if name == 'ui' and args.output:
                args.output.mkdir(parents=True, exist_ok=True)
                run.append(str(args.output.resolve()))
            subprocess.run(run, cwd=work, check=True)


if __name__ == '__main__':
    main()
