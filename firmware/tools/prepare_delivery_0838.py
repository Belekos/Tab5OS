#!/usr/bin/env python3
"""Copy verified 0.8.38 build evidence/binaries and update delivery documents.

Run only after verify_linux.py succeeds and all current regression reports are
complete. This does not compile, alter sketch sources, flash or publish. Run
audit_publication.py and package_release_0838.py after the last document change.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil

VERSION = '0.8.38'
SKETCH = 'Tab5OS_Arduino_v0.8.38_Ecriture'
ROOT = Path(__file__).resolve().parents[1]
PROOFS = ROOT / 'tools/verification'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def safe(root, relative):
    path = (root / relative).resolve()
    assert path.is_relative_to(root.resolve()), relative
    return path


def verified_proof(name):
    data = read(PROOFS / name)
    assert data.get('version', data.get('release')) == VERSION, name
    assert data.get('passed') is True or data.get('result') == 'PASS', name
    for group in ('sourceHashes', 'testSourceHashes', 'integrationSourceHashes', 'evidenceHashes', 'screenshots'):
        for relative, expected in data.get(group, {}).items():
            assert sha(safe(ROOT, relative)) == expected, (name, relative)
    return data


def section(path, name, content):
    text = path.read_text(encoding='utf-8')
    pattern = r'(?s)<!-- ' + re.escape(name) + r' -->.*?<!-- /' + re.escape(name) + r' -->'
    replacement = '<!-- ' + name + ' -->\n' + content + '\n<!-- /' + name + ' -->'
    text, count = re.subn(pattern, lambda match: replacement, text)
    assert count == 1, (path, name, count)
    path.write_text(text, encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', type=Path, required=True)
    args = parser.parse_args()
    environment = args.environment.resolve()
    qa = environment / 'qa'
    build = environment / 'build'
    report_path = qa / 'firmware-verification-0838.json'
    report = read(report_path)
    assert report['version'] == VERSION and report['passed'] is True
    assert report['buildExitCode'] == 0 and report['sourcesUnchanged'] is True
    assert report['physicalDeviceTested'] is False
    assert Path(report['build']['sketch']).resolve() == (ROOT / SKETCH).resolve()
    manifests = [read(qa / ('source-manifest-' + name + '.json'))['files'] for name in ('start', 'end', 'final')]
    assert manifests[0] == manifests[1] == manifests[2], 'Build source manifests differ'
    actual = [{'path': path.relative_to(ROOT / SKETCH).as_posix(), 'bytes': path.stat().st_size, 'sha256': sha(path)}
              for path in sorted((ROOT / SKETCH).rglob('*'))
              if path.is_file() and path.relative_to(ROOT / SKETCH).parts[0] != 'sdcard']
    assert actual == manifests[0], 'Current source differs from verified build'
    for relative, expected in report['evidenceSha256'].items():
        assert sha(safe(qa, relative)) == expected, ('build evidence changed', relative)
    names = ['explorer-0838.json', 'explorer-ui-0838.json', 'explorer-tree-0838.json',
             'drawing-input-0838.json', 'english-defaults-0838.json',
             'preserved-components-0838.json', 'languages-0838.json',
             'drawing-localization-0838.json', 'resources-0838.json']
    tests = {name: verified_proof(name) for name in names}
    for locale, row in tests['languages-0838.json']['packs'].items():
        assert sha(ROOT / SKETCH / 'sdcard/System/languages' / (locale + '.json')) == row['sha256']
    artifacts = {row['suffix']: row for row in report['artifacts']}
    for suffix in ('.bin', '.elf', '.map'):
        row = artifacts[suffix]
        assert row['file'] == SKETCH + '.ino' + suffix
        source = build / row['file']
        assert source.stat().st_size == row['bytes'] and sha(source) == row['sha256']
    destination = PROOFS / 'build-0838'
    destination.mkdir(parents=True, exist_ok=True)
    evidence = set(report['evidenceSha256']) | {'toolchain-versions.json', 'firmware-verification-0838.json', 'lvgl-arabic-patch.json'}
    for relative in sorted(evidence):
        source = safe(qa, relative)
        target = safe(destination, relative)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    shutil.copyfile(report_path, PROOFS / report_path.name)
    firmware = ROOT / 'firmware'
    debug = firmware / 'debug'
    debug.mkdir(parents=True, exist_ok=True)
    previous = 'Tab5OS_Arduino_v0.8.37_Ecriture.ino'
    allowed = {'Tab5OS_v0.8.37.bin', 'Tab5OS_v0.8.38.bin', 'SHA256.txt',
               'debug/' + previous + '.elf', 'debug/' + previous + '.map',
               'debug/' + SKETCH + '.ino.elf', 'debug/' + SKETCH + '.ino.map'}
    assert {path.relative_to(firmware).as_posix() for path in firmware.rglob('*') if path.is_file()} <= allowed, 'Unexpected firmware file; not removing it'
    for suffix in ('.bin', '.elf', '.map'):
        target = firmware / 'Tab5OS_v0.8.38.bin' if suffix == '.bin' else debug / artifacts[suffix]['file']
        shutil.copyfile(build / artifacts[suffix]['file'], target)
        assert sha(target) == artifacts[suffix]['sha256']
    for path in [firmware / 'Tab5OS_v0.8.37.bin', debug / (previous + '.elf'), debug / (previous + '.map')]:
        path.unlink(missing_ok=True)
    binary_hash = artifacts['.bin']['sha256']
    (firmware / 'SHA256.txt').write_text(binary_hash + '  Tab5OS_v0.8.38.bin\n', encoding='utf-8')
    status = '**Firmware 0.8.38 compilé et vérifié.** Les tests ciblés sur ordinateur passent ; aucun essai sur le Tab5 physique n’a été effectué.'
    for name in ('README.md', 'LISEZ_MOI.md', 'COMPILATION.md'):
        section(ROOT / name, 'build-status', status)
    section(ROOT / 'LISEZ_MOI.md', 'firmware-install',
            'L’image [firmware/Tab5OS_v0.8.38.bin](firmware/Tab5OS_v0.8.38.bin) est le firmware **OTA**. '
            'Depuis Tab5OS, utiliser la mise à jour du firmware avec ce fichier. Ce BIN applicatif ne doit pas être écrit à l’adresse zéro comme une image fusionnée. '
            'Son SHA-256 figure dans `firmware/SHA256.txt`.')
    english = ROOT / 'READ_FIRST_EN.txt'
    text = english.read_text(encoding='utf-8').replace(
        'Firmware 0.8.38 is being prepared and verified. No validated 0.8.38 image is available at this stage. VERIFICATION.md will record current execution evidence separately from historical results.',
        'Firmware 0.8.38 has been compiled and verified; its current host tests passed. VERIFICATION.md records execution evidence separately from historical results.')
    english.write_text(text, encoding='utf-8')
    explorer = tests['explorer-0838.json']['checks']
    ui = tests['explorer-ui-0838.json']['checks']
    mouse = tests['drawing-input-0838.json']['result']['checks']
    preserved = tests['preserved-components-0838.json']['filesChecked']
    size = report['firmwareBytes']
    margin = report['otaFreeBytes']
    content = f"""# Vérification 0.8.38

