"""Validate an explicitly declared C++ sketch; no Arduino prototypes are needed.

This is a build-local replacement for the optional Arduino ctags generation step.
It succeeds only when the source sketch passes the target compiler syntax check,
and emits no tags because the source already contains its declarations. Arduino's
ctags excerpt has removed headers, so it cannot itself be compiled as C++.
"""
import json
import pathlib
import subprocess
import sys

source=pathlib.Path(sys.argv[1])
settings=json.loads(pathlib.Path(sys.argv[2]).read_text())
sketch=pathlib.Path(settings['sketch'])
if not source.is_file() or list(sketch.parent.glob('*.ino')) != [sketch]:
    raise RuntimeError('Expected one explicitly declared .ino sketch and a ctags input file.')
include_cache=pathlib.Path(settings['includeCache'])
includes=json.loads(include_cache.read_text()) if include_cache.exists() else settings['fallbackIncludes']
command=[settings['clang'],'-x','c++','-fsyntax-only',*settings['flags']]
command.extend('-I'+folder for folder in includes)
command.append(str(sketch))
raise SystemExit(subprocess.run(command).returncode)
