# Tab5OS 0.8.53 — sources

[Présentation en français](../README.fr.md) · [English presentation](../README.md)

Le sketch Arduino se trouve dans [Tab5OS_Arduino_v0.8.53](Tab5OS_Arduino_v0.8.53/). Les applications ELF et Python utilisent les services de l'OS : affichage, entrées, stockage et réseau.

- [Installer ou mettre à jour](../docs/INSTALL_FR.md) · [English installation](../docs/INSTALL_EN.md)
- [Compiler le firmware](../docs/BUILD.md)
- [Créer une application](DEVELOPPEMENT.md)
- [Sources des applications natives](Tab5OS_Arduino_v0.8.53/tools/native-apps/)
- [Exemples MicroPython](examples/micropython/)
- [Documentation des applications](APPLICATIONS.md)

Les fichiers de `Tab5OS_Arduino_v0.8.53/sdcard` sont un ensemble partiel de ressources et de sources Python, pas un paquet SD complet. Les binaires et certaines ressources générées sont distribués séparément.

Un ELF doit cibler l'ABI Tab5OS ; il partage la mémoire du firmware. Les applications utilisent l'instance LVGL et les pilotes de l'OS. Les notices des composants tiers sont conservées avec leurs sources ; voir [LICENCES.md](LICENCES.md) et [NOTICE.md](../NOTICE.md).
