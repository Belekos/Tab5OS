# Audio 0.8.47 — séquence matérielle de démarrage

Le retour utilisateur invalide toute conclusion antérieure de disparition du claquement. La présente correction traite une erreur de séquence établie dans le code livré, pas un bruit enregistré sur une Tab5 physique.

## Cause établie

Dans M5Unified 0.2.23 réellement utilisé, `Speaker_Class::_begin_raw` appelle le callback matériel `enabled=true` **avant** `_setup_i2s` et la création du worker. Le callback Tab5 réinitialise l’ES8388, active ses sorties, enlève le mute puis active l’amplificateur. MCLK/BCLK et les transferts DMA ne sont pas encore établis. La correction 0.8.45 du changement de fréquence source avait supprimé un redémarrage supplémentaire ; elle n’avait pas corrigé cette première séquence.

La source exacte `Speaker_Class.inl` a pour SHA-256 `1229920ffe04ccef8f59c37d2dc848a5d34dfd5d5424639bc46c13102eb6fc17`, identique à celle des preuves antérieures. Le nom d’extension est bien `.inl`, inclus par `M5Unified.cpp`.

## Modification

Le correctif est préparé dans une **copie isolée** de M5Unified, contrôlée par empreintes avant modification. Le projet et les commandes de compilation sélectionnent explicitement cette copie. La bibliothèque installée d’origine n’est pas écrasée.

Un réglage optionnel `enable_after_i2s` est activé uniquement par le pilote Tab5OS sur P4. La séquence devient :

1. Amplificateur désactivé et codec mis en sourdine.
2. Configuration I2S à la fréquence fixe existante ; interrogation de la taille DMA réellement allouée par ESP-IDF.
3. Activation des horloges et transfert de zéros correspondant à un tour complet des descripteurs, plus un descripteur. Les écritures attendent les buffers DMA natifs ; ce n’est pas un `delay` d’attente estimée.
4. Initialisation du codec avec mute placé **avant** l’activation des sorties DAC ; démutage et activation de l’amplificateur seulement après réussite de la configuration.
5. Création du worker Speaker et publication du succès.

Aucun échantillon utilisateur n’est utilisé pour le préchargement ; aucun début de morceau n’est supprimé et aucun fondu logiciel n’est ajouté. La fréquence de sortie, le rééchantillonnage, le volume et le routage casque existants restent disponibles. Les erreurs de configuration, d’information DMA, d’activation, d’écriture, d’I²C et de création de tâche empêchent l’annonce de réussite et déclenchent la fermeture du matériel partiellement initialisé. L’échec de création du sémaphore est désormais traité avant tout démarrage matériel, avec libération du verrou de démarrage.

Le worker conserve les horloges lorsqu’un codec **externe** attend des données. Sa branche d’arrêt I2S au repos concerne exclusivement `use_dac` (DAC interne), qui est faux pour l’ES8388. Une attente HTTP après `claim()` n’annule donc pas la séquence sécurisée. Lors d’un arrêt complet, le callback mute/amplificateur est exécuté avant l’arrêt du worker et des horloges.

Le pilote n’effectue plus un deuxième `begin()` indistinct après un échec, ni deux `Serial.flush()` sur le chemin de démarrage. Dans la configuration livrée, le repli réappliquait exactement les mêmes DMA 256×8 déjà sélectionnés : il ne constituait pas une réduction mémoire. L’ancienne option de compilation inactive `TAB5_SPEAKER_LONG_DMA=1` n’est pas une configuration validée de cette livraison.

## Propagation des erreurs et propriété des ressources

Une seconde anomalie confirmée concernait `writePcm` : lorsque le bloc PCM était plein et que le démarrage matériel échouait, la boucle de consommation retrouvait le même bloc plein et tentait indéfiniment le même démarrage. Le pilote mémorise désormais l’échec jusqu’à une nouvelle acquisition explicite de la sortie et rend un échec au producteur. Le lecteur distingue cette panne d’une fin normale de fichier ou d’une interruption réseau : il ferme la lecture, sans passer au titre suivant ni reconnecter une radio dont le transport n’est pas en cause.

