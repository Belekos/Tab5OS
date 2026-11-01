#!/usr/bin/env python3
"""Copy verified 0.8.36 build evidence/binaries and update delivery documents.

Run only after verify_linux.py succeeds and all current regression reports are
complete. This does not compile, alter sketch sources, flash or publish. Run
audit_publication.py and package_release_0836.py after the last document change.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil

VERSION = '0.8.36'
SKETCH = 'Tab5OS_Arduino_v0.8.36_Ecriture'
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
    for group in ('sourceHashes', 'integrationSourceHashes', 'evidenceHashes', 'screenshots'):
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
    report_path = qa / 'firmware-verification-0836.json'
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
    names = ['explorer-0836.json', 'explorer-ui-0836.json', 'explorer-tree-0836.json',
             'drawing-input-0836.json', 'usb-modifiers-0836.json',
             'preserved-components-0836.json', 'languages-0836.json',
             'drawing-localization-0836.json', 'resources-0836.json']
    tests = {name: verified_proof(name) for name in names}
    for locale, row in tests['languages-0836.json']['packs'].items():
        assert sha(ROOT / SKETCH / 'sdcard/System/languages' / (locale + '.json')) == row['sha256']
    artifacts = {row['suffix']: row for row in report['artifacts']}
    for suffix in ('.bin', '.elf', '.map'):
        row = artifacts[suffix]
        assert row['file'] == SKETCH + '.ino' + suffix
        source = build / row['file']
        assert source.stat().st_size == row['bytes'] and sha(source) == row['sha256']
    destination = PROOFS / 'build-0836'
    destination.mkdir(parents=True, exist_ok=True)
    evidence = set(report['evidenceSha256']) | {'toolchain-versions.json', 'firmware-verification-0836.json', 'lvgl-arabic-patch.json'}
    for relative in sorted(evidence):
        source = safe(qa, relative)
        target = safe(destination, relative)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    shutil.copyfile(report_path, PROOFS / report_path.name)
    firmware = ROOT / 'firmware'
    debug = firmware / 'debug'
    debug.mkdir(parents=True, exist_ok=True)
    previous = 'Tab5OS_Arduino_v0.8.35_Ecriture.ino'
    allowed = {'Tab5OS_v0.8.35.bin', 'Tab5OS_v0.8.36.bin', 'SHA256.txt',
               'debug/' + previous + '.elf', 'debug/' + previous + '.map',
               'debug/' + SKETCH + '.ino.elf', 'debug/' + SKETCH + '.ino.map'}
    assert {path.relative_to(firmware).as_posix() for path in firmware.rglob('*') if path.is_file()} <= allowed, 'Unexpected firmware file; not removing it'
    for suffix in ('.bin', '.elf', '.map'):
        target = firmware / 'Tab5OS_v0.8.36.bin' if suffix == '.bin' else debug / artifacts[suffix]['file']
        shutil.copyfile(build / artifacts[suffix]['file'], target)
        assert sha(target) == artifacts[suffix]['sha256']
    for path in [firmware / 'Tab5OS_v0.8.35.bin', debug / (previous + '.elf'), debug / (previous + '.map')]:
        path.unlink(missing_ok=True)
    binary_hash = artifacts['.bin']['sha256']
    (firmware / 'SHA256.txt').write_text(binary_hash + '  Tab5OS_v0.8.36.bin\n', encoding='utf-8')
    status = '**Firmware 0.8.36 compilé et vérifié.** Les tests ciblés sur ordinateur passent ; aucun essai sur le Tab5 physique n’a été effectué.'
    for name in ('README.md', 'LISEZ_MOI.md', 'COMPILATION.md'):
        section(ROOT / name, 'build-status', status)
    section(ROOT / 'LISEZ_MOI.md', 'firmware-install',
            'L’image [firmware/Tab5OS_v0.8.36.bin](firmware/Tab5OS_v0.8.36.bin) est le firmware **OTA**. '
            'Depuis Tab5OS, utiliser la mise à jour du firmware avec ce fichier. Ce BIN applicatif ne doit pas être écrit à l’adresse zéro comme une image fusionnée. '
            'Son SHA-256 figure dans `firmware/SHA256.txt`.')
    english = ROOT / 'READ_FIRST_EN.txt'
    text = english.read_text(encoding='utf-8').replace('Firmware 0.8.36 is being built and verified.', 'Firmware 0.8.36 has been compiled and verified; its targeted host tests passed.')
    english.write_text(text, encoding='utf-8')
    explorer = tests['explorer-0836.json']['checks']
    ui = tests['explorer-ui-0836.json']['checks']
    usb = tests['usb-modifiers-0836.json']['checks']
    mouse = tests['drawing-input-0836.json']['result']['checks']
    preserved = tests['preserved-components-0836.json']['filesChecked']
    size = report['firmwareBytes']
    margin = report['otaFreeBytes']
    content = f'''# Vérification 0.8.36

Version préparée depuis le ZIP 0.8.35 livré, SHA-256 `fc3c89c9e84acc6e50d29025081a1cfdb1a57437518c15ee36e77a7b6b160d47`. Les résultats ci-dessous portent sur les sources finales 0.8.36. Les anciens rapports conservent leur version d’origine. **Aucun essai sur le Tab5 physique n’a été effectué.**

## Firmware

Compilation complète Linux/GCC réussie avec Arduino M5Stack 3.3.9 et GCC RISC-V 14.2.0, sources inchangées pendant la compilation. La vérification contrôle l’ELF RISC-V 32 bits à ABI simple précision, l’image ESP32-P4, sa somme de contrôle, son empreinte de validation et l’empreinte ELF embarquée.

| Élément | Résultat |
|---|---|
| Image OTA | [Tab5OS_v0.8.36.bin](firmware/Tab5OS_v0.8.36.bin) |
| Taille | {size:,} octets |
| Partition OTA | {report['otaPartitionBytes']:,} octets |
| Marge disponible | {margin:,} octets, soit {margin / 1024:.1f} Kio |
| SHA-256 | `{binary_hash}` |
| Rapport | [firmware-verification-0836.json](tools/verification/firmware-verification-0836.json) |

Les preuves de compilation, empreintes de sources et dépendances figurent dans `tools/verification/build-0836`. Les 23 archives de dépendances et Arduino CLI ont été comparés au verrou SHA-256 fourni. Le BIN est une image applicative OTA, pas une image fusionnée à écrire à l’adresse zéro.

## Contrôles exécutés

| Périmètre | Résultat | Rapport |
|---|---|---|
| Sélection, ouverture et commandes de l’explorateur | PASS — {explorer:,} contrôles ; chemins de production extraits, widgets et focus LVGL réels, stockage et disponibilité de l’arbre simulés | [explorer-0836.json](tools/verification/explorer-0836.json) |
| Présentation et focus | PASS — {ui:,} contrôles ; composants de l’explorateur et police livrée avec LVGL | [explorer-ui-0836.json](tools/verification/explorer-ui-0836.json) |
| Modèle d’arborescence | PASS — dépliage/repli, profondeur 129, 8 193 entrées, tri, annulation, suppression USB et refus d’allocation simulés | [explorer-tree-0836.json](tools/verification/explorer-tree-0836.json) |
| Modificateurs du clavier USB | PASS — {usb:,} contrôles ; huit interfaces, pressions/relâchements, débranchement et débordement de file | [usb-modifiers-0836.json](tools/verification/usb-modifiers-0836.json) |
| Routage de la souris Dessin | PASS — {mouse:,} contrôles ; route principale extraite et LVGL, frontières USB/éditeur simulées | [drawing-input-0836.json](tools/verification/drawing-input-0836.json) |
| Composants conservés | PASS — {preserved:,} fichiers de production identiques à la livraison 0.8.35 | [preserved-components-0836.json](tools/verification/preserved-components-0836.json) |
| Langues | PASS — 14 packs, couverture, paramètres de formatage, glyphes et limites de taille | [languages-0836.json](tools/verification/languages-0836.json) |
| Libellés nouveaux de l’explorateur | PASS — comparaison réelle des sources à la livraison 0.8.35, clés couvertes dans les 14 packs | [drawing-localization-0836.json](tools/verification/drawing-localization-0836.json) |
| Ressources SD et secours embarqué | PASS — 175 ressources Factory et 40 ressources embarquées, concordance des données et empreintes | [resources-0836.json](tools/verification/resources-0836.json) |

Les tests C/C++ ciblés emploient AddressSanitizer et UndefinedBehaviorSanitizer. LeakSanitizer ne peut pas inspecter tous les processus dans cet environnement ; les bancs qui suivent les allocations vérifient leur libération explicitement. Ces contrôles ne mesurent pas la cadence d’affichage du P4 ni le comportement de la microSD, du tactile ou de l’USB réels.

Le contrôle de préservation ne rejoue pas les essais historiques de Dessin, de Git, de mémoire ou de Cartes. Il confirme l’identité des fichiers du périmètre défini ; il ne prouve pas à lui seul l’absence d’effet d’interaction sur tablette. L’intégration de l’explorateur dans le sketch principal est modifiée, notamment pour rendre les métadonnées de l’arbre lorsque la récupération explicite du cache Fichiers concerne l’application inactive ; aucun nettoyage périodique n’est ajouté.

## Validation sur tablette

Vérifier un clic simple sur un dossier à droite, puis un double-clic : le premier ouvre uniquement à droite ; le second révèle aussi le dossier à gauche. Essayer des dossiers profonds et paginés, les chevrons, Ctrl/Maj, la fermeture des menus, puis les actions copier/coller/Git depuis chaque panneau. La cible doit toujours correspondre à la dernière sélection. Confirmer également la fluidité et l’absence de flash en usage réel.
'''
    (ROOT / 'VERIFICATION.md').write_text(content, encoding='utf-8')
    (ROOT / 'VERIFICATION_0.8.36.md').write_text(content, encoding='utf-8')
    print(json.dumps({'version': VERSION, 'prepared': True, 'firmwareBytes': size,
                      'otaFreeBytes': margin, 'evidenceFiles': len(evidence),
                      'next': 'audit_publication.py, then package_release_0836.py'}))


if __name__ == '__main__':
    main()
