# Revue indépendante d’intégration 0.8.46

Revue en lecture seule des interfaces et de la propriété des ressources, distincte des essais des auteurs. Sources exactes dans `integration-review-source.json`. Aucun nouveau résultat matériel n’est revendiqué.

## Réseau et audio

Le passage du bail radio de la connexion exclusive au flux établi intervient après création du récepteur, du transport, du décodeur et de la sortie audio. Le budget provisoire est alors relâché : ses allocations figurent déjà dans le tas réellement libre. Le compteur de flux établi reste actif jusqu’à fermeture des ressources ; les transitions de pilote attendent ce compteur en plus du mutex. La fermeture du bail partagé ne libère donc pas un mutex acquis entre-temps par Cartes.

Le constructeur vérifie à nouveau l’arrêt réseau, l’annulation et la connexion Cartes retenue après acquisition du mutex. Cela ferme la course entre le premier contrôle et l’acquisition. Les objets Cartes de transport demeurent sur leur tâche ; les références de contexte d’annulation sont restaurées avant retour. La recherche locale ne dépend pas de la possibilité d’ouvrir un socket. Les appels de transition Wi-Fi de l’audio ferment leur radio avant d’attendre le bail de contrôle. Aucun cycle de verrous supplémentaire identifié sur ces parcours.

Le worker audio ne publie son arrêt qu’après fin des scopes de stockage et d’état. Sa dernière opération sur le moteur est la publication de l’état arrêté ; le trampoline libère ensuite sa propre tâche. Les commandes arrivées à cette frontière restent dans la file et sont reprises par le service graphique suivant. Une pile encore en attente de nettoyage FreeRTOS est toujours comptée par l’échantillon réel du tas ; la revue ne lui attribue aucune libération immédiate garantie.

## Présentation graphique

Les plages de miroir sont enregistrées synchroniquement dans les métadonnées du cache. Leur union avec les nouvelles zones dessinées est écrite en mémoire avant publication. La frontière de deux événements postérieurs à la publication reste inchangée : le front balayé n’est pas choisi comme cible CPU. Une géométrie non vérifiée après miroir, un échec de cache ou un échec de publication empêche l’affichage du tampon incomplet et prépare la reprise contrôlée. Les chemins génériques gardent l’écriture immédiate. Aucune fréquence de dalle, priorité ISR, allocation de framebuffer ou politique de fence modifiée.

Les essais actuels couvrent le vrai code de présentation/cache et les routines SDK épinglées, avec modèle explicite des événements et de la mémoire. Leur réduction des octets passés à la synchronisation cache ne mesure pas le débit PSRAM, le temps d’interruption ni la disparition des flashs.

## Historique Dessin

Les instantanés partiels ne partagent pas de pointeur de pixels avec le document. Un pointeur nul signifie que ce calque est transféré depuis le document courant lors de l’échange, puis supprimé de l’état déplacé dans l’historique. L’éviction ne libère que les tampons réellement possédés. Les opérations géométriques/multicalques gardent leurs instantanés complets, ce qui permet aux échanges intermédiaires de rétablir dimensions et nombre de calques avant d’atteindre un état partiel.

Les consommateurs directs d’`undo[]` ont été examinés : le pinceau utilise le calque capturé ; la prévisualisation texte, les transformations et le déplacement de sélection ouvrent des instantanés complets. Le budget des sélections et de l’ajout de calque garantit que les pixels et le masque courants restent sous la limite de l’historique. Une hypothèse de refus tardif d’annulation au-delà de cette limite a été tentée sur le noyau réel et rejetée dès l’admission de sélection existante ; aucune correction n’a été introduite pour cette hypothèse non établie.

Les suites actuelles reprennent les tests historiques pertinents sur le nouveau noyau, sous ASan/UBSan, avec traces comparées à la référence authentifiée et allocations empoisonnées. Leurs résultats détaillés et limites restent dans leurs propres rapports ; cette revue ne les présente pas comme une seconde exécution indépendante.

## Synchronisation de volume corrigée après revue

La revue a trouvé une asymétrie préexistante : `_volume` était écrit sous `StateGuard` par le réglage utilisateur mais lu sans ce verrou dans les démarrages local et radio. Les deux lectures et leur transmission immédiate au pilote sont maintenant protégées par le même verrou. Les appels de volume de démarrage ne gardent pas ce verrou pendant l’ouverture de fichier, la revendication I2S ou la connexion réseau. Le setter du pilote ne réalise ici que la mise à jour du volume ; aucun délai ou nouvelle tâche n’a été ajouté. Le comportement du volume d’alarme est conservé. La compilation et les preuves audio sont régénérées pour ces sources.

Ce défaut de synchronisation ne démontre pas l’origine des claquements ou des resets physiques ; les tests de durée de vie ne prouvent pas non plus que chaque champ ordinaire du firmware est exempt de course.

## Limites

Pas de blocage établi supplémentaire au terme de cette revue des interactions modifiées. Cela ne démontre pas l’absence de courses sur tout le firmware, ni le comportement du C6, des interruptions, de l’I2S ou de la dalle sur appareil. Les tris Média par métadonnées, les transformations globales Dessin et les attentes justifiées d’arrêt de lecteur conservent des parcours synchrones à mesurer.
