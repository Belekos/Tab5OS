"""Compile the exact compatibility source as C against the shipping P4 SDK."""
from pathlib import Path
import datetime,hashlib,json,os,re,subprocess
here=Path(__file__).resolve().parent;release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.28_Ecriture';source=sketch/'Tab5CryptoLockCompat.c'
build=Path('C:/workspace/tab5-crypto-lock-0828');build.mkdir(exist_ok=True)
database=Path(os.environ.get('TAB5_COMPILE_DATABASE','C:/workspace/tab5-build-0827-clang/compile_commands.json'))
entries=json.loads(database.read_text());entry=next(x for x in entries if x['file'].endswith('.c'))
args=entry['arguments'][:]
for i,x in enumerate(args):
    normalized=x.replace('\\','/')
    if '/work/Tab5OS_' in normalized:args[i]=x.replace('0.8.27','0.8.28')
out=build/'Tab5CryptoLockCompat.c.o'
args[args.index('-o')-1]=str(source);args[args.index('-o')+1]=str(out)
compile=subprocess.run(args,capture_output=True,text=True,encoding='utf-8',errors='replace')
if compile.returncode:print(compile.stdout+compile.stderr);raise SystemExit(compile.returncode)
nm=Path(args[0]).parent/'llvm-nm.exe'
sdk=Path('C:/Users/belek/AppData/Local/Arduino15/packages/m5stack/tools/esp32p4_es-libs/3.3.9')
def symbols(path):
    p=subprocess.run([str(nm),'--defined-only','--print-file-name',str(path)],capture_output=True,text=True,encoding='utf-8',errors='replace')
    if p.returncode:raise RuntimeError(p.stderr)
    return p.stdout
ours=symbols(out);original=symbols(sdk/'lib/libesp_security.a')
our_names=re.findall(r'\bT (esp_crypto_\w+_lock_(?:acquire|release))\b',ours)
sdk_lines='\n'.join(line for line in original.splitlines() if 'esp_crypto_lock.c.obj' in line)
sdk_names=re.findall(r'\bT (esp_crypto_\w+_lock_(?:acquire|release))\b',sdk_lines)
if len(our_names)!=14 or len(our_names)!=len(set(our_names)) or set(our_names)!=set(sdk_names):
    raise RuntimeError('Compatibility symbol set differs from the real SDK lock module: '+repr((our_names,sdk_names)))
# Partial link against the real archive proves it does not pull a second set
# of lock instances. The final firmware linker map is checked separately.
linker=Path(args[0]).parent/'ld.lld.exe';linked=build/'compat-with-sdk.o';mapfile=build/'compat-with-sdk.map'
link=subprocess.run([str(linker),'-r','-Map='+str(mapfile),str(out),str(sdk/'lib/libesp_security.a'),'-o',str(linked)],capture_output=True,text=True,encoding='utf-8',errors='replace')
if link.returncode:print(link.stdout+link.stderr);raise SystemExit(link.returncode)
linked_symbols=symbols(linked)
linked_names=re.findall(r'\bT (esp_crypto_\w+_lock_(?:acquire|release))\b',linked_symbols)
maptext=mapfile.read_text()
if sorted(linked_names)!=sorted(our_names) or 'esp_crypto_lock.c.obj' in maptext:
    raise RuntimeError('SDK duplicate lock module was extracted')
su=out.with_suffix('.su');usage=su.read_text()
frames={}
for line in usage.splitlines():
    p=line.split('\t')
    if len(p)>=2:frames[p[0].rsplit(':',1)[-1]]=int(p[1])
report=dict(version='0.8.28',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,physicalDeviceTested=False,target='ESP32-P4 rev<3 / M5Stack Arduino 3.3.9 / ESP-IDF 5.5.4',compileExit=compile.returncode,linkExit=link.returncode,symbolCount=len(our_names),symbols=sorted(our_names),sdkOriginalModuleExtracted=False,method='Real P4 C compile and symbol parity with shipping SDK archive; relocatable link proves no duplicate SDK lock module. Final firmware provenance requires its separate final linker-map check. Own stack frames exclude callees.',stackFrames=frames,sources={'Tab5CryptoLockCompat.c':hashlib.sha256(source.read_bytes()).hexdigest()},tests={str(Path(__file__).relative_to(release)).replace('\\','/'):hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},sdkHashes={n:hashlib.sha256((sdk/n).read_bytes()).hexdigest() for n in ['lib/libesp_security.a','versions.txt','sdkconfig']})
(release/'tools/verification/crypto-lock-target-0828.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
print('PASS P4 C compile; 14 SDK-equivalent symbols; no duplicate SDK lock module; ECDSA own stack '+str(frames['esp_crypto_ecdsa_lock_acquire'])+' bytes')
