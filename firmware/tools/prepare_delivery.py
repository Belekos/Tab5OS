"""Install only a verified current OTA firmware and exact build evidence."""
from pathlib import Path
import argparse,hashlib,json,re,shutil
from release_config import ROOT,VERSION,TAG,SKETCH,BASELINE_VERSION
from build_linux.compile_linux import manifest
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text(encoding='utf-8'))
def safe(root,relative):
 p=(root/relative).resolve();assert p.is_relative_to(root.resolve()),relative;return p
def proof(path):
 d=read(path);assert d.get('version',d.get('release'))==VERSION and (d.get('passed') is True or d.get('result')=='PASS'),path
 for group in ('sourceHashes','testSourceHashes','testHashes','integrationSourceHashes','evidenceHashes','screenshots'):
  for n,h in d.get(group,{}).items():assert sha(safe(ROOT,n))==h,(path,n)
 return d
def section(p,name,content):
 s=p.read_text();s,n=re.subn(r'(?s)<!-- '+name+r' -->.*?<!-- /'+name+r' -->',lambda m:'<!-- '+name+' -->\n'+content+'\n<!-- /'+name+' -->',s);assert n==1,(p,name);p.write_text(s)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--environment',type=Path,required=True);e=ap.parse_args().environment.resolve();qa=e/'qa';build=e/'build';sketch=ROOT/SKETCH;proofs=ROOT/'tools/verification'
 report=read(qa/f'firmware-verification-{TAG}.json');assert report['passed'] and report['sourcesUnchanged'] and report['buildExitCode']==0 and report['version']==VERSION and not report['physicalDeviceTested']
 manifests=[read(qa/f'source-manifest-{n}.json')['files'] for n in ['start','end','final']];assert manifests[0]==manifests[1]==manifests[2]==manifest(sketch)
 for n,h in report['evidenceSha256'].items():assert sha(safe(qa,n))==h,n
 scope=read(ROOT/'tools/release_scope.json');assert scope['version']==VERSION and scope['finalized']
 names=[x['file'] for x in scope['targetedReports']]+[f'preserved-components-{TAG}.json',f'languages-{TAG}.json',f'evolutions-localization-{TAG}.json',f'resources-{TAG}.json']
 tests={n:proof(proofs/n) for n in names}
 dest=proofs/f'build-{TAG}';dest.mkdir(exist_ok=True)
 for n in set(report['evidenceSha256'])|{'toolchain-versions.json','lvgl-arabic-patch.json',f'firmware-verification-{TAG}.json'}:
  p=safe(qa,n);q=safe(dest,n);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
 shutil.copyfile(qa/f'firmware-verification-{TAG}.json',proofs/f'firmware-verification-{TAG}.json')
 firmware=ROOT/'firmware';debug=firmware/'debug';debug.mkdir(parents=True,exist_ok=True);previous=f'Tab5OS_Arduino_v{BASELINE_VERSION}_Ecriture.ino'
 allowed={f'Tab5OS_v{BASELINE_VERSION}.bin',f'Tab5OS_v{VERSION}.bin','SHA256.txt','debug/'+previous+'.elf','debug/'+previous+'.map','debug/'+SKETCH+'.ino.elf','debug/'+SKETCH+'.ino.map'}
 assert {p.relative_to(firmware).as_posix() for p in firmware.rglob('*') if p.is_file()}<=allowed,'Unexpected firmware file; not removing it'
 artifacts={x['suffix']:x for x in report['artifacts']}
 for suffix in ['.bin','.elf','.map']:
  row=artifacts[suffix];p=build/row['file'];assert sha(p)==row['sha256'] and p.stat().st_size==row['bytes']
  q=firmware/f'Tab5OS_v{VERSION}.bin' if suffix=='.bin' else debug/row['file'];shutil.copyfile(p,q);assert sha(q)==row['sha256']
 for p in [firmware/f'Tab5OS_v{BASELINE_VERSION}.bin',debug/(previous+'.elf'),debug/(previous+'.map')]:p.unlink(missing_ok=True)
 binary_hash=artifacts['.bin']['sha256'];(firmware/'SHA256.txt').write_text(binary_hash+f'  Tab5OS_v{VERSION}.bin\n')
 status=f'**Firmware {VERSION} compilé et vérifié.** Tests ciblés sur ordinateur réussis ; aucun essai sur Tab5 physique effectué.'
 for n in ['README.md','LISEZ_MOI.md','COMPILATION.md']:section(ROOT/n,'build-status',status)
 section(ROOT/'LISEZ_MOI.md','firmware-install',f'L’image [firmware/Tab5OS_v{VERSION}.bin](firmware/Tab5OS_v{VERSION}.bin) est le firmware **OTA**. Utiliser la mise à jour de Tab5OS ; ce BIN applicatif ne doit pas être écrit à zéro comme une image fusionnée. SHA-256 : `firmware/SHA256.txt`.')
 rows='\n'.join('| '+x['description']+' | PASS | ['+x['file']+'](tools/verification/'+x['file']+') |' for x in scope['targetedReports'])
 verified=f'''# Vérification {VERSION}

Compilation complète réussie avec Arduino M5Stack3.3.9 / ESP-IDF5.5.4 / GCC RISC-V14.2. Sources et copies Arduino vérifiées ; ELF32 little-endian RISC-V, ABI simple précision, image ESP32-P4, checksum et SHA embarqué validés. **Aucun essai sur Tab5 physique.**

| Élément | Résultat |
|---|---|
| Firmware OTA | [Tab5OS_v{VERSION}.bin](firmware/Tab5OS_v{VERSION}.bin) |
| Taille | {report['firmwareBytes']} octets |
| Partition | {report['otaPartitionBytes']} octets |
| Marge | {report['otaFreeBytes']} octets |
| SHA-256 | `{binary_hash}` |
| Sources figées | {report['sourceFilesChecked']} fichiers contrôlés |
| Objets de la compilation | {report['objectFiles']} |
| Rapport | [firmware-verification-{TAG}.json](tools/verification/firmware-verification-{TAG}.json) |

La table de partitions est identique à la référence {BASELINE_VERSION}. Les 23 archives de dépendances et Arduino CLI sont revérifiés par SHA-256. Le rapport contrôle aussi les backports crypto, l’allocateur Bluetooth, le chemin d’interruption DSI en IRAM, les compteurs internes et les correctifs JPEG/LVGL et le démarrage audio M5Unified réellement liés. Les manifestes et commandes sont joints dans `tools/verification/build-{TAG}`.

## Contrôles et exécutions ciblés actuels

| Périmètre | Résultat | Preuve |
|---|---|---|
{rows}

Les rapports donnent les sources exactes, modèles, fautes injectées et limites. Les anciens rapports restent historiques, conservés byte-identiques à la référence ; aucune nouvelle exécution ne leur est attribuée. Le registre `tools/release_scope.json` exige une preuve actuelle pour chaque source de production modifiée. Les tests hôte ne mesurent ni le temps de démarrage, ni le CPU/énergie, ni les périphériques physiques.

[Diagnostic](AUDIT_{VERSION}.md) · [Ressources](BILAN_RESSOURCES_{VERSION}.md) · [Procédure matériel](TESTS_TAB5_{VERSION}.md).
'''
 for n in ['VERIFICATION.md',f'VERIFICATION_{VERSION}.md']:(ROOT/n).write_text(verified)
 print(json.dumps({'version':VERSION,'prepared':True,'firmwareBytes':report['firmwareBytes'],'otaFreeBytes':report['otaFreeBytes']}))
if __name__=='__main__':main()
