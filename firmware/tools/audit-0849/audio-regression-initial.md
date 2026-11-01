# Audit initial : contrat audio commun et régression historique

Date : 2026-10-28. Audit du code, sans modification de production et sans Tab5 physique.

## Périmètre et références

- Sources de travail : Tab5OS v0.8.49, copiées depuis la livraison v0.8.48 signalée défaillante par l’utilisateur.
- Référence historique : `Tab5OS_Arduino_v0.4.10_FullPlayer.zip`, explicitement décrite comme fonctionnelle par l’utilisateur le 25 août. Ce n’est pas une affirmation qu’elle est la dernière version fonctionnelle. SHA-256 : `e075642af455705c6cb8fab8c5a4981bc98d92f9635086c09bf74f425e4a7f0e`.
- Dépendances réellement préparées pour .48 : Arduino M5Stack 3.3.9 / ESP-IDF 5.5.4 ; M5Unified 0.2.23, copie isolée modifiée par `tools/build_linux/prepare_m5unified_audio.py` ; ESP8266Audio installé dans `build_0848/sketchbook/libraries`.
- La référence .4.10 indique une version minimale M5Unified ; la version exacte installée sur l’appareil à l’époque n’est pas disponible. La comparaison est certaine pour le code Tab5OS, pas pour le binaire historique des bibliothèques.

## Résultat principal

Le changement de contrat le plus net est global : **l’arrêt d’une source PCM est devenu un arrêt complet du worker Speaker, de l’I²S, du codec et de l’amplificateur**. Il concerne MP3, FLAC, radio, vidéo, SDR, applications ELF et alarme. Ce changement est indépendant du menu média.

Ce couplage protège actuellement les pointeurs PCM : la version ancienne effaçait ses buffers immédiatement après un `Speaker.stop()` asynchrone. Il ne faut donc pas restaurer simplement l’ancien code. En revanche, le contrat actuel multiplie les cycles analogiques et les allocations internes pendant une pause, un déplacement dans un fichier ou un changement de piste. C’est une cause plausible des claquements, pas encore une preuve physique de leur origine.

## Comparaison vérifiable

| Contrat | Référence .4.10 | Version .48 / copie initiale .49 | Conséquence établie |
|---|---|---|---|
| Démarrage | Speaker démarré au boot, avec ancien redémarrage à froid supplémentaire | Speaker démarré à la première prise de sortie | Allocation I²S reportée sur le démarrage média |
| `stop(client)` | Marqueur `Speaker.stop(0)` puis effacement immédiat PCM | `Speaker.end()` puis effacement PCM | Fin du consommateur avant réutilisation ; cycle codec/ampli à chaque stop |
| `release(client)` | Retire le propriétaire, transport reste actif | Stop, libération du tampon PCM, propriétaire vide | Récupération mémoire réelle, mais arrêt complet du transport |
| Pause locale / seek | Stop logique sur canal | Appelle le même stop complet | Reprise exige un nouveau démarrage I²S/codec |
| DMA configuré | 512 × 12 | 256 × 8, valeur M5Unified par défaut | Réserve DMA stéréo nominale : 24 576 → 8 192 octets ; marge temporelle réduite |
| Taux physique | Reconfigure le transport au taux source | Sortie fixe 48 kHz, rééchantillonnage M5Unified | Plus de cycle supplémentaire au premier MP3 44,1 kHz ; charge du mélangeur différente |
| Buffers PCM communs | Stockage fixe | 3 × 2048 × 2 = 12 288 octets PSRAM par propriétaire | Ressource libérée à `release()` ; aucun besoin DMA direct pour ces buffers CPU |
| Cœurs | Audio séparé de la boucle UI dans le code ancien | Décodeur priorité 5 et Speaker priorité 6 sur le cœur opposé à l’UI ; radio réseau priorité 4 côté UI | Le second cœur est déjà utilisé ; changer encore l’affinité sans mesure n’est pas une conclusion |

Les octets DMA ci-dessus viennent de la configuration et du budget stéréo du projet. Ils ne constituent pas une mesure du heap physique complet : TCB, descripteurs et métadonnées s’ajoutent. Les réductions de DMA augmentent la sensibilité à une suspension du worker, mais ne prouvent pas un sous-débit constaté.

## Chemin commun actuel

