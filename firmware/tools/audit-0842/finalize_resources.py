#!/usr/bin/env python3
"""Derive resource tables from the verified final ELF and retained .41 baseline."""
from pathlib import Path
import argparse,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--verification',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[2]
b=json.loads((r/'tools/audit-0842/build-baseline.json').read_text());old=json.loads((r/'tools/audit-0842/linked-baseline-sections.json').read_text());v=json.loads(a.verification.read_text());assert v['passed'] and v['sourcesUnchanged'] and v['version']=='0.8.42';assert v['partitions']==b['partitions'];sec=v['memoryLinkChecks']['allSections'];resources=json.loads((r/'tools/verification/resources-0842.json').read_text())
def n(x):return f'{x:,}'.replace(',',' ')
rows=[]
for key in ['.dram0.data','.dram1.data','.dram0.bss','.dram1.bss','.iram0.text','.tcm.text','.tcm.data','.rtc_noinit']:
 before=old['sections'].get(key,0);after=sec.get(key,0);rows.append(f'| `{key}` | {n(before)} | {n(after)} | {after-before:+,} |')
oldbss=sum(old['sections'].get(k,0) for k in ['.dram0.data','.dram1.data','.dram0.bss','.dram1.bss']);newbss=sum(sec.get(k,0) for k in ['.dram0.data','.dram1.data','.dram0.bss','.dram1.bss'])
text=f'''# Bilan des ressources — Tab5OS 0.8.42

Les valeurs Flash et sections proviennent du BIN/ELF final réellement compilé, comparé à l’archive livrée 0.8.41. Les mesures hôte ci-dessous décrivent leurs scénarios, pas la tablette. Aucun total de tas libre après démarrage, temps de boot, fragmentation réelle, CPU ou consommation électrique n’est déduit des sections de lien.

## Flash et OTA

| Mesure | Référence 0.8.41 | 0.8.42 vérifiée |
|---|---:|---:|
| Firmware applicatif | {n(b['firmwareBytes'])} octets | {n(v['firmwareBytes'])} octets |
| Capacité de chaque partition OTA | {n(b['otaPartitionBytes'])} octets | {n(v['otaPartitionBytes'])} octets |
| Marge disponible | {n(b['otaFreeBytes'])} octets | {n(v['otaFreeBytes'])} octets |
| Occupation du slot | {100*b['firmwareBytes']/b['otaPartitionBytes']:.2f} % | {100*v['firmwareBytes']/v['otaPartitionBytes']:.2f} % |

Gain net du BIN : **{n(b['firmwareBytes']-v['firmwareBytes'])} octets**. Table des partitions strictement identique : NVS, otadata, deux slots OTA de 6,25 Mio, SPIFFS et coredump. Pas de migration, chiffrement ou formatage entrepris. L’indicateur Application/OTA compare de la Flash : la nouvelle image occupe {v['firmwareBytes']/1048576:.3f} Mio sur 6,25 Mio, sans rapport avec le tas SRAM.

Les 40 ressources finales de réparation représentent {n(resources['rawBytes'])} octets reconstruits exactement et {n(resources['compressedBytes'])} octets compressés. Leur gain théorique de données est {n(resources['rawBytes']-resources['compressedBytes'])} octets. Le net du BIN diffère : nouveau manifeste de version, décompression, diagnostics et autres corrections ajoutent du code/données. Les fichiers complets restent distribués sur SD ; aucune application ni police n’a été supprimée.

## Sections mémoire liées

| Section | 0.8.41, octets | 0.8.42, octets | Variation nouveau − ancien |
|---|---:|---:|---:|
'''+'\n'.join(rows)+f'''
| Somme data+BSS internes ci-dessus | {n(oldbss)} | {n(newbss)} | {newbss-oldbss:+,} |

La somme data+BSS augmente de {n(newbss-oldbss)} octets si le nombre est positif ; cette variation comprend les nouveaux états de synchronisation/diagnostic. La région RTC est séparée. Les 14 424 octets des tables SHA1/XZ restent en Flash comme en 0.8.41 : aucun nouveau gain RAM ne leur est attribué. IRAM est du code interne ; SPM/TCM/LP ne sont pas de la PSRAM interchangeable. Le placement et l’agrégat INTERNAL sont décrits dans l’audit mémoire.

## Comparaisons effectivement exécutées sur hôte

| Scénario contrôlé | Avant | Après | Limite |
|---|---:|---:|---|
| Affichage : attente maximale handler, DMA à 16 ms | 24 ms | 0 ms | Modèle déterministe LVGL réel, pas latence P4 |
| Même modèle, intervalle maximal du service d’entrée | 32 ms | 8 ms | Boucle modèle à 8 ms |
| DMA suspendu, attente maximale handler | 100 ms | 0 ms | La publication reste différée tant que le buffer est occupé |
| Rapports de survol souris distribués pour un lot de 32 | 32 | 1 | Les 20 points d’un tracé avec bouton restent 20 |
| Parcours de tas sur pénurie interne stable de 20 ms | 66 | 4 | Comptage hôte, essai dépend de l’ordonnanceur hôte |
| Copies de chaînes pour les 16 accesseurs audio scalaires, 10 000 itérations | 960 000 | 0 | Compteur de copies du type String hôte, pas allocation physique |
| Vue native, attente UI sous contention artificielle de 80 ms | ~80,7 ms | ~0,002 ms | Test contention hôte, pas benchmark ESP32 |
| Réparation, pic PSRAM suivi par adaptateur hôte | Sans décompression | 95 627 octets | Seulement pendant la réparation explicite ; zéro allocation au repos |

Le buffer de réparation et le workspace zlib sont libérés à chaque fin/échec/annulation. La sortie est bornée à 128 Kio +1 et le workspace à 48 Kio, sans repli SRAM. Les tests couvrent les échecs d’allocation, flux invalides et fautes d’E/S. Aucun nouveau buffer d’image plein écran n’est ajouté au pipeline DSI.

## Mesures à faire sur appareil

Libre/minimum/plus grand bloc par HP, SPM/TCM, LP et PSRAM ; marges de piles et tâches actives ; démarrage froid avec même SD/options ; vingt cycles d’ouverture/fermeture ; radio/vidéo/cartes simultanées ; latences UI et compteurs DSI pendant les flashs. Le minimum depuis boot peut rester bas après une libération normale. Une grande PSRAM libre ne suffit pas à une allocation interne ou DMA contiguë.

Voir [TESTS_TAB5_0.8.42.md](TESTS_TAB5_0.8.42.md). Les diagnostics détaillés sont à la demande ; ils peuvent eux-mêmes retarder brièvement la tâche et ne doivent pas tourner en continu pendant une mesure de fluidité.
'''
(r/'BILAN_RESSOURCES_0.8.42.md').write_text(text)
report={'version':'0.8.42','passed':True,'physicalDeviceTested':False,'firmwareBytesBefore':b['firmwareBytes'],'firmwareBytesAfter':v['firmwareBytes'],'firmwareBytesSaved':b['firmwareBytes']-v['firmwareBytes'],'otaFreeBytesBefore':b['otaFreeBytes'],'otaFreeBytesAfter':v['otaFreeBytes'],'dataBssBytesBefore':oldbss,'dataBssBytesAfter':newbss,'rtcNoInitBytesBefore':old['sections'].get('.rtc_noinit',0),'rtcNoInitBytesAfter':sec.get('.rtc_noinit',0),'partitionsUnchanged':True,'rawResourceBytes':resources['rawBytes'],'compressedResourceBytes':resources['compressedBytes'],'dynamicDeviceMeasurements':None}
(r/'tools/audit-0842/resource-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
