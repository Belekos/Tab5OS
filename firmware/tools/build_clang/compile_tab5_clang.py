"""Rebuild Tab5OS with Espressif Clang and its GNU linker on Windows.

No previous Tab5OS compilation cache is required. This script changes neither
the installed Arduino core nor its linker scripts. Library installation and
flashing are deliberately separate operations.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode=True
from prepare_m5gfx import prepare_m5gfx
from prepare_audio import prepare_audio
from prepare_lvgl import prepare_lvgl
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'build_linux'))
from prepare_fs_errno import prepare_fs_errno
from prepare_m5unified_audio import prepare_m5unified_audio
from normalize_clang_deps import normalize_build


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def manifest(root):
    # SD assets are distributed separately. Their generated firmware payloads
    # are headers in this directory and remain covered by this manifest.
    return [{'path':p.relative_to(root).as_posix(),'bytes':p.stat().st_size,
             'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
            for p in sorted(root.rglob('*')) if p.is_file()
            and p.relative_to(root).parts[0] != 'sdcard']


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2), encoding='utf-8')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release-root',type=Path,required=True)
    parser.add_argument('--libraries-dir',type=Path,required=True,
                        help='Directory containing M5Unified, M5GFX, lvgl, ArduinoJson and ESP8266Audio')
    parser.add_argument('--work-dir',type=Path,required=True)
    parser.add_argument('--build-dir',type=Path)
    parser.add_argument('--arduino-data',type=Path,
                        default=Path(os.environ.get('LOCALAPPDATA',''))/'Arduino15')
    parser.add_argument('--arduino-cli',type=Path,
                        default=Path(r'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'))
    parser.add_argument('--clang-root',type=Path,
                        default=Path(r'C:\Espressif\tools\esp-clang\esp-20.1.1_20250829\esp-clang'))
    parser.add_argument('--gcc-root',type=Path)
    parser.add_argument('--jobs',type=int,default=4)
    parser.add_argument('--mode',choices=['prepare','database','build'],default='build')
    opt=parser.parse_args()
    release=opt.release_root.resolve()
    sketches=list(release.glob('Tab5OS_Arduino_v*_Ecriture'))
    sketches=[p for p in sketches if (p/(p.name+'.ino')).is_file()]
    if len(sketches)!=1:
        raise RuntimeError('Expected exactly one Tab5OS Arduino sketch under release-root.')
    sketch=sketches[0]
    work=opt.work_dir.resolve()
    build=opt.build_dir.resolve() if opt.build_dir else work/'build'
    if work==sketch or sketch in work.parents or build==sketch or sketch in build.parents:
        raise RuntimeError('Build/work directories must be outside the source sketch.')
    data=opt.arduino_data.resolve()
    sdk=data/'packages'/'m5stack'/'tools'/'esp32p4_es-libs'/'3.3.9'
    core=data/'packages'/'m5stack'/'hardware'/'esp32'/'3.3.9'
    gcc=opt.gcc_root.resolve() if opt.gcc_root else data/'packages'/'m5stack'/'tools'/'esp-rv32'/'2601'
    clang=opt.clang_root.resolve()/'bin'
    linker=clang/'riscv32-esp-elf-clang-ld.exe'
    helpers=Path(__file__).resolve().parent
    for path in [opt.arduino_cli,clang/'clang.exe',clang/'clang++.exe',clang/'llvm-ar.exe',clang/'llvm-size.exe',
                 linker,core/'platform.txt',sdk/'flags'/'cpp_flags',gcc/'riscv32-esp-elf',helpers/'ctags_declared.py',helpers/'prepare_m5gfx.py',helpers/'prepare_audio.py']:
        if not path.exists():raise RuntimeError('Required dependency missing: '+str(path))
    libraries=opt.libraries_dir.resolve()
    versions={'M5Unified':'0.2.23','M5GFX':'0.2.30','lvgl':'9.6.0','ArduinoJson':'7.4.3','ESP8266Audio':'2.4.1'}
    dependency_report=[]
    for name,version in versions.items():
        path=libraries/name/'library.properties'
        props=dict(line.split('=',1) for line in path.read_text(encoding='utf-8').splitlines() if '=' in line)
        if props.get('version')!=version:
            raise RuntimeError(name+' requires metadata version '+version+', found '+str(props.get('version')))
        dependency_report.append({'name':name,'version':version,'path':str(path.parent)})
    work.mkdir(parents=True,exist_ok=True)
    library_paths={name:libraries/name for name in versions}
    library_paths['M5GFX']=prepare_m5gfx(libraries/'M5GFX',work)
    library_paths['M5Unified']=prepare_m5unified_audio(libraries/'M5Unified',work)
    library_paths['ESP8266Audio']=prepare_audio(libraries/'ESP8266Audio',work)
    library_paths['lvgl']=prepare_lvgl(libraries/'lvgl',work)
    library_paths['FS']=prepare_fs_errno(core/'libraries/FS',work)
    dependency_report.append({'name':'FS','version':'3.3.9','path':str(core/'libraries/FS'),'patch':'precise errno only'})
    for item in dependency_report:
        item['buildPath']=str(library_paths[item['name']])
    (work/'temp').mkdir(exist_ok=True)
    (work/'sketchbook').mkdir(exist_ok=True)
    config=work/'arduino-cli.yaml'
    config.write_text('directories:\n  data: '+json.dumps(data.as_posix())+'\n  downloads: '+json.dumps((work/'downloads').as_posix())+
                      '\n  user: '+json.dumps((work/'sketchbook').as_posix())+'\nmetrics:\n  enabled: false\nupdater:\n  enable_notification: false\n')
    # Clang does not implement these GCC-specific options. All ABI, exception,
    # optimization, section and stack-protection settings remain as supplied.
    skipped={'-freorder-blocks','-fstrict-volatile-bitfields','-fno-tree-switch-conversion'}
    for name in ['c_flags','cpp_flags','S_flags']:
        flags=[f for f in (sdk/'flags'/name).read_text().split() if f not in skipped]
        flagfile=work/name
        flagtext=' '.join(flags)
        if not flagfile.exists() or flagfile.read_text()!=flagtext:
            flagfile.write_text(flagtext)
    def property_argument(value):
        # Arduino's recipe parser needs quotes around the entire argument,
        # not embedded after '=' (Clang would receive those as path bytes).
        return '"'+value+'"' if any(c.isspace() for c in value) else value
    target='--target=riscv32-esp-unknown-elf '+property_argument('--gcc-toolchain='+gcc.as_posix())+' -Wno-error=reserved-user-defined-literal'
    extra='-fstack-usage -DLV_CONF_INCLUDE_SIMPLE '+property_argument('-I'+sketch.as_posix())
    from prepare_dsi_cache_safe import prepare_dsi_cache_safe
    dsi_archives=prepare_dsi_cache_safe(sdk,clang/'clang.exe',work,
        compiler_flags=['--target=riscv32-esp-unknown-elf','--gcc-toolchain='+gcc.as_posix(),'-Wno-error=reserved-user-defined-literal'],
        archiver=clang/'llvm-ar.exe',excluded_flags=skipped)
    dsi_link='-Wl,--whole-archive '+' '.join(property_argument(p.as_posix()) for p in dsi_archives)+' -Wl,--no-whole-archive'
    fqbn='m5stack:esp32:m5stack_tab5:ChipVariant=prev3,PSRAM=enabled,PartitionScheme=default,FlashMode=dio'
    props={
        'compiler.path':clang.as_posix()+'/',
        'compiler.c.cmd':'clang','compiler.cpp.cmd':'clang++','compiler.S.cmd':'clang',
        'compiler.c.elf.cmd':'clang++','compiler.ar.cmd':'llvm-ar','compiler.size.cmd':'llvm-size',
        'compiler.c.flags':'-MMD -c "@'+(work/'c_flags').as_posix()+'" {compiler.warning_flags} {compiler.optimization_flags} {compiler.common_werror_flags} '+target,
        'compiler.cpp.flags':'-MMD -c "@'+(work/'cpp_flags').as_posix()+'" {compiler.warning_flags} {compiler.optimization_flags} {compiler.common_werror_flags} '+target,
        'compiler.S.flags':'-MMD -c -x assembler-with-cpp "@'+(work/'S_flags').as_posix()+'" {compiler.warning_flags} {compiler.optimization_flags} '+target,
        'compiler.c.extra_flags':extra,'compiler.cpp.extra_flags':extra,
        'compiler.c.elf.extra_flags':target+' '+property_argument('-fuse-ld='+linker.as_posix())+' -Wl,--wrap=esp_intr_alloc '+dsi_link,
    }
    # Tab5OS supplies explicit function declarations. Verify that property with
    # the real target compiler before the Arduino builder proceeds, instead of
    # depending on the optional Exuberant Ctags Windows executable.
    declared=work/'declared-sketch.json'
    fallback_includes=[str(core/'cores'/'esp32'),str(core/'variants'/'m5stack_tab5')]
    fallback_includes.extend(str(library_paths[name]/'src') for name in library_paths)
    fallback_includes.extend(str(core/'libraries'/name/'src') for name in ['SPI','SD','Preferences','NetworkClientSecure','Network','WiFi','HTTPClient','Wire','SPIFFS'])
    write_json(declared,{'clang':str(clang/'clang++.exe'),'sketch':str(sketch/(sketch.name+'.ino')),'includeCache':str(build/'libraries.cache'),'fallbackIncludes':fallback_includes,
        'flags':['--target=riscv32-esp-unknown-elf','--gcc-toolchain='+gcc.as_posix(),'-Wno-error=reserved-user-defined-literal',
            '-w','-DLV_CONF_INCLUDE_SIMPLE','-DF_CPU=360000000L','-DARDUINO=10607','-DARDUINO_M5STACK_TAB5','-DARDUINO_ARCH_ESP32',
            '-DARDUINO_BOARD="M5STACK_TAB5"','-DARDUINO_VARIANT="m5stack_tab5"','-DARDUINO_PARTITION_default_16MB',
            '-DARDUINO_HOST_OS="windows"','-DARDUINO_FQBN="'+fqbn+'"','-DESP32=ESP32','-DCORE_DEBUG_LEVEL=0','-DBOARD_HAS_PSRAM',
            '-DARDUINO_USB_MODE=1','-DARDUINO_USB_CDC_ON_BOOT=1','-DARDUINO_USB_MSC_ON_BOOT=0','-DARDUINO_USB_DFU_ON_BOOT=0',
            '@'+(work/'cpp_flags').as_posix(),'@'+(sdk/'flags'/'defines').as_posix(),'-I'+sketch.as_posix(),
            '-iprefix',(sdk/'include').as_posix()+'/','@'+(sdk/'flags'/'includes').as_posix(),'-I'+(sdk/'dio_qspi'/'include').as_posix()]})
    props['tools.ctags.pattern']='"'+Path(sys.executable).as_posix()+'" "'+(helpers/'ctags_declared.py').as_posix()+'" "{source_file}" "'+declared.as_posix()+'"'
    command=[str(opt.arduino_cli),'compile','--verbose','--config-file',str(config),'--fqbn',fqbn,
             '--build-path',str(build),'--jobs',str(max(1,opt.jobs))]
    for key,value in props.items():command.extend(['--build-property',key+'='+value])
    for name in library_paths:command.extend(['--library',str(library_paths[name])])
    command.append(str(sketch))
    if opt.mode=='database':command.append('--only-compilation-database')
    write_json(work/'compile-args.json',{'command':command,'dependencies':dependency_report,'skippedGccOptions':sorted(skipped)})
    if opt.mode=='prepare':print(work/'compile-args.json');return 0
    # Arduino CLI compares the first depfile source as an exact Windows string.
    # Clang emits forward slashes. Normalize only verified existing cache paths;
    # unchanged object/source timestamps and the CLI's own checks remain in force.
    write_json(work/'dependency-cache-normalization.json', normalize_build(build, apply=True))
    initial=manifest(sketch)
    write_json(work/'source-manifest-start.json',{'checkedAt':utc_now(),'root':str(sketch),'files':initial})
    tool_versions={}
    for name,cmd in [('arduino-cli',[str(opt.arduino_cli),'version']),('clang',[str(clang/'clang.exe'),'--version']),('linker',[str(linker),'--version'])]:
        result=subprocess.run(cmd,capture_output=True,text=True,check=True)
        tool_versions[name]=result.stdout.strip()
    write_json(work/'toolchain-versions.json',tool_versions)
    result={'mode':opt.mode,'startedAt':utc_now(),'exitCode':None,'log':str(work/(opt.mode+'.log'))}
    write_json(work/(opt.mode+'-result.json'),result)
    env=os.environ.copy()
    env['ARDUINO_BUILD_CACHE_PATH']=str(work/'cache')
    env['TEMP']=env['TMP']=str(work/'temp')
    try:
        with (work/(opt.mode+'.log')).open('wb') as log:
            proc=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,env=env)
            result['pid']=proc.pid
            write_json(work/(opt.mode+'-result.json'),result)
            print(json.dumps(result),flush=True)
            result['exitCode']=proc.wait()
        final=manifest(sketch)
        write_json(work/'source-manifest-end.json',{'checkedAt':utc_now(),'root':str(sketch),'files':final})
        result['sourcesUnchanged']=initial==final
        if not result['sourcesUnchanged']:
            result['sourceValidationError']='Source files changed during compilation.'
            result['exitCode']=1
    except Exception as exc:
        result['exitCode']=1
        result['error']=str(exc)
    finally:
        result['finishedAt']=utc_now()
        write_json(work/(opt.mode+'-result.json'),result)
        print(json.dumps(result),flush=True)
    return result['exitCode']


if __name__=='__main__':
    raise SystemExit(main())
