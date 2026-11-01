#!/usr/bin/env python3
"""Register current evidence and explicit production changes; reject stale proofs."""
import hashlib,json,sys
from pathlib import Path
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r/'tools'));from check_preserved_0842 import preserved_path
s=r/'Tab5OS_Arduino_v0.8.42_Ecriture';proofs=r/'tools/verification'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
b=json.loads((proofs/'baseline-0841-source.json').read_text());old={k.replace('Tab5OS_Arduino_v0.8.41_Ecriture.ino',s.name+'.ino'):v for k,v in b['files'].items()};current={p.relative_to(s).as_posix():sha(p) for p in s.rglob('*') if p.is_file() and preserved_path(p.relative_to(s).as_posix())};changed=sorted(k for k in set(old)|set(current) if old.get(k)!=current.get(k))
skip={'languages-0842.json','evolutions-localization-0842.json','resources-0842.json','preserved-components-0842.json','firmware-verification-0842.json','publication-audit-0842.json','publication-scope-0842.json'}
rows=[];coverage={};errors=[]
for p in sorted(proofs.glob('*0842.json')):
 if p.name in skip:continue
 d=json.loads(p.read_text())
 if d.get('version',d.get('release'))!='0.8.42' or d.get('baseline') is True:continue
 if not (d.get('passed') is True or d.get('result')=='PASS') or not d.get('sourceHashes'):continue
 bad=[]
 for group in ['sourceHashes','integrationSourceHashes','testSourceHashes','testHashes','evidenceHashes','screenshots']:
  for name,expected in d.get(group,{}).items():
   path=(r/name).resolve()
   if not path.is_relative_to(r) or not path.is_file() or sha(path)!=expected:bad.append(name)
 if bad:errors.append(p.name+': '+repr(bad));continue
 titles={
 'apps-elf-loader':'Chargement ELF : erreurs, annulation et transfert de propriété',
 'apps-elf-lifecycle':'Cycles de vie des tâches et services ELF',
 'apps-elf-save':'Sauvegarde ELF : intégrité et fautes SD',
 'apps-image-io':'Écriture image et JPEG : fautes SD/flush/close',
 'apps-native-view':'Vue native : contention, entrées et libérations',
 'apps-target':'Compilation cible des applications/exports image',
 'display':'Framebuffer/cache/DSI et reprise',
 'display-latency':'Comparaison latence affichage et survol .41/.42',
 'display-modules-target':'Compilation cible écran/souris',
 'main-target':'Compilation cible du main régénéré',
 'maps-input':'Cartes/souris : navigation, boutons et zoom',
 'media-audio-accessors-after':'Audio : accesseurs et copies de chaînes',
 'media-audit':'Synthèse des contrôles réseau et média',
 'media-catalogue-sort':'Tri du catalogue média',
 'media-compatibility':'Compatibilité C6 sans refus automatique',
 'media-http-after':'HTTP : trames tronquées et fins chunked',
 'media-network-gate':'Synchronisation et annulation réseau',
 'media-recording':'Enregistrement : erreurs et conservation du fichier',
 'media-target':'Compilation cible audio/vidéo/HTTP',
 'media-video-lifecycle':'Vidéo : arrêt, détachement et relance',
 'memory':'Admission mémoire, quotas et diagnostics',
 'memory-target':'Compilation cible mémoire',
 'persistence':'Paramètres et progression : lectures/écritures interrompues',
 'settings-ui':'Réglages et informations système : LVGL, 14 langues',
 'atomic-storage':'Remplacement atomique et lecture JSON',
 'storage-target':'Compilation cible coffre et clés',
 'storage2-target':'Compilation cible persistance et stockage',
 'storage-volume':'Volumes/leases et génération des handles',
 'system-resources':'Réparation : reconstruction, mémoire et fautes SD',
 'system-resources-target':'Compilation cible des ressources compressées',
 'ui-diagnostics':'Diagnostic UI : horloge, bornes et journal RTC',
 'ui-diagnostics-target':'Compilation cible du diagnostic UI',
 'vault':'Coffre : vraie crypto, récupération et fautes',
 'writer-storage':'Documents/workspace : cycles et conservation sur erreur',
 }
 title=titles.get(p.stem.removesuffix('-0842'),p.stem.removesuffix('-0842').replace('-',' '))
 rows.append({'file':p.name,'description':title,'physicalDeviceTested':False})
 for name,digest in {**d.get('sourceHashes',{}),**d.get('integrationSourceHashes',{})}.items():
  if name.startswith(s.name+'/'):coverage.setdefault(name[len(s.name)+1:],[]).append(p.name)
