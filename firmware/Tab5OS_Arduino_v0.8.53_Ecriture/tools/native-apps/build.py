#!/usr/bin/env python3
"""Build a Tab5 ABI-1/2/3/4/5 app with Espressif GCC, or freestanding Clang/LLD."""
import argparse,os,pathlib,shutil,subprocess,sys,tempfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('source',type=pathlib.Path)
p.add_argument('-o','--output',type=pathlib.Path)
p.add_argument('--toolchain',type=pathlib.Path,help='toolchain bin directory (or set ESP_RV32_BIN)')
p.add_argument('--compiler',choices=['gcc','clang'],default='gcc',help='clang uses an Espressif Clang bin directory; no hosted C library')
p.add_argument('--extra-source',action='append',type=pathlib.Path,default=[])
p.add_argument('--define',action='append',default=[])
p.add_argument('--include',action='append',type=pathlib.Path,default=[])
p.add_argument('--math',action='store_true')
p.add_argument('--legacy-c',action='store_true',help='Allow legacy C integer/pointer conversions; platform remains RV32')
p.add_argument('--common',action='store_true',help='Allow legacy C tentative global definitions')
p.add_argument('--opt',choices=['size','speed'],default='size')
a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[2]
tool=a.toolchain or (pathlib.Path(os.environ['ESP_RV32_BIN']) if 'ESP_RV32_BIN' in os.environ else None)
prefix='llvm-' if a.compiler=='clang' else 'riscv32-esp-elf-'
exe=('clang' if a.compiler=='clang' else 'riscv32-esp-elf-gcc')+('.exe' if os.name=='nt' else '')
cc=str(tool/exe) if tool else shutil.which(exe)
if not cc or not pathlib.Path(cc).is_file():p.error('Compiler not found. Pass --toolchain its bin directory and select --compiler gcc or clang.')
if not a.source.is_file():p.error('source does not exist')
if a.compiler=='clang' and a.math:p.error('--math needs the GCC toolchain libraries. Use SDK callbacks or --compiler gcc.')
for source in a.extra_source:
 if not source.is_file():p.error('extra source does not exist: '+str(source))
out=a.output or a.source.with_suffix('.app.elf');out.parent.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='.tab5-build-',dir=out.parent) as temporary:
 candidate=pathlib.Path(temporary)/out.name
 cmd=[cc,*(['--target=riscv32-esp-elf','-fuse-ld=lld'] if a.compiler=='clang' else []),'-march=rv32imafc_zicsr_zifencei','-mabi=ilp32','-O3' if a.opt=='speed' else '-Os','-fPIC',*(['-Wno-error=int-conversion','-Wno-error=incompatible-pointer-types','-Wno-error=implicit-function-declaration'] if a.legacy_c else []),*(['-fcommon'] if a.common else []),'-ffunction-sections','-fdata-sections','-fvisibility=default' if a.compiler=='clang' else '-fvisibility=hidden','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-nostdlib','-nostartfiles','-shared','-Wl,-e,tab5_app_main,-Bsymbolic,--hash-style=sysv,--build-id=none,-z,max-page-size=4096,--gc-sections','-I',str(root),*[str(p) for p in [a.source]+a.extra_source],*['-D'+d for d in a.define],*[v for d in a.include for v in ['-I',str(d)]],*(['-lm','-lgcc'] if a.math else []),'-o',str(candidate)]
 try:
  if a.compiler=='clang':
   # The bare-metal Clang driver ignores -shared. Compile each translation unit
   # explicitly, then invoke LLD for the ET_DYN image expected by the loader.
   objects=[]
   for index,source in enumerate([a.source]+a.extra_source):
    obj=pathlib.Path(temporary)/(str(index)+'.o');objects.append(str(obj))
    flags=[cc,'--target=riscv32-esp-elf','-Wno-missing-multilib','-march=rv32imafc_zicsr_zifencei','-mabi=ilp32','-O3' if a.opt=='speed' else '-Os','-fPIC','-ffunction-sections','-fdata-sections','-fvisibility=default','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables',*(['-fcommon'] if a.common else []),*(['-Wno-error=int-conversion','-Wno-error=incompatible-pointer-types','-Wno-error=implicit-function-declaration'] if a.legacy_c else []),'-I',str(root),*['-D'+d for d in a.define],*[v for d in a.include for v in ['-I',str(d)]],'-c',str(source),'-o',str(obj)]
    subprocess.run(flags,check=True)
   linker=str(pathlib.Path(cc).with_name('ld.lld'+('.exe' if os.name=='nt' else '')))
   subprocess.run([linker,'-m','elf32lriscv','-shared','-e','tab5_app_main','-Bsymbolic','--hash-style=sysv','--build-id=none','-z','max-page-size=4096','--gc-sections',*objects,'-o',str(candidate)],check=True)
  else:
   subprocess.run(cmd,check=True)
  readelf=str(pathlib.Path(cc).with_name(prefix+'readelf'+('.exe' if os.name=='nt' else '')))
  result=subprocess.run([readelf,'-Ws',str(candidate)],check=True,text=True,capture_output=True)
  imports={line.split()[-1] for line in result.stdout.splitlines() if ' UND ' in line and len(line.split())>=8}
  if not imports or not imports.issubset({'tab5_app_api_v1','tab5_app_api_v2','tab5_app_api_v3','tab5_app_api_v4','tab5_app_api_v5','tab5_app_api_v6'}):
   sys.exit('Unsupported imports: '+repr(sorted(imports))+'; use only Tab5AppApi.h callbacks.')
  strip=str(pathlib.Path(cc).with_name(prefix+'strip'+('.exe' if os.name=='nt' else '')))
  subprocess.run([strip,'--strip-unneeded',str(candidate)],check=True)
  os.replace(candidate,out)
 except OSError as error:
  sys.exit('Cannot run the selected toolchain: '+str(error)+'; existing output was preserved.')
print(out.resolve())
