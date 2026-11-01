# Tab5OS 0.8.53 — Explorateur, bandeau, clavier et SRAM

La référence est la 0.8.52 livrée, archive SHA-256 `d603cf2c465a5ffecffbde90ba90bc158bdab52d770a0cc4337184298c26a696`. Cette reprise conserve les applications, les données personnelles, les partitions et les mécanismes d’affichage/audio stabilisés. Les changements portent sur les demandes fonctionnelles suivantes et des économies SRAM ciblées.

## Corrections et causes établies

| Zone | Cause ou limite constatée | Modification |
| --- | --- | --- |
| Menus de l’explorateur | Ce sont des boutons de `Tab5ContextMenu`, pas les listes natives LVGL corrigées en .52. Leur ancien callback déplaçait le focus clavier pour représenter le survol, mais le mode souris masque cette surbrillance clavier. | Appliquer le style de survol partagé et retirer les callbacks qui déplaçaient le focus logique. Aucun timer ajouté. |
| Barre d’actions | Son bord inférieur touchait exactement le début des lignes. | Séparation verticale de 8 pixels ; cibles tactiles conservées. |
| Ouverture rapide des images | Visualisateur et Dessin n’étaient pas distingués et Dessin manquait dans la barre. | Œil pour le visualisateur, pinceau pour Dessin, Ouvrir avec conservé. Le projet T5I reste ouvert dans Dessin au double-clic. |
| Réveil | Symbole vectoriel peu lisible aux petites tailles. | Réveil redessiné avec cadran, aiguilles, cloches et pieds, dans les couleurs du thème. |
| Meshtastic | Le hook empêchait le virtuel lorsque l’USB annonçait un clavier, sans demande explicite possible. | Bouton clavier et clic dans la saisie demandent le clavier virtuel ; l’ouverture automatique conserve la préférence pour le clavier physique. |
| Bibliothèques | Intitulés longs. | « Apps ELF » et « Apps Python », traduits dans les 14 langues. |
| Bandeau latéral | Sept destinations codées en dur. | Raccourcis fondés sur les identifiants stables du catalogue existant, pour applications natives, ELF et Python ; capacité bornée par l’écran, sans défilement. |

Le bandeau conserve les choix de l’ancien format et utilise un nouveau format de préférences. La migration à la lecture n’écrit pas automatiquement les paramètres ; l’ancien enregistrement est conservé. Une application retirée de son support ne doit jamais être remplacée implicitement par une autre application ayant pris son ancien index dans le catalogue.

Les applications natives gardent leur icône sans libellé dans le bandeau. Les applications externes utilisent leurs icônes lorsqu’elles existent, avec un préfixe lisible en secours. La capacité est calculée selon la hauteur disponible, avec une limite de dix raccourcis sur la disposition actuelle. Les scans passent par le service existant et restent limités aux contextes de catalogue nécessaires ; ils ne sont pas ajoutés à la lecture des médias ou à l’édition.

## Gestion mémoire

L’index de pagination EPUB occupait une réserve statique même lorsque le lecteur était fermé. Il devient une ressource possédée par le lecteur, allouée à la demande en PSRAM puis libérée. Un repli interne limité à la taille de cet index conserve la possibilité de paginer si la PSRAM refuse l’allocation. Un double refus produit un message explicite ; le lecteur reste utilisable et une action ultérieure peut retenter la pagination. Aucun nouvel essai d’allocation périodique au repos n’est introduit.

Les tables de quantification JPEG et la table de gains du tuner FC0013 sont immuables ; leur qualification constante permet leur placement en Flash. Les algorithmes d’encodage, de pagination et de réglage du gain restent conservés. Aucun buffer DMA, pile de tâche, ISR, framebuffer ou buffer audio n’a été déplacé.

Les mesures sur le firmware final sont les suivantes :

| Mesure, octets | 0.8.52 | 0.8.53 |
| --- | ---: | ---: |
| DATA + BSS internes | 94 128 | 91 508 |
| BIN applicatif | 6 106 224 | 6 113 184 |
| Marge de la partition OTA | 447 376 | 440 416 |

Le gain SRAM statique **net est de 2 620 octets**, nouvelles fonctions comprises. Le gain brut des cinq symboles contrôlés est de 2 624 octets : 2 048 pour la réserve EPUB remplacée par un pointeur, 384 pour JPEG et 192 pour le tuner. IRAM, TCM, RTC et partitions sont inchangées. Le repli EPUB en SRAM consomme à nouveau la taille de l’index pendant son utilisation. Le bilan détaillé figure dans `VERIFICATION.md`. La taille des sections statiques n’est pas une mesure de mémoire libre à l’exécution. Les nouvelles interfaces ont aussi un coût en objets LVGL ; les relevés sur appareil restent nécessaires.

## Vérifications et limites

Compilation complète ESP32-P4 réussie : 1 426 objets, 2 040 fichiers contrôlés, sources finales identiques avant/après compilation. Les contrôles d’intégrité du BIN, de correspondance ELF et de capacité OTA passent. SHA-256 du BIN : `a6a43f66e8cef604ffa8ea7680ae358ddf30ffa5752e543414072526100409fd`.

Les principaux bancs actuels passent : 232 contrôles Explorateur, 822 visualisateur, 148 Meshtastic, 130 icônes, 119 bandeau et 78 contrôles indépendants de persistance/planification. Pour la mémoire : 84 112 contrôles de l’index, 1 004 allocations/libérations équilibrées, 1 440 encodages JPEG et 64 032 scénarios tuner comparés à la référence. Les nombres de contrôles ne sont pas des durées d’endurance sur appareil.

Les défauts de survol et d’ouverture du clavier ont été reproduits sur la référence .52, puis vérifiés sur le code .53. Les bancs utilisent LVGL réel et les contrôleurs de production, avec ASan/UBSan. Les transports USB, Bluetooth, les accès fichiers ou les lancements applicatifs sont remplacés uniquement aux frontières décrites dans les rapports de chaque banc. Ces tests ne constituent pas un démarrage complet de l’OS sur Tab5.

Les essais de mémoire couvrent allocation PSRAM, repli interne, double refus, reprise et cycles de fermeture. Les JPEG produits sont comparés octet par octet à la référence ; les écritures du tuner sont comparées avec un bus I²C simulé. Le visualisateur T5I utilise le véritable parseur et le rendu des calques. Les pixels WebP ne sont pas validés par le banc qui remplace son décodeur.

L’ancien test de pagination conservé dans le projet comporte une attente de géométrie déjà fausse dans la .52. Son échec est reproduit avant/après et consigné ; l’interface n’a pas été modifiée pour satisfaire cette attente obsolète. Les tests actuels de l’index et de son cycle de vie sont distincts.

Tous les textes préexistants et les métadonnées des 14 packs sont conservés ; six clés d’interface sont ajoutées. Les sources, les preuves et les rapports historiques sont conservés sans transformer les anciennes exécutions en nouveaux résultats.

Aucun essai sur Tab5 physique n’est revendiqué. La validation USB/Bluetooth réelle, les coupures d’alimentation pendant la persistance, la réception RF, les latences et l’absence de scintillement sur appareil restent à contrôler. Le protocole est fourni dans `TESTS_TAB5_0.8.53.md`.
