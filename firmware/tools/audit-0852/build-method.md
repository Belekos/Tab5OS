# Compilation et vérification 0.8.52

La référence est l’archive finale 0.8.51 livrée, contrôlée par SHA-256
`66c290a307a184856e02dfefa0af348c13210b6ea2378606a97d95612e033bf7`.
Les rapports 0.8.51 et antérieurs restent historiques.

La chaîne est reconstruite dans un dossier isolé avec Arduino CLI 1.5.1,
M5Stack Arduino 3.3.9 / ESP-IDF 5.5.4, GCC RISC-V 14.2.0, M5Unified 0.2.23,
M5GFX 0.2.30 et LVGL 9.6.0. Les archives sont vérifiées contre le verrou
fourni. Les adaptations de dépendances restent celles de la version livrée ;
les bibliothèques du SDK installé ne sont pas modifiées.

La cible conserve ESP32-P4 prev3, PSRAM, Flash DIO et les partitions.
Un manifeste avant/après interdit de certifier une compilation pendant
laquelle une source aurait changé. La vérification porte sur le firmware
lié, l’ELF RISC-V, les copies Arduino, l’image P4, les limites OTA et la
provenance effective des unités audio, DSI, allocation et synchronisation
de cache. Les sections statiques ne mesurent pas le tas disponible au repos.

Les tests sur ordinateur ne remplacent pas une souris USB réelle, le rendu
sur le panneau Tab5 ni les contraintes temporelles PSRAM/DSI. Aucun essai
matériel n’est annoncé. Le BIN final, lorsqu’il est produit et validé, est
applicatif OTA et ne doit pas être écrit à l’adresse zéro comme une image
fusionnée.
