# Cartes, clavier tactile et dossiers image — 0.8.50

Les changements sont appliqués sur l’archive 0.8.49 dont le SHA-256 est `0a6928c6f494f076d359d1ede44a6609a3bf9978e7970b604469787a5c0ee861`. Les résultats ci-dessous ont été exécutés après reconstruction des sources ; ils ne reprennent pas les résultats perdus lors de l’interruption.

## Corrections

- Un bouton clavier est disponible dans les boîtes de saisie communes, notamment la recherche Cartes. Il ouvre ou ferme le clavier tactile même lorsqu’un clavier USB est branché.
- Le champ de recherche Cartes reçoit le focus d’édition à l’ouverture. Un clic dans ce champ conserve le clavier tactile lorsqu’il a été ouvert manuellement. Entrée valide et Échap ferme la boîte ; fermer détruit les références au champ.
- Les nouveaux exports JPEG de Cartes utilisent `/Picture/Maps`. Wikipédia enregistre ses images en ligne et celles des archives ZIM sous `/Picture/Wikipedia`.
- Les dossiers et fichiers déjà présents dans `/Pictures` ne sont pas déplacés, fusionnés ou supprimés. Les fichiers existants et les chemins utilisateur restent accessibles.
- L’aide de l’export JPEG utilise le nouveau chemin. Sa clé traduite est remplacée dans les packs, sans conserver un doublon de la longue aide.
- Intégrations dans le main pour le lot Réveil/charte : icône de réveil par défaut index16 et application des surbrillances partagées aux boutons.

Les workers Cartes, la recherche réseau/locale, la gestion SD et le pipeline d’export existants ne sont pas modifiés.

## Vérifications exécutées

`tools/maps-input-tests-0850/run.py` extrait les fonctions réelles du main : construction/fermeture de boîte, affichage du clavier, distribution des touches et lancement d’export. Elles sont compilées avec LVGL9.6, le véritable `Tab5Focus`, `Tab5TextInput`, `WriterDocument` et les styles partagés. ASan et UBSan sont actifs.

- Témoin 0.8.49 : absence du bouton clavier reproduite.
- **2 204 contrôles** dans la version corrigée, sur **100 cycles** d’ouverture/fermeture.
- USB présent et absent ; clavier normal264px et IME354px ; champ et boutons vérifiés à l’intérieur de la boîte, au-dessus du clavier sur1280×720.
- Texte physique « Le Havre », validation Entrée et événement READY du clavier tactile, fermeture Échap.
- Clic dans le champ après ouverture manuelle conservant le clavier tactile.
- Export vers `/Picture/Maps`, collision avec fichier ou `.tmp` existant, ancien fichier `/Pictures/Maps` intact.

`tools/maps-input-tests-0850/wiki_paths.py` exécute la fonction réelle `image_actions` et la branche réelle d’extraction ZIM dans CPython, avec des fichiers temporaires et des frontières réseau/interface contrôlées : **12 contrôles** passent. JPEG/PNG/WebP et conservation des anciens fichiers sont vérifiés.

Manifestes : `tools/verification/map-input-paths-0850.json` et `wiki-picture-paths-0850.json`. Les empreintes de sources et des tests permettent de vérifier que les preuves correspondent aux fichiers livrés.

## Portée et essais matériels

Le panneau clavier et les périphériques sont des frontières contrôlées dans le test : il ne mesure ni latence tactile, ni USB réel, ni SD. Les vrais objets LVGL, le focus et les chemins de code cités sont exécutés. LeakSanitizer est désactivé dans ce conteneur ; ASan/UBSan restent actifs. La compilation complète du firmware est rapportée séparément.

Sur Tab5 : ouvrir Cartes → recherche sans clavier USB, puis avec un clavier USB. Ouvrir le clavier par son bouton, toucher le champ, saisir et valider. Recommencer puis annuler par Échap. Avec une langue IME, vérifier que la zone de saisie reste visible. Exporter un JPEG, puis enregistrer une image Wikipédia ; vérifier les nouveaux chemins `/Picture` et l’accès aux anciennes images de `/Pictures`.
