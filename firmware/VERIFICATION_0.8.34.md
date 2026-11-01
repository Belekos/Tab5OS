# Vérification 0.8.34

Version reconstruite depuis le ZIP 0.8.33 livré (SHA-256 `c2716546724868064a0fa22808efc425d15b0ff055a92e32efb50fa965b9d8d6`). Les sources temporaires de la première préparation 0.8.34 ont été supprimées par maintenance ; les corrections ont été reconstruites et les tests ci-dessous réexécutés. Les anciens compteurs de cette préparation ne sont pas réutilisés. **Aucun essai sur Tab5 physique n’est revendiqué.**

## Firmware

<!-- firmware-verification -->
Compilation complète réussie avec Arduino CLI 1.5.1, core M5Stack 3.3.9 et GCC RISC-V 14.2.0 (`esp-rv32/2601`). Cible : `m5stack:esp32:m5stack_tab5:ChipVariant=prev3,PSRAM=enabled,PartitionScheme=default,FlashMode=dio`.

1 959 fichiers sources sont restés identiques pendant la compilation. Les 1 855 copies Arduino contrôlées correspondent aux sources, les 382 lignes de prototypes générées sont identifiées et le corps du sketch est préservé. 1 404 objets ont été produits. ELF32 RISC-V, image ESP32-P4, checksum, hash de validation, correspondance du hash ELF embarqué et provenance des verrous crypto passent les contrôles. Le décodeur JPEG compilé possède la même empreinte que celui des bancs hôte.

| Livrable / contrôle | Valeur |
| --- | --- |
| Image OTA | [Tab5OS_v0.8.34.bin](firmware/Tab5OS_v0.8.34.bin) |
| Taille BIN | 6 043 744 octets |
| Partition OTA | 6 553 600 octets |
| Marge | 509 856 octets |
| SHA-256 | `a098b1cd56c826978de523024c3ebc11cbfdda9e935a30a42182a06905f159f0` |
| Preuve | [firmware-verification-0834.json](tools/verification/firmware-verification-0834.json) |
<!-- /firmware-verification -->

ELF et map associés dans `firmware/debug`. Pas de bootloader ni d’image fusionnée. Le choix de cible prev3 ne prouve pas la compatibilité avec toutes les révisions matérielles.

## Tests courants

| Contrôle | Résultat et portée | Rapport |
| --- | --- | --- |
| Moteur Dessin | PASS — 86 279 contrôles, 600 cycles, 360 scénarios d’allocation : opacité par geste, six formes de pinceau, déformation locale, calques, historique, annulation et erreurs. | [image-core-0834.json](tools/verification/image-core-0834.json) |
| Rendu de la vue | PASS — 1 093 696 contrôles, 196 cas de vue/rectangle partiel : zoom, origines signées, géométrie limite, rendu régional et transparence. | [image-render-0834.json](tools/verification/image-render-0834.json) |
| Souris / routage OS | PASS — 9 046 contrôles et 1 000 cycles : LVGL réel, bouton principal normal/inversé, clic droit, molette, déconnexion et interruption par un dialogue. | [drawing-input-0834.json](tools/verification/drawing-input-0834.json) |
<!-- codec-verification -->
| JPEG | PASS — 54 979 contrôles : import standard et gris, réductions 1/2, 1/4 et 1/8, export vérifié par libjpeg, annulation, erreurs IO et allocation. | [image-codec-0834.json](tools/verification/image-codec-0834.json) |
<!-- /codec-verification -->
<!-- native-verification -->
| Interface native | PASS — 362 contrôles, 14 captures, cinq fermetures : gestes, contexte, clavier, formes, zoom/pan, JPEG, allocations et suspension. Pic PSRAM suivi : 12 035 768 octets ; zéro restant à la fermeture. | [image-native-0834.json](tools/verification/image-native-0834.json) |
<!-- /native-verification -->
| Composants préservés | PASS — 582 fichiers mémoire, Git, affichage et Cartes identiques à la livraison 0.8.33. | [preserved-components-0834.json](tools/verification/preserved-components-0834.json) |

