# Tab5OS 0.8.43 — accueil, catalogue et structure mémoire

Cette livraison reprend la 0.8.42 compilée. Elle conserve les applications, fichiers, préférences, clés, formats de données et choix d’autosauvegarde. La table de partitions et les firmwares de communication C6 ne sont pas modifiés.

| Module | Correction | Limite de la validation |
|---|---|---|
| Accueil | Plus de scan des bibliothèques ELF/Python tant que seul l’accueil natif est visible. Demandes et révisions sont conservées jusqu’à l’ouverture de la bibliothèque. | Les tests exécutent les fonctions réelles ; la latence de la SD physique reste à mesurer. |
| Catalogue | Suppression des exclusions nominatives Quake/Duke3D/AtelierImage, découverte des ELF selon leur type et les racines déclarées. Préférences explicites de visibilité conservées. | Les bornes de capacité et la validation du chargeur restent nécessaires ; un ELF n’est pas une application isolée. |
| Travail en arrière-plan | Énumération sur une tâche ponctuelle du cœur 0, publication du résultat sur UI. Annulation coopérative et maintien du précédent catalogue en cas d’erreur. | Le cas où aucune tâche ne peut être admise conserve une solution incrémentale compatible. |
| Audio et horloge | Les composants du bandeau ne reconstruisent plus leur état sans changement ; instantané audio sur révision, horloge et réseau sur état utile modifié. | Le balayage matériel DSI continue ; un écran au repos n’est pas un périphérique éteint. |
| Mémoire | Compteurs optionnels UI alloués paresseusement en PSRAM ; aucun tampon DMA ou pile interne déplacé sans preuve de compatibilité. | Les tailles ELF ne sont pas des mesures de tas libre au démarrage. |
| Nettoyage | Retrait de redirections vers AtelierImage, constructeur d’anciennes cartes d’accueil et sous-titre Open With redondant. Outils actifs de compilation découvrant la version courante ; exclusions retirées du générateur Factory. | Migrations des anciennes données, compatibilité SDK/matériel et preuves historiques conservées pour leur rôle réel. |

Les bibliothèques visibles gardent une vérification périodique de secours : certains écrivains POSIX ne publient pas de révision du système de fichiers. Elle n’a plus lieu à l’accueil natif et ne doit pas être présentée comme un mécanisme entièrement déclenché par événements. Les événements d’E/S locaux et l’entrée dans un menu déclenchent le travail utile.

Le P4 possède deux cœurs HP ; LVGL reste sur le cœur UI, les lectures de catalogue passent sur l’autre. Le C6 possède sa propre mémoire et exécute le firmware ESP-hosted de communication : ce n’est pas un second tas PSRAM ni un cœur FreeRTOS P4 supplémentaire. Aucun nouveau protocole ou firmware C6 n’est introduit. La configuration SDK compilée règle le P4 à 360 MHz ; le maximum matériel de 400 MHz n’est pas une mesure de cette livraison.

Aucun essai physique n’a été effectué. L’origine des flashs turquoise au repos n’est pas démontrée. La sécurité conservatrice de présentation des deux buffers est maintenue ; la réduire pour obtenir une meilleure cadence sans preuve aurait exposé des images en cours de balayage.

Voir [AUDIT_0.8.43.md](AUDIT_0.8.43.md), [BILAN_RESSOURCES_0.8.43.md](BILAN_RESSOURCES_0.8.43.md), [VERIFICATION.md](VERIFICATION.md) et [TESTS_TAB5_0.8.43.md](TESTS_TAB5_0.8.43.md).
