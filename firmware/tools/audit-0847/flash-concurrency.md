# Revue indépendante : Flash, cache et concurrence — 0.8.47

## Portée et référence

Revue de la livraison 0.8.46 authentifiée (ZIP SHA-256 `22d99628cc3ab6a9673be1886755ec9a3092aaede3f017a7e0e20443cc0816a6`), du SDK M5Stack 3.3.9 ESP32-P4 ES restauré et du code ESP-IDF 5.5.4 correspondant. Aucun essai physique. Les constats ci-dessous décrivent la référence avant la refonte des modules Média et affichage ; les résultats de ces corrections sont consignés séparément.

## Ce qui est établi

1. `Tab5MediaSession::service()` est appelé par `loop()` avant `lv_timer_handler()`. Dans la référence il exécute directement les transactions NVS après 1,2 s sans modification. Un changement d'onglet, de sélection, de recherche ou de défilement rend l'état navigateur à enregistrer. Le record contient les quatre vues, leurs textes et positions (~3,5 Kio). Une simple écriture n'est pas un travail constant : les pages NVS peuvent nécessiter une collecte/effacement.
2. Le contrôle de hash évite déjà une écriture lorsque l'état redevient identique. Aucun enregistrement NVS systématique à chaque passage de boucle n'a été trouvé. Un échec Média est repris après 5 s tant que l'état reste à enregistrer. La migration initiale du volume peut aussi produire une écriture sans interaction immédiate.
3. Le SDK effectif active la PSRAM à 200 MHz et un cache L2 de 256 Kio, lignes de 128 octets. `CONFIG_SPIRAM_XIP_FROM_PSRAM` et `CONFIG_SPI_FLASH_AUTO_SUSPEND` ne sont pas activés. Il ne suffit pas de définir ces macros dans le sketch : les composants SDK sont précompilés.
4. Le code `spi_flash_disable_interrupts_caches_and_other_cpu()` d'IDF 5.5.4 suspend l'ordonnancement, les interruptions non compatibles cache désactivé, puis le cache externe. L'autre cœur attend par IPC. Une tâche NVS sur le cœur 0 ne permet donc pas au cœur 1 de continuer librement pendant les transactions Flash protégées. Elle reste utile pour retirer le travail NVS synchrone du callback UI et coalescer les demandes.
5. Ce même code vérifie que la pile courante est interne. Le worker de persistance 0.8.47 utilise une pile interne ; son snapshot volumineux reste en PSRAM. Cette séparation conserve la contrainte du pilote Flash et ne prétend pas supprimer ses suspensions de cache.
6. Le pilote LCD continue de balayer le framebuffer entre les rendus LVGL. Il peut donc produire une erreur de sous-alimentation DMA au repos visuel. En revanche, l'absence d'activité LVGL n'établit ni un défaut du pilote, ni une faute NVS.

## Une fausse piste éliminée

Le callback `mipi_dsi_dma_trans_done_cb` n'a pas de marqueur `IRAM_ATTR` visible dans le fichier C du pilote. Cela ne signifie pas qu'il s'exécute depuis la Flash : le `ld/sections.ld` du SDK place explicitement ce callback, le gestionnaire d'interruption du pont DSI et les cinq fonctions DW-GDMA de la chaîne en IRAM. Le SDK active également `CONFIG_LCD_DSI_ISR_HANDLER_IN_IRAM` et `CONFIG_LCD_DSI_OBJ_FORCE_INTERNAL`. Il serait incorrect d'ajouter arbitrairement un correctif d'attribut sur ce seul constat.

Le script `verify_flash_context.py` contrôle la configuration et ces placements ; son option `--elf` vérifie aussi les sections réelles du firmware lié. Le JSON précise séparément si cette dernière vérification a été exécutée. Ce contrôle statique n'émule pas les interruptions d'une Tab5.

## Ce qui n'est pas prouvé

