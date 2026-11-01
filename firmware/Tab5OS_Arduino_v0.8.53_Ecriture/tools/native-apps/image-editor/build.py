#!/usr/bin/env python3
"""Build the actual Tab5 P4 image editor ELF; requires OS ABI6 (0.8.32+)."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parent
subprocess.run([sys.executable,str(root.parent/'build.py'),str(root/'app.c'),
 '--extra-source',str(root/'image_core.c'),'--extra-source',str(root/'runtime.c'),
 '--math','--opt','speed',*sys.argv[1:]],check=True)
