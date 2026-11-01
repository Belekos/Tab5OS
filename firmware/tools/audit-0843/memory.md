# Mémoire et répartition du travail — 0.8.43

## Référence et limites

Référence : projet et ELF livrés en 0.8.42, dont l’archive est conservée intacte. Le relevé `memory.json` et le script `tools/memory-tests-0843/inventory.py` reproduisent les tailles à partir de l’ELF et de l’objet RISC-V réel. Aucun tas libre, défaut électrique, pourcentage CPU ou temps de réponse sur Tab5 physique n’est déduit de ces valeurs.

Le défaut de l’accueil est étudié séparément par les lots accueil/catalogue/affichage. Dans les modules mémoire, `tab5ResourcesService()` parcourt huit états de demande et lit les compteurs d’échec : il ne parcourt pas les tas, ne purge pas de caches et ne fait aucune allocation au repos sans demande. Il n’est donc pas une cause démontrée du lag périodique. Réduire les marges d’admission ou les piles sans mesures aurait seulement masqué une pénurie potentielle.

## Structure effectivement utilisée

| Ressource | Affectation actuelle | Décision dans ce lot |
|---|---|---|
| HP SRAM interne | Code IRAM, états des pilotes, contrôle RTOS, piles usuelles, petites synchronisations/atomiques | Garder l’interne pour ces contraintes ; aucune baisse arbitraire de pile ou de réserve |
| SPM/TCM | Banque distincte de 8 Kio, code/données configurés et tas éventuellement exposé | Ne pas l’additionner à HP/LP ou la présenter comme 8 Kio de PSRAM |
| LP SRAM | Banque de 32 Kio, réservations SDK et diagnostics RTC | Les deux journaux UI conservent leurs 216 octets et leur format ; pas de Flash/SD pour leur persistance |
| PSRAM, matériel 32 Mio | Documents, buffers d’images/tuiles, tables d’applications, VM et caches compatibles | Le nouveau compteur optionnel y est alloué seulement sur activation explicite |
| DMA | Capacité et contraintes de chaque périphérique, pas une banque supplémentaire | Aucun descripteur ou buffer DMA déplacé aveuglément en PSRAM |
| Agrégat INTERNAL | Tas internes HP + SPM/TCM + LP inscrits au SDK | Ne pas l’additionner à ses sous-régions ; lire aussi les plus grands blocs |

Le SDK configuré réserve 256 Kio de la SRAM HP pour le cache L2. Les 768 Kio physiques HP ne deviennent donc pas un tas libre de 768 Kio. Le reste comprend encore code, données statiques et réservations, puis piles/objets dynamiques. Les compteurs réellement exposés sur l’appareil restent la référence.

Le SDK possède un seuil initial `SPIRAM_MALLOC_ALWAYSINTERNAL=4096`, mais l’OS appelle déjà `heap_caps_malloc_extmem_enable(0)`. Les `malloc/new` ordinaires privilégient donc déjà la PSRAM, même pour les petits objets. Il serait inexact de présenter une nouvelle migration générale vers la PSRAM comme le correctif de cette version. Rétablir sans mesure un seuil élevé pourrait accélérer certains objets fréquemment accédés mais consommer beaucoup plus de SRAM et aggraver les refus Bluetooth. Ce lot ne modifie pas cette politique.

Les tables catalogue et staging sont déjà en PSRAM ; les icônes décodées ont un budget PSRAM borné. Les quelques tables de pointeurs/clefs de l’accueil totalisent environ 1 Kio interne ; elles ne suffisent pas à expliquer une surcharge de centaines de Kio. D’autres états permanents importants sont les contextes SDK/pilotes, la liseuse, les polices et Dessin. Une table statique ne représente pas une fuite ; une grosse PSRAM libre ne suffit pas à fournir une pile interne contiguë.

## Correction ciblée : diagnostic optionnel paresseux

En 0.8.42, le profilage UI était désactivé par défaut mais ses compteurs, timestamps et variables de boucle occupaient toujours **578 octets de symboles BSS internes**. Ces données ne sont ni utilisées par un ISR ni nécessaires à un pilote : elles sont de bons candidats à la PSRAM à la demande.

`Tab5UiDiagnostics` regroupe maintenant ses compteurs dans un seul état de **576 octets en PSRAM**, créé lors de la première activation. Il n’alloue rien au démarrage, ne crée ni tâche ni timer et ne possède aucun repli en SRAM. La désactivation conserve cet état et ses derniers compteurs pour consultation ; une nouvelle activation les remet à zéro sans réallocation. `begin()` libère cet état si la fonction est rappelée. Les journaux RTC restent indépendants, disponibles après certains resets même quand aucune allocation PSRAM n’est possible.

`enable(bool)` retourne maintenant un booléen. Si la PSRAM manque, il retourne faux, le profilage reste désactivé et le journal précédent n’est pas effacé. Les appelants qui ignoraient le retour restent compatibles ; l’intégration principale peut remettre son interrupteur dans l’état réel et afficher l’erreur existante. Les scopes gardent leur garde par génération, y compris si l’état est libéré et réalloué à la même adresse.

