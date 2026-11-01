# Audio — audit des durées de vie et des démarrages, 0.8.45

Référence : archive livrée 0.8.44, conservée intacte. Aucun matériel Tab5, réseau réel, codec physique ou sortie acoustique n'est exécuté ici. Ce rapport distingue les défauts établis des causes encore à confirmer pour les incidents signalés.

## 1. Défaut établi : le client HTTP radio survivait à son transport

`Tab5HttpAudioSource::close` faisait `HTTPClient::end()` puis détruisait son `NetworkClient`, alors que l'objet HTTP restait un membre vivant. Dans le SDK M5Stack Arduino 3.3.9 réellement compilé, `HTTPClient::disconnect(false)` ne remet son pointeur client à zéro que si `connected()` est vrai. Si le pair a déjà fermé la connexion sans données restantes, ce pointeur non propriétaire est conservé. Un deuxième `close`, une nouvelle ouverture ou le destructeur peuvent donc appeler une méthode virtuelle sur le transport déjà détruit.

Le problème est reproduit sous ASan avec **la classe radio complète de la 0.8.44** et les méthodes exactes du SDK `~HTTPClient`, `end`, `disconnect`, `connected`, `getStreamPtr`. Il ne s'agit pas d'une hypothèse de manque de mémoire. Le même type de correction avait été appliqué au lecteur vidéo en 0.8.44, mais pas à cette seconde implémentation radio : le périmètre de la correction précédente était incomplet.

Le membre HTTP devient un `std::optional<HTTPClient>` sans allocation d'objet supplémentaire. Chaque ouverture construit son objet HTTP et chaque fermeture le détruit **pendant que le transport est encore vivant**, avant de détruire le client. La fermeture répétée, l'échec de GET, la disparition du pair et la réouverture suivent tous cet ordre. La tâche de réception reste jointe avant la destruction de sa source et de son tampon, selon le protocole existant.

Les URL distantes complètes ne sont plus imprimées à l'ouverture radio, car elles peuvent contenir des identifiants ou tokens. Les codes HTTP/TLS restent consultables.

## 2. Défaut établi : attente des en-têtes HTTP sans coopération explicite

La radio utilisait des `NetworkClient` bruts, contrairement aux chemins cartographiques et vidéo. Le `Stream::timedRead` du SDK boucle sur `read` jusqu'au caractère ou au timeout ; cette boucle ne contient aucun rendement d'ordonnancement. Une réception d'en-tête fragmentée peut donc occuper la tâche audio de priorité 5 pendant son attente. Le test exécute la méthode SDK exacte et établit l'absence de rendement dans cette frontière contrôlée de la référence.

La radio utilise maintenant le `Tab5CooperativeClient` déjà présent dans l'OS. Celui-ci rend ponctuellement la main à FreeRTOS pendant les lectures/attentes, et observe l'annulation sur le transport qui le possède. Son contexte est la source elle-même, dont la durée de vie dépasse toute l'ouverture/réception. Une commande audio n'annule le transport que pendant son ouverture ; une commande normale arrivée pendant la lecture ne coupe pas le flux. L'abandon explicite ou l'interdiction réseau restent pris en compte pendant la réception.

Cette modification n'interrompt pas magiquement un appel DNS, une connexion TCP ou une poignée de main TLS déjà engagée dans le pilote. Elle ne prouve pas que tous les resets précédents étaient des watchdogs ; le journal de la tablette reste nécessaire.

## 3. Démarrage MP3 : deux configurations d'horloge concurrentes au niveau architectural

Le pilote établissait une horloge physique I2S de 48 kHz à la prise de sortie, puis `configure` la modifiait en redémarrant entièrement `M5.Speaker` à chaque changement de fréquence source. Or `M5Unified::playRaw` reçoit déjà cette fréquence **par tampon** et le mixer natif adapte ses données à l'horloge de sortie configurée.

L'ordre réel de `AudioGeneratorMP3::begin` est : `SetChannels`, `output.begin`, puis `SetRate` lors de la première trame décodée. Pour un MP3 44,1 kHz, le pilote réinitialisait donc I2S/codec/amplificateur après le premier `begin` à 48 kHz. Le callback Tab5 de M5Unified effectue effectivement des séquences d'alimentation codec et des écritures SPK_EN à chaque `end`/`begin`.

Le pilote conserve maintenant son horloge de sortie configurée. Il vide un éventuel tampon partiel à son **ancienne fréquence source**, puis transmet la nouvelle fréquence à `playRaw`, qui utilise le rééchantillonnage natif existant. La fréquence source et la fréquence de sortie restent distinctes dans les diagnostics. Le redémarrage privé `applySampleRate` devenu inutile est retiré. Les tailles DMA, de buffers et de piles ne sont pas diminuées et aucun silence artificiel, fade ou délai fixe n'est ajouté.