Version préparée depuis le ZIP 0.8.37 livré, SHA-256 `f8ec50004a841bc6d7f3502de240dd4eece58262f5360e00447e3ab47cb27a24`. Les essais 0.8.38 ci-dessous portent sur les sources finales. Les rapports antérieurs conservent leur version d’origine. **Aucun essai sur le Tab5 physique n’a été effectué.**

## Firmware

Compilation complète Linux/GCC réussie avec Arduino M5Stack 3.3.9 et GCC RISC-V 14.2.0, sources inchangées pendant la compilation. La vérification contrôle l’ELF RISC-V 32 bits à ABI simple précision, l’image ESP32-P4, sa somme de contrôle, son empreinte de validation et l’empreinte ELF embarquée.

| Élément | Résultat |
|---|---|
| Image OTA | [Tab5OS_v0.8.38.bin](firmware/Tab5OS_v0.8.38.bin) |
| Taille | {size:,} octets |
| Partition OTA | {report['otaPartitionBytes']:,} octets |
| Marge disponible | {margin:,} octets, soit {margin / 1024:.1f} Kio |
| SHA-256 | `{binary_hash}` |
| Rapport | [firmware-verification-0838.json](tools/verification/firmware-verification-0838.json) |

Les preuves de compilation, empreintes de sources et dépendances figurent dans `tools/verification/build-0838`. Les 23 archives de dépendances et Arduino CLI ont été comparés au verrou SHA-256 fourni. Le BIN est une image applicative OTA, pas une image fusionnée à écrire à l’adresse zéro.

