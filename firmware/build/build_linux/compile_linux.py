"""Build Tab5OS on Linux, using its pinned Arduino/GCC dependencies."""
import argparse, datetime, hashlib, importlib.util, json, os, subprocess, sys
from pathlib import Path
sys.dont_write_bytecode=True

def now(): return datetime.datetime.now(datetime.timezone.utc).isoformat()
def write(path, data): path.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
def manifest(root):
    # Host-render intermediates are neither firmware inputs nor package content.
    return [{'path':p.relative_to(root).as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(root.rglob('*')) if p.is_file() and p.relative_to(root).parts[0]!='sdcard' and not p.relative_to(root).as_posix().startswith('tools/native-apps/image-editor/test-output/')]

def sketch_path(release, name=None):
    candidates=[release/name] if name else sorted(release.glob('Tab5OS_Arduino_v*'))
    candidates=[p for p in candidates if (p/(p.name+'.ino')).is_file()]
    if len(candidates)!=1:raise RuntimeError('Expected exactly one source sketch')
    return candidates[0]

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--release-root',type=Path,required=True)
    ap.add_argument('--environment',type=Path,required=True)
    ap.add_argument('--sketch-name',default=None)
    ap.add_argument('--database-only',action='store_true',help='Generate Arduino source and compilation database without linking firmware')
    ap.add_argument('--jobs',type=int,default=4)
    args=ap.parse_args()
    release=args.release_root.resolve(); envroot=args.environment.resolve()
    sketch=sketch_path(release,args.sketch_name)
    work=envroot/('target-qa' if args.database_only else 'qa'); build=envroot/('target-build' if args.database_only else 'build')
    if sketch==envroot or sketch in envroot.parents: raise RuntimeError('Environment must be outside sketch')
    work.mkdir(exist_ok=True)
    cli=envroot/'arduino-cli'; config=envroot/'arduino-cli.yaml'; libraries=envroot/'sketchbook/libraries'
    versions={'M5Unified':'0.2.23','M5GFX':'0.2.30','lvgl':'9.6.0','ArduinoJson':'7.4.3','ESP8266Audio':'2.4.1'}
    dependencies=[]
    for name,version in versions.items():
        props=dict(line.split('=',1) for line in (libraries/name/'library.properties').read_text().splitlines() if '=' in line)
        if props.get('version')!=version: raise RuntimeError('Unexpected dependency version: '+name)
        dependencies.append({'name':name,'version':version,'path':str(libraries/name)})
    helper=release/'build/build_clang/prepare_lvgl.py'
    sys.path.insert(0,str(helper.parent))
    spec=importlib.util.spec_from_file_location('prepare_lvgl',helper); mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    lvgl=mod.prepare_lvgl(libraries/'lvgl',work)
    from prepare_m5gfx_jpeg import prepare_m5gfx_jpeg
    m5gfx=prepare_m5gfx_jpeg(libraries/'M5GFX',work/'firmware')
    from prepare_m5unified_audio import prepare_m5unified_audio
    m5unified=prepare_m5unified_audio(libraries/'M5Unified',work/'firmware')
    from prepare_fs_errno import prepare_fs_errno
    fs_source=envroot/'data/packages/m5stack/hardware/esp32/3.3.9/libraries/FS'
    fs=prepare_fs_errno(fs_source,work/'firmware')
    dependencies.append({'name':'FS','version':'3.3.9','path':str(fs),'source':str(fs_source.resolve()),'patch':'precise errno only'})
    gcc=envroot/'data/packages/m5stack/tools/esp-rv32/2601/bin/riscv32-esp-elf-g++'
    from prepare_dsi_cache_safe import prepare_dsi_cache_safe
    sdk=envroot/'data/packages/m5stack/tools/esp32p4_es-libs/3.3.9'
    from verify_heap_probe import probe_sdk
    heap_probe=probe_sdk(sdk)
    heap_probe['verifierSha256']=hashlib.sha256((release/'build/build_linux/verify_heap_probe.py').read_bytes()).hexdigest()
    write(work/'heap-probe-sdk.json',heap_probe)
    dsi_archives=prepare_dsi_cache_safe(sdk,gcc.with_name('riscv32-esp-elf-gcc'),work/'firmware')
    dsi_link='-Wl,--whole-archive '+' '.join('\"'+str(p)+'\"' for p in dsi_archives)+' -Wl,--no-whole-archive'
    tool_versions={}
    for label,command in [('arduino-cli',[str(cli),'version']),('gcc',[str(gcc),'--version'])]:
        tool_versions[label]=subprocess.run(command,capture_output=True,text=True,check=True).stdout.strip()
    write(work/'toolchain-versions.json',tool_versions)
    fqbn='m5stack:esp32:m5stack_tab5:ChipVariant=prev3,PSRAM=enabled,PartitionScheme=default,FlashMode=dio'
    include_argument='-I'+str(sketch)
    if any(c.isspace() for c in include_argument): include_argument='"'+include_argument+'"'
    flags='-fstack-usage -DLV_CONF_INCLUDE_SIMPLE '+heap_probe['compilerDefine']+' '+include_argument
    command=[str(cli),'compile','--verbose','--config-file',str(config),'--fqbn',fqbn,'--build-path',str(build),'--jobs',str(max(1,args.jobs)),'--library',str(fs),'--library',str(lvgl),'--library',str(m5gfx),'--library',str(m5unified),'--build-property','compiler.c.extra_flags='+flags,'--build-property','compiler.cpp.extra_flags='+flags,'--build-property','compiler.c.elf.extra_flags=-Wl,--wrap=esp_intr_alloc -Wl,--wrap=esp_cache_msync '+dsi_link,str(sketch)]
    if args.database_only:command.insert(2,'--only-compilation-database')
    write(work/'compile-args.json',{'command':command,'dependencies':dependencies,'heapProbe':heap_probe})
    initial=manifest(sketch); write(work/'source-manifest-start.json',{'root':str(sketch),'checkedAt':now(),'files':initial})
    result={'mode':'database' if args.database_only else 'build','startedAt':now(),'exitCode':None,'log':str(work/'build.log'),'sketch':str(sketch)}
    write(work/'build-result.json',result)
    env=os.environ.copy(); env['ARDUINO_BUILD_CACHE_PATH']=str(envroot/'cache')
    if env.get('HTTPS_PROXY'):env['ARDUINO_NETWORK_PROXY']=env['HTTPS_PROXY']
    with (work/'build.log').open('wb') as log:
        proc=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,env=env)
        result['pid']=proc.pid; write(work/'build-result.json',result); print(json.dumps(result),flush=True)
        result['compilerExitCode']=proc.wait()
        result['exitCode']=result['compilerExitCode']
    final=manifest(sketch); write(work/'source-manifest-end.json',{'root':str(sketch),'checkedAt':now(),'files':final})
    result['sourcesUnchanged']=initial==final
    if not result['sourcesUnchanged']:result['exitCode']=1;result['sourceValidationError']='Source files changed during compilation.'
    result['finishedAt']=now();write(work/'build-result.json',result);print(json.dumps(result),flush=True)
    return result['exitCode']
if __name__=='__main__':raise SystemExit(main())