reasons={
'Tab5AtomicFile':'Distingue absence ENOENT et erreur ; conserve précédent/backup ; contrôle flush et close avant promotion.',
'Tab5Audio':'Supprime copies de chaînes des accesseurs scalaires et refuse faux succès après échec d’enregistrement/flush/close.',
'Tab5DisplayPresent':'Présentation DSI non bloquante sans réutilisation prématurée du framebuffer.',
'Tab5DisplayRefresh':'Garde du timer LVGL existant ; différer seulement le rendu pendant la barrière DSI.',
'Tab5Executable':'Chargeur ELF asynchrone, fin/annulation et sauvegarde qui conserve le précédent sur faute.',
'Tab5HiddenFiles':'Préserve configuration précédente lors de lecture JSON incomplète ou erreur SD.',
'Tab5ImageWrite':'Valide absence, flush et close avant promotion des sorties image.',
'Tab5Jpeg':'Valide sondes, flush et close de l’export JPEG ; aucune promotion sur erreur.',
'Tab5KeyStore':'État de déverrouillage consultable sans attendre le verrou des travaux crypto.',
'Tab5Memory':'Échantillonnage limité aux capacités demandées et diagnostics détaillés explicites.',
'Tab5Mouse':'Regroupe le survol en conservant transitions, deltas et points de tracé.',
'Tab5NativeView':'Sépare verrou entrées/trame et évite l’attente UI pendant copie de pixels.',
'Tab5NetworkHttp':'Vérifie la fin de trame HTTP chunked avant publication du transfert.',
'Tab5OS_Arduino':'Intègre services non bloquants, horloge native, diagnostics et persistance sûre sans autosauvegarde imposée.',
'Tab5ResourceManager':'Réduit les parcours de tas lors d’un déficit tout en conservant quotas/marges et refus exacts.',
'Tab5Secrets':'Échec fermé sur sondes SD indéterminées ; préserve banques et clés ; vérifie finalisation.',
'Tab5SystemResources':'Compresse les ressources exactes et répare progressivement en PSRAM bornée ; flush/close/readback contrôlés.',
'Tab5Tools':'Décode JSON complet dans une valeur candidate avant publication ; erreur distincte d’absence.',
'Tab5UiDiagnostics':'Journal de latence borné optionnel et contexte RTC sans données personnelles ni écritures Flash.',
'Tab5Vault':'Contrôle la synchronisation/finalisation du fichier chiffré avant promotion.',
'Tab5Video':'Arrêt/reprise coopératifs sans attente UI, publication et nettoyage après détachement sûr.',
'Tab5Writing':'Évite de remplacer un workspace après une erreur de lecture.',
'WriterStorage':'Préserve document/précédent lors d’erreurs de présence, écriture ou finalisation.',
'WriterWorkspace':'Chargement transactionnel, borné, EOF et erreurs distingués.',
}
changes={}
for path in changed:
 reason=next((v for k,v in reasons.items() if Path(path).name.startswith(k)),None)
 candidates=coverage.get(path,[])
 if not reason or not candidates:errors.append('No rationale/proof: '+path);continue
 # Whole-target compilation backs the main integration; module regressions
 # remain separate required gates, and headers use their current module proof.
 preferred='main-target-0842.json' if path.endswith('.ino') else next((x for x in candidates if 'target' not in x and x not in ['media-audit-0842.json']),candidates[0])
 if preferred not in candidates:preferred=candidates[0]
 changes[path]={'reason':reason,'report':preferred,'additionalReports':[x for x in candidates if x!=preferred]}
if errors:print('\n'.join(errors));raise SystemExit(1)
scope={'version':'0.8.42','baselineVersion':'0.8.41','baselineArchiveSha256':b['sourceArchiveSha256'],'finalized':True,'targetedReports':rows,'sourceChanges':changes}
(r/'tools/release_scope_0842.json').write_text(json.dumps(scope,ensure_ascii=False,indent=2)+'\n');print(json.dumps({'passed':True,'targetedReports':len(rows),'changedSources':len(changes)}))
