from pathlib import Path
import subprocess,json,hashlib,os,datetime
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture'
build=Path('C:/workspace/tab5-elf-resources-0825');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    wrapper=build/'run.cmd';wrapper.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\npython "'+str(Path(__file__).resolve())+'"\nexit /b %errorlevel%\n')
    raise SystemExit(subprocess.call(['cmd','/c',str(wrapper)]))
paths=[sketch/'Tab5Executable.cpp',sketch/'Tab5NativeView.cpp',sketch/'src/tab5_elf/esp_elf.c',sketch/'Tab5ResourceManager.cpp',sketch/'Tab5ResourceManager.h',sketch/'Tab5ResourcePolicy.h',sketch/'Tab5AppApi.h',here/'elf_resources.cpp',Path(__file__)]
hashes={str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
def extract(path,signature):
    source=path.read_text(encoding='utf-8');start=source.index(signature);opening=source.index('{',start);depth=1;at=opening+1
    while depth:
        depth+=(source[at]=='{')-(source[at]=='}');at+=1
    return source[start:at]
for name,signature in [('allocation','void* appAllocate(size_t bytes)'),('release','void appRelease(void* pointer)'),('cleanup','void cleanup()'),('cache-create','bool createCache('),('cache-reclaim','size_t reclaim(void*)')]:
    (build/f'elf-{name}-production.inc').write_text(extract(paths[0],signature),encoding='utf-8')
(build/'native-close-production.inc').write_text(extract(paths[1],'void close()'),encoding='utf-8')
(build/'elf-deinit-production.inc').write_text(extract(paths[2],'void esp_elf_deinit('),encoding='utf-8')
subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/MD','/utf-8','/I'+str(build),'/I'+str(sketch),str(here/'elf_resources.cpp'),str(sketch/'Tab5ResourceManager.cpp'),'/Fe:elf_resources.exe'],cwd=build,check=True)
run=subprocess.run([str(build/'elf_resources.exe')],capture_output=True,text=True);print(run.stdout);print(run.stderr);run.check_returncode()
assert hashes=={str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},'Production changed during test'
report=json.loads(run.stdout);report.update(version='0.8.25',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),physicalDeviceTested=False,scope='Exact production appAllocate/appRelease/cleanup/cache functions, NativeView close and Espressif ELF deinit extracted; actual ResourceManager compiled. Host tracked aligned allocator with OOM injection. Files, storage, hardware, LVGL and SDK task boundaries are modeled: this does not execute P4 firmware or measure physical SRAM/PSRAM. Repeated ABI1..4 allocations, 32-slot cap, partial launches, destructor cleanup, per-reservation slot recovery, console eviction and image-source cache drop are exercised.',sources=hashes)
(here.parent/'verification/elf-resources-0825.json').write_text(json.dumps(report,indent=2)+'\n')
