# Vérification 0.8.35

Version préparée depuis le ZIP 0.8.34 livré, SHA-256 `04ddb6b02b3517d9a146d5e2d83a1a5b046db765de07f7117324b7a4e791eb17`. Les contrôles ci-dessous ont été exécutés sur les sources 0.8.35. Les résultats des anciennes versions restent historiques et ne remplacent pas ces preuves. **Aucun essai sur Tab5 physique n’est revendiqué.**

## Firmware

<!-- firmware-verification -->
**PASS.** Compilation complète de 1 404 objets, sortie 0, sources inchangées. 1 961 fichiers de sources vérifiés et 1 856 copies Arduino comparées. Le prétraitement Arduino ajoute 384 lignes de prototypes ; la conservation des sources d’origine a été contrôlée.

| Élément | Valeur |
| --- | --- |
| Image OTA | [Tab5OS_v0.8.35.bin](firmware/Tab5OS_v0.8.35.bin) |
| Taille | 6 059 904 octets |
| Partition OTA | 6 553 600 octets |
| Marge | 493 696 octets |
| SHA-256 | `66854d904eaeedc011a7da19f8a33f4483a9b448e18ab138379ab64279360e0e` |
| Cible | ESP32-P4, ELF RISC-V 32 bits, ABI simple précision |
| Chaîne | Arduino CLI 1.5.1, M5Stack 3.3.9, GCC RISC-V 14.2.0 |

L’intégrité de l’image, son hash embarqué, sa version et sa correspondance avec l’ELF sont validés. Le paquet fournit l’image OTA, son empreinte, l’ELF de débogage et la carte de liens. Rapport complet : [firmware-verification-0835.json](tools/verification/firmware-verification-0835.json).

Installer l’image OTA avec les ressources SD 0.8.35 selon [LISEZ_MOI.md](LISEZ_MOI.md). Elle ne doit pas être utilisée comme image fusionnée à l’adresse zéro.
<!-- /firmware-verification -->

## Contrôles exécutés

| Domaine | Résultat et portée | Preuve |
| --- | --- | --- |
| Interface Dessin | PASS — 588 contrôles, 21 captures, 5 fermetures. LVGL et interface de production : clic secondaire, couleur figée pendant le geste, texte direct, Entrée/Échap, Unicode, annulation, panneau superposé, survol, zoom 1600 %, navigation 8192 px, formats et fichiers renommés. | [image-native-0835.json](tools/verification/image-native-0835.json) |
| Entrées souris | PASS — 9 046 contrôles, 1 000 cycles : boutons, gaucher, molette, débranchement et priorité des fenêtres modales. Routage extrait du programme principal courant. | [drawing-input-0835.json](tools/verification/drawing-input-0835.json) |
| Explorateur | PASS — 5 259 contrôles : sélection depuis l’aperçu de dossier, dossier courant, pagination, focus différé, choix des applications, T5I/WebP, refus mémoire et menus de 17/32 actions avec LVGL réel. | [explorer-0835.json](tools/verification/explorer-0835.json) |
| Moteur Dessin | PASS — 86 372 contrôles, 600 cycles, 360 scénarios d’allocation : pinceaux, déformation, calques, historique et annulation. | [image-core-0835.json](tools/verification/image-core-0835.json) |
| Rendu | PASS — 1 093 696 contrôles sur 196 vues ; pixels comparés, zoom, origine et régions mises à jour. | [image-render-0835.json](tools/verification/image-render-0835.json) |
| Formats d’image | PASS — 381 999 contrôles, 116 refus d’allocation : JPEG/PNG/WebP aux dimensions originales, BMP 1600×1200 et 8192×16, PNG transparent/palette/16 bits/Adam7, WebP, export vérifié par Pillow, erreurs de lecture/écriture et budget de 18 Mio. | [image-codec-0835.json](tools/verification/image-codec-0835.json) |
| PNG utilisé par Map | PASS — 6 578 252 contrôles, 30 variantes, 512 cycles, 68 allocations refusées : décodeur PNG, validation et raster des tuiles de production. | [map-png-0835.json](tools/verification/map-png-0835.json) |
| Composants conservés | PASS — 582 fichiers du gestionnaire mémoire, de Git, de l’affichage et de Map identiques à la livraison 0.8.34. Le PNG partagé est modifié et vérifié séparément ci-dessus. | [preserved-components-0835.json](tools/verification/preserved-components-0835.json) |
| Traductions | PASS — 22 nouvelles clés dans les 14 packs ; 179 clés utilisées couvertes ; traductions et métadonnées antérieures conservées. Référence : 3 377 clés. Pack russe : 520 006 / 524 288 octets. | [drawing-localization-0835.json](tools/verification/drawing-localization-0835.json), [languages-0835.json](tools/verification/languages-0835.json) |
| Ressources | PASS — 175 ressources Factory, 40 ressources intégrées ; contenu SD cohérent, modules Python valides. | [resources-0835.json](tools/verification/resources-0835.json) |

