# Applications et émulation — 0.8.16

Les `.app.elf` construits avec le SDK Tab5OS sont chargés depuis la SD, sans reflashage. Ils partagent la mémoire de l’OS : ce sont des programmes natifs coopératifs, pas des processus isolés. Un binaire PC, un ELF d’une autre plateforme et un firmware `.bin` ne sont pas interchangeables.

Quake et Duke Nukem 3D sont retirés de cette distribution. Leurs données ou sauvegardes déjà présentes sur une carte ne sont pas supprimées. Le catalogue local et l’accueil personnalisable sont décrits dans APPLICATIONS.md.

## Dessin intégré

Depuis la 0.8.33, l’éditeur image fait partie du firmware et utilise les composants graphiques, icônes et polices de l’OS. Il est renommé Dessin en 0.8.36. Il ne nécessite plus `AtelierImage.app.elf`. Le format T5I conserve les calques ; BMP et JPEG permettent l’échange d’images aplaties. Les sources de l’ancienne interface ELF restent fournies à titre historique.

## Sélection et reprise

Dans Fichiers, sélectionner un document, une ROM ou un dossier, puis **Ouvrir avec…**. Le sélecteur ouvre `/Applications` et ne propose que les ELF compatibles par extension et les dossiers. Le programme choisi reçoit le chemin original dans `argv[1]`. Exécuter démarre le programme.

Le lanceur possède aussi Choisir pour l’application, Fichier et Dossier pour l’argument. Récents et favoris conserve 24 couples application/argument dans `/System/native-apps.json`. Favori marque le couple ; Reprendre relance le plus récent. Les anciens éléments non favoris sont évincés en premier.

`hash_file.app.elf` demeure un exemple calculant une empreinte FNV-1a non cryptographique d’un fichier. Choisir un vrai fichier comme argument pour l’utiliser.

## Game Boy

Ouvrir une ROM `.gb` prépare automatiquement `/Applications/gameboy.app.elf`. Sans argument, le programme ouvre la démonstration libre `/Games/GB/Tab5_Pocket.gb`.

| Commande | Clavier USB / tactile |
|---|---|
| Directions | Flèches / croix de 88 px |
| A / B | Z ou W / X / boutons 128 × 104 px |
| Select / Start | Retour arrière / Entrée / boutons 190 × 68 px |
| Sauvegarder l’état | Ctrl+S / bouton disquette |
| Recommencer | Ctrl+R / bouton flèche circulaire |
| Quitter avec sauvegarde | Échap / Quitter |
| Volume | Page haut/bas / curseur tactile |
| Couper / rétablir le son | M |
| Afficher les commandes | F1 / Clavier |

Sur AZERTY comme sur QWERTY, les positions Z et W sont acceptées pour A ; X commande B. Le réglage de volume utilise le volume général et est conservé dans les réglages de l’OS. Le clavier transmet les touches maintenues et leurs combinaisons ; la voie tactile LVGL reste à un pointeur. Le compteur donne les images produites puis présentées par seconde. La fluidité réelle reste à tester sur Tab5.

Peanut-GB et MiniGB APU couvrent la Game Boy monochrome. ROMs de 32 Kio à 4 Mio, cartouches reconnues par le cœur ; certains titres restent incompatibles. Les cartouches mixtes peuvent utiliser leur mode DMG. Ce cœur monochrome ne gère pas les jeux CGB exclusifs ; utiliser le nouveau programme Game Boy Color. GBA utilise une application séparée décrite plus bas. Pas de câble link.

La RAM de cartouche est enregistrée toutes les 30 secondes si modifiée et à la sortie normale dans `/Saves/GB/<empreinte>.sav`. Un `.state` versionné conserve le cœur, les registres audio et la RAM au moment de la sauvegarde. Son empreinte doit correspondre à la ROM et à son contenu. Le dernier état compatible est repris automatiquement ; un état altéré est ignoré. La version précédente `.old` permet de récupérer une écriture interrompue avant son renommage final. Un changement de structure d’émulateur peut invalider `.state` tout en conservant `.sav`.

