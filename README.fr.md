# Tab5OS

**Un environnement nomade pour M5Stack Tab5.**

[English](README.md) · [Installation](docs/INSTALL_FR.md) · [Galerie](docs/screenshots/README.fr.md)

![Aperçus de l'interface Tab5OS](docs/screenshots/cover-fr.png)

Tab5OS réunit écriture, dessin, lecture, médias, cartes et outils de développement sur le **M5Stack Tab5 / ESP32-P4**. Son interface graphique s'utilise au doigt, avec un clavier USB et une souris. Documents et applications peuvent être conservés sur microSD ou support USB.

## Ce que l'on peut faire

| Usage | Outils |
| --- | --- |
| Écrire et lire | Éditeur de texte/code, ebooks et Wikipédia hors ligne avec ses archives locales |
| Dessiner | Pinceaux, calques, sélections, formes, texte, transformations, annuler/rétablir et projets T5I ; export BMP/JPEG |
| Gérer et partager les fichiers | Explorateur SD/USB, actions contextuelles, « Ouvrir avec » et partage local depuis le navigateur d'un ordinateur |
| Écouter et regarder | Lecteur multimédia et webradio |
| Explorer | Cartes en ligne/hors ligne, recherche de lieux et repères |
| Programmer et se connecter | Terminal, Git, MicroPython, applications natives ELF, outils série, commandes SSH distantes et SFTP |
| Communiquer | Discussion IRC et Meshtastic avec une radio externe compatible |
| Imprimer et protéger ses documents | Impression IPP, imprimantes USB prises en charge, chiffrement de fichiers et coffre avec accès par PIN |
| Jouer | Sources de Doom, Maze3D et d'émulateurs Game Boy/Color, NES, Sega 8 bits, GBA, SNES et Mega Drive |
| Personnaliser | Thèmes, langues d'interface, raccourcis d'applications, calculatrice et réveil ; RTL-SDR avec un récepteur USB compatible |

Les outils intégrés et les applications externes ELF/Python se retrouvent dans l'accueil et le bandeau personnalisable. Les applications externes et les contenus hors ligne nécessitent leurs fichiers sur le support de stockage ; les ROMs et les données de jeux commerciaux ne sont pas fournies.

## Aperçus de l'interface

La couverture et la [galerie](docs/screenshots/README.fr.md) présentent des **rendus LVGL sur ordinateur**, avec des données de démonstration. Ils illustrent l'interface ; ce ne sont pas des captures d'un Tab5 physique.

## Installer ou mettre à jour

Suivre le [guide d'installation](docs/INSTALL_FR.md), y compris la préparation des ressources microSD correspondantes.

- **Tab5OS déjà installé :** installer le BIN applicatif OTA depuis la mise à jour intégrée.
- **Première installation :** utiliser l'image P4 fusionnée. Elle remplace le contenu de la Flash interne, y compris les réglages.
- **MicroSD déjà utilisée :** sauvegarder ses fichiers personnels et suivre la mise à jour des ressources sans remplacer tout le dossier `/System`.

**Disponibles ici : les sources 0.8.53. Le firmware précompilé, le paquet SD complet et la fiche Tab5OS dans M5Burner ne sont pas encore publiés.**
