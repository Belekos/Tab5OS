# Tab5OS 0.8.21 — calculs, lecture et terminal

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.21 réussis.** Preuve : [firmware-verification.json](tools/verification/history-0821/firmware-verification.json).
<!-- /build-status -->

Mettre à jour le firmware et les ressources SD ensemble selon [LISEZ_MOI.md](LISEZ_MOI.md). Les documents et réglages personnels restent à conserver.

## Calculatrice

L'historique défilant affiche une expression puis son résultat sur deux lignes. Chaque ligne peut être reprise dans la saisie. Un opérateur poursuit le résultat précédent ; un chiffre commence un nouveau calcul. La réutilisation conserve la précision du résultat, y compris son signe et la priorité des opérations. Les fonctions du pavé sont conservées. Les conversions passent dans une fenêtre dédiée. L'historique est limité à la session ouverte, à 128 calculs ou 64 Kio de texte ; les plus anciens sont retirés si cette limite est atteinte et la fermeture de la calculatrice libère l'ensemble.

## Interrupteurs et taille du texte

Un interrupteur comporte deux parties : le fond et le bouton mobile. Chacune possède maintenant une couleur active et une couleur inactive, soit quatre réglages. L'aperçu utilise deux véritables interrupteurs, dans les deux états. Les anciens thèmes gardent leurs couleurs de repli.

La taille du texte concernait les documents ; le nouvel éditeur ne recevait plus sa valeur. Le réglage s'appelle désormais **Texte : éditeur et ebooks**, affiche un aperçu et transmet la taille à l'éditeur. Le lecteur utilise exactement la taille choisie, sans l'ancien supplément de deux pixels. Les menus et commandes gardent leur taille propre.

## Lecture ebook

La pagination estimait des mots indivisibles alors que le moteur graphique pouvait les replier, notamment avec les mots longs, le CJK et les changements de style. Certains retraits et une fin de bloc non canonique pouvaient aussi faire dépasser la zone de lecture.

La place occupée est désormais mesurée avec le moteur de spans LVGL utilisé pour le rendu. Une ligne qui ne tient pas passe à la page suivante. Les positions de texte restent exprimées en chapitre, bloc et octet ; les anciens caches de pagination sont invalidés. Les tests comparent les fragments de texte pour détecter pertes et doublons entre pages.

## Terminal

Les tubes `|`, l'entrée depuis un fichier avec `<` et les substitutions `$(commande)` ou avec accents graves sont pris en charge. `cat`, `head`, `tail`, `grep` et `wc` lisent l'entrée du tube en l'absence de fichier. Les guillemets, variables, statuts, erreurs et annulations sont contrôlés par des tests du moteur réel. Voir [TERMINAL.md](TERMINAL.md) pour les exemples et limites.

Le contrôle d'annulation imposait une attente à chaque vérification. La sortie saturée déplaçait également son contenu à répétition, puis l'interface réinjectait fréquemment tout le texte. Ces traitements sont remplacés par une cadence bornée et des tampons circulaires. Les gros contenus temporaires sont placés en PSRAM, avec une libération après la fin effective de la tâche. Les essais de répétition sur hôte suivent les allocations ; ils ne mesurent pas la SRAM physique du Tab5. La commande `memory` permet de consulter les deux mémoires sur l'appareil.

## Python et radios

L'exécution depuis l'éditeur lançait MicroPython sans appeler la reprise de son interface. Le lancement réussi affiche désormais l'application et conserve la page de retour. Le choix de recherche radio commence par **Tous**, sans filtre MP3 ou AAC imposé ; les formats de flux incompatibles restent exclus.

## Validation

<!-- resource-summary -->
Les 14 packs couvrent les 2736 clés de référence contrôlées. Factory contient 93 ressources officielles ; le secours firmware en contient 38. Les grandes polices et les dictionnaires de saisie sont distribués sur SD. Preuves : [languages-0821.json](tools/verification/languages-0821.json), [ime-0819.json](tools/verification/ime-0819.json), [resources-0821.json](tools/verification/resources-0821.json).
<!-- /resource-summary -->

Les preuves et leurs périmètres figurent dans [VERIFICATION.md](VERIFICATION.md). Aucun essai physique ni flash automatique n'est revendiqué. Cette version conserve les corrections Wi-Fi de la 0.8.20 ; elle n'ajoute ni VPN ni Tor.