Le bouton Recommencer réinitialise la console ; il ne supprime pas la RAM sauvegardée de la cartouche. Une coupure d’alimentation ne peut pas enregistrer la dernière position. L’écoulement réel de l’horloge RTC pendant l’arrêt de la tablette n’est pas émulé.

L’audio utilise le pilote partagé ; une radio ou musique active garde son accès et le jeu peut rester silencieux. Les deux buffers RGB565 OS sont échangés sans copie supplémentaire vers le front buffer. L’application conserve son propre buffer source. Les ressources sont libérées après la fin effective du worker, jamais pendant son exécution.

## Compilation native

```text
python tools/native-apps/build.py tools/native-apps/gameboy/gameboy.c --opt speed --toolchain CHEMIN/esp-rv32/2601/bin -o sdcard/Applications/gameboy.app.elf
```

Le SDK utilise GCC RISC-V 14.2.0 du core M5Stack. `--opt speed` choisit `-O3` ; le défaut `--opt size` privilégie la taille. `Tab5AppApi.h` décrit les ABI 1, 2, 3 et 4. ABI 1 reste compatible. ABI 2 ajoute vidéo, boutons, PCM et sauvegardes bornées dans les dossiers de sauvegarde des applications fournies ; les bits 9/10 sont les demandes ponctuelles de sauvegarde/reprise à zéro.

Le tas suivi est plafonné à 8 Mio pour l’ABI 2, avec 4 Mio par allocation. C’est un plafond, pas une réservation permanente. Les allocations et fichiers oubliés sont récupérés après la terminaison effective. L’application doit consulter l’arrêt et rendre la main ; le système ne supprime pas brutalement son worker.

Le chargeur dérive de `elf_loader` Espressif 1.3.3. Son alignement et sa synchronisation de cache P4 corrigés sont conservés. Les imports restent limités à l’API Tab5, sans seconde initialisation LVGL/écran. Les sources, licences et provenance Peanut-GB/MiniGB APU sont dans `tools/native-apps/gameboy`.

## Game Boy Color

