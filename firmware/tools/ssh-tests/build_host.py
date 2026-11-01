from pathlib import Path
import subprocess,json,os,hashlib,re
here=Path(__file__).resolve().parent
sketch=here.parent.parent/'Tab5OS_Arduino_v0.8.22_Ecriture'
src=sketch/'src/tab5ssh'
out=Path(os.environ.get('TAB5_SSH_TEST_DIR','C:/workspace/tab5-ssh-0822-host'));out.mkdir(parents=True,exist_ok=True)
mbed=Path(os.environ.get('MBEDTLS_ROOT',str(here.parents[2]/'mbedtls-idf554-0822'))).resolve()
mbed_version=re.search(r'#define MBEDTLS_VERSION_STRING\s+"([^"]+)"',(mbed/'include/mbedtls/build_info.h').read_text()).group(1)
mbed_inputs=sorted((mbed/'library').glob('*.c'))+sorted((mbed/'library').glob('*.h'))+sorted((mbed/'include').rglob('*.h'))
digest=hashlib.sha256()
for path in mbed_inputs:digest.update(path.relative_to(mbed).as_posix().encode());digest.update(path.read_bytes())
source_hash=digest.hexdigest()
crypto=out/('mbedtls-'+mbed_version+'-'+source_hash[:12]);crypto.mkdir(exist_ok=True)
mbed_sources=sorted((mbed/'library').glob('*.c'))
if not (crypto/'built.ok').exists():
 result=subprocess.run(['cl','/nologo','/MDd','/D_DEBUG','/W2','/wd4996','/utf-8','/I'+str(mbed/'include'),'/I'+str(mbed/'library'),'/c']+[str(p) for p in mbed_sources],cwd=crypto,capture_output=True,text=True,encoding='utf-8',errors='replace')
 (crypto/'build.log').write_text(result.stdout+result.stderr,encoding='utf-8')
 if result.returncode:print((result.stdout+result.stderr)[-16000:]);raise SystemExit(result.returncode)
 (crypto/'built.ok').write_text('1')
common=['cl','/nologo','/MDd','/D_DEBUG','/W3','/wd4996','/wd4267','/utf-8','/DLIBSSH2_MBEDTLS','/DLIBSSH2_STATIC','/DLIBSSH2_NO_MD5','/DLIBSSH2_NO_HMAC_RIPEMD','/DLIBSSH2_NO_DSA','/DLIBSSH2_NO_RSA_SHA1','/DLIBSSH2_NO_AES_CBC','/DLIBSSH2_NO_3DES','/I'+str(src),'/I'+str(mbed/'include')]
sources=[p for p in sorted(src.glob('*.c')) if p.name not in ('agent_win.c',)]
build=subprocess.run(common+['/c']+[str(p) for p in sources],cwd=out,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/'build.log').write_text(build.stdout+build.stderr,encoding='utf-8')
if build.returncode: print((build.stdout+build.stderr)[-12000:]);raise SystemExit(build.returncode)
app=subprocess.run(common+['/std:c++17','/EHsc','/DTAB5_SSH_HOST_TEST','/I'+str(here),'/I'+str(sketch),str(here/'client.cpp'),str(sketch/'Tab5Ssh.cpp')]+[str(out/(p.stem+'.obj')) for p in sources]+[str(p) for p in crypto.glob('*.obj')]+['/Fe:'+str(out/'client.exe'),'/link','ws2_32.lib','crypt32.lib','bcrypt.lib','user32.lib','advapi32.lib'],cwd=out,capture_output=True,text=True,encoding='utf-8',errors='replace')
(out/'app-build.log').write_text(app.stdout+app.stderr,encoding='utf-8')
if app.returncode:print((app.stdout+app.stderr)[-12000:])
else:
 commit=subprocess.run(['git','-C',str(mbed),'rev-parse','HEAD'],capture_output=True,text=True).stdout.strip()
 provenance={'mbedtlsVersion':mbed_version,'mbedtlsCommit':commit,'mbedtlsSourcesSha256':source_hash,'mbedtlsBuildInfoSha256':hashlib.sha256((mbed/'include/mbedtls/build_info.h').read_bytes()).hexdigest(),'configuration':'Native Windows default mbedTLS configuration; same pinned core source as ESP-IDF 5.5.4, not target hardware/configuration.','clientSha256':hashlib.sha256((out/'client.exe').read_bytes()).hexdigest(),'sourceUrl':'https://github.com/espressif/mbedtls/tree/'+commit,'cryptoObjects':str(crypto)}
 app_inputs=list(src.glob('*.c'))+list(src.glob('*.h'))+list(sketch.glob('Tab5Ssh*'))+[sketch/'Tab5TerminalPath.h',here/'SshPlatform.h',here/'client.cpp',here/'build_host.py']
 provenance['appInputs']={str(path.relative_to(here.parent.parent)):hashlib.sha256(path.read_bytes()).hexdigest() for path in app_inputs if path.is_file()}
 (out/'build-provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
 print('Real libssh2 mbedTLS',mbed_version,'host client built:',out/'client.exe')
 print('Native crypto objects:',crypto)
raise SystemExit(app.returncode)