Les bancs natifs utilisent AddressSanitizer et UndefinedBehaviorSanitizer. Le suivi des allocations exige un retour à zéro ; les pics observés sont 12 394 128 octets dans le banc d’interface et 15 409 662 octets dans celui des codecs. Ce sont des mesures du banc hôte, pas une mesure de toute la mémoire de la tablette. LeakSanitizer n’est pas disponible pour tous les bancs de cet environnement ; chaque rapport indique son suivi et sa portée.

## Corrections vérifiées pendant la revue

- L’ancien export BMP utilisait une ligne sur la pile dimensionnée pour 1024 pixels. L’écriture se fait désormais en blocs bornés, avec tests des grandes largeurs et du remplissage final.
- Le lecteur UTF-8 consomme correctement un caractère sur quatre octets, en un seul glyphe de remplacement si nécessaire. Effacer ce caractère pendant la saisie ne laisse plus de pixels résiduels.
- La capacité de menu passe de 16 à 32 éléments : une liste de 17 actions n’est plus refusée.
- La détection par signature permet d’ouvrir une image renommée et empêche une fausse extension T5I de choisir un enregistrement destructeur par défaut.

La limite de déplacement de la vue a également été adaptée aux images de 8192 pixels de côté. Les coordonnées au survol et les couleurs courantes ne recréent pas l’aperçu à chaque mouvement de souris.

## Dépendances et reproductibilité

Le décodeur JPEG M5GFX corrigé en 0.8.34 est conservé dans les copies de dépendances isolées, avec la même empreinte entre tests et firmware. Pour PNG, le miniz du banc x86 est configuré avec les accès portables déjà utilisés sur RISC-V ; les contrôles UBSan restent actifs. Les empreintes de la source et de cette copie de test sont consignées dans les rapports. Aucun remplacement silencieux de la dépendance firmware n’est effectué.

Les sources des bancs, journaux, captures et empreintes sont incluses. Les scripts des tests se trouvent dans `tools/image-native-tests-0835`, `tools/drawing-input-tests-0835`, `tools/explorer-tests-0835`, `tools/image-core-tests-0835`, `tools/image-codec-tests-0835` et `tools/map-png-tests-0835`. Chaque rapport associe le résultat aux fichiers effectivement testés. Le paquet final est contrôlé avant sa création puis après extraction.

## Limites pratiques

Les tests hôtes simulent le stockage et les frontières matérielles. Ils ne mesurent ni la fluidité sur le P4, ni le débit de la PSRAM ou de la carte SD, ni la souris USB physique. Les tests PNG de Map ne sollicitent pas les serveurs de tuiles et ne refont pas une recherche de lieux sur tablette. Les 582 comparaisons d’empreintes ne constituent pas une nouvelle exécution de tous les anciens tests Git ou mémoire.

Les imports ne réduisent plus les images. Ils restent soumis à 8192 pixels par côté, au budget de pixels du document et à l’enveloppe de 18 Mio de l’application. Un import peut réussir puis une opération exigeant un historique être refusée faute de mémoire ; l’image reste intacte. JPEG progressif, orientation EXIF automatique et BMP compressé ou indexé ne sont pas pris en charge. Le WebP animé ouvre sa première image. Voir [DESSIN.md](DESSIN.md).
