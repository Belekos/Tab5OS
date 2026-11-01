# Tab5OS 0.8.53

Cette version reprend la 0.8.52 livrée et améliore l’explorateur, les icônes, le clavier virtuel Meshtastic, le bandeau latéral et l’utilisation de la SRAM. Les données, partitions et choix d’autosauvegarde sont conservés. Voir le [rapport fonctionnel](RAPPORT_FONCTIONNEL_0.8.53.md) et les [essais Tab5](TESTS_TAB5_0.8.53.md).

<!-- build-status -->
**Compilation complète ESP32-P4 et vérification du BIN réussies.** Tests ciblés et limites dans [VERIFICATION.md](VERIFICATION.md). Aucun essai physique effectué.
<!-- /build-status -->

Aucun essai sur le Tab5 physique n’est revendiqué. Les contrôles réalisés sont détaillés dans [VERIFICATION.md](VERIFICATION.md).

## Installation

**Mettre à jour le firmware et les ressources SD ensemble, notamment `Applications/Pentest.app.elf`, son profil `.launch.json`, les langues, claviers, polices, dictionnaires et `System/maps/gazetteer`.** Pentest n'est plus un outil intégré au firmware : son fichier ELF est nécessaire pour le lancer. Une carte conservant les anciennes ressources ne charge pas les nouveaux libellés et l'aide corrigée.

<!-- firmware-install -->
L’image [firmware/Tab5OS_v0.8.53.bin](firmware/Tab5OS_v0.8.53.bin) est le firmware **OTA**. Utiliser la mise à jour de Tab5OS ; ce BIN applicatif ne doit pas être écrit à zéro comme une image fusionnée. SHA-256 : `firmware/SHA256.txt`.
<!-- /firmware-install -->

Les ressources se trouvent dans `Tab5OS_Arduino_v0.8.53_Ecriture/sdcard`. Pour une carte neuve en FAT32, copier le **contenu** de ce dossier à la racine.

Sur une carte déjà utilisée, conserver une sauvegarde avant le premier démarrage de la 0.8.53. Pour une mise à jour depuis les versions anciennes concernées, la migration existante déplace les données de `/M5Writer` vers `/System` et garde les collisions sous `/System/migration_0824` ; ne pas effacer ces dossiers manuellement pendant une migration interrompue.

Mise à jour des ressources :

1. Sauvegarder les documents et les modules personnalisés. Conserver les réglages, clés, archives Wikipédia, ROMs et sauvegardes.
2. Copier `sdcard/Factory/Tab5OS-0.8.53` dans `/Factory/Tab5OS-0.8.53` sur la carte.
3. Actualiser depuis cette livraison les modules de démarrage de la réparation : `MicroPython/lib/tab5factory.py`, `tab5workshop.py`, `tab5fs.py`, `tab5gui.py`, `tab5ui.py`, `tab5apps.py`, ainsi que `MicroPython/update_sd.py`. Sauvegarder leurs éventuelles adaptations avant remplacement.
4. Redémarrer et ouvrir `/MicroPython/update_sd.py` depuis l’Explorateur. **Réparer et mettre à jour** installe les ressources officielles proposées. **Restaurer les fichiers modifiés** remplace volontairement les ressources personnalisées ; réserver cette action à ce besoin.
5. Redémarrer pour charger les nouvelles langues, polices et dispositions de clavier.

Ne pas écraser tout le dossier `System` d’une carte utilisée. **Réglages → Stockage → Restaurer les fichiers manquants** est un secours distinct : les petites ressources absentes sont recréées, mais les fichiers existants ne sont pas actualisés. Les polices, dictionnaires CJK et l’index des villes restent des ressources SD. L’index `System/maps/gazetteer/cities.t5geo` pèse environ 38 Mo ; il est copié lors de l’installation ou de la réparation demandée, jamais parcouru intégralement au démarrage. Voir [REPARATION_SD.md](REPARATION_SD.md).

## Nouveautés 0.8.53

Consulter [RAPPORT_FONCTIONNEL_0.8.53.md](RAPPORT_FONCTIONNEL_0.8.53.md), [EVOLUTIONS_0.8.39.md](EVOLUTIONS_0.8.39.md) et [VERIFICATION.md](VERIFICATION.md). Les nouvelles fonctions conservent la gestion mémoire du P4, les quatorze langues et les composants graphiques de l’OS. La difficulté de réalisation ne représente pas la valeur des évolutions demandées.

**Dessin est intégré au firmware et accessible depuis l’accueil.** Les corrections de la 0.8.35 restent présentes : seconde couleur au clic droit, texte direct, options escamotables, taille initiale de 2 px et imports aux dimensions originales. Voir [DESSIN.md](DESSIN.md).

La surveillance batterie, les diagnostics écran et les commandes série de l’éditeur sont conservés. Les notes 0.8.28 décrivant le refus Wi-Fi ou la copie de secours avant coupure restent historiques.
Les fonctions précédentes, dont les 2 048 détections SDR et les commandes série du terminal, restent disponibles. Le SDR reste natif ; RDS2 et ADS-B ne sont pas implémentés.

