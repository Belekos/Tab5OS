# DSI / accès Flash — correction 0.8.48

## Défaut établi dans la configuration livrée

Le SDK M5Stack 3.3.9 / ESP-IDF 5.5.4 active `CONFIG_LCD_DSI_ISR_HANDLER_IN_IRAM` et `CONFIG_LCD_DSI_OBJ_FORCE_INTERNAL`, mais pas `CONFIG_LCD_DSI_ISR_CACHE_SAFE`, `CONFIG_DW_GDMA_ISR_IRAM_SAFE` ni `CONFIG_DW_GDMA_OBJ_DRAM_SAFE`.

Le placement des fonctions en IRAM n'est donc pas suffisant : les interruptions DSI et DW-GDMA ne sont pas enregistrées avec `ESP_INTR_FLAG_IRAM`. Le système peut les masquer pendant une opération Flash. Or `mipi_dsi_dma_trans_done_cb` doit réarmer la liste DMA après chaque image. Arrêter de modifier LVGL ne supprime pas cette dépendance : l'écran continue de lire son framebuffer PSRAM au repos.

Deuxième condition : les structures natives DW-GDMA (groupe, canal et enveloppe de liste) étaient allouées avec `MALLOC_CAP_DEFAULT`. La politique globale du projet `heap_caps_malloc_extmem_enable(0)` permet leur placement en PSRAM. Promouvoir uniquement l'interruption à IRAM aurait donc laissé des pointeurs inaccessibles pendant l'arrêt du cache.

Ces défauts sont établis dans le code et la configuration. Leur part exacte dans les flashs observés sur chaque Tab5 exige les essais physiques. Cela ne prouve pas que chaque flash a la même origine.

## Modification

`tools/build_linux/prepare_dsi_cache_safe.py` reconstruit deux unités natives exactes de la version 5.5.4 : `esp_lcd_panel_dpi.c` et `dw_gdma.c`. Les sources d'origine et leurs empreintes sont fournies dans `dsi_cache_safe/upstream` et `UPSTREAM.json`. Le SDK installé n'est pas modifié.

- Options natives cache-safe activées dans ces deux unités ; contextes DW-GDMA explicitement en mémoire interne.
- Les objets bus et le placement ISR déjà internes sont une précondition vérifiée, car le bus et les scripts de liaison ne sont pas reconstruits.
- Les noms `libesp_lcd.a(esp_lcd_panel_dpi.o)` et `libesp_hw_support.a(dw_gdma.o)` préservent les règles de placement du linker SDK. Les archives isolées sont liées avant les archives précompilées. Le binaire final doit être contrôlé.
- Les chemins de garde des quatre fonctions DW appelées depuis le callback DSI conservent leurs codes d'erreur et leurs diagnostics, avec formats et tag en DRAM ; pas de chaîne `__FUNCTION__` en Flash.
- Le log natif de sous-alimentation de l'écran est remplacé par un appel C fort `tab5_display_underrun_isr`. Ce hook ne fait ni allocation, ni accès fichier, ni I/O série, ni manipulation LVGL. Il enregistre un compteur et l'horodatage en mémoire interne. Son absence fait échouer la liaison.
- Le service UI lit le delta par `tab5DisplayTakeUnderrunEvent`. Premier événement immédiat, puis au plus un rapport par seconde contenant le nombre cumulé de nouveaux événements. L'agrégation ne retarde pas le traitement de l'écran et ne perd pas les comptes.
- L'observateur facultatif d'interruption conserve le statut, le nombre d'appels et les flags réellement passés à l'allocateur, mais ne recompte pas les sous-alimentations.

Aucun changement d'horloge écran, résolution, taille DMA, framebuffer, partition, données utilisateur ou politique d'enregistrement. Aucun tampon plein écran supplémentaire : une image RGB565 de 720 × 1280 vaut 1 843 200 octets et ne tient pas dans la SRAM interne disponible. Un tampon SRAM intermédiaire sans consommateur DMA adapté ajouterait des copies plutôt que résoudre le masquage des interruptions.

## Vérifications réellement exécutées

- Compilation GCC RISC-V des deux unités natives contre le SDK M5Stack 3.3.9 : zéro erreur, zéro avertissement. Journal détaillé dans l'environnement de build ; la compilation complète est comptabilisée séparément.
- Test hôte de la totalité de `dw_gdma.c`, avec types publics SDK réels et frontières HAL/allocateur/IRQ contrôlées : 200 cycles création/utilisation/destruction pour chaque version, plus erreurs d'allocation et d'IRQ. Sous AddressSanitizer et UndefinedBehaviorSanitizer.
- Modèle cache-off : 12 000 événements masqués et zéro callback dans l'original ; 12 000 callbacks et zéro événement masqué avec les unités corrigées. Ce sont des événements simulés, pas une mesure de fréquence ou de latence sur Tab5.
- Zéro bloc suivi et zéro handle d'interruption restant après chaque cycle et scénario d'erreur. LSan est désactivé, car le runtime sous ptrace ne permet pas son exécution ; les allocations natives sont suivies explicitement.
- Tests du code réel Signals : 3 034 assertions avec observation activée et 3 033 sans wrapper. Échec d'installation, transmission exacte de l'IRQ au pilote, compteur indépendant de l'observateur, agrégation de 2 000 événements, et débordement de `millis()`.
- Empreintes des sources et résultats : `tools/verification/dsi-native-0848.json`, `display-signals-0848.json`.

## Limites et essai sur Tab5

La simulation ne reproduit ni le débit PSRAM réel, ni la contention DMA, ni les signaux électriques. Le code natif IRQ peut désormais fonctionner pendant les accès Flash ; cela ne garantit pas une bande passante suffisante si plusieurs maîtres saturent la PSRAM.

Après la compilation complète et le contrôle du binaire, relever les diagnostics `[DSI]` et les événements de sous-alimentation pendant : repos, changement répété des menus Media, premier trait de Dessin, puis vidéo. Conserver les compteurs avant/après et les heures exactes d'un flash. Un flash sans incrément du compteur reste à investiguer : l'absence d'événement natif ne prouve pas l'absence de tout défaut de transfert ou de panneau.

Source primaire de l'API : https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html ; sources natives et licences livrées pour reproductibilité.
