"""Precise errno backport for the pinned M5Stack Arduino 3.3.9 FS library.

No change to File ownership, filesystem layout, retry policy or I/O order.
The upstream library is copied first; the installed SDK is never modified.
"""
from pathlib import Path
import hashlib,json,shutil
EXPECTED_VFS_SHA256='08cb38a5436dd8a8bab6454cdafd2fda373050b163d21b5fa1e718db65be715a'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def patch_vfs(data:bytes)->bytes:
    if hashlib.sha256(data).hexdigest()!=EXPECTED_VFS_SHA256:
        raise RuntimeError('FS errno backport requires the exact pinned M5Stack Arduino 3.3.9 VFS source')
    text=data.decode().replace('\r\n','\n')
    def change(old,new,count=1):
        nonlocal text
        if text.count(old)!=count:raise RuntimeError('Unexpected pinned FS source context: '+old[:90])
        text=text.replace(old,new)
    change('    log_e("File system is not mounted");\n    return', '    log_e("File system is not mounted");\n    errno = ENODEV;\n    return',6)
    change('    log_e("%s does not start with /", fpath);\n    return','    log_e("%s does not start with /", fpath);\n    errno = EINVAL;\n    return')
    change('    log_e("bad arguments");\n    return','    log_e("bad arguments");\n    errno = EINVAL;\n    return',2)
    change('        log_e("Folder name malloc failed");\n        return','        log_e("Folder name malloc failed");\n        errno = ENOMEM;\n        return')
    change('    log_e("malloc failed");\n    return','    log_e("malloc failed");\n    errno = ENOMEM;\n    return',5)
    change('  if (!temp) {\n    return;\n  }','  if (!temp) {\n    errno = ENOMEM;\n    return;\n  }',2)
    change('    log_e("strdup(%s) failed", fpath);\n    free(temp);\n    return;','    log_e("strdup(%s) failed", fpath);\n    free(temp);\n    errno = ENOMEM;\n    return;')
    change('  if (name == NULL) {\n    return FileImplPtr();','  if (name == NULL) {\n    errno = ENOMEM;\n    return FileImplPtr();')
    change('    log_e("%s is a file", fpath);\n    return false;','    log_e("%s is a file", fpath);\n    errno = EEXIST;\n    return false;')
    # Absence is valid only when all fallback failures are compatible with it.
    # Keep a non-ENOENT fopen error, or the first non-absence fallback failure.
    # A known directory retains opendir's error; a known file after ENOENT is
    # an inconsistent lookup (EAGAIN), not permission to replace committed data.
    change('    int stat_rc = stat(temp, &dir_stat);','    int stat_rc = stat(temp, &dir_stat);\n      const int stat_errno = stat_rc == 0 ? 0 : errno;')
    change('''        _d = opendir(temp);
        _isDirectory = (_d != NULL);
      } else {
        log_e("fopen(%s, %s) failed: %d (%s)", temp, mode, fopen_errno, strerror(fopen_errno));
      }''','''        _d = opendir(temp);
        _isDirectory = (_d != NULL);
        if (!_d) {
          const int directory_errno = errno ? errno : EIO;
          if (fopen_errno && fopen_errno != ENOENT && fopen_errno != EISDIR) errno = fopen_errno;
          else if (stat_rc != 0 && stat_errno != ENOENT) errno = stat_errno ? stat_errno : EIO;
          else if (directory_errno != ENOENT) errno = directory_errno;
          else if (stat_rc == 0 || fopen_errno == EISDIR) errno = EAGAIN;
          else errno = fopen_errno ? ENOENT : EIO;
        }
      } else {
        log_e("fopen(%s, %s) failed: %d (%s)", temp, mode, fopen_errno, strerror(fopen_errno));
        errno = fopen_errno == ENOENT ? EAGAIN : fopen_errno;
      }''')
    change('''    _f = fopen(temp, mode);
    if (!_f) {
      log_e("fopen(%s) failed", temp);
    }
    if (!stat(temp, &_stat)) {''','''    _f = fopen(temp, mode);
    const int fopen_errno = _f ? 0 : errno;
    if (!_f) {
      log_e("fopen(%s) failed", temp);
    }
    if (!stat(temp, &_stat)) {''')
    change('''      log_e("stat(%s) failed", temp);
    }
  }
  free(temp);''','''      log_e("stat(%s) failed", temp);
    }
    if (!_f) errno = fopen_errno;
  }
  free(temp);''')
    change('''  fflush(_f);
  // workaround for https://github.com/espressif/arduino-esp32/issues/1293
  fsync(fileno(_f));''','''  const int flushed = fflush(_f);
  const int flush_errno = flushed == 0 ? 0 : (errno ? errno : EIO);
  // workaround for https://github.com/espressif/arduino-esp32/issues/1293
  fsync(fileno(_f));
  if (flush_errno) errno = flush_errno;''')
    return text.replace('\n','\r\n').encode()
def prepare_fs_errno(source:Path,work:Path)->Path:
    source=source.resolve();work=work.resolve();upstream=source/'src/vfs_api.cpp'
    original=upstream.read_bytes();patched=patch_vfs(original)
    target=work/'dependencies/FS'
    if target==source or target.is_relative_to(source):raise RuntimeError('Isolated FS must not overwrite or nest within the installed SDK')
    source_files={p.relative_to(source).as_posix():sha(p) for p in source.rglob('*') if p.is_file()}
    expected_files=dict(source_files)
    expected_files['src/vfs_api.cpp']=hashlib.sha256(patched).hexdigest()
    if target.exists():
        actual={p.relative_to(target).as_posix():sha(p) for p in target.rglob('*') if p.is_file()}
        if actual!=expected_files:raise RuntimeError('Isolated FS differs from the exact expected backport; remove this build-owned dependency copy before rebuilding')
    else:
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copytree(source,target)
        (target/'src/vfs_api.cpp').write_bytes(patched)
    report={'version':'M5Stack Arduino 3.3.9 / ESP-IDF 5.5.4','passed':True,
      'kind':'precise-errno-backport','source':str(source),'isolated':str(target),
      'upstreamSha256':hashlib.sha256(original).hexdigest(),'patchedSha256':hashlib.sha256(patched).hexdigest(),
      'patchScriptSha256':sha(Path(__file__)),'files':{'src/vfs_api.cpp':sha(target/'src/vfs_api.cpp')},
      'originalFiles':source_files,'patchedFiles':{p.relative_to(target).as_posix():sha(p) for p in target.rglob('*') if p.is_file()},
      'cachedSdkUnmodified':source_files=={p.relative_to(source).as_posix():sha(p) for p in source.rglob('*') if p.is_file()},
      'changes':['Known allocation failures report ENOMEM; unmounted/invalid argument branches report ENODEV/EINVAL.',
      'Existing non-directory mkdir target reports EEXIST.',
      'No false absence: non-absence fopen/stat/opendir failures survive fallback; inconsistent existing-file lookup reports EAGAIN; directory opendir failures remain visible.',
      'First fflush failure survives the following fsync attempt.'],
      'limitations':['Metadata-only: no extra retries, new queue, I/O scheduling, file layout or File ownership change.',
      'Physical SD fault and errno behavior of the binary SDK remain subject to hardware testing.']}
    (work/'fs-errno-patch.json').write_text(json.dumps(report,indent=2)+'\n')
    return target
if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('work',type=Path);args=p.parse_args()
    print(prepare_fs_errno(args.source,args.work))