1. `Tab5AudioEngine::startLocalDecoder()` appelle `stopPlayback(false)` avant chaque source.
2. `cleanupDecoder()` appelle `_output->stop()`, détruit les décodeurs et sources, libère les workspaces MP3/AAC, puis `Tab5AudioDevice.release(Audio)`.
3. `AudioOutputTab5Speaker::stop()` appelle `Tab5AudioDriver::stop()`.
4. Ce dernier appelle `M5.Speaker.end()` ; `_end_locked()` coupe le codec/ampli, attend la fin du worker, remet ses slots et son interpolation à zéro, arrête puis désinstalle I²S.
5. Le prochain `claim()` alloue les 12 Kio PCM et `Speaker.begin()` réalloue I²S/DMA/worker, précharge des zéros, réinitialise ES8388 puis active l’ampli.
6. Le premier bloc décodé est soumis avec son taux source ; le Speaker rééchantillonne vers 48 kHz.

Points de code : `Tab5AudioDriver.cpp` fonctions `claim`, `release`, `ensureSpeakerRunning`, `configure`, `pushCurrentBlock`, `stop` ; `Tab5Audio.cpp` fonctions `cleanupDecoder`, `startLocalDecoder`, `execTogglePause`, `execSeekPermille`. La bibliothèque effective est `build_0848/qa/firmware/dependencies/M5Unified/src/utility/Speaker_Class.inl`.

### Propriété inter-applications

Le périphérique est centralisé et arbitré par `Tab5AudioClient` (Audio, Video, Native, Alarm, Sdr). Vidéo, ELF et SDR utilisent ce même pilote. MicroPython ne prend pas directement le Speaker dans les sources examinées. L’enregistrement arrête la lecture, suspend le pilote, prend M5.Mic, puis termine le micro avant de reprendre.

`release()` a toutefois un résultat `void` et abandonne après 250 ms d’attente sur son mutex. ELF et SDR réessaient explicitement si leur propriété reste active ; les chemins Audio, Video et Alarm n’ont pas tous ce même contrat. Cela peut conserver le propriétaire et le buffer si le mutex est réellement bloqué assez longtemps. Le code le permet ; aucune trace matérielle ne confirme que ce cas explique les caches observés. La propriété doit être libérée de manière explicite et observable, sans libérer de mémoire encore consommée.

Le routage casque est un client concurrent du mutex du pilote et de l’I²C. Il est vérifié depuis le service UI toutes les 200 ms seulement si une sortie est active. Les modifications de l’expander et leur partage avec les autres sous-systèmes doivent être rapprochés de l’audit I²C global ; un simple changement de menu n’explique pas ces contraintes.

## Hypothèses écartées par le code

- **Le worker Speaker ne coupe pas I²S dès qu’il manque un bloc PCM sur Tab5.** Le chemin idle de la sortie externe remplit des zéros puis attend une notification. L’arrêt I²S idle appartient au chemin DAC interne `use_dac`, non utilisé ici. Il ne faut pas invoquer une coupure automatique entre `claim()` et le premier décodage.
- **Le premier `lastSample` MP3/FLAC n’est pas non initialisé.** Les constructeurs/begin initialise les deux valeurs à zéro. MP3 émet un premier zéro avant sa première trame ; FLAC ne consomme ce zéro que si le nombre de canaux est déjà connu.
- **`ConsumeSample()` qui retourne faux juste après la soumission d’un bloc plein ne perd pas cet échantillon.** ESP8266Audio le réessaie ; le pilote n’avait pas encore écrit cet échantillon. Ce contrat est cohérent.
- **La présence d’un gros heap PSRAM ne garantit pas l’allocation I²S.** DMA et worker requièrent de la mémoire interne ; le budget radio tente déjà de réserver ces ressources avant TCP.
- **La réinitialisation du mélangeur .48 ne démontre pas la disparition du claquement.** Elle corrige une contamination inter-session réelle, mais l’utilisateur rapporte le défaut sur matériel après cette livraison.

## Autres anomalies certaines, à traiter sans les confondre avec le claquement

- ESP8266Audio `AudioGeneratorFLAC::begin()` ignore la valeur de retour de `output->begin()`. La sous-classe Tab5 FLAC ne corrige pas ce contrat, contrairement au garde de l’AAC. Si `claim()` échoue pour sortie occupée ou manque de tampon PSRAM sans mettre `_outputFailed`, le FLAC peut se déclarer en lecture puis rester en attente d’une sortie qu’il ne possède pas. C’est un défaut de propagation d’erreur ; il ne prouve pas un pop audio.
- Les workspaces de décodage, sources réseau et tampon PCM sont effectivement libérés par `cleanupDecoder()` et `release()`. Dire simplement « pas de purge audio » serait inexact. Il faut démontrer quels chemins atteignent le nettoyage et quels catalogues, frames vidéo ou objets UI survivent volontairement ou par erreur. Cette analyse relève aussi du propriétaire des caches global.
- `Speaker.end()` assure actuellement un join ; son attente interne boucle jusqu’à la fin effective du worker. Si I²S ou I²C se bloque, cette durée n’est pas bornée au niveau Tab5. Remplacer ce join par un timeout suivi de `free()` créerait une corruption mémoire.

