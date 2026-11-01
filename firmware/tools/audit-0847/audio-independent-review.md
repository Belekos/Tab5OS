# Revue indépendante de la séquence audio 0.8.47

Relecture du vrai pilote `Tab5AudioDriver.cpp`, du patch isolé
`tools/build_linux/prepare_m5unified_audio.py`, de M5Unified 0.2.23 et des fonctions
I2S d'ESP-IDF 5.5.4. Les tests instrumentés de l'auteur sont décrits dans
`tools/verification/audio-startup-0847.json`. Cette revue n'est pas un essai
acoustique et ne conclut pas à la disparition matérielle du claquement.

## Séquence et propriété

- Le patch est optionnel dans M5Unified ; le pilote Tab5 l'active explicitement.
  Les sources originales de la bibliothèque sont vérifiées par empreinte puis
  copiées dans la dépendance isolée de compilation. L'installation originale
  n'est pas modifiée.
- Avant la préparation I2S, le callback désactive l'amplificateur et applique
  le mute. Après configuration, I2S est démarré et reçoit des zéros. Le codec
  n'est réactivé qu'après réussite de ces opérations. Les erreurs de setup,
  prime, codec et création de tâche sont propagées ; l'arrêt et la libération
  ne dépendent pas de l'existence d'un worker réussi.
- Le reset/configuration ES8388 s'effectue avec des horloges établies et
  l'amplificateur désactivé. Le mute est appliqué avant l'alimentation des sorties
  DAC. Le démutage puis l'activation de l'amplificateur suivent la réussite de
  toutes les écritures de configuration.
- La fermeture conserve le callback de désactivation avant l'arrêt du worker
  et d'I2S. Le pilote joint le lecteur M5Unified avant la libération des buffers
  PCM. La pause d'inactivité du worker garde les horloges sur le chemin codec
  externe Tab5 : l'arrêt I2S de cette branche est réservé au DAC interne.
- Les changements de fréquence source restent des métadonnées de `playRaw` ;
  la sortie reste à 48 kHz avec rééchantillonnage M5Unified. Ce comportement
  existe déjà dans le ZIP 0.8.46 authentifié : il est conservé et testé, pas
  ajouté par la présente correction. Les mesures avant/après ne montrent
  aucun redémarrage I2S sur changement de fréquence. Le commentaire historique
  du pilote ne prouve pas un redémarrage 44,1 kHz dans la livraison 0.8.46.

## Taille et progression DMA

La revue a demandé de ne pas déduire la taille DMA du seul produit de paramètres
utilisateur. Dans `i2s_std.c`, le nombre de slots actifs vaut un en mono et deux
en stéréo ; `i2s_get_buf_size` peut ensuite aligner le nombre de frames pour le
cache ou limiter la taille d'un descripteur. Le patch utilise maintenant
`i2s_channel_get_info().total_dma_buf_size`, disponible dans l'état READY, et
ajoute un descripteur réel pour la phase de zéros.

Dans le SDK lu, `total_dma_buf_size` est exactement `desc_num * buf_size`.
Les allocations DMA sont initialement à zéro, avec synchronisation de cache
lorsque nécessaire. `i2s_channel_write` obtient ses nouveaux buffers à travers
la file de descripteurs terminés. Le prime suit donc la progression native des
transferts et les écritures partielles, sans sommeil ajouté pour simuler une
stabilisation. La preuve reste une preuve du chemin logiciel et des API : elle
ne mesure pas les niveaux analogiques ou les broches MCLK/BCLK sur une Tab5.

## Échec de sortie et attente native

La revue a identifié un chemin réel de boucle non bornée dans `writePcm` :
buffer plein, échec de redémarrage du speaker, index toujours plein, puis nouvelle
tentative immédiate. Le code corrigé mémorise l'échec de sortie pour le flux
courant, le rend observable aux clients et le réinitialise lors d'une nouvelle
prise en charge. Le buffer n'est pas présenté comme envoyé après un refus.

La sémantique de `playRaw` a été vérifiée dans le vrai M5Unified 0.2.23 : un canal
occupé donne `-1` à l'intérieur de `_play_raw`, qui attend le sémaphore natif avant
de réessayer. Le retour `false` externe n'est donc pas la pression ordinaire de
la file pour les appels présents (canal explicite, requête finie, hors worker
speaker). La boucle externe de 250 essais était redondante et a été retirée.
Le test du pilote exécute cette fonction native avec 500 attentes à 4 kHz avant
acceptation, puis avec un refus réel : la première situation réussit sans
état d'erreur, la seconde est signalée sans boucle de redémarrage.
Les résultats et empreintes figurent dans `tools/verification/audio-driver-0847.json`.

## Limites de validation

Une indisponibilité I2C persistante ne permet pas de garantir électriquement
qu'une commande de mute a été reçue ; le code doit signaler l'échec, ce qu'il
fait. La stabilité analogique, le volume de sortie, les transitions casque et
les éventuels artefacts dus à la suspension globale des caches demandent des
essais sur appareil. Une absence de redémarrage I2S dans les tests n'est pas une
mesure acoustique.

Sources SDK :
[i2s_std.c](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_driver_i2s/i2s_std.c),
[i2s_common.c](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_driver_i2s/i2s_common.c).
