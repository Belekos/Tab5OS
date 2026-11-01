"""Require the final firmware to use the guarded compatibility lock module."""
from pathlib import Path
import argparse,datetime,hashlib,json,re,subprocess

def verify_source_copy(original,copied,line_directive):
    """Accept exact bytes, or only Arduino's exact initial source-location line.

    No whitespace/BOM/newline normalization or arbitrary directive stripping:
    the complete original body must remain byte-identical.
    """
    if copied==original:return 'byte-identical'
    if copied==line_directive+original:return 'arduino-line-directive'
    raise RuntimeError('Final build copy differs from current compatibility source')

here=Path(__file__).resolve().parent;release=here.parents[1]
p=argparse.ArgumentParser();p.add_argument('--build-dir',type=Path,default=Path('C:/workspace/tab5-build-0828-clang'));options=p.parse_args()
build=options.build_dir
elf=next(build.glob('*.ino.elf'));mapfile=elf.with_suffix('.map')
source=release/'Tab5OS_Arduino_v0.8.28_Ecriture/Tab5CryptoLockCompat.c'
compiled=build/'sketch/Tab5CryptoLockCompat.c'
text=mapfile.read_text(encoding='utf-8',errors='replace')
if 'esp_crypto_lock.c.obj' in text:raise RuntimeError('Final firmware still includes the original SDK crypto lock module')
original=source.read_bytes();copied=compiled.read_bytes()
escaped_path=str(source.resolve()).replace('\\','\\\\').replace('"','\\"')
line_directive=('#line 1 "'+escaped_path+'"\n').encode('utf-8')
copy_mode=verify_source_copy(original,copied,line_directive)
assert verify_source_copy(original,original,line_directive)=='byte-identical'
assert verify_source_copy(original,line_directive+original,line_directive)=='arduino-line-directive'
negative_copies={
    'modified-body':line_directive+original[:-1]+bytes([original[-1]^1]),
    'different-path':line_directive.replace(b'Tab5CryptoLockCompat.c',b'Other.c')+original,
    'different-line':line_directive.replace(b'#line 1 ',b'#line 2 ')+original,
    'extra-directive':line_directive+line_directive+original,
    'extra-newline':line_directive+original+b'\n',
}
for label,corrupt in negative_copies.items():
    try:verify_source_copy(original,corrupt,line_directive)
    except RuntimeError:pass
    else:raise RuntimeError('Source-copy negative control accepted '+label)
entries=json.loads((build/'compile_commands.json').read_text())
entry=next(x for x in entries if Path(x['file']).name=='Tab5CryptoLockCompat.c')
nm=Path(entry['arguments'][0]).parent/'llvm-nm.exe'
process=subprocess.run([str(nm),'--defined-only',str(elf)],capture_output=True,text=True,encoding='utf-8',errors='replace')
if process.returncode:raise RuntimeError(process.stderr)
symbols=re.findall(r'\bT (esp_crypto_\w+_lock_(?:acquire|release))\b',process.stdout)
addresses={name:int(address,16) for address,name in re.findall(r'^([0-9a-f]+)\s+T (esp_crypto_\w+_lock_(?:acquire|release))$',process.stdout,re.MULTILINE|re.IGNORECASE)}
if len(symbols)!=len(set(symbols)):raise RuntimeError('Duplicate final crypto lock symbols')
required=['esp_crypto_'+unit+'_lock_'+action for unit in ['ecdsa','ecc','sha_aes','mpi'] for action in ['acquire','release']]
for symbol in required:
    if symbols.count(symbol)!=1:raise RuntimeError('Missing final lock symbol '+symbol)
    pattern=r'\.text\.'+re.escape(symbol)+r'\s+(0x[0-9a-f]+)\s+0x[0-9a-f]+[^\n]*Tab5CryptoLockCompat\.c\.o'
    owners=[int(match,16) for match in re.findall(pattern,text,re.IGNORECASE)]
    if owners.count(addresses[symbol])!=1:raise RuntimeError('Final ELF address not uniquely owned by compatibility object: '+symbol)
report=dict(version='0.8.28',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,physicalDeviceTested=False,method='Final ELF nm and GNU linker map: original SDK lock object absent; eight essential lock symbols unique, with ELF addresses matching their sections owned by Tab5CryptoLockCompat.c.o. Build source must be byte-identical, or contain only one initial #line 1 directive naming the exact source path followed by its byte-identical body. No other normalization accepted.',sdkOriginalModuleExtracted=False,requiredSymbols=required,definedSymbols=sorted(symbols),symbolAddresses={name:hex(addresses[name]) for name in required},sourceCopyIdentical=copied==original,sourceBodyIdentical=True,sourceCopyMode=copy_mode,arduinoLineDirectiveBytes=len(line_directive) if copy_mode=='arduino-line-directive' else 0,sourceCopyNegativeControls=list(negative_copies),compiledSourceSha256=hashlib.sha256(copied).hexdigest(),artifacts={n.name:hashlib.sha256(n.read_bytes()).hexdigest() for n in [elf,mapfile]},sources={'Tab5CryptoLockCompat.c':hashlib.sha256(original).hexdigest()},tests={str(Path(__file__).relative_to(release)).replace('\\','/'):hashlib.sha256(Path(__file__).read_bytes()).hexdigest()})
(release/'tools/verification/crypto-lock-link-0828.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
print('PASS final firmware crypto lock ownership; SDK original absent; 8 essential symbols unique with matching ELF/map addresses; source body byte-identical; copy mode='+copy_mode+'; 5 negative source-copy controls rejected')
