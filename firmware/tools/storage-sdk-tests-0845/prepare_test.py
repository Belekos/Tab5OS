#!/usr/bin/env python3
"""Validate isolated-copy identity and OS-independent manifest path syntax."""
from pathlib import Path,PureWindowsPath
import hashlib,importlib.util,json,shutil,tempfile,ast
here=Path(__file__).resolve().parent;r=here.parent.parent;script=r/'tools/build_linux/prepare_fs_errno.py'
spec=importlib.util.spec_from_file_location('patch',script);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
checks=0
def check(value):
 global checks
 checks+=1
 assert value
# This is the actual pathlib operation now present at each manifest key site.
windowsRoot=PureWindowsPath(r'C:\sdk\libraries\FS');windowsFile=windowsRoot/'src'/'vfs_api.cpp'
check(windowsFile.relative_to(windowsRoot).as_posix()=='src/vfs_api.cpp')
check(str(windowsFile.relative_to(windowsRoot))!='src/vfs_api.cpp')
check(Path('/sdk/libraries/FS/src/vfs_api.cpp').relative_to(Path('/sdk/libraries/FS')).as_posix()==windowsFile.relative_to(windowsRoot).as_posix())
tree=ast.parse(script.read_text());sites=[n for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and n.func.attr=='relative_to'];check(len(sites)==4)
parents={child:node for node in ast.walk(tree) for child in ast.iter_child_nodes(node)}
for site in sites:
 parent=parents[site];check(isinstance(parent,ast.Attribute) and parent.attr=='as_posix')
with tempfile.TemporaryDirectory(prefix='tab5-fs-prepare-0845-') as scratch:
 root=Path(scratch);source=root/'source';(source/'src').mkdir(parents=True)
 for f in (here/'sdk').iterdir():shutil.copyfile(f,source/'src'/f.name)
 before={p.relative_to(source).as_posix():sha(p) for p in source.rglob('*') if p.is_file()}
 target=m.prepare_fs_errno(source,root/'work');first=json.loads((root/'work/fs-errno-patch.json').read_text())
 again=m.prepare_fs_errno(source,root/'work');second=json.loads((root/'work/fs-errno-patch.json').read_text())
 check(target==again);check(first==second);check(first['cachedSdkUnmodified']);check(first['originalFiles']==before)
 check(first['patchScriptSha256']==sha(script));check(first['patchedSha256']=='81899ba6143cd904234e0ba4cad952fe929d692893dc6a52942677d5e954857c')
 check(all('\\' not in name for group in ['originalFiles','patchedFiles'] for name in first[group]))
 check(before=={p.relative_to(source).as_posix():sha(p) for p in source.rglob('*') if p.is_file()})
 (target/'unexpected.cpp').write_text('// unexpected source')
 try:m.prepare_fs_errno(source,root/'work')
 except RuntimeError:check(True)
 else:raise AssertionError('Unexpected isolated source silently accepted')
report={'version':'0.8.45','passed':True,'physicalDeviceTested':False,'checks':checks,'sourceHashes':{str(script.relative_to(r)):sha(script)},'testSourceHashes':{str(f.relative_to(r)):sha(f) for f in [Path(__file__),*(here/'sdk').iterdir()] if f.is_file()},'method':__doc__,'patchedVfsSha256':'81899ba6143cd904234e0ba4cad952fe929d692893dc6a52942677d5e954857c','limitations':['Actual copy/second-call strict idempotence executed on Linux. Windows manifest syntax checked with PureWindowsPath and all actual manifest key sites; Windows Arduino compilation is not claimed.']}
(r/'tools/verification/fs-prepare-0845.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':True,'checks':checks,'scriptSha256':sha(script),'patchedVfsUnchanged':True}))