Pour créer une application compatible ou préparer une contribution, consulter [DEVELOPPEMENT.md](DEVELOPPEMENT.md), [CONTRIBUTING.md](CONTRIBUTING.md) et [LICENCES.md](LICENCES.md). Les composants n’ont pas tous la même licence ; la publication doit préserver leurs notices et résoudre le choix de licence du code principal.

## Utilisation

Le réglage **Texte : éditeur et ebooks** choisit 20, 24, 28 ou 32 pixels pour ces documents, avec un aperçu immédiat. La taille des menus et des boutons est indépendante. Le changement est transmis à l'éditeur sans effacer la sélection ou le défilement ; le lecteur recalcule ses pages pour la nouvelle taille.

La calculatrice conserve chaque expression puis son résultat dans une zone défilante. Toucher une de ces lignes la replace dans la saisie. Un opérateur après un résultat poursuit le calcul ; un chiffre commence une nouvelle expression. Les conversions s'ouvrent dans une fenêtre distincte. L'historique appartient à la session de calculatrice : il conserve jusqu'à 128 calculs ou 64 Kio de texte, puis retire les plus anciens ; fermer la calculatrice l'efface.

Dans l'éditeur de thèmes, le fond et le bouton d'un interrupteur ont chacun une couleur pour l'état actif et une autre pour l'état inactif. Les thèmes plus anciens restent utilisables.

La recherche de radios propose **Tous**, **MP3** et **AAC**, avec **Tous** par défaut. Les formats de flux non pris en charge restent exclus des résultats importables.

Le terminal prend en charge les tubes `|`, les substitutions `$(commande)` et les accents graves, ainsi que les redirections. Les limites et exemples sont décrits dans [TERMINAL.md](TERMINAL.md). La commande `memory` affiche notamment la mémoire libre et le plus grand bloc disponible en SRAM et PSRAM.


- L’accueil présente les applications intégrées, une icône **Apps ELF** et une icône **Apps Python**. Chaque icône ouvre son propre menu ; le retour conserve la position.
- **Réglages → Interface → Affichage → Accueil et bandeau** permet d’**Ajouter une application** native, ELF ou Python au bandeau. Les flèches réordonnent les raccourcis ; la corbeille retire un raccourci sans supprimer l’application. Le bandeau accueille jusqu’à **10 raccourcis**, sans défilement, et son interrupteur permet de le masquer. Accueil reste accessible en haut en dehors du menu principal ; les pages récupèrent la largeur libérée.
- Le thème se choisit dans une liste et s’applique immédiatement. Le réglage des arrondis permet d’adoucir les formes sans imposer une autre palette.
- Dans l’Explorateur, les volumes sont des racines de l’arbre gauche. **Actions → Ouvrir avec…** choisit l’application pour la sélection ; **Édition** regroupe presse-papiers, renommage, suppression et sélection multiple. Afficher les fichiers cachés et masquer une sélection restent deux actions distinctes.
- **USB rapide** est de nouveau accessible sans disque branché. Le changement prend effet après redémarrage. Pour une SN30 Pro, commencer par un câble USB de données et un mode HID Android ou XInput Windows ; l’appairage Bluetooth et le protocole Switch ne sont pas implémentés. Voir [MANETTES_USB.md](MANETTES_USB.md).

Les packs chinois simplifié, japonais et coréen comprennent une méthode de saisie. Le chinois utilise le pinyin avec choix de candidats ; le japonais permet romaji, kana et conversion d’une lecture exacte ; le coréen compose les syllabes modernes en disposition 2-beolsik. Les limites de conversion sont décrites dans [NOUVEAUTES_v0.8.19.md](NOUVEAUTES_v0.8.19.md).

Les raccourcis Ctrl, Alt et Meta restent utilisables avec les dispositions non latines ; les caractères saisis normalement suivent la langue du clavier.

Le Terminal propose une aide par commande et des outils réseau et de téléchargement. L'application ELF **Pentest** regroupe des audits ciblés. Voir [TERMINAL.md](TERMINAL.md), [PENTEST.md](PENTEST.md) et [RESEAU_DIAGNOSTIC.md](RESEAU_DIAGNOSTIC.md). Aucun tunnel VPN ni client Tor n'est livré : la supervision actuelle ne garantit pas un passage de toutes les connexions par un tunnel.

Le coffre appareil exige de créer un PIN de 4 à 12 chiffres dès la première utilisation, y compris après la mise à jour d’un coffre sans PIN. Déverrouiller le coffre demande ce PIN ; le modifier exige le PIN actuel. Les anciens coffres conservent leur mot de passe : sauvegarder les clés avant toute intervention. Voir [SECURITE.md](SECURITE.md).

Pour reconstruire les sources, consulter [COMPILATION.md](COMPILATION.md). Les notes des versions antérieures décrivent leur état historique.