## Contrôles exécutés sur la 0.8.38

| Périmètre | Résultat | Rapport |
|---|---|---|
| Gestes, sélection et commandes de l’explorateur | PASS — {explorer:,} contrôles ; chemins de production extraits et LVGL réel, stockage et frontières applicatives simulés | [explorer-0838.json](tools/verification/explorer-0838.json) |
| Présentation et focus | PASS — {ui:,} contrôles avec composants de production et police livrée | [explorer-ui-0838.json](tools/verification/explorer-ui-0838.json) |
| Arbre de dossiers et supports | PASS — modèle de production, simulations du stockage et des limites de mémoire ; portée détaillée dans le rapport | [explorer-tree-0838.json](tools/verification/explorer-tree-0838.json) |
| Noms de fichiers et dossiers par défaut | PASS — audit et essais des nouvelles valeurs anglaises, compatibilité des valeurs personnelles documentée | [english-defaults-0838.json](tools/verification/english-defaults-0838.json) |
| Routage de la souris Dessin | PASS — {mouse:,} contrôles ; route principale extraite et LVGL, frontières USB/éditeur simulées | [drawing-input-0838.json](tools/verification/drawing-input-0838.json) |
| Composants conservés | PASS — {preserved:,} fichiers de production identiques à la livraison 0.8.37 | [preserved-components-0838.json](tools/verification/preserved-components-0838.json) |
| Langues | PASS — 14 packs, couverture, paramètres de formatage, glyphes et limites de taille | [languages-0838.json](tools/verification/languages-0838.json) |
| Libellés modifiés | PASS — comparaison réelle à la livraison 0.8.37 et couverture dans les 14 packs | [drawing-localization-0838.json](tools/verification/drawing-localization-0838.json) |
| Ressources SD et secours embarqué | PASS — concordance de Factory, ressources SD et ressources intégrées | [resources-0838.json](tools/verification/resources-0838.json) |

Les tests C/C++ ciblés emploient AddressSanitizer et UndefinedBehaviorSanitizer lorsque précisé dans leur rapport. Les tests exécutés sur ordinateur ne mesurent pas les délais SD, la cadence d’affichage ou les entrées physiques du P4. Les images de test utilisent le rendu LVGL ; les données et certaines frontières applicatives y sont injectées.

## Preuves historiques conservées

Le chargeur et le parseur des langues ne sont pas réécrits dans cette version. Leurs sources sont contrôlées identiques à la 0.8.37 ; les rapports [locale-0837.json](tools/verification/locale-0837.json) et [locale-parser-0837.json](tools/verification/locale-parser-0837.json) restent des exécutions **0.8.37**, sans être présentés comme de nouveaux essais. Les mesures de vitesse de cette version restent historiques et ne prédisent pas le temps de démarrage réel du Tab5.

Le contrôle de préservation conserve aussi les résultats historiques des moteurs non modifiés. Il ne rejoue pas les essais de Git, de Dessin, de mémoire, de Cartes ou des modificateurs USB et ne prouve pas à lui seul l’absence d’interaction sur tablette. Les sources modifiées de l’explorateur, de l’intégration et des valeurs par défaut sont exclues explicitement de ce contrôle et possèdent leurs vérifications 0.8.38.

## Parcours et validation sur tablette

Les règles figurent dans [EXPLORATEUR.md](EXPLORATEUR.md) et [ERGONOMIE_0.8.38.md](ERGONOMIE_0.8.38.md). Vérifier la racine ouverte au premier accès, la conservation du dossier après retour à l’explorateur, l’absence de fichiers dans l’arbre gauche et l’apparition/retrait de la racine USB. À droite, un clic doit seulement sélectionner ; un double-clic ouvre. Contrôler les chevrons, les raccourcis de recherche/tri, les commandes d’Édition et les contextes Git. Les nouveaux noms de fichiers/dossiers doivent rester anglais quelle que soit la langue de l’interface, sans renommer les données personnelles existantes.
"""
    (ROOT / 'VERIFICATION.md').write_text(content, encoding='utf-8')
    (ROOT / 'VERIFICATION_0.8.38.md').write_text(content, encoding='utf-8')
    print(json.dumps({'version': VERSION, 'prepared': True, 'firmwareBytes': size,
                      'otaFreeBytes': margin, 'evidenceFiles': len(evidence),
                      'next': 'audit_publication.py, then package_release_0838.py'}))


if __name__ == '__main__':
    main()