| Mesure réelle du module | 0.8.42 | 0.8.43 |
|---|---:|---:|
| Somme des symboles BSS internes | 578 octets | 9 octets |
| Journaux RTC retenus | 216 octets | 216 octets |
| État de compteurs PSRAM après première activation | 0 | 576 octets |
| Allocations PSRAM avant première activation | 0 | 0 |
| Plus grand cadre GCC du module | 224 octets | 224 octets |

La différence brute de symboles BSS est de **569 octets** ; ce n’est pas encore la variation globale du firmware ou du tas. Les alignements et les changements des autres modules doivent être mesurés dans l’ELF final. Le bilan de livraison présente ce résultat final séparément.

## Les processeurs et les cœurs

Le second cœur HP du P4 est déjà utilisé. Ce n’est pas la même chose que le C6 dédié aux communications. Le LP core du P4 n’est pas un troisième cœur HP exécutant les mêmes tâches FreeRTOS.

| Exécution | Charges relevées dans le code/configuration | Conséquence |
|---|---|---|
| P4 HP core 1, UI priorité 1 | Boucle Arduino/LVGL, entrées, publication UI ; worker cartes priorité 2 et producteur radio priorité 4 lorsqu’actifs | Déplacer une opération longue de l’UI peut améliorer sa réactivité, mais ne prouve pas une famine au repos |
| P4 HP core 0 | Audio priorité 5 et speaker 6, vidéo, ELF/Python, plusieurs workers fichiers et USB | Le cœur est utilisé ; un nouveau job catalogue basse priorité doit céder aux travaux audio/USB |
| ESP32-C6 | Wi-Fi/BLE et transport ESP-hosted avec firmware de communications existant | Ce n’est pas un processeur de calcul OS disponible sans nouveau firmware/protocole, hors de ce correctif |
| P4 LP core | Environnement distinct basse consommation | Sa mémoire, ses API et ses usages ne sont pas interchangeables avec un worker HP |

La compilation actuelle définit `ARDUINO_RUNNING_CORE=1` et `F_CPU=360000000L` ; le SDK donne aussi 360 MHz par défaut. Cela décrit la configuration, pas une mesure de fréquence matérielle ou une limitation physique nouvellement imposée.

Pour le catalogue, le lot dédié utilise un worker ponctuel sur core 0 et publie ses résultats sur UI. Son budget prévu ajoute transitoirement une pile interne de 8 Kio, avec les marges d’admission existantes, et réutilise le staging PSRAM. Une admission non récupérante est préférable pour ce travail optionnel : elle évite une attente UI de récupération de mémoire avant de revenir au scan incrémental existant. Le lot catalogue valide ce contrat et sa propriété ; les high-water marks sur tablette restent nécessaires pour conclure sur la pile.

Les deux cœurs P4 partagent les contrôleurs mémoire et périphériques. L’offload ne transforme pas la PSRAM en mémoire privée, ne supprime pas une saturation du bus DSI et n’isole pas un ELF natif. Il évite d’exécuter des E/S longues depuis LVGL quand le contrat de publication est maîtrisé.

## Validation exécutée

- **1 011 vérifications ASan/UBSan sur les vraies sources UI diagnostics**, seules horloge/reset/allocateur et attribution d’affichage étant simulés. Couvre deux refus d’allocation sans repli, nouvelle tentative, absence d’allocation au démarrage, conservation après désactivation, libération/reset, scope ancien, seuil 20 ms, limite par phase, ring huit éléments, UTF-8 et resets/checksums.
- Compilation réelle de `Tab5UiDiagnostics.cpp` avec le SDK P4 M5Stack 3.3.9 / IDF 5.5.4 / GCC RISC-V 14.2.0 : **PASS**, sans avertissement. Cadre GCC maximal 224 octets ; cela ne mesure pas la pile cumulée.
- Comparaison des symboles de l’ELF 0.8.42 et de l’objet P4 courant, empreintes et script joints. Le rapport du build final confirme les effets de lien et la présence du code dans le firmware.

Preuves : `tools/verification/ui-diagnostics-0843.json`, `ui-diagnostics-target-0843.json`, `tools/audit-0843/memory.json`. LeakSanitizer est indisponible sous cet environnement ; les allocations du test sont suivies explicitement et toutes les allocations réussies sont rendues en fin de test.

## Mesures restant sur Tab5

Comparer accueil inactif puis souris, avec et sans profilage, et menus ELF/Python en scan. Relever `m`, `d`, `tasks`, les compteurs DSI et l’application active. Pour les piles, distinguer présence du worker, pile allouée et marge minimale réellement constatée. Pour un refus Bluetooth, relever capacités et plus grand bloc interne avant l’activation ; ne pas conclure sur la seule PSRAM. Aucun changement d’affinité générale, de taille de pile ou de firmware C6 n’a été fait sans preuve.
