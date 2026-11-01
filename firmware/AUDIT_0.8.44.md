# Tab5OS 0.8.44 — audit des interactions médias, cartes et stockage

Cette version corrige des défauts vérifiables de la livraison 0.8.43. Le ZIP de référence est conservé intact ; son empreinte est `a0c58c0ba1003722e3f834eac0618dc937c5395afb7cc650dd03220730595e52`. Les sources actuelles, leurs dépendances et les chemins réellement utilisés ont été examinés. Aucun test sur une Tab5 physique n'a été effectué : les incidents physiques ne sont pas déclarés disparus.

## Diagnostic et corrections

| Retour utilisateur | Cause établie par le code ou le test | Correction et portée |
| --- | --- | --- |
| Streaming vidéo redémarre la tablette | Use-after-free reproduit sous ASan : après déconnexion du pair, `HTTPClient::end()` peut conserver son pointeur non propriétaire ; le transport était supprimé avant la destruction du client HTTP | Destruction du client HTTP avant celle du transport. La cause exacte de chaque reset de la tablette reste à confirmer par son journal |
| Vidéo et musique jouent ensemble | Les vidéos sans PCM ne demandaient pas le bail audio ; ce comportement était explicite | Toutes les vidéos suspendent la musique avant ouverture et rendent le bail après fermeture des ressources. Un enregistrement reste préservé, avec vidéo muette |
| Pas assez de mémoire pour radio après cartes | Le worker des cartes pouvait attendre encore 60/220 ms après annulation/fermeture, gardant sa pile interne de 16 Kio ; des lectures HTTP pouvaient rester bloquées | Annulation coopérative des en-têtes/corps, fin sans cette attente si requête périmée ou priorité média. Aucun gain réel de tas ni fuite permanente n'est inventé |
| Cartes rend la SD indisponible | Les erreurs `ENOSPC`/`ENOMEM` pouvaient devenir `EIO`, déclenchant une récupération SD ; les accès temporairement exclus ne définissaient pas `errno` ; `EBADF` déclenchait aussi une récupération | Première cause conservée, `EBUSY`/`EBADF` explicites, récupération réservée aux erreurs de transport existantes. Publication du cache vérifiée jusqu'à `flush`/`close`, ancien fichier protégé |
| Cartes annonce parfois absence de Wi-Fi | La décision UI dépendait d'un état réseau publié par le moteur audio, potentiellement ancien pendant un travail long | Lecture de l'état SDK mis en cache et de l'autorisation réseau ; politique audio scalaire sans copies de texte. Une connexion physique perdue reste distincte d'un Wi-Fi simplement activé |
| Média flashe, accueil flashe encore parfois | Balayages du lecteur inutiles établis ; au repos contrôlé, la référence ne produit toutefois déjà aucun transfert LVGL | Lecteur piloté par révision, progressions regroupées à environ 4 Hz maximum. Compteurs d'affichage consultables par `d`. Aucune preuve permettant de déclarer les flashs matériels corrigés |

## Effets sur les autres fonctions

La correction vidéo conserve le protocole existant de priorité : la tâche audio ferme elle-même ses sources, décodeurs, flux réseau et sortie avant d'acquitter. La vidéo libère ses ressources avant de rendre le bail. Les dernières commandes utilisateur et la reprise restent traitées par l'audio. L'enregistrement n'est jamais arrêté pour obtenir ce bail ; les vidéos peuvent rester muettes pendant celui-ci.

Un bail depuis un moteur audio entièrement inactif est acquitté sans allouer une tâche de 16 Kio uniquement pour cette formalité. Il reste protégé par la génération atomique du protocole existant. Les commandes et reprises actives ne passent pas dans ce raccourci ; aucune opération réseau/fichier/décodage n'est exécutée dans le service graphique.

Le lecteur respecte les révisions indépendantes de l'audio et la propriété des notifications du lecteur vidéo plein écran. Les demandes forcées et événements restent en attente si une copie d'état échoue. Les tests incluent une copie partiellement échouée : elle n'est pas affichée. La limite des progressions évite d'accroître les transferts à chaque publication audio de 50 ms.

Les contrôles SD distinguent une panne de communication d'une saturation ou d'une contention. Les temporaires ne sont publiés qu'après validation de l'écriture, de la taille, de la synchronisation et de la fermeture. Le rollback conserve l'erreur initiale et l'ancien fichier selon la politique existante. Il n'y a ni formatage ni montage forcé supplémentaire. FAT et les baux ne garantissent pas une transaction universelle face à une coupure d'alimentation ou à deux écrivains sur le même chemin.