Chaque rapport lie les sources exécutées par SHA-256. Le défaut d’opacité est reproduit sur le moteur .33 exact : à 50 %, les passages répétés sans relâcher donnaient un alpha de 254 ; le moteur corrigé conserve 127. La dureté agit sur le bord et la diffusion sur une répartition spatiale stable. Les zones de travail sont réservées avant le geste et libérées à sa validation ou annulation.

Le rendu utilise une vue de taille bornée, indépendante du zoom. Un oracle indépendant contrôle zoom et déplacements, y compris les coordonnées négatives et les mises à jour partielles. Aucun pixel du document n’est modifié par la navigation et le rendu n’alloue pas de mémoire.

Le banc souris compile le service de production avec LVGL. Il extrait textuellement le routage du sketch principal ; la file USB et la frontière d’appel à l’éditeur sont simulées. Le banc natif séparé exécute l’éditeur réel, son moteur, les codecs, LVGL, la police de l’OS, le focus, la saisie et le service d’écriture. Les interfaces matérielles restent simulées.

Le décodeur JPEG M5GFX reçoit cinq corrections ciblées dans une copie isolée : quatre expressions évitent des décalages de nombres négatifs et `memmove` traite le chevauchement lors du compactage d’un bord d’image. Les bancs reproduisent ces défauts avant correctif. Le décodeur des tests JPEG, de l’interface et du firmware doit avoir la même empreinte ; les dépendances partagées ne sont pas modifiées.

ASan et UBSan sont utilisés dans les bancs qui les indiquent. LeakSanitizer est désactivé dans cet environnement ; les compteurs d’allocation instrumentés sont vérifiés. Les erreurs d’écriture traitées par le logiciel sont testées, sans garantir l’atomicité FAT lors d’une coupure électrique.

## Ressources et archive

Les 14 packs couvrent 3 355 clés de référence, avec 52 ajouts par langue. L’audit ne relève aucune clé manquante ni erreur de format, glyphes ou retours à la ligne. Le plus grand pack, russe, occupe 517 816 octets sous la limite de 512 Kio. Les valeurs et métadonnées préexistantes sont préservées.

175 ressources Factory et 40 ressources embarquées sont vérifiées, ainsi que la syntaxe Python. Rapports : [languages-0834.json](tools/verification/languages-0834.json), [drawing-localization-0834.json](tools/verification/drawing-localization-0834.json) et [resources-0834.json](tools/verification/resources-0834.json).

Le paquet est vérifié avant création et après extraction : sources compilées, rapports, BIN/ELF/map, ressources SD/Factory et inventaire de publication. Les sources des bancs et les captures finales sont incluses. Les anciennes sorties intermédiaires et l’ancien ELF Atelier Image sont exclus.

## Portée matérielle

L’utilisateur a confirmé Git fonctionnel et l’absence de nouveaux flashs dans son usage de la 0.8.33. La politique mémoire, Git et le pilote d’affichage sont préservés octet par octet sur les 582 fichiers contrôlés. Le sketch principal et le service souris ont des modifications d’intégration testées séparément. Les anciens tests mémoire/Git ne sont pas présentés comme réexécutés.

Les contrôles hôte ne mesurent pas la fluidité du Tab5, le débit PSRAM, le comportement de la carte SD ni celui du périphérique USB physique. Après installation conjointe du firmware et des ressources SD, vérifier les traits lents à faible opacité, zoom/Main/bouton central, aperçus de formes, annulation, déformation locale, import/export de JPEG personnels, puis la navigation entre applications, Git et Cartes dans l’usage habituel.

Voir [DESSIN.md](DESSIN.md) pour les commandes et limites (dont JPEG progressif et orientation EXIF automatique non pris en charge). [VERIFICATION_0.8.33.md](VERIFICATION_0.8.33.md) conserve les résultats historiques.
