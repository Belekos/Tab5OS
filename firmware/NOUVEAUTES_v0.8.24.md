# Tab5OS 0.8.24 — commandes, fichiers et sessions

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.24 réussis.** Preuve : [firmware-verification.json](tools/verification/firmware-verification.json).
<!-- /build-status -->

## Changements

- Manettes HID et XInput : correction du routage des rapports HID, navigation de l'OS et accès MicroPython. Une comparaison avec les premières versions reproduit un rapport valide mal décodé en 0.8.23. Les essais utilisent des paquets simulés, sans manette physique.
- RTL-SDR : clavier numérique avec point, unités explicites, réglages en réception, pas de fréquence, axes réglables, spectre rectangulaire, bandes et balayage, liste des détections à rappeler, enregistrer ou supprimer. La plage réellement accordable dépend de la clé ; l'AM n'est pas limitée à une bande FM. Voir [RTL_SDR.md](RTL_SDR.md) pour les limites.
- Explorateur : bandeau de menus explicites avec raccourcis, clic droit, tri nom/date/taille croissant ou décroissant, dossiers regroupés avant les fichiers, commandes adaptées à la sélection. L'ordre est partagé avec les sélecteurs et les aperçus de dossiers. Le tri est un choix de session.
- Aperçus : compteur et navigation de dossier fixes, contenu défilant ; aucun décodage d'image sur simple sélection et aucun bouton supplémentaire pour ouvrir le dossier sélectionné à gauche. L'ouverture utilise le double-clic ou Entrée. Le terminal s'ouvre dans le dossier courant, y compris USB.
- Médias : volume et navigation conservés, dernière sélection retrouvée sans lecture automatique, étoile pour les favoris. Une vraie base de 600 stations dans 75 pays et des packs nationaux sont fournis, sans favoris musicaux imposés.
- Cartes : premier affichage générique à 0°, 0°, zoom 2 ; mémorisation optionnelle du centre et du zoom à la sortie. La sauvegarde n'écrit pas sur la carte SD à chaque déplacement.
- Dossier système : `/System` remplace `/M5Writer`. La migration conserve les fichiers et les collisions dans `/System/migration_0824`, accepte une reprise après interruption et maintient les anciens chemins via un alias. Sauvegarder la carte avant l'installation.
- Les quatorze packs de langues suivent les nouvelles commandes.

## Installation et vérification

Mettre à jour ensemble le firmware et le contenu SD officiel, dont `/System`, les modules MicroPython et les applications ELF. Pour une carte déjà utilisée, suivre [LISEZ_MOI.md](LISEZ_MOI.md) sans remplacer tout le dossier contenant les données personnelles.

Les rapports actuels et les preuves historiques identiques sont vérifiés par [verify_features_0824.py](tools/verify_features_0824.py). Un rapport historique garde sa version et sa portée ; une source ou un banc modifié impose un nouvel essai. Les limites et l'état de la compilation figurent dans [VERIFICATION.md](VERIFICATION.md). Aucun essai radio, USB ou mémoire sur tablette réelle n'est revendiqué.

<!-- resource-summary -->
Les 14 packs couvrent les 2985 clés de référence contrôlées. Factory contient 174 ressources officielles ; le secours firmware en contient 40. Les grandes polices et les dictionnaires de saisie sont distribués sur SD. Preuves : [languages-0824.json](tools/verification/languages-0824.json), [ime-0824.json](tools/verification/ime-0824.json), [resources-0824.json](tools/verification/resources-0824.json).
<!-- /resource-summary -->