Le pilote d'affichage n'est pas modifié. L'inspection du M5GFX réellement compilé ne montre pas de publication DSI cachée à chaque zone. La clôture extérieure synchronise le cache ; la présentation explicite soumet la trame. Le double tampon peut toutefois être indisponible si les contrôles d'initialisation échouent ; seul le journal du vrai appareil permet de connaître la branche utilisée. Les hypothèses de contention PSRAM/DSI ou de variante de dalle restent à mesurer.

## Architecture et mémoire

L'UI et LVGL restent propriétaires de l'affichage sur leur tâche. Les workers existants réalisent les travaux audio, vidéo et cartes avec les primitives FreeRTOS actuelles. Aucun ordonnanceur parallèle ni tâche permanente nouvelle n'est ajouté. Les deux cœurs HP du P4 sont déjà utilisés ; le C6 gère les communications et n'est pas un tas supplémentaire partageable avec le P4.

La PSRAM héberge les grandes données compatibles ; les piles internes, blocs DMA et contraintes contiguës restent distincts. Une pile cartographique de 16 Kio retenue, même temporairement, peut empêcher une admission audio malgré beaucoup de PSRAM libre. L'admission de la pile vidéo de 24 Kio précède toujours la suspension de l'audio : une vraie pénurie interne peut donc encore empêcher le lancement. Les marges de sécurité et tailles de piles ne sont pas abaissées pour faire disparaître le message.

La version ajoute une révision audio atomique ; elle ne revendique pas une réduction du tas de démarrage. Les mesures liées au binaire final et les coûts transitoires figurent dans [BILAN_RESSOURCES_0.8.44.md](BILAN_RESSOURCES_0.8.44.md).

## Validation et conservation

Les tests ciblés exécutent les fonctions de production avec LVGL réel, les méthodes de durée de vie HTTP du SDK exact et des frontières injectées pour RTOS, stockage et périphériques. ASan/UBSan sont actifs ; LSan n'est pas exploitable ici. Les références et allocations simulées sont suivies explicitement. Le modèle ne remplace ni un démarrage complet ni un test réseau/audio/SD/DSI de la tablette.

Compilation complète, image et partitions : [VERIFICATION_0.8.44.md](VERIFICATION_0.8.44.md). Les preuves courantes comportent des empreintes des sources et sont distinctes des preuves historiques. Les 14 packs de langue, ressources et consommateurs des interfaces modifiées sont vérifiés. Les détails par module se trouvent sous `tools/audit-0844`.

Les applications disponibles, choix d'enregistrement/autosauvegarde, fichiers, préférences, clés et coffres sont conservés. Aucun effacement sensible, changement de table de partitions, restriction C6 fondée sur une version inconnue ou exclusion nominative d'ELF n'est ajouté. Les nouvelles traces vidéo ne journalisent plus l'URL distante complète, qui pourrait contenir des identifiants.

## Problèmes résiduels

| Criticité | Point non résolu matériellement | Investigation courte |
| --- | --- | --- |
| Haute | Reset réel en streaming, même après correction du use-after-free | Capturer le journal complet, cause de reset, contexte RTC vidéo et backtrace ; noter format/protocole sans secret |
| Haute | SD réellement sans réponse sous forte charge | Capturer errno exact et lignes SD, mémoire/bloc maximal, espace libre et scénario ; pas de formatage ni réinitialisation des paramètres |
| Haute | Flashs Média et accueil | Relever `double buffering: active/unavailable`, puis `d` avant/après ; comparer soumissions LVGL, underruns et instant du flash |
| Moyenne | Refus mémoire carte→radio restant | `m` avant/après, admission radio et marges de piles. Distinguer pile retenue, fragmentation, DMA et PSRAM |
| Moyenne | Annulation pendant DNS/TCP/poignée de main TLS | Les appels engagés restent soumis aux délais du pilote ; mesurer leur durée, sans fermer le transport depuis une autre tâche |
| Moyenne | Certaines opérations synchrones et limites natives existantes | Ouvertures/enregistrements volumineux, indexation, décodeurs et ELF non coopératifs restent à profiler ; aucun isolement matériel ELF n'est annoncé |

La procédure précise est [TESTS_TAB5_0.8.44.md](TESTS_TAB5_0.8.44.md). La livraison atteint un état compilé et vérifié dans cet environnement ; elle ne clôt pas les anomalies physiques sans ces essais.