Les anciens 250 essais autour de `playRaw` ont été retirés après lecture de son implémentation exacte. La saturation ordinaire de ses slots est attendue **à l’intérieur** de l’API native ; elle ne produit pas le retour `false` traité par le pilote. Une attente de 500 ticks à 4 kHz est explicitement testée et continue à réussir. Aucun seuil arbitraire ne transforme cette attente en panne permanente.

Le worker SDR utilise la même sortie : une panne PCM libère sa possession et conserve le spectre. Une commande utilisateur permet une nouvelle tentative immédiate ; la réception ne relance pas le matériel défaillant à chaque bloc. La fin du worker attend la libération effective de la sortie avant de rendre ses buffers. Cette adaptation est nécessaire à la nouvelle propagation d’erreur, sans ajout de service ou de tâche.

## Ressources et périmètre

Pas de nouvelle tâche, de tampon permanent, de changement de cœur ou de décodeur. Le préchargement utilise 256 octets temporaires dans l’appel de démarrage du worker audio. La géométrie DMA provient de `i2s_channel_get_info`, afin de respecter l’alignement éventuellement appliqué par le SDK. Pour la configuration Tab5 courante (mono matériel, 256 trames ×8, 16 bits), les buffers DMA représentent 4096 octets ; les deux slots physiques I2S ne doivent pas être confondus avec deux canaux actifs DMA. Le PCM intermédiaire reste 12288 octets en PSRAM. Les tailles de piles et les sections finales relèvent du bilan de compilation ; aucune marge réelle de pile sur appareil n’a été mesurée ici.

`M5.begin(cfg)` configure les broches et les callbacks Speaker sans démarrer I2S. Désactiver `internal_spk` aurait retiré cette configuration nécessaire ; aucun changement de ce type n’a été appliqué. Le micro reste géré par M5Unified ; les arrêts/jonctions et arbitrages audio/vidéo existants sont conservés. Les modifications de générations des catalogues audio/vidéo sont documentées séparément avec le menu Média.

## Vérifications

