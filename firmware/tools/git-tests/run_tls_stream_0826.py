from pathlib import Path
import hashlib,json,os,subprocess,sys
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture'
build=Path('C:/workspace/tab5-git-tls-stream-0826-asan');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    script=build/'run.cmd';script.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(script)]))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
relative='src/tab5git/src/libgit2/streams/mbedtls.c'
production=sketch/relative;baseline=release.parent/'Tab5OS_Ecriture_v0.8.25/Tab5OS_Arduino_v0.8.25_Ecriture'/relative
results=[]
for name,path in [('current',production),('baseline',baseline)]:
    source=path.read_text(encoding='utf-8');start=source.index('static void mbedtls_stream_free(');end=source.index('int git_mbedtls__set_cert_location(',start)
    harness=(here/'tls_stream_0826.cpp').read_text(encoding='utf-8').replace('/* PRODUCTION */',source[start:end])
    generated=build/(name+'.cpp');generated.write_text(harness,encoding='utf-8');executable=build/(name+'.exe')
    compile=subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/fsanitize=address','/utf-8','/Zi','/D_CRT_SECURE_NO_WARNINGS',str(generated),'/Fe:'+str(executable),'/link','/INCREMENTAL:NO'],cwd=build,capture_output=True,text=True,errors='replace')
    if compile.returncode:print(compile.stdout+compile.stderr);raise SystemExit(1)
    run=subprocess.run([str(executable)]+(['baseline'] if name=='baseline' else []),cwd=build,capture_output=True,text=True)
    results.append(dict(name=name,sourceSha256=sha(path),generatedSha256=sha(generated),exitCode=run.returncode,output=run.stdout+run.stderr))
    print(name,run.returncode,run.stdout+run.stderr)
report=dict(version='0.8.26',passed=results[0]['exitCode']==0 and results[1]['exitCode']!=0 and 'socket close after free' in results[1]['output'],physicalDeviceTested=False,method='Unchanged extracted production mbedTLS stream constructors/destructor, real C++, allocation/socket/TLS boundaries instrumented for failure and exact ownership. Baseline .25 reproduces double socket close/free after SSL setup failure. Compiled and run with MSVC AddressSanitizer; ownership assertions stop the baseline before it dereferences the released socket. No physical network or display test.',results=results,sources={relative:sha(production)},tests={p.name:sha(p) for p in [Path(__file__),here/'tls_stream_0826.cpp']})
(release/'tools/verification/git-tls-stream-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
