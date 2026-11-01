# Audit technique et stabilisation — Tab5OS 0.8.42

## Portée et référence

La livraison est issue des sources réelles de l’archive 0.8.41, SHA-256 `01389322d10ae80b668ce369b2586e41831af03e3e407ecd3d60da62dd2667ff`. Cette référence récupérable, ses empreintes et les anciennes preuves sont conservées. Aucun firmware plus ancien supposé n’a servi de base. Le relevé initial se trouve dans [build-baseline.json](tools/audit-0842/build-baseline.json).

Les changements ciblent les blocages UI, la synchronisation, les allocations superflues et l’intégrité des fichiers/secrets. Aucun retrait d’application, réécriture générale, changement de partition, réinitialisation de clés, déplacement automatique de fichiers personnels ou autosauvegarde forcée n’est introduit. Les frameworks et pilotes adaptés sont réutilisés. Les noms de fichiers nouveaux restent anglais ; les quatorze langues et les styles natifs sont conservés.

Les résultats hôte établissent des propriétés du code. Ils ne mesurent pas le comportement du bus DSI, de la microSD, du C6, des périphériques USB ou de la PSRAM sous charge réelle. Aucun essai physique, temps de démarrage, pourcentage CPU, taux d’images ou disparition des flashs sur Tab5 n’est revendiqué.

## Architecture et exécution

| Ensemble | Rôle, contexte et synchronisation |
|---|---|
| Arduino/ESP-IDF/FreeRTOS | Tâche UI Arduino, priorité 1, pile 24 Kio ; services courts et traitement LVGL. Workers applicatifs existants pour stockage/réseau/interpréteurs. Pas de nouvel ordonnanceur. |
| Écran et entrées | LVGL 9.6, M5Unified 0.2.23/M5GFX 0.2.30, MIPI-DSI ; buffers PSRAM sous contraintes cache/DMA. USB/tactile publient les entrées ; objets LVGL créés et détruits sur UI. |
| Mémoire | `Tab5ResourceManager` réserve les besoins futurs et collecte les caches explicitement récupérables. `Tab5Memory` sépare capacités et diagnostics. PSRAM pour documents/images/caches compatibles ; piles internes et DMA restent distincts. |
| Stockage | `Tab5Storage` protège le cycle montage/récupération ; `Tab5VolumeFS` route SD/USB et suit générations/handles. Workers fichiers, copies bornées, temporaires et remplacements conservant le précédent. Un lease ne verrouille pas toutes les opérations interapplications sur un chemin. |
| Applications | Natives UI + workers ; VM MicroPython à la demande ; ELF natif dans l’espace mémoire du firmware. Publication de fin, annulation coopérative et destruction sur le propriétaire approprié. |
| Communications/multimédia | ESP-hosted P4/C6, réseau/TLS depuis workers, portes réseau/audio existantes. Piles radio/audio/vidéo et décodeurs dimensionnés par le code ; marges réelles restent à relever. |
| Paramètres et coffre | Paramètres validés avant publication ; NVS pour enveloppes clés/PIN, banques AES-GCM SD pour secrets. Lecture en erreur ne vaut pas donnée absente. Aucune clé ou valeur de secret ajoutée aux diagnostics. |

Le détail des tâches, piles, priorités, cœurs, allocations et destructions figure dans [memory-tasks.md](tools/audit-0842/memory-tasks.md). Le SDK exact est IDF 5.5.4, core M5Stack 3.3.9, GCC RISC-V 14.2.0 ; les versions et SHA des dépendances sont verrouillés. Les pilotes/variants matériels retenus sont ceux de la livraison précédente, sans restriction ajoutée d’après un numéro de version non reconnu.

## Anomalies confirmées et corrections

