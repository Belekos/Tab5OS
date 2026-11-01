#!/usr/bin/env python3
"""Enable the bounded native heap probe only for the audited SDK binary/headers.

Unknown or missing files produce the public ESP-IDF fallback, never a build
failure. Build callers must add compilerDefine to BOTH C and C++ flags.
"""
from pathlib import Path
import argparse
import hashlib
import json

EXPECTED = {
    'lib/libheap.a': '54385a765f7d64d3a01229e276852b968eb05baee6a2250a4f7d7754c860a752',
    'include/heap/tlsf/tlsf_control_functions.h': '2e89b64ac4357ce8c951b540355dfe3279953f96ee46fdac0ac22972bd1d070f',
    'include/heap/tlsf/tlsf_block_functions.h': 'c0e8ccac7b83c6964a8e19d83c69e1e2651c25101e34feb36b3e10b96d3fe5a8',
    'include/heap/tlsf/include/tlsf.h': '69799f4f1e7c5f8d3a35e89f5efb8014aa00e6e49f96939b93fc39ceb95d0e8b',
}

def probe_sdk(sdk):
    sdk = Path(sdk)
    observed = {}
    for relative in EXPECTED:
        try:
            observed[relative] = hashlib.sha256((sdk/relative).read_bytes()).hexdigest()
        except OSError:
            observed[relative] = None
    known = observed == EXPECTED
    return {'known': known, 'compilerDefine': '-DTAB5_HEAP_PROBE_PINNED=' + ('1' if known else '0'),
            'mode': 'bounded-native-fit' if known else 'public-sdk-fallback',
            'sdk': str(sdk), 'sha256': observed, 'expectedSha256': EXPECTED}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', '--sdk-root', dest='sdk', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    text = json.dumps(probe_sdk(args.sdk), indent=2) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
    print(text, end='')

if __name__ == '__main__':
    main()
