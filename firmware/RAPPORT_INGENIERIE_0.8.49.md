# Tab5OS 0.8.49 — architecture, mémoire et cycles de vie

## Statut

Cette version repart de l’archive 0.8.48 effectivement livrée et de ses échecs signalés sur l’appareil. Elle modifie les mécanismes communs de mémoire, d’affichage et de sortie audio. Les essais sur hôte et la compilation cible ne démontrent pas la disparition physique des flashs ni du claquement analogique. La validation sur Tab5 reste requise ; ce document ne constitue pas une certification production.

## Diagnostic établi

La version audio 0.4.10 déclarée fonctionnelle utilisait déjà des buffers graphiques en PSRAM. Le changement significatif n’est donc pas un simple passage SRAM→PSRAM : LVGL est passé d’un pool privé à des allocations individuelles dans le tas partagé. Dans le SDK réellement compilé, les requêtes de plus grand bloc parcourent ce tas sous verrou critique ; une réallocation déplacée peut y copier les données, interruptions masquées. Les synchronisations de cache de grandes images tiennent aussi un verrou critique partagé. Le réarmement DSI est exécuté par une interruption du cœur UI : le placement IRAM seul ne protège pas contre ces sections critiques.

Ces mécanismes constituent des risques établis par le code et le désassemblage. Leur contribution exacte à chaque flash est une hypothèse à corréler aux compteurs DSI sur appareil. Le deuxième cœur est déjà utilisé par les décodeurs et la sortie audio ; déplacer une tâche ne supprime pas les verrous de tas/cache partagés.

Autres défauts confirmés : release audio abandonné silencieusement après un délai de mutex ; stop logique entraînant des destructions/recréations I²S ; maintien de trois images après fin vidéo ; admission radio exigeant 1 Mio contigu avant un ring capable de fonctionner à taille réduite. Les catalogues conservés, documents, historiques et lectures en pause sont distingués des fuites.

## Corrections par mécanisme

- **LVGL / mémoire** : réutilisation du TLSF natif LVGL dans des segments PSRAM extensibles de 64 Kio, avec mutex de tâche ordinaire. Les objets restent stables ; un segment vide est rendu. Les grosses demandes utilisent des segments adaptés, sans pool fixe de 2 Mio ni réservation de 32 Mio. La plage configurée de 32 Mio décrit les classes d’allocation possibles. Les copies de réallocation LVGL sortent du verrou critique du tas global. Les compteurs dédiés distinguent octets réservés et utilisés.
- **Admission globale** : consultation non mutante des classes/bitmaps TLSF pour la taille demandée, sans parcourir tous les blocs du tas. L’admission conserve les capacités interne/DMA/PSRAM, les réservations et la récupération des caches. Elle ne garantit pas l’allocation future face à la concurrence : le malloc final et sa gestion d’échec restent nécessaires. L’adaptateur privé est limité au SDK/configuration vérifiés ; ailleurs l’API publique native reste le repli, sans interdiction de l’application. Un résultat Fit/NoFit n’est jamais affiché comme un maximum mesuré.
- **Cache / transport graphique** : grandes plages DATA PSRAM éligibles découpées en appels natifs alignés de 16 Kio, validation de la plage entière avant toute opération. Les cas non couverts restent natifs. Les deux phases writeback puis invalidation conservent leur ordre ; aucune allocation, attente arbitraire ou réinitialisation d’écran ajoutée.
- **Audio partagé** : acquittement par le vrai worker avant réutilisation des PCM ; séparation entre arrêt logique et destruction du transport. Pause, seek et transitions immédiates conservent I²S ; fermeture réelle rend le périphérique et les buffers. Release ne renonce plus silencieusement après le timeout de routage. FLAC vérifie le démarrage de sa sortie. Les mesures coûteuses de fragmentation/piles sortent de la télémétrie automatique.
- **Vidéo** : les frames inutilisées sont restituées dans le contexte UI qui possède l’emprunt LVGL. La dernière image affichée est préservée ; les slots manquants sont reconstruits au prochain démarrage avec retour arrière sur erreur.
- **Radio** : budget construit à partir du ring choisi, du codec et des ressources de transport nécessaires ; repli de taille réellement essayé avant l’échec, sans nouvelle exclusivité du réseau empêchant Cartes.
- **Sortie de Media** : restitution événementielle des lignes/groupes et des catalogues rechargeables après masquage et mémorisation de la navigation. Les lectures actives, pauses, commandes en attente et verrous de stockage conservent leurs veto ; la réouverture reconstruit la vue.
- **Diagnostics** : journaux automatiques par compteurs ; détails fragmentation/piles sur demande. Les octets de pool LVGL font déjà partie de la PSRAM, ils ne s’y additionnent pas.

