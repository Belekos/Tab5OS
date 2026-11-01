# Reconstruction et validation 0.8.50

La référence récupérable est l’archive 0.8.49 livrée, SHA-256
`0a6928c6f494f076d359d1ede44a6609a3bf9978e7970b604469787a5c0ee861`.
Les premiers fichiers temporaires de travail 0.8.50 ont été perdus pendant
l’interruption. Cette livraison reconstruit les corrections sur la base .49
authentifiée ; seuls les tests réellement réexécutés dans cette reconstruction
sont utilisés comme preuves actuelles. Les rapports .49 restent historiques.

Chaîne isolée : Arduino CLI 1.5.1, M5Stack Arduino 3.3.9 / ESP-IDF 5.5.4,
GCC RISC-V 14.2.0, M5Unified 0.2.23, M5GFX 0.2.30 et LVGL 9.6.0.
Les 23 archives de dépendances et Arduino CLI sont contrôlés par les SHA-256
épinglés. Les copies adaptées de LVGL, M5GFX, M5Unified et FS, les deux unités
natives DSI/DW ainsi que les adaptateurs mémoire/cache restent ceux de .49.
Le SDK téléchargé n’est pas modifié.

La cible conserve `ChipVariant=prev3`, PSRAM activée, Flash DIO et les
partitions existantes. Le fichier BIN séparé est applicatif OTA, pas une image
fusionnée à écrire à l’adresse zéro.

Une compilation exploratoire a détecté un conflit `int32_t`/`int` dans le
nouveau rendu de texte sur la cible RISC-V. Le type a été rendu explicite,
puis les sources gelées pour la compilation finale. Un manifeste modifié
pendant une compilation refuse sa certification, même si le compilateur
termine sans erreur.

La vérification finale porte sur l’ELF et les copies Arduino réels : image
ESP32-P4, intégrité et SHA ELF embarqué, limites OTA, provenance des unités
audio/DSI, mémoire interne des interruptions, lien effectif des wrappers de
cache et d’allocation, configuration TLSF LVGL et adaptations FS. Le simple
fait de fournir les options de compilation n’établit pas leur utilisation.

Les tests LVGL sur ordinateur utilisent la bibliothèque 9.6.0 réelle et,
pour Dessin, l’adaptateur TLSF natif .49. Les frontières ESP/RTC/NVS/audio
simulées sont indiquées dans les rapports concernés. Ces essais ne mesurent
ni le son analogique, ni les bus, ni la latence tactile d’une Tab5 physique.

La publication vérifie les empreintes de sources et de tests, les ressources
SD/Factory, les quatorze langues et les mécanismes stabilisés conservés.
L’archive est contrôlée par CRC, extraite et revérifiée avant livraison.