L'appel superflu à la remise sous tension est démontré et supprimé. Son rôle exact dans le claquement acoustique demeure **probable**, pas établi par une mesure sur haut-parleur. Le rééchantillonnage peut modifier le coût CPU par rapport à un chemin avec horloges identiques ; aucune mesure de CPU ou de qualité acoustique n'est revendiquée. Cette solution utilise le fonctionnement natif du SDK, sans nouvelle couche de décodage.

## 4. Vérifications exécutées

### Transport et tampon radio

Le test exécute les classes de production complètes `Tab5HttpAudioSource`, `Tab5BufferedRadioSource` et `Tab5PsramStream`, les helpers coopératifs réels et les méthodes SDK précitées. Les frontières RTOS, socket, allocation et les données réseau sont adaptées ; le producteur tourne réellement dans un thread hôte.

- Use-after-free de référence reproduit, trace `radio-before-asan.log`.
- 200 cycles de transport HTTP/HTTPS simulé, déconnexion du pair et fermetures répétées.
- Échecs d'ouverture, codes HTTP négatifs/403/404/500, refus chunked existant, annulation pendant l'ouverture.
- Corps lu sans altération, métadonnées ICY retirées, commandes de lecture courantes ne coupant pas le transport.
- En-têtes fragmentés : absence de rendement de la référence vérifiée ; rendement du helper réel et annulation vérifiés dans la correction.
- 100 cycles de source radio complète avec producteur, préchargement, lecture et fermeture jointe.
- 811 points de contrôle de scénario réussis ; aucun client ni allocation de tampon suivie restant après les cycles.

### Pilote commun PCM

Le test compile le fichier de production `Tab5AudioDriver.cpp` et son en-tête complets, ainsi que le corps exact de `Speaker_Class::_play_raw` du M5Unified utilisé. La réception de la requête du mixer et le matériel sont des frontières contrôlées ; le test ne présente pas une sortie acoustique simulée comme une validation physique.

| Scénario | 0.8.44 | 0.8.45 |
| --- | ---: | ---: |
| Redémarrage lors de la première fréquence MP3 44,1 kHz | 1 | 0 |
| Redémarrages sur une séquence de 8 fréquences source après ce début | 9 cumulés | 0 |
| Fréquences d'entrée transmises correctement au SDK | 8/8 | 8/8 |
| 100 cycles de restitution de PCM : allocations suivies restantes | 0 | 0 |

Le tampon partiel avant changement de fréquence conserve ses données et son ancienne fréquence. Les sources 8/16/22,05/24/32/44,1/48/96 kHz sont transmises comme métadonnées d'entrée, avec sortie physique stable dans le test. Les conversions U8 mono, S24 stéréo et S32 stéréo sont vérifiées ; le client vidéo passe par le même pilote, et les formats/canaux invalides restent refusés.

ASan et UBSan sont actifs. LSan reste désactivé, non exploitable ici ; les clients et buffers suivis sont contrôlés explicitement. La compilation objet ESP32-P4 des moteurs et du pilote est réussie avec la base de commandes de la 0.8.45. Les consommateurs et le lien final relèvent de la compilation complète.

Scripts : `tools/audio-tests-0845/transport.py`, `driver.py`. Preuves : `tools/verification/audio-transport-0845.json`, `audio-driver-0845.json`, `audio-target-0845.json`.

## 5. Revue des effets et limites restantes

Le worker audio, son cycle de vie, les queues de commandes et les protocoles de priorité de la 0.8.44 ne sont pas réécrits. Le bail inactif sans tâche reste compatible ; les lecteurs actifs ferment leurs propres ressources. L'enregistrement, les choix de sauvegarde et les données de l'utilisateur ne sont pas modifiés.

Les buffers PCM utilisent déjà trois blocs, en accord avec la documentation exacte de `playRaw` pour une réutilisation sans callback. Aucun défaut nouveau de rotation de ces buffers n'a été établi. Le `stop` garde son attente de fin du lecteur M5 avant de libérer les blocs. Le retrait des redémarrages de fréquence ne retire pas cette clôture nécessaire.

`AudioFileSourceSD` lit directement `SD` dans la bibliothèque ; le worker est entouré d'un bail de stockage et la récupération consulte `storageIdle`. Toute évolution de récupération doit continuer à attendre sa fermeture par le propriétaire, sans invalider un fichier audio vivant. Le sentinel de lecture invalide de la bibliothèque est une limite à surveiller ; aucun chemin déclencheur concurrent valide n'a été établi sous les baux actuels.

Sur Tab5 : tester les enchaînements local → radio → autre station → vidéo → retour → enregistrement, les fermetures pendant ouverture et les coupures réseau. Relever erreurs HTTP/TLS, cause de reset, plus grand bloc interne et sorties I2S. Vérifier le claquement au démarrage séparément des flashs d'affichage. Le use-after-free est corrigé par preuve de code ; l'absence totale de reset, de flash ou de claquement sur matériel n'est pas annoncée.
