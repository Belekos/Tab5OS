"""Check PIC math supplements against host libm, including IEEE edge cases."""
import argparse,json,os,pathlib,subprocess

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--output',type=pathlib.Path,required=True)
 p.add_argument('--cc',default='cl' if os.name=='nt' else 'gcc')
 p.add_argument('--sanitize',action='store_true')
 a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 msvc=pathlib.Path(a.cc).name.lower() in ('cl','cl.exe')
 if msvc and a.sanitize:p.error('--sanitize requires GCC or Clang')
 src=pathlib.Path(__file__).with_name('math_compat_test.c')
 exe=a.output/('math.exe' if msvc else 'math')
 if msvc:
  cmd=[a.cc,'/nologo','/O2','/std:c11','/Zc:preprocessor',str(src),'/Fe:'+str(exe)]
 else:
  cmd=[a.cc,'-std=gnu11','-O1' if a.sanitize else '-O2','-g','-fno-builtin',str(src),'-lm','-o',str(exe)]
  if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
 print('Building math compatibility probe',flush=True)
 result=subprocess.run(cmd,cwd=a.output,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)
 (a.output/'math-build.log').write_text(result.stdout)
 if result.returncode:raise SystemExit('Build failed: '+str(a.output/'math-build.log'))
 result=subprocess.run([str(exe)],cwd=a.output,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=30)
 (a.output/'math-run.log').write_text(result.stdout)
 report={'passed':result.returncode==0,'compiler':a.cc,'sanitized':a.sanitize,'output':result.stdout.strip()}
 (a.output/'math-tests.json').write_text(json.dumps(report,indent=2)+'\n')
 print(result.stdout.strip(),flush=True)
 raise SystemExit(result.returncode)

if __name__=='__main__':main()