| Priorité | Cause établie dans le code | Correction et preuve |
|---|---|---|
| P0 — intégrité | Des `exists()` faux sur EIO étaient assimilés à une absence dans la récupération de fichiers/coffre ; certains chemins pouvaient alors effacer un précédent ou accepter une table vide. | Présence, absence ENOENT et erreur distinguées ; remplacement/récupération conditionnels, ancien fichier ou banque conservé en échec. Tests sur vraie crypto et injection de fautes. |
| P0 — intégrité | Le SDK Arduino appelle `fflush/fsync` sans retourner leurs erreurs dans `File::flush()` ni `getWriteError()`. | Contrôle de la frontière SDK au moment du flush dans les chemins corrigés ; tests EIO sans faux indicateur write-error. Un flush réussi n’est pas une garantie matérielle contre une coupure. |
| P1 — réactivité | La tâche UI attendait des événements de présentation DSI ; un `vTaskDelay` laisse FreeRTOS tourner mais ne traite pas les entrées de cette même tâche. | Présentation non bloquante, rafraîchissement différé pendant la propriété du framebuffer ; aucune réutilisation prématurée. Tests LVGL/SDK et compilation P4. |
| P1 — réactivité | Chargement ELF synchrone : lecture jusqu’à 4 Mio, validation/relocation et attentes dans le chemin UI. | Worker existant effectue la préparation, publication explicite, annulation et libération ordonnées ; erreurs affichées une seule fois. |
| P1 — concurrence | La vue ELF retenait le même verrou pendant une copie volumineuse et l’UI/les entrées attendaient sans limite. | Verrous séparés et acquisition non bloquante du service graphique ; publication reportée sans sacrifier les entrées. |
| P1 — réactivité | `stop()` vidéo attendait la fin du worker jusqu’à 5,5 s depuis UI ; détachement/finalisation avaient une fenêtre de concurrence. | Fermeture coopérative, dernière intention différée, nettoyage après fin publiée. Interleaving de détachement explicitement testé. |
| P1 — réactivité | L’état `KeyStore::unlocked()` consulté en boucle attendait le mutex retenu par NVS/crypto. | Publication bornée de l’état, sans attente de la crypto pour la consultation UI. Tests avec worker volontairement retenu. |
| P2 — mémoire/CPU | Accesseurs audio construisant six chaînes inutiles ; tentatives d’admission parcourant plusieurs tas alors que les octets libres étaient déjà insuffisants. | Lecture du seul champ requis ; vérifications contiguës limitées aux capacités demandées lorsque le budget en octets tient. Marges de sécurité conservées. |
| P2 — Flash | Quarante ressources de réparation en texte brut et seulement 243 232 octets libres dans OTA au départ. | Compression par zlib déjà liée, décompression bornée en PSRAM lors de la seule réparation explicite, relecture exacte avant commit. Aucun fichier SD retiré. |
| P2 — diagnostic | Peu de moyens pour attribuer le lag à un service précis sans collecte répétitive coûteuse. | Profilage UI désactivé par défaut, événements bornés RTC, relevés de mémoire et piles à la demande ; pas d’écriture permanente Flash/SD. |

Les correctifs sont détaillés dans [NOUVEAUTES_v0.8.42.md](NOUVEAUTES_v0.8.42.md). Les audits par module séparent le constat initial et la validation : [boucle UI](tools/audit-0842/core-ui.md), [affichage/entrées](tools/audit-0842/display.md), [mémoire/tâches](tools/audit-0842/memory-tasks.md), [stockage/sécurité](tools/audit-0842/storage-security.md), [applications](tools/audit-0842/apps-lifecycle.md), [réseau/média](tools/audit-0842/network-media.md), [ressources/Flash](tools/audit-0842/resources-build.md). Les preuves sont liées depuis [VERIFICATION.md](VERIFICATION.md).

## Mémoire : ce que signifient les chiffres

Les 768 Kio physiques de SRAM HP ne sont pas 768 Kio de tas libre. Cache L2, code IRAM, données fixes, réservations SDK et piles en consomment une partie. Les régions HP, SPM/TCM, LP et PSRAM doivent rester distinctes ; INTERNAL est un agrégat SDK et ne s’additionne pas à ses sous-régions. Le relevé utilisateur « 275/347 Kio » décrivait un tas exposé, pas toute la capacité physique. Les mesures exactes dépendent du SDK, de la configuration et des services réellement actifs.

