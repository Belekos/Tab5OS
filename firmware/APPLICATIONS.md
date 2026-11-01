# Applications et accueil

Tab5OS réunit des outils intégrés et des applications ELF ou Python installées sur microSD ou USB. L'accueil donne accès à l'Explorateur, l'Éditeur, Dessin, eBook, Média, Cartes, Calculatrice, Réglages, Terminal, Réveil, RTL-SDR et Meshtastic. Les menus **Apps ELF** et **Apps Python** présentent les applications présentes sur le support ; **F5** actualise le catalogue.

## Applications ELF

Télécharger et décompresser le [paquet SD complet](../downloads/Tab5OS_0.8.53_SD.zip). Sur une carte neuve, copier le **contenu du dossier `sdcard`** à la racine de la microSD. Pour une carte déjà utilisée, suivre la [procédure de mise à jour](../docs/INSTALL_FR.md#actualiser-une-microsd-existante). Les exécutables sont aussi disponibles dans [sdcard/Applications](Tab5OS_Arduino_v0.8.53/sdcard/Applications/). Copier les ressources SD suivant le [guide d'installation](../docs/INSTALL_FR.md), puis ouvrir **Apps ELF** ou sélectionner un fichier dans l'Explorateur et choisir **Ouvrir avec**.

| Application | Exécutable | Fichier à ouvrir |
| --- | --- | --- |
| Pentest | `Pentest.app.elf` | Sans fichier ; [guide](PENTEST.md) |
| Game Boy | `gameboy.app.elf` | ROM `.gb` |
| Game Boy Color | `gameboy_color.app.elf` | ROM `.gbc` ou `.gb` |
| NES | `NES.app.elf` | ROM `.nes` |
| Sega 8 bits | `Sega8.app.elf` | ROM Master System `.sms` ou Game Gear `.gg` |
| GBA | `GBA.app.elf` | ROM `.gba` |
| SNES | `SNES.app.elf` | ROM `.sfc` ou `.smc` |
| Mega Drive | `MegaDrive.app.elf` | ROM `.gen` ou `.md` |
| Doom | `Doom.app.elf` | IWAD `.wad` compatible PrBoom |
| Maze3D | `Maze3D.app.elf` | Sans fichier |
| Empreinte de fichier | `hash_file.app.elf` | Fichier à lire ; résultat FNV-1a 32 bits dans la console, non cryptographique |

Les jeux commerciaux ne sont pas fournis. Pour une ROM Mega Drive `.md`, utiliser **Ouvrir avec → MegaDrive**, car cette extension sert aussi aux documents Markdown. Maze3D est autonome : ramasser les trois clés jaunes et rejoindre la sortie verte.

### Lancement

Le lanceur permet de choisir un fichier ou un dossier, puis mémorise un mode par application : **Demander au lancement**, **Sans fichier** ou **Toujours ce fichier**. Un lancement explicite depuis **Ouvrir avec** utilise le fichier sélectionné. Pentest possède un profil de lancement sans fichier.

| Support | Chemin dans le lanceur | Chemin reçu par l'application |
| --- | --- | --- |
| microSD | `sd:/Dossier/fichier.ext` | `/Dossier/fichier.ext` |
| USB | `usb:/Dossier/fichier.ext` | `/USB/Dossier/fichier.ext` |

Saisir le chemin directement, même s'il contient des espaces. Les récents et favoris conservent les couples application/fichier dans `/System/native-apps.json`.

### Commandes des jeux

| Action | Clavier |
| --- | --- |
| Directions | Flèches |
| A / B | Z ou W / X |
| Select / Start | Retour arrière / Entrée |
| Aide et commandes propres au jeu | F1 |
| Sauvegarde d'état | Ctrl+S |
| Volume | Page haut / Page bas |
| Sourdine | M |
| Quitter | Échap |

Les jeux utilisant l'entrée commune proposent aussi les commandes tactiles et les [manettes USB](MANETTES_USB.md). Les sauvegardes sont rangées sous `/Saves`, dans le dossier de chaque application. Dans Doom, **Ctrl+R** ouvre le menu : enregistrer avant de quitter et charger la partie depuis ce menu. Dans Maze3D, **X** fait courir, **Retour arrière** affiche le plan et **Entrée** crée un labyrinthe.

La compatibilité dépend du format, de la cartouche et du cœur d'émulation. Les ELF doivent cibler l'API Tab5OS : ils partagent la mémoire de l'OS. Un programme PC ou un firmware `.bin` ne peut pas servir d'application ELF Tab5OS.

## Applications Python

Les [scripts MicroPython](Tab5OS_Arduino_v0.8.53/sdcard/MicroPython/) peuvent être lancés depuis **Apps Python**, l'Explorateur ou l'Éditeur.

| Script | Usage |
| --- | --- |
| `irc.py` | Discussion IRC ; [guide](IRC.md) |
| `wikipedia.py` | Catalogue et préparation de la lecture Wikipédia ; [guide](WIKIPEDIA.md) |
| `libraries.py` | Modules et projets ; [guide](MICROPYTHON_BIBLIOTHEQUES.md) |
| `import_maps.py` | Import des cartes ; [guide](CARTES.md) |
| `import_radios.py` | Import des catalogues radio |
| `create_theme.py` | Accès à la création de thème |
| `update_sd.py` | Réparation et mise à jour des ressources ; [guide](REPARATION_SD.md) |
| `example.py`, `interface.py` | Exemples de scripts et d'interface |

Quitter une application Python revient à l'écran précédent. **Masquer**, lorsqu'il est proposé, conserve l'exécution ; **Ctrl+Alt+P** permet de reprendre l'application active et **Ctrl+.** demande son arrêt.

## Personnaliser l'accueil et le bandeau

Dans **Réglages → Interface → Affichage → Accueil et bandeau**, choisir les applications visibles, leur ordre, leur symbole et la présentation en grille ou liste. **Enregistrer** conserve les changements de l'accueil. **Alt+7** ouvre Réglages même si son raccourci est masqué.

Le bandeau peut accueillir dix raccourcis natifs, ELF ou Python. Retirer un raccourci ne supprime pas le programme. Un raccourci vers un support débranché reste mémorisé et redevient utilisable après reconnexion.

## Ajouter ses applications

| Emplacement sur SD ou USB | Contenu reconnu |
| --- | --- |
| `/MicroPython` | Applications déclarées et scripts `.py` immédiats |
| `/Applications` et `/Apps` | Fichiers immédiats `.py`, `.elf`, `.app.elf` |
| Un sous-dossier de ces emplacements | `main.py`, `main.app.elf` ou `main.elf` |

Le catalogue ne lance pas les programmes pendant la recherche. Il accepte au plus 64 entrées, outils intégrés compris ; les autres fichiers restent accessibles depuis l'Explorateur.

Pour une icône personnalisée, placer un fichier ICO voisin du programme, `icon.ico` dans son dossier ou une icône de même nom sous `/Icons`. Utiliser une image BMP interne de 24 ou 32 bits, au plus 64 × 64 pixels et un fichier de 32 Kio maximum.

Les [sources natives et recettes de compilation](Tab5OS_Arduino_v0.8.53/tools/native-apps/) accompagnent les applications. Le [guide de développement](DEVELOPPEMENT.md) décrit les projets C/ELF et MicroPython. Dessin est intégré au firmware ; son [guide](DESSIN.md) décrit les outils et les formats d'image.