Ouvrir un fichier `.gbc` sélectionne `/Applications/gameboy_color.app.elf`. Sans argument, la démonstration libre `/Games/GBC/Tab5_Color.gbc` s’ouvre. Le programme accepte aussi les `.gb` par **Ouvrir avec**. Ses sources et sa licence GPL-2.0 sont fournies sous `tools/native-apps/gameboy-color` ; le cœur provient de [retro-go / gnuboy](https://github.com/ducalex/retro-go/tree/master/retro-core/components/gnuboy).

Commandes identiques à Game Boy. Sauvegardes indépendantes dans `/Saves/GBC`, reprise automatique d’un état valide correspondant à la ROM ; erreurs d’écriture affichées. ROMs complètes de 32 Kio à 4 Mio, MBC0/1/2/3/5. Les contrôleurs de cartouches spéciaux, câble link, caméra et accessoires ne sont pas émulés. L’horloge n’avance pas en temps réel pendant l’arrêt. Une ROM mixte peut s’exécuter en couleur ; les performances et la compatibilité des jeux commerciaux restent à évaluer sur Tab5.

Le cœur s’exécute dans une application chargée séparément. Ses allocations sont suivies en PSRAM et libérées après la fin du worker. Il ne réinitialise ni LVGL ni l’écran. Les essais utilisent uniquement notre démonstration libre et des sauvegardes de test ; aucun jeu commercial n’est fourni. Le programme GBA est décrit plus bas. Le projet RetroESP32-P4 ne constitue pas un binaire directement chargeable par le SDK Tab5OS.

```text
python tools/native-apps/gameboy-color/build.py --toolchain CHEMIN/esp-rv32/2601/bin -o sdcard/Applications/gameboy_color.app.elf
```

## Accès matériel depuis une application C

ABI 3 ajoute le callback `hardware`, commun au module Python `machine`. Les ABI 1 et 2 restent inchangées. Voir `tools/native-apps/HARDWARE.md` pour les broches, opérations, erreurs et limites. Ce SDK d’applications ne fournit pas l’ensemble des symboles ESP-IDF ; recompiler le firmware et charger un `.app.elf` sont deux parcours distincts.

## Manettes USB

Les manettes USB HID standards sont prises en charge par l’OS pour les jeux utilisant l’entrée commune, dont GB/GBC, NES, Sega8 et Maze3D. Ouvrir Clavier · F1 → Configurer la manette et associer A, B, Select, Start, X, Y, L et R, en relâchant chaque bouton entre deux étapes. L’association est mémorisée pour le modèle connecté. Les axes X/Y et la croix sont automatiquement reconnus. Le pilote ne couvre pas les protocoles propriétaires ou Bluetooth. Les touches du clavier restent utilisables et Ctrl+Alt+H est un autre accès à l’aide. Voir NOUVEAUTES_0.8.14.md.

## NES et Sega8

Ouvrir une ROM `.nes` dans Fichiers sélectionne `/Applications/NES.app.elf`. `.sms` et `.gg` sélectionnent `/Applications/Sega8.app.elf`. On peut aussi choisir **Ouvrir avec** ou le sélecteur du lanceur. Sans ROM, ces deux programmes indiquent le fichier attendu ; aucune ROM commerciale n’est fournie.

NES utilise Nofrendo issu de retro-go, accepte le format iNES classique jusqu’à 2 Mio et les mappers présents dans ce cœur. NES 2.0 et les cartouches dépassant cette limite sont refusées. Sega8 utilise SMSPlus, accepte les images Master System / Game Gear sans en-tête de copieur jusqu’à 1 Mio. La compatibilité de chaque jeu et la cadence réelle doivent être mesurées sur Tab5 ; les essais sur ordinateur ne donnent pas le nombre d’images par seconde du P4.

Les commandes communes, volume, aide F1 et manette sont ceux du tableau Game Boy ci-dessus. Pour Sega, A/B deviennent les deux boutons de la console, Start correspond à Start sur Game Gear et à Pause sur Master System. La sauvegarde d’état se fait par Ctrl+S et à la sortie normale. Le dernier état valide est repris automatiquement. La mémoire de cartouche est également conservée lorsqu’elle est utilisée. Les dossiers sont `/Saves/NES` et `/Saves/Sega8`. Les états portent une enveloppe avec longueur et CRC ; un état tronqué ou altéré est ignoré. Le CRC détecte les erreurs accidentelles, ce n’est pas une signature de sécurité.

La cadence vise 50/60 Hz selon le système avec saut d’images sous charge. Les pixels et l’audio passent par l’API de l’OS, sans seconde instance LVGL ni réinitialisation de l’écran. Les allocations PSRAM sont faites au lancement et reprises par l’OS après la fin du programme. L’arène est dimensionnée sur la ROM, environ ROM + 1 Mio pour NES ou ROM arrondie + 1,5 Mio pour Sega8 ; elle n’est pas réservée en permanence.

```text
python tools/native-apps/nes/build.py --toolchain CHEMIN/esp-rv32/2601/bin -o sdcard/Applications/NES.app.elf
python tools/native-apps/sega8/build.py --toolchain CHEMIN/esp-rv32/2601/bin -o sdcard/Applications/Sega8.app.elf
```

## Maze3D

Lancer `/Applications/Maze3D.app.elf` sans argument. Ramasser les trois clés jaunes puis rejoindre la sortie verte. Flèches pour avancer, reculer et tourner ; X/B pour courir ; Retour arrière/Select pour le plan ; Entrée/Start pour un nouveau labyrinthe. F1 affiche ces commandes. Ctrl+S sauvegarde ; Échap sauvegarde puis quitte. La partie est reprise depuis `/Saves/Maze3D/last.state`, avec sauvegarde périodique et à la sortie normale.

Le rendu utilise un lancer de rayons en 320 × 200, agrandi par la vue de l’OS. Ce jeu autonome n’est pas un port de Doom ou de Quake. Ses sources sont fournies ; aucun fichier de jeu externe n’est requis.

```text
python tools/native-apps/maze3d/build.py --toolchain CHEMIN/esp-rv32/2601/bin -o sdcard/Applications/Maze3D.app.elf
```

## GBA, SNES et Mega Drive

Les programmes **GBA**, **SNES** et **MegaDrive** sont dans `/Applications`. Sélectionner un fichier dans l’explorateur prépare son application ; Ouvrir avec reste disponible. Les jeux ne sont pas inclus.

| Application | Fichier | Cœur | Budget de l’arène |
|---|---|---|---|
| GBA | `.gba`, jusqu’à 7 Mio | gpSP, interpréteur | 12 Mio |
| SNES | `.sfc` / `.smc`, jusqu’à 6 Mio | Snes9x | 10 Mio |
| MegaDrive | `.gen` / `.md`, jusqu’à 8 Mio | Gwenesis | ROM + 3 Mio |

Le budget est alloué au lancement puis libéré. Une ROM Mega Drive `.md` s’ouvre avec **Ouvrir avec → MegaDrive**, car cette extension reste associée aux documents Markdown. Le format copieur `.smd` n’est pas accepté. Le BIOS libre GBA et sa licence sont fournis avec le code ; aucun BIOS propriétaire n’est nécessaire dans ce port. Pas de JIT GBA, d’accessoires ou de multijoueur. Les jeux SNES à coprocesseur particulier peuvent être incompatibles.

Les touches supplémentaires sont S = X, position A/Q = Y, position Q/A = L, E = R. Les manettes se configurent depuis l’aide F1. Sauvegarde Ctrl+S, reprise d’un état compatible et volume commun à l’OS. Les sauvegardes se trouvent dans `/Saves/GBA`, `/Saves/SNES` et `/Saves/MegaDrive`. La compatibilité et la cadence sur P4 doivent être confirmées avec vos jeux.

## Doom

Choisir un IWAD `.wad` de Doom, Doom II ou Freedoom compatible PrBoom dans le sélecteur du lanceur ou avec **Ouvrir avec**. Les données du jeu ne sont pas incluses. Affichage : 320 × 200.

**Ctrl+R** ouvre le menu ; **Ctrl+S** demande une sauvegarde rapide en partie. Recharger depuis le menu du jeu. Flèches pour se déplacer, A (Z/W) pour valider ou tirer, B (X) pour courir, Start pour les portes et Select pour changer d’arme. Page haut/bas, M et F1 conservent volume, sourdine et aide. Échap quitte ; enregistrer avant de quitter.

Configuration et sauvegardes sont dans `/Saves/Doom`. Pas de réseau multijoueur, de packs haute résolution ni de garantie pour tous les mods. La fluidité et la stabilité sur P4 restent à confirmer sur matériel.

```text
python tools/native-apps/doom/build.py --toolchain CHEMIN/esp-rv32/2601/bin -o sdcard/Applications/Doom.app.elf
```

## ABI 4

L’ABI 4 conserve le préfixe ABI 3 et ajoute les touches X/Y/L/R, l’heure/calendrier et un service arithmétique dont les paramètres sont des représentations entières. Le tas suivi peut atteindre 20 Mio au total, avec 16 Mio maximum par bloc, sous réserve de mémoire réellement disponible. Les sauvegardes atomiques sont bornées à 2 Mio ; `.cfg` et `.dsg` s’ajoutent à `.sav` et `.state` dans les dossiers autorisés. Les ABI antérieures conservent leurs plafonds.

`TAB5_APP_STACK(bytes)` déclare une pile de 16 à 256 Kio, multiple de 4 Kio, dans la section en lecture seule `.tab5.stack`. Le chargeur contrôle cette demande ; les piles étendues sont en PSRAM et sont supprimées seulement après suspension effective du worker. La pile par défaut reste 16 Kio. Le SDK compile les ELF RV32 avec ABI entière `ilp32` ; le firmware Arduino reste `ilp32f`. Les interfaces entre les deux ne passent aucun flottant natif.

Les fichiers UPSTREAM et les licences sont conservés dans chaque dossier de sources. Un ELF n’est pas isolé par une MMU ; une faute dans du code natif peut encore arrêter l’OS.