- `audio-startup-0847.json` : fonctions réelles `begin`, `_begin_raw`, `end`, `_end_locked`, callback ES8388 et écriture I²C groupée. 24 configurations mono/stéréo et DMA différentes, dont une longueur non alignée : activation avant horloges dans 24 cas avant, zéro après. 41 échecs injectés, dont création du sémaphore, écritures partielles, nettoyage et chemin sans activation de la nouvelle option. 319 vérifications avant, 4504 après.
- Même suite : exécution des branches réelles d’attente et de reprise de `spk_task`. 1000 cycles codec externe sans arrêt/reprise des horloges ; un contrôle DAC interne exerce effectivement son arrêt/reprise. 20040 vérifications. La frontière d’attente FreeRTOS est simulée ; ce n’est pas une mesure de temps réel.
- `audio-driver-0847.json` : pilote Tab5 complet et publication `_play_raw` du SDK, huit fréquences sources, conversions PCM, 100 cycles audio/vidéo, aucun tampon suivi restant. Le premier échec natif n’est plus suivi d’une relance automatique identique ; la boucle de consommation échoue sans réessai infini. Pression des slots pendant 500 ticks à 4 kHz sans panne. Le nombre nul de redémarrages lors des changements de fréquence est identique dans la version 0.8.46 et la version corrigée : c’est une conservation vérifiée, pas un gain nouveau.
- `audio-decoder-0847.json` : service réel du lecteur, quatre combinaisons panne de sortie locale/radio et retour du décodeur vrai/faux ; aucun passage au suivant ou reconnexion sur panne de sortie. Deux fins de flux normales et deux cas pause/priorité conservés.
- `audio-sdr-0847.json` : worker SDR et DSP réels, 2000 cycles, échec PCM avec maintien du spectre, reprise explicite, attente de libération différée ; 20710 vérifications avant et 20692 après (les répétitions erronées supprimées expliquent la baisse du compte).
- `audio-catalogue-0847.json` : 921 vérifications avant/après sur les méthodes réelles, commandes en file, volume, générations, sélection et reprise par URL, sélection du suivant après déplacement du titre ou de la station et modification du catalogue entre sélection et exécution. Les frontières réseau, codec et fichiers sont contrôlées.
- `audio-transport-regression-0847.json` : réexécution dédiée de la suite transport sur les sources 0.8.46 et actuelles, 1023 vérifications dans chaque cas : 200 transports HTTP/HTTPS, 100 producteurs avec threads hôtes, 50 pauses/reprises, zéro client et allocation suivie restant. Les classes de source, tampon et fermeture sont réelles, ainsi que les méthodes HTTP/Stream extraites du SDK exact. Le dispatch de reprise est contrôlé ; sa résolution réelle de station est couverte séparément par `audio-catalogue-0847.json`. Les deux versions attendent déjà les notifications, sans polling de démarrage.
- `network-gate-regression-0847.json` : garde réseau complète compilée, 1835 vérifications dans chaque version, 100 cycles radio et 800 transactions sérialisées de demandes concurrentes. Une radio établie permet l'admission Cartes ; sa fermeture ne rend pas le mutex d'une autre requête ; les transitions pilote attendent la fin des transports. Ces comportements existaient dans 0.8.46 et sont conservés. Synchronisation FreeRTOS adaptée aux threads hôtes : aucune conclusion de débit Wi-Fi, qualité audio ou charge radio/Cartes réelle.
- `audio-target-0847.json` : compilation ESP32-P4 de Driver, Audio, Video et SDR actuels. `audio-sdk-target-0847.json` compare la bibliothèque d’origine et la copie corrigée, sans `-w`, avec diagnostics de retours invalides, variables non initialisées et utilisation après libération. Les deux compilent ; un même avertissement préexistant du SDK (`periph_ctrl.h`, `-Wliteral-suffix`), aucun nouveau message. La compilation globale vérifie séparément la liaison et la sélection des dépendances.

ASan/UBSan sont actifs pour les suites hôtes ; LeakSanitizer n’est pas utilisé. I²C, DMA, ordonnanceur et signal acoustique sont des frontières contrôlées. Les assertions de séquence ne prouvent pas à elles seules la disparition physique du bruit.

Références de mise en œuvre : [M5Unified 0.2.23](https://github.com/m5stack/M5Unified/tree/0.2.23), [I2S ESP-IDF 5.5.4](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_driver_i2s/i2s_common.c), [configuration des slots](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_driver_i2s/i2s_std.c), [pilote ES8388 Espressif](https://github.com/espressif/esp-adf/blob/release/v2.x/components/audio_hal/driver/es8388/es8388.c).

## Validation physique encore nécessaire

Sur haut-parleur puis casque : lancement à froid d’un MP3, lancement après arrêt, pause/reprise, déplacement dans le morceau, titre suivant, radio après attente réseau, vidéo puis retour musique. Relever si le bruit survient à l’activation du matériel, à la première note ou lors de la coupure. Comparer le début du contenu avec le fichier original. Conserver le journal `[audio-driver]` et toute erreur I²C ou I2S. Aucun reset radio/vidéo ni claquement physique n’est déclaré définitivement résolu sans cet essai.

Limite résiduelle importante : le worker Speaker natif utilise encore `portMAX_DELAY` pour ses écritures DMA en lecture et son arrêt attend la fin réelle du worker. Si les interruptions EOF matérielles cessent complètement, une jonction peut donc rester bloquée. Le délai borné du préchargement corrige son propre démarrage, pas ce cas de lecture déjà engagée. Forcer la suppression de la tâche risquerait de libérer de la mémoire encore référencée par I2S ; aucune telle suppression n’a été introduite. Un test matériel d’arrêt EOF et un mécanisme d’annulation natif correctement joint sont nécessaires avant de garantir la récupération de ce défaut matériel exceptionnel.