- La suspension du cache CPU n'est pas une preuve que le DMA DSI cesse de lire la PSRAM. Le DMA emprunte son propre chemin ; une assertion causale exigerait les compteurs et l'instant du défaut sur appareil.
- Déplacer NVS vers un worker n'est pas une preuve de disparition des flashs turquoise ni des claquements audio.
- Les écritures Média ne peuvent expliquer à elles seules tous les flashs au repos prolongé : l'état normal devient propre, puis n'écrit plus.
- L'augmentation existante de priorité AXI du lecteur DW-GDMA est un réglage global déjà présent dans la référence, pas une preuve de stabilité ni un motif pour ajouter d'autres priorités arbitraires.

## Autres chemins examinés

| Chemin | Déclenchement dans la référence | Conclusion |
|---|---|---|
| Réveil NVS | Modification explicite, transition de programmation, démarrage après veille | Pas d'écriture à chaque sondage de 250 ms |
| Mesh NVS | Révision des préférences modifiée, worker d'I/O existant | Pas de périodicité d'écriture indépendante des changements |
| Série NVS | Configuration explicitement demandée, worker existant | Pile interne et résultat d'écriture nécessaires |
| Identifiants Wi-Fi NVS | Connexion devant mémoriser, option auto, oubli explicite | Aucun journal périodique NVS trouvé |
| Carte NVS | Style, réglage de débit, mémorisation de lieux/options | Écritures liées aux actions, encore synchrones selon le chemin |
| Réglages système | Fichier microSD après changement, reprise 800 ms si échec | Distinct de NVS ; peut bloquer l'UI si stockage lent |
| Progression de lecture/document | microSD, choix et politique existants | Ne pas supprimer ni forcer des sauvegardes pour masquer le coût |
| Coffre/clés | Opération explicite avec vérification et résultat | Ne pas changer en écriture différée sans modifier les garanties de succès |

La revue des workers Audio, Cartes et Série ne révèle pas d'appel direct LVGL dans leur moteur. Les services UI des modules traitent leurs résultats dans la boucle graphique. Cela n'est pas une certification exhaustive de chaque callback ELF/MicroPython ; les programmes ELF natifs restent sans isolation matérielle.

## Conséquences pour la refonte

La persistance Média peut être retirée du chemin UI avec un instantané immuable, une seule demande en vol, le résultat de la révision effectivement écrite, des reprises bornées et une pile interne. Il faut conserver les paramètres existants et ne jamais annoncer l'enregistrement d'une révision plus récente que celle réellement écrite. Une fermeture ou un nouveau `begin()` ne doit pas détruire un instantané encore utilisé.

Une instrumentation à la demande peut rapprocher début/fin et durée des transactions NVS des compteurs de sous-alimentation DSI, sans écriture de journal en Flash. Cela permet de distinguer une latence UI d'un défaut de balayage. Modifier XIP ou l'auto-suspend exigerait une reconstruction cohérente du SDK et une validation des modèles de Flash, des révisions P4, d'ESP-Hosted, du chargeur ELF et des buffers exécutables ; ce n'est pas un correctif par macro locale.

## Sources primaires consultées

- ESP-IDF 5.5.4 : [cache_utils.c](https://github.com/espressif/esp-idf/blob/v5.5.4/components/spi_flash/cache_utils.c).
- ESP-IDF 5.5.4 : [contraintes Flash](https://github.com/espressif/esp-idf/blob/v5.5.4/docs/en/api-reference/peripherals/spi_flash/spi_flash_concurrency.rst).
- [Guide ESP32-P4 des accès Flash concurrents](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/spi_flash/spi_flash_concurrency.html), utilisé pour orientation ; les conclusions de configuration reposent sur les fichiers SDK épinglés, pas sur la documentation stable courante.
- SDK restauré : `dio_qspi/include/sdkconfig.h`, `ld/sections.ld`, `lib/libesp_lcd.a` ; empreintes dans `tools/verification/flash-context-0847.json`.