Les réglages, clés, fichiers utilisateurs, partitions, choix d’autosauvegarde et applications sont conservés. Aucun nouveau mécanisme d’isolation n’est revendiqué pour ELF ou MicroPython dans cet espace mémoire partagé.

## Vérifications et mesures disponibles

Les rapports détaillés se trouvent dans `tools/verification/*0849.json`, les preuves SDK et comparaisons dans `tools/audit-0849/`. Les tests utilisent les sources réellement modifiées et enregistrent leurs empreintes ; les rapports historiques ne sont pas des tests de cette version.

- Allocateur : vrai LVGL 9.6, objets/labels réels, realloc sur place/déplacé/échoué, restitution des segments et accès concurrents. Exemple hôte : 10 000 petites allocations demandent 19 segments globaux au lieu de 10 000 allocations individuelles ; 1 119 832 octets vivants pour 1 249 904 réservés. Ce ne sont pas des mesures de RAM au repos sur Tab5.
- Admission : comparaison avec malloc/memalign TLSF natif, non-mutation du tas, fragmentation et récupération du gestionnaire réel.
- Cache : comparaison différentielle avec la fonction native SDK 5.5.4, erreurs, alignements, flags, contexte ISR/critique, ordre et couverture des plages. Le HAL est instrumenté sur hôte ; aucun temps DMA physique n’est déduit.
- Audio : vrais worker M5 et pilote commun avec frontières RTOS/I²S contrôlées, arrêt pendant une écriture, contention release, erreurs de démarrage et cycles de clients. Absence de résidu numérique testée ; bruit analogique non mesuré.
- Vidéo/radio : cycles de fin/fermeture/reprise et mémoire fragmentée, aucune allocation ou connexion suivie restante aux fins testées. Les catalogues et états volontairement conservés sont distingués.
- Non-régression : accès fichiers, écritures temporaires/remplacement, récupération, partage radio/cartes et affichage des catégories mémoire. ASan/UBSan actifs sur les bancs concernés ; LSan indisponible dans l’environnement, limites précisées dans chaque rapport.

Compilation finale complète réussie : ESP32-P4, SDK M5Stack 3.3.9 / ESP-IDF 5.5.4, GCC RISC-V 14.2, LVGL 9.6.0. 1 425 objets liés, 2 035 fichiers sources contrôlés, 1 923 copies Arduino contrôlées ; sources inchangées pendant la passe. Les objets invariants sont réutilisables ; LVGL et M5Unified ont été reconstruits. Les chemins TLSF/admission, cache, DSI et stopAndWait sont vérifiés dans l’ELF réellement produit. Les 14 packs de langues conservent leurs entrées et intègrent le format de diagnostic ajouté.

| Mesure en octets | 0.8.48 livrée | 0.8.49 | Évolution |
|---|---:|---:|---:|
| Image application OTA | 6 084 512 | 6 093 264 | +8 752 |
| Capacité partition OTA | 6 553 600 | 6 553 600 | inchangée |
| Marge OTA | 469 088 | 460 336 | −8 752 |
| Données statiques + BSS | 93 848 | 93 968 | +120 |
| Code IRAM | 98 538 | 98 882 | +344 |
| Code / données TCM | 794 / 64 | 794 / 64 | inchangés |
| RTC noinit diagnostic | 1 200 | 1 200 | inchangé |

