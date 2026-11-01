"""Normalize Clang's first dependency path for Arduino CLI's Windows cache.

Dry run by default. Use --apply only while the relevant objects are not being
compiled. This changes no sources, object bytes, dependency lists or timestamps
artificially: it only rewrites the spelling of an existing source path.
"""
import argparse
import json
import os
from pathlib import Path
import tempfile


def first_tokens(text, count=2):
    """Read Make tokens using Arduino CLI 1.5.1's depfile escape rules."""
    tokens = []
    value = []
    start = None
    i = 0
    while i < len(text):
        c = text[i]
        if c == '\r':
            i += 1
            continue
        if c == '\\':
            if i + 1 >= len(text):
                raise ValueError('unfinished backslash escape')
            j = i + 1
            if text[j] == '\r':
                j += 1
            if j >= len(text):
                raise ValueError('unfinished backslash escape')
            following = text[j]
            if following != '\n':
                if start is None:
                    start = i
                value.append(following if following in ' #\\' else '\\' + following)
            i = j + 1
            continue
        if c == '$':
            if i + 1 >= len(text) or text[i + 1] != '$':
                raise ValueError('invalid dollar escape')
            if start is None:
                start = i
            value.append('$')
            i += 2
            continue
        if c.isspace():
            if value:
                tokens.append((''.join(value), start, i))
                if len(tokens) == count:
                    return tokens
                value = []
            start = None
        else:
            if start is None:
                start = i
            value.append(c)
        i += 1
    if value:
        tokens.append((''.join(value), start, i))
    return tokens


def make_token(path):
    return path.replace('\\', '\\\\').replace('$', '$$').replace(' ', '\\ ').replace('#', '\\#')


def stamp(path):
    st = path.stat()
    return (st.st_dev, st.st_ino, st.st_size, st.st_mtime_ns, st.st_ctime_ns)


def normalize(dep, apply=False):
    obj = dep.with_suffix('.o')
    if not obj.is_file():
        return 'skipped-no-object'
    before_dep, before_obj = stamp(dep), stamp(obj)
    data = dep.read_bytes()
    text = data.decode('utf-8')
    tokens = first_tokens(text)
    if len(tokens) != 2 or not tokens[0][0].endswith(':'):
        return 'skipped-invalid-dependency'
    target = Path(tokens[0][0][:-1])
    source = Path(tokens[1][0])
    if not target.is_absolute() or not source.is_absolute():
        return 'skipped-relative-path'
    if not target.is_file() or not target.samefile(obj) or not source.is_file():
        return 'skipped-path-mismatch'
    normalized = os.path.normpath(tokens[1][0])
    native = Path(normalized)
    if source.resolve() != native.resolve() or not source.samefile(native):
        return 'skipped-source-identity'
    before_source = stamp(source)
    snapshots = [(dep, before_dep), (obj, before_obj), (source, before_source)]

    def stable():
        return all(stamp(path) == previous for path, previous in snapshots)

    if not stable():
        return 'skipped-changed'
    if normalized == tokens[1][0]:
        return 'unchanged'
    begin, end = tokens[1][1:]
    updated = text[:begin] + make_token(normalized) + text[end:]
    parsed = first_tokens(updated)
    if parsed[0][0] != tokens[0][0] or parsed[1][0] != normalized:
        return 'skipped-invalid-rewrite'
    if not apply:
        return 'would-normalize'
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode='wb', dir=dep.parent,
                                         prefix=dep.name + '.normalize-',
                                         suffix='.tmp', delete=False) as out:
            temporary = Path(out.name)
            out.write(updated.encode('utf-8'))
            out.flush()
            os.fsync(out.fileno())
        # The original depfile, source and object must all still be unchanged.
        if not stable() or dep.read_bytes() != data or not stable():
            return 'skipped-changed'
        os.replace(temporary, dep)
        temporary = None
        return 'normalized'
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def normalize_build(build, apply=False):
    build = build.resolve()
    counts = {}
    errors = []
    for dep in sorted(build.rglob('*.d')):
        if any(part.lower() == 'libsdetect' for part in dep.relative_to(build).parts):
            continue
        try:
            if dep.is_symlink() or not dep.resolve().is_relative_to(build):
                status = 'skipped-outside-build'
            else:
                status = normalize(dep, apply)
        except (OSError, UnicodeError, ValueError) as error:
            status = 'skipped-unreadable-or-changed'
            errors.append({'file': dep.relative_to(build).as_posix(), 'reason': str(error)})
        counts[status] = counts.get(status, 0) + 1
    return {'mode': 'apply' if apply else 'dry-run',
            'build': str(build), 'counts': counts, 'errors': errors}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path,
                         default=Path('C:/workspace/tab5-build-0821-clang'))
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    if not args.build_dir.is_dir():
        parser.error('build directory does not exist')
    print(json.dumps(normalize_build(args.build_dir, args.apply), indent=2))


if __name__ == '__main__':
    main()
