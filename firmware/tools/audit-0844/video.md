# Vidéo et interactions audio — 0.8.44

## Causes établies

**Criticité élevée : transport réseau utilisé après libération.** Le lecteur 0.8.43 conservait `HTTPClient` comme membre permanent de `Tab5VideoInput`. `close()` exécutait `HTTPClient::end()` puis supprimait le `NetworkClient` emprunté. Dans le SDK réellement utilisé (M5Stack Arduino 3.3.9), `HTTPClient::disconnect(false)` laisse son pointeur `_client` intact lorsque le pair est déjà déconnecté. Le `close()` suivant, ou le destructeur de `HTTPClient`, appelle alors des méthodes virtuelles sur ce transport libéré. La séquence existe notamment après fin de flux, échec HTTP ou reconnexion.

Ce défaut est reproduit sur la source 0.8.43 par ASan : `heap-use-after-free`, appel de `HTTPClient::connected()` depuis `Tab5VideoInput::close()`. Le test extrait les méthodes pertinentes du SDK installé, pas une réimplémentation corrigée. Il établit un risque réel de crash ; il ne prouve pas à lui seul que tous les resets observés sur la tablette viennent de cette cause.

**Musique non suspendue par les vidéos silencieuses.** Le code initial ne demandait la priorité audio qu'après détection du PCM AVI, ou après un premier échec d'acquisition réseau pour évincer une radio. Un MJPEG local ou distant sans PCM gardait donc intentionnellement la musique locale active. Ce comportement explique directement le retour utilisateur.

## Corrections ciblées

- `HTTPClient` est désormais un `std::optional` intégré à l'objet d'entrée, construit par ouverture et détruit **avant** la suppression du transport emprunté. Il n'ajoute pas d'allocation sur le tas pour le client HTTP. `close()` peut être répété sans conserver de pointeur vers un transport détruit. Si les en-têtes HTTP arrivent mais que le flux est déjà fermé, l'ouverture restitue immédiatement client et lease avant la prochaine tentative.
- Toutes les vidéos demandent la priorité audio existante depuis leur worker avant d'ouvrir un fichier ou une connexion. L'owner audio suspend et ferme sa propre lecture ; le worker vidéo ne ferme jamais des ressources appartenant à l'audio. La reprise conserve le protocole existant : fermeture socket, décodeur et sortie PCM, puis restitution de la priorité. Une commande utilisateur ultérieure conserve sa priorité selon le mécanisme audio existant.
- L'enregistrement n'est jamais interrompu. En cas de refus de priorité, une vidéo silencieuse reste possible si le snapshot confirme un enregistrement actif ; l'AVI n'essaie alors ni une seconde acquisition ni une sortie PCM. Un refus provenant d'un autre propriétaire reste une erreur explicite, avec un message déjà traduit.
- Les étapes HTTP/TLS vidéo et réception du flux renseignent le journal réseau RTC existant, sans nouveau timer, buffer ou écriture permanente. Leur fin ne supprime pas les contextes cartes/radio. L'URL distante complète n'est plus journalisée par le lecteur : ses paramètres peuvent contenir des identifiants ou jetons.

Le protocole HTTP/1.0, la validation des conteneurs, les limites des images, le triple buffer JPEG, les délais réseau, les priorités et les piles FreeRTOS restent inchangés. Les fichiers, réglages, enregistrements et partitions ne sont pas modifiés.

## Vérifications exécutées

- **412 scénarios** ASan/UBSan sur `Tab5VideoInput`, `taskMain`, le parser AVI et `playAvi` de production, avec les méthodes de propriété `HTTPClient` extraites du SDK verrouillé. Connexion HTTP/HTTPS contrôlée, déconnexion avant fermeture, erreurs HTTP, URL refusée, 100 cycles d'échec/reprise/fermeture, Stop, Wi-Fi désactivé, flux disparu après en-têtes, priorité audio refusée, annulation durant acquisition. Aucun client réseau ni lease restant dans la fixture.
- AVI PCM synthétique parcouru par le parser réel : une acquisition et une écriture PCM lorsque la priorité est détenue ; zéro acquisition/écriture pendant enregistrement, vidéo terminée et enregistrement inchangé. La sortie JPEG matérielle est simulée dans ce test.
- Régression de cycle de vie avec les méthodes de production : remplacement avant démarrage du worker, dernière intention conservée, Stop annulant le remplacement, un seul owner actif, aucune attente de join dans l'UI, détachement durant publication finale, frames attachées conservées jusqu'au retrait LVGL, erreurs de mémoire/création de tâche.
- Compilation objets ESP32-P4 de `Tab5Video.cpp` et `Tab5MediaSession.cpp` avec la base de commandes 0.8.44 et les dépendances verrouillées. La compilation complète et la taille globale relèvent du rapport de livraison.

La pile du worker reste **24 Kio**. Les frames locales GCC ne mesurent pas la pile cumulée : `taskMain` 576 octets, `playAvi` 4 336 octets et `playRawMjpeg` 4 224 octets, identiques à la 0.8.43 ; l'ouverture réseau passe de 256 à 128 octets locaux. Aucune réduction de pile n'est faite sur cette seule base. Pas de nouvelle tâche permanente.

## Limites et essais sur Tab5

Pas de tablette physique, de TLS réel, de JPEG DMA ou de sortie I²S testé ici. Une capture de reset doit inclure le motif ESP-IDF/panic, la backtrace, le dernier contexte RTC et les lignes `[VIDEO][MEM]`/`[STACK] video`. Le risque d'épuisement interne lors de l'admission de la pile vidéo demeure distinct de la disponibilité de PSRAM : la pile est admise avant l'entrée du worker et avant la suspension audio.

Sur une carte de test : musique locale puis vidéo MJPEG, radio puis vidéo distante, fin de flux/réouverture, Stop pendant connexion et réception, vingt cycles vidéo/fermeture/radio. Vérifier la suspension de musique et sa reprise, l'absence de double audio et l'absence de baisse persistante du tas. Tester séparément enregistrement plus vidéo silencieuse et AVI PCM : le WAV en cours doit continuer et la vidéo rester muette. Aucun redémarrage ni formatage ne constitue un critère de réussite.

Les flashs physiques ne sont pas déclarés corrigés par ce lot ; le changement d'interface Média est audité séparément.