Les gains recherchés portent sur les sections critiques et la restitution des allocations, pas sur une baisse artificielle du code statique. Le contrôle TLSF LVGL cible utilise 2 528 octets PSRAM ; les segments sont demandés selon le besoin. La table de partitions est identique à la référence.

SHA-256 de l’image application : `f343ab0b29f3dc1e7395fe75315612ea9c9fee6d98cc6971dea6fb27f2629de9`.

Aucun firmware fusionné de 16 Mio n’est recommandé pour cette mise à jour : le binaire livré séparément est l’image application, afin de conserver le partitionnement et les données.

La taille des données statiques du lien, les octets de heap libres et la capacité physique de la puce sont des mesures différentes. Le plafond RAM affiché par Arduino ne représente pas à lui seul SRAM + TCM/SPM + LP RAM + PSRAM. Le diagnostic `m` expose les régions réellement enregistrées par ce SDK et leurs intersections ; le DMA est une capacité de certains octets existants, pas une mémoire à additionner. Aucun gain de RAM libre au démarrage sur appareil n’est inventé à partir de la seule taille du binaire.

## Limites et problèmes restant à investiguer

| Criticité | Point résiduel | Vérification / prochaine action |
|---|---|---|
| Haute | Flashs physiques et bruit au premier démarrage à froid non certifiés | Même scénario/fichier que 0.8.48, compteurs DSI et chronologie UI ; mesure de la séquence codec/amplificateur si le bruit persiste |
| Haute | Les réallocations non-LVGL utilisant directement le SDK peuvent encore copier sous verrou critique | Profiler les opérations concernées ; ne pas remplacer partout realloc par malloc/copie/free sans analyser le pic mémoire et la croissance sur place |
| Moyenne | Certaines premières lectures de catalogues et opérations UI restent synchrones ; récupération UI pouvant attendre une pile FreeRTOS | Mesurer durée des callbacks ; évolution ciblée en états différés si encore pénalisante, sans masquer par de nouveaux délais |
| Moyenne | Segments LVGL partiellement utilisés conservent de la capacité libre interne au pool | Comparer backing/used/pools via diagnostic ; ne pas déplacer les pointeurs LVGL vivants pour compacter |
| Haute | Coexistence réseau/SD, vidéo réelle, Bluetooth et USB imprimante sans essai matériel dans cet environnement | Procédure `TESTS_TAB5_0.8.49.md` puis périphériques physiques ; aucune promesse de validation RF/SD/USB |
| Moyenne | Adaptateur d’admission lié à une ABI privée du SDK | Hashes/configuration vérifiés, repli public pour tout autre environnement ; retester lors d’une mise à jour SDK |

Le coffre, le chiffrement et toutes les applications tierces n’ont pas fait l’objet d’une nouvelle certification exhaustive. Aucune donnée sensible n’a été réinitialisée pour contourner une erreur. La fermeture d’une application native ne constitue pas une barrière de sécurité matérielle.

## Avertissements de compilation

La passe GCC ciblée sur neuf unités, avec `-w` retiré et les erreurs d’initialisation, de retour et d’usage après libération activées, réussit. Elle conserve **320 avertissements**, dont 316 dans le fichier principal. La comparaison avec les sources 0.8.48 authentifiées retrouve les mêmes catégories et comptes ; les deux nouvelles unités sans historique n’émettent aucun avertissement. Les avertissements existants restent visibles dans `tools/verification/build-0849/compiler-warning-pass.json` et `compiler-warning-comparison.json`. Une compilation réussie n’est donc pas présentée comme une compilation sans avertissement.