Une application peut disposer de beaucoup de PSRAM libre et manquer d’un bloc interne contigu pour une pile, le Bluetooth, le réseau ou une allocation DMA. Les minima depuis démarrage, plus grands blocs, besoins et quotas expliquent ce refus mieux qu’un total unique. Les piles USB permanentes recensées représentent déjà 22 Kio, auxquelles s’ajoutent UI, SDK et workers actifs ; il s’agit d’un inventaire de tailles, pas d’une mesure réelle du tas au démarrage.

Les corrections réduisent du travail et des copies, mais le diagnostic/synchronisation ajoute aussi de petites données statiques. Aucun gain global de SRAM au repos n’est inventé. Le [bilan des ressources](BILAN_RESSOURCES_0.8.42.md) distingue tailles de lien, mesures hôte et observations matérielles indisponibles.

L’indication Application/Partition OTA compare la taille du firmware en Flash à la capacité du slot OTA. Elle ne mesure pas la RAM utilisée. Les deux partitions OTA de 6 553 600 octets restent en place ; aucune procédure de migration n’est nécessaire puisque la table est inchangée. Le BIN livré est applicatif OTA, pas une image fusionnée à écrire à zéro.

## Hypothèses et problèmes résiduels

| Criticité | Point restant | Méthode d’investigation |
|---|---|---|
| Haute, établie | Une file USB saturée conserve seulement le dernier rapport et peut perdre des transitions. | Réduire d’abord les longues phases UI identifiées ; toute correction producteur doit préserver ordre des boutons et chemins de dessin. Le regroupement du survol ne corrige pas cette limite. |
| Haute | Flashs turquoise et lag physique après corrections : cause électrique, sous-alimentation DSI, cache/DMA ou concurrence restent possibles. | Deux minutes idle puis souris/charge, compteurs DSI + invalidations + timings UI, révision écran/alimentation, journal du reset. Pas de reset périodique ajouté pour masquer le défaut. |
| Haute | Pénurie interne/fragmentation Bluetooth ou application lourde non reproduite sur appareil. | Échantillons par capacité, budget exact, plus grand bloc et tâches avant activation, après premier cycle puis vingt cycles. Ne pas réduire arbitrairement les piles. |
| Haute | Erreurs SD électriques, durabilité FAT et coupure pendant commit. | Carte de test sauvegardée, journaux errno/opération, remontage contrôlé existant ; mesure réelle SPI/alimentation si panne reproductible. Les erreurs logiques corrigées ne démontrent pas une carte matériellement fiable. |
| Haute | Code ELF natif non coopératif ou pointeur invalide. | Binaires de confiance et ABI fournie, journal/watchdog ; aucune isolation matérielle n’est annoncée. La PSRAM suivie n’empêche pas un accès sauvage. |
| Moyenne | Certaines opérations SD restent synchrones dans UI : ouvertures/cartes/catalogues, chargement de source Python, paramètres et actions coffre. | Profilage par phase et tailles réelles de données. Une migration supplémentaire vers worker exige propriété/annulation sûres et mesures. |
| Moyenne | Réseau C6/TLS, radio/vidéo/USB et Meshtastic non testés physiquement. | Scénarios connexion/interruption/fermeture, offline et coexistence. La compatibilité C6 inconnue n’est pas un refus automatique ; cette politique a été retestée. |
| Faible | Une traduction longue d’un bouton de diagnostic série peut rester serrée sur une largeur étroite. | Captures/géométrie 14 langues jointes ; ne pas attribuer ce détail préexistant au lag ou le masquer par retrait de traduction. |

La [procédure sur Tab5](TESTS_TAB5_0.8.42.md) précise les scénarios, mesures et journaux nécessaires. La compilation et les tests reproductibles disponibles sont les limites concrètes de la validation de cette livraison.
