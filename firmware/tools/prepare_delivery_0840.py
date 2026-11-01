#!/usr/bin/env python3
"""Copy the verified .40 firmware/evidence and write an honest delivery status.

Run after verify_linux.py and every current gate. Does not compile or publish.
"""
from pathlib import Path
import argparse,hashlib,json,re,shutil
VERSION='0.8.40';SKETCH='Tab5OS_Arduino_v0.8.40_Ecriture'
ROOT=Path(__file__).resolve().parents[1];PROOFS=ROOT/'tools/verification'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def safe(root,relative):
 p=(root/relative).resolve();assert p.is_relative_to(root.resolve()),relative;return p
def verified_proof(name):
 d=read(PROOFS/name);assert d.get('version',d.get('release'))==VERSION,name
 assert d.get('passed') is True or d.get('result')=='PASS',name
 for group in ('sourceHashes','testSourceHashes','integrationSourceHashes','evidenceHashes','screenshots'):
  for relative,expected in d.get(group,{}).items():assert sha(safe(ROOT,relative))==expected,(name,relative)
 return d
def section(path,name,content):
 text=path.read_text(encoding='utf-8');pattern=r'(?s)<!-- '+re.escape(name)+r' -->.*?<!-- /'+re.escape(name)+r' -->'
 text,count=re.subn(pattern,lambda m:'<!-- '+name+' -->\n'+content+'\n<!-- /'+name+' -->',text)
 assert count==1,(path,name,count);path.write_text(text,encoding='utf-8')
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--environment',type=Path,required=True);args=ap.parse_args()
 env=args.environment.resolve();qa=env/'qa';build=env/'build'
 report_path=qa/'firmware-verification-0840.json';report=read(report_path)
 assert report['version']==VERSION and report['passed'] is True and report['buildExitCode']==0 and report['sourcesUnchanged'] is True
 assert report['physicalDeviceTested'] is False
 assert Path(report['build']['sketch']).resolve()==(ROOT/SKETCH).resolve()
 manifests=[read(qa/('source-manifest-'+n+'.json'))['files'] for n in ('start','end','final')]
 assert manifests[0]==manifests[1]==manifests[2],'Build source manifests differ'
 actual=[{'path':p.relative_to(ROOT/SKETCH).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted((ROOT/SKETCH).rglob('*')) if p.is_file() and p.relative_to(ROOT/SKETCH).parts[0]!='sdcard' and not p.relative_to(ROOT/SKETCH).as_posix().startswith('tools/native-apps/image-editor/test-output/')]
 assert actual==manifests[0],'Current sources differ from verified build'
 for relative,expected in report['evidenceSha256'].items():assert sha(safe(qa,relative))==expected,relative
 scope=read(ROOT/'tools/release_scope_0840.json');assert scope['version']==VERSION and scope.get('finalized') is True
 names=[r['file'] for r in scope['targetedReports']]+['preserved-components-0840.json','languages-0840.json','evolutions-localization-0840.json','resources-0840.json']
 tests={n:verified_proof(n) for n in names}
 for locale,row in tests['languages-0840.json']['packs'].items():assert sha(ROOT/SKETCH/'sdcard/System/languages'/(locale+'.json'))==row['sha256']
 artifacts={r['suffix']:r for r in report['artifacts']}
 for suffix in ('.bin','.elf','.map'):
  row=artifacts[suffix];assert row['file']==SKETCH+'.ino'+suffix
  p=build/row['file'];assert p.stat().st_size==row['bytes'] and sha(p)==row['sha256']
 destination=PROOFS/'build-0840';destination.mkdir(parents=True,exist_ok=True)
 evidence=set(report['evidenceSha256'])|{'toolchain-versions.json','firmware-verification-0840.json','lvgl-arabic-patch.json'}
 for relative in sorted(evidence):
  p=safe(qa,relative);q=safe(destination,relative);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
 shutil.copyfile(report_path,PROOFS/report_path.name)
 firmware=ROOT/'firmware';debug=firmware/'debug';debug.mkdir(parents=True,exist_ok=True)
 previous='Tab5OS_Arduino_v0.8.39_Ecriture.ino'
 allowed={'Tab5OS_v0.8.39.bin','Tab5OS_v0.8.40.bin','SHA256.txt','debug/'+previous+'.elf','debug/'+previous+'.map','debug/'+SKETCH+'.ino.elf','debug/'+SKETCH+'.ino.map'}
 assert {p.relative_to(firmware).as_posix() for p in firmware.rglob('*') if p.is_file()}<=allowed,'Unexpected firmware file; not removing it'
 for suffix in ('.bin','.elf','.map'):
  q=firmware/'Tab5OS_v0.8.40.bin' if suffix=='.bin' else debug/artifacts[suffix]['file'];shutil.copyfile(build/artifacts[suffix]['file'],q);assert sha(q)==artifacts[suffix]['sha256']
 for p in [firmware/'Tab5OS_v0.8.39.bin',debug/(previous+'.elf'),debug/(previous+'.map')]:p.unlink(missing_ok=True)
 binary_hash=artifacts['.bin']['sha256'];(firmware/'SHA256.txt').write_text(binary_hash+'  Tab5OS_v0.8.40.bin\n')
 status='**Firmware 0.8.40 compilé et vérifié.** Les tests ciblés sur ordinateur passent ; aucun essai sur le Tab5 physique n’a été effectué.'
 for n in ('README.md','LISEZ_MOI.md','COMPILATION.md'):section(ROOT/n,'build-status',status)
 for n in ('README.md','LISEZ_MOI.md'):
  p=ROOT/n;text=p.read_text(encoding='utf-8').replace('La 0.8.40 prépare les évolutions','La 0.8.40 apporte les évolutions').replace('Cette version prépare les évolutions','Cette version apporte les évolutions');p.write_text(text,encoding='utf-8')
 section(ROOT/'LISEZ_MOI.md','firmware-install','L’image [firmware/Tab5OS_v0.8.40.bin](firmware/Tab5OS_v0.8.40.bin) est le firmware **OTA**. Depuis Tab5OS, utiliser la mise à jour du firmware avec ce fichier. Ce BIN applicatif ne doit pas être écrit à l’adresse zéro comme une image fusionnée. Son SHA-256 figure dans `firmware/SHA256.txt`.')
 english=ROOT/'READ_FIRST_EN.txt';text=english.read_text(encoding='utf-8').replace('Firmware 0.8.40 is being prepared; no validated current image is claimed.','Firmware 0.8.40 has been compiled and verified; the current host tests passed.');english.write_text(text,encoding='utf-8')
 rows=[]
 for row in scope['targetedReports']:
  d=tests[row['file']];count=d.get('checks',d.get('result',{}).get('checks') if isinstance(d.get('result'),dict) else None)
  detail=(' — '+str(count)+' contrôles') if count is not None else ''
  rows.append('| '+row['description']+' | PASS'+detail+' | ['+row['file']+'](tools/verification/'+row['file']+') |')
 preserved=tests['preserved-components-0840.json']['filesChecked'];size=report['firmwareBytes'];margin=report['otaFreeBytes']
 content=f'''# Vérification 0.8.40

Version préparée depuis le ZIP 0.8.39 livré, SHA-256 `a4274b8fb44642306dc9c78e212d58d9064147d9bbdb0e624b4afb612af7d5ff`. Les essais actuels portent sur les empreintes finales des sources. Les rapports antérieurs conservent leur version d’origine. **Aucun essai sur le Tab5 physique n’a été effectué.**

## Firmware

Compilation complète Linux/GCC réussie avec Arduino M5Stack 3.3.9 et GCC RISC-V 14.2.0, sources inchangées pendant la compilation. Le contrôle vérifie l’ELF RISC-V 32 bits à ABI simple précision, l’image ESP32-P4, ses empreintes et sa place dans la partition OTA.

| Élément | Résultat |
|---|---|
| Image OTA | [Tab5OS_v0.8.40.bin](firmware/Tab5OS_v0.8.40.bin) |
| Taille | {size:,} octets |
| Partition OTA | {report['otaPartitionBytes']:,} octets |
| Marge | {margin:,} octets |
| SHA-256 | `{binary_hash}` |
| Rapport | [firmware-verification-0840.json](tools/verification/firmware-verification-0840.json) |

Les preuves de compilation figurent dans `tools/verification/build-0840`. Les 23 archives de dépendances et Arduino CLI sont contrôlés avec le verrou SHA-256 fourni. Le BIN est applicatif OTA, pas une image fusionnée à écrire à l’adresse zéro.

## Contrôles actuels

| Périmètre | Résultat | Rapport |
|---|---|---|
'''+ '\n'.join(rows)+f'''
| Composants hors périmètre modifié | PASS — {preserved:,} fichiers identiques à la livraison 0.8.39 ; chaque source modifiée est déclarée avec son rapport actuel | [preserved-components-0840.json](tools/verification/preserved-components-0840.json) |
| Langues | PASS — 14 packs, paramètres de formatage, glyphes, couverture et limites de taille | [languages-0840.json](tools/verification/languages-0840.json) |
| Nouveaux libellés | PASS — comparaison au ZIP 0.8.39, traductions existantes conservées | [evolutions-localization-0840.json](tools/verification/evolutions-localization-0840.json) |
| Ressources SD et secours intégré | PASS — concordance des fichiers, de Factory et des ressources embarquées | [resources-0840.json](tools/verification/resources-0840.json) |

Les rapports détaillent les adaptateurs et simulations utilisés, les limites mémoire testées et les éventuels sanitizers. Un test hôte n’est pas un essai d’impression, de formatage ou de connexion radio sur un appareil réel. Le registre `tools/release_scope_0840.json` fixe les rapports attendus et les changements de code explicitement couverts ; l’archive est réextraite et revalidée avant livraison.

## Historique et limites

Les preuves 0.8.39 et antérieures restent historiques. La conservation d’une source ne prouve pas à elle seule l’absence d’interaction avec une nouvelle fonction. Aucun temps de démarrage, débit USB, portée radio ni disparition du scintillement sur tablette n’est déduit de la compilation.

La portée livrée et les développements qui nécessitent encore un pilote, une adaptation d’ABI ou un matériel externe sont décrits dans [NOUVEAUTES_v0.8.40.md](NOUVEAUTES_v0.8.40.md) et [EVOLUTIONS_0.8.39.md](EVOLUTIONS_0.8.39.md). Les priorités 1, 2 et 3 représentent la difficulté de réalisation, sans jugement sur la valeur des fonctions.
'''
 (ROOT/'VERIFICATION.md').write_text(content,encoding='utf-8');(ROOT/'VERIFICATION_0.8.40.md').write_text(content,encoding='utf-8')
 print(json.dumps({'version':VERSION,'prepared':True,'firmwareBytes':size,'otaFreeBytes':margin,'evidenceFiles':len(evidence),'next':'audit_publication.py, then package_release_0840.py'}))
if __name__=='__main__':main()
