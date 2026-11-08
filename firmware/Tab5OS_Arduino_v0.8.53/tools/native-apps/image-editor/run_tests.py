#!/usr/bin/env python3
"""Compile and execute the production raster core with ASan/UBSan."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
RELEASE = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, default=RELEASE / 'tools/verification/image-core-0832.json')
args = parser.parse_args()
sources = [HERE / name for name in ('image_core.c', 'image_core.h', 'image_selection.h', 'test_core.c', 'test_relief.c', 'run_tests.py')]
flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
compiler = os.environ.get('CC', 'cc')
with tempfile.TemporaryDirectory(prefix='tab5-image-core-') as work:
    binary = Path(work) / 'test-core'
    subprocess.run([compiler, *flags, str(HERE / 'image_core.c'), str(HERE / 'test_core.c'), '-o', str(binary)], check=True)
    env = os.environ.copy()
    # LeakSanitizer cannot inspect /proc task state in this execution sandbox.
    # Every injected allocation is independently tracked and must reach zero.
    env['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'
    env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    result = subprocess.run([str(binary)], env=env, text=True, capture_output=True)
    print(result.stdout, end='')
    if result.stderr:
        print(result.stderr, end='')
    result.check_returncode()
    summary = json.loads(result.stdout.strip().splitlines()[-1])
    relief_binary = Path(work) / 'test-relief'
    subprocess.run([compiler, *flags, str(HERE / 'image_core.c'), str(HERE / 'test_relief.c'), '-o', str(relief_binary)], check=True)
    relief = subprocess.run([str(relief_binary)], env=env, text=True, capture_output=True)
    print(relief.stdout, end='')
    if relief.stderr:
        print(relief.stderr, end='')
    relief.check_returncode()
    summary['reliefRegression'] = json.loads(relief.stdout.strip().splitlines()[-1])
report = {
    'release': '0.8.53',
    'component': 'native-image-editor-core',
    **summary,
    'sanitizers': ['AddressSanitizer', 'UndefinedBehaviorSanitizer'],
    'leakValidation': 'Injected allocator asserts zero outstanding bytes after every scenario; LeakSanitizer disabled because sandbox /proc task introspection is unavailable.',
    'compiler': subprocess.run([compiler, '--version'], text=True, capture_output=True, check=True).stdout.splitlines()[0],
    'flags': flags,
    'sourceHashes': {str(p.relative_to(RELEASE)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
    'coverage': [
        'round/square brush, opacity, hardness, diffusion, eraser, clipping to selection',
        'line, filled/outline rectangle/ellipse, text and French accented glyphs',
        'layer add/duplicate/reorder/delete/opacity/visibility/merge preserving composition',
        'undo/redo, canceled gesture preserving prior redo, bounded history',
        'brightness/contrast/saturation and RGB gains, selection and visible-layer scope',
        'resize/crop, translation, scaling, shear, quarter-turn rotation with undo',
        'T5I exact layered round trip, BMP24 flattened round trip, truncated/corrupt inputs, partial/failed streams',
        '336 transient/persistent injected allocation-failure scenarios with atomic image preservation',
        '16MiB allocator limit, maximum1024 dimensions, history reclamation and 400 repeated editing cycles',
        'cooperative scheduler callback invoked during pixel processing',
        'relief selected colors across all profiles, transparent layers, flat settings, soft selection and opacity, undo/redo, cancellation, 48 allocation-failure scenarios',
        'relief legacy white/matte golden rasters, colored light channel symmetry, matte/glossy highlights, black/white light defaults, alpha invariance, setting clamps and captured options'
    ],
    'limitations': [
        'Host tests execute production C core; they do not execute ESP32-P4 ELF loading, touch/display hardware or SD filesystem.',
        'The host allocator enforces total live-byte bounds but does not model the native arena fragmentation; UI integration tests cover the real app allocator separately.'
    ]
}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
print(args.output)