## Axes de correction proposés, avant modification coordonnée

### A. Séparer l’arrêt des références PCM et la destruction du périphérique

Donner au pilote commun une primitive de quiescence acquittée : après réception du marqueur d’arrêt par le worker Speaker, aucun slot ne référence les buffers du propriétaire. Pause, seek et transition rapprochée peuvent garder les horloges et le codec actifs ; fermeture finale, mise en veille, enregistrement et récupération mémoire peuvent réellement détruire le transport.

L’acquittement doit provenir de l’état ou d’un événement du worker, pas d’un `delay(N)`, ni d’une estimation de durée de bloc. La bibliothèque a déjà des slots atomiques et un sémaphore réveillé à la libération. Son callback de buffer n’est pas une preuve suffisante pris seul : un buffer encore en attente remplacé par un marqueur n’appelle pas ce callback, et `end()` abandonne les slots sans callback.

Il faut explicitement choisir le contrat d’une fin définitive et d’un remplacement immédiat pour ne pas transformer cette correction en conservation permanente de DMA. Un simple `stop()` sans destruction sur tous les chemins réduirait les cycles mais contredirait l’objectif de restitution de ressources. L’ancienne réinitialisation à froid et ses reprises par tailles DMA différentes ne doivent pas revenir.

### B. Traiter l’échec et les contraintes au même niveau partagé

Rendre observable l’achèvement de `release()` ; garantir qu’un client ne publie pas sa fermeture tant qu’il est encore propriétaire ; propager l’échec de début FLAC comme pour les autres codecs. Conserver un seul propriétaire pour I²S/codec/routage. Éviter les modifications simultanées du DMA, de l’affinité, de l’ordre codec et du tampon PCM : elles empêcheraient d’attribuer les résultats.

Aucun réglage analogique supplémentaire, fade ou temporisation arbitraire n’est justifié par cet audit seul. Un claquement à la toute première ouverture peut persister même si les transitions ne recyclent plus le codec ; il faudra le différencier sur matériel.

## Validation nécessaire du changement de contrat

### Tests exécutables sur hôte

Un test pertinent doit traverser **le vrai `Tab5AudioDriver` et le vrai worker M5Unified préparé par le script**, avec un backend I²S instrumenté et un ordonnancement contrôlé. Il doit enregistrer les transitions begin/enable/stop/end, les lectures de buffers et le signal de libération. Un test de conversion PCM isolé ne couvre ni la propriété ni les cycles du transport.

Scénarios minimaux : premier démarrage, deux pistes successives 44,1/48 kHz, pause/reprise, seek, arrêt durant un slot courant et un slot en attente, fermeture immédiate, transfert Audio→Video→Audio, retour après micro, refus de claim et échec du démarrage FLAC, échec de libération du mutex, boucles de 100 ouvertures/fermetures. Vérifier absence de lecture après restitution, propriétaire vide à la fermeture, buffers/DMA/worker libérés une seule fois, absence de cycle codec sur les transitions explicitement chaudes et présence de destruction à la fermeture définitive.

### Essais Tab5 indispensables

Même morceau MP3 puis FLAC, même volume, sorties haut-parleur et casque séparées ; mesurer premier démarrage, changement de piste, pause/reprise et seek. Répéter après cartes, dessin et vidéo. Relever avant/après mémoire interne libre/minimum/plus grand bloc, PSRAM, nombre de cycles codec et latence maximale de soumission PCM. Les clics doivent être corrélés au premier échantillon, au changement d’ampli ou à un manque DMA ; idéalement capturer I²S et sortie analogique. Sans ces essais, ne pas annoncer « claquements corrigés ».

## Limites et statut

Cet audit établit les différences de code et les contrats mémoire/concurrence ; il ne prouve pas une cause matérielle unique. Le driver, les dépendances et l’UI ne sont pas modifiés par cette étape. Les prochaines modifications doivent être coordonnées avec les audits globaux mémoire, I²C et DMA/affichage, puis compilées et testées dans un ensemble cohérent.
