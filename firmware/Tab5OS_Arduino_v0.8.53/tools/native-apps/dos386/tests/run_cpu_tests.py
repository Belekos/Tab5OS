"""Exercise real Tiny386 interpreter/fast/predecoded cores; MSVC tests interpreter only."""
import argparse,json,os,pathlib,subprocess,time

HERE=pathlib.Path(__file__).resolve().parent
CORE=HERE.parent/'core'
FEATURES=['I386_ENABLE_FPU','I386_ENABLE_MMX','I386_ENABLE_SSE','I386_ENABLE_SSE2','I386_ENABLE_SSE3','I386_ENABLE_SSSE3']

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--output',type=pathlib.Path,required=True)
 p.add_argument('--cc',default='cl' if os.name=='nt' else 'gcc')
 p.add_argument('--sanitize',action='store_true')
 a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 msvc=pathlib.Path(a.cc).name.lower() in ('cl','cl.exe')
 if msvc and a.sanitize:p.error('--sanitize requires GCC or Clang; MSVC runs the interpreter without sanitizers')
 results=[]
 for mode in (['interpreter'] if msvc else ['interpreter','fast','predecoded']):
  defs=FEATURES+([] if mode=='interpreter' else ['USE_FAST'])+(['FAST_PD','FAST_PD_SIZE=262144'] if mode=='predecoded' else [])
  exe=a.output/(mode+('.exe' if msvc else ''))
  if msvc:
   compat=a.output/'compat';compat.mkdir(exist_ok=True);(compat/'unistd.h').write_text('/* Host test requires no POSIX IO. */\n')
   cmd=[a.cc,'/nologo','/O2','/std:c11','/Zc:preprocessor','/wd4244','/wd4267','/I'+str(CORE),'/I'+str(compat),'/FI'+str(HERE/'cpu_host_compat.h'),*['/D'+d for d in defs],str(HERE/'cpu_safety.c'),str(CORE/'fpu.c'),'/Fe:'+str(exe)]
  else:
   cmd=[a.cc,'-std=gnu11','-O2','-g','-D_GNU_SOURCE','-DI386_ALIGNED_ONLY','-I',str(CORE),*['-D'+d for d in defs],str(HERE/'cpu_safety.c'),str(CORE/'fpu.c'),'-lm','-o',str(exe)]
   if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']
  config='host ASan/UBSan -O2 (GCC fast_exec: -O3, simple block ordering)' if a.sanitize else 'normal host optimization'
  print('Building '+mode+': '+config,flush=True)
  build_log=a.output/(mode+'-build.log');started=time.monotonic()
  try:
   build=subprocess.run(cmd,cwd=a.output,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=900)
  except subprocess.TimeoutExpired as exc:
   output=exc.stdout or ''
   if isinstance(output,bytes):output=output.decode(errors='replace')
   build_log.write_text(output+'\nBuild timed out after 900 seconds.\n')
   raise SystemExit('Build timed out: '+str(build_log))
  build_log.write_text(build.stdout)
  if build.returncode:raise SystemExit('Build failed: '+str(build_log))
  print('Running '+mode+' (build %.1f s)'%(time.monotonic()-started),flush=True)
  env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
  run_log=a.output/(mode+'-run.log')
  try:
   run=subprocess.run([str(exe)],cwd=a.output,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
  except subprocess.TimeoutExpired as exc:
   output=exc.stdout or ''
   if isinstance(output,bytes):output=output.decode(errors='replace')
   run_log.write_text(output+'\nExecution timed out after 60 seconds.\n')
   raise SystemExit('Execution timed out: '+str(run_log))
  run_log.write_text(run.stdout)
  results.append({'mode':mode,'passed':run.returncode==0,'output':run.stdout.strip()})
  print(mode+': '+run.stdout.strip(),flush=True)
  if run.returncode:raise SystemExit(run.returncode)
 (a.output/'cpu-tests.json').write_text(json.dumps({'passed':True,'compiler':a.cc,'sanitized':a.sanitize,'results':results},indent=2)+'\n')

if __name__=='__main__':main()
