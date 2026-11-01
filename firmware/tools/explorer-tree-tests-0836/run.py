#!/usr/bin/env python3
"""Run the exact production tree/model against deterministic FS and PSRAM doubles."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,tempfile
root=Path(__file__).resolve().parent
release=root.parent.parent
sketch=release/'Tab5OS_Arduino_v0.8.36_Ecriture'
build=Path(tempfile.mkdtemp(prefix='tab5-explorer-tree-0836-'))
production=['Tab5ExplorerTree.h','Tab5DirectoryModel.h','Tab5FileSort.h']
for name in production:shutil.copyfile(sketch/name,build/name)
for path in root.glob('*.h'):shutil.copyfile(path,build/path.name)
shutil.copyfile(root/'test_tree.cpp',build/'test_tree.cpp')
command=['g++','-std=c++17','-DARDUINO','-I'+str(build),'-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-use-after-free','-fno-omit-frame-pointer','-fsanitize=address,undefined',str(build/'test_tree.cpp'),'-o',str(build/'test')]
result=subprocess.run(command,capture_output=True,text=True)
output=result.stdout+result.stderr
if result.returncode==0:
 result=subprocess.run([str(build/'test')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 output+=result.stdout+result.stderr
(root/'results.txt').write_text(output,encoding='utf-8')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report={'version':'0.8.36','passed':result.returncode==0,'physicalDeviceTested':False,'exitCode':result.returncode,'sanitizer':'AddressSanitizer + UndefinedBehaviorSanitizer (LeakSanitizer unavailable under host ptrace; tracked PSRAM allocations must reach zero)' ,'method':'Unmodified production tree, directory model and sort header; deterministic mock filesystem and tracked PSRAM allocator; no UI or device execution.','output':output,'sourceHashes':{str((sketch/n).relative_to(release)):sha(sketch/n) for n in production},'evidenceHashes':{str(p.relative_to(release)):sha(p) for p in sorted(root.iterdir()) if p.is_file()}}
(release/'tools/verification/explorer-tree-0836.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(output,end='')
raise SystemExit(result.returncode)
