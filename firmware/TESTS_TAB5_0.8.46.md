# Tests sur Tab5 — 0.8.46

Aucun de ces essais matériels n'a été exécuté ici. Garder la version précédente pour comparaison. Utiliser le BIN par la procédure OTA habituelle ; ce n'est pas une image fusionnée à écrire à l'adresse 0. Il n'y a ni changement de partitions, ni formatage, ni réinitialisation de paramètres.

## Relevé commun

Conserver le journal série depuis le démarrage : version, cause du reset, dalle et état `double buffering`. Avant puis après le symptôme : envoyer `m` (mémoire/stockage) et `d` (affichage/activité). Pour mesurer les blocages, `p` active le profilage existant, `d` affiche ses durées, puis `p` le désactive. Une durée de phase n'est pas une mesure CPU. Ne pas communiquer de mot de passe, PIN ou clé ; masquer les identifiants personnels éventuellement présents.

## Parcours ciblés

1. **Radio + Cartes.** Démarrer une station, attendre sa lecture, ouvrir Cartes et zoomer pendant le chargement. Lancer ensuite la génération d'une petite zone autorisée. La lecture ne doit pas rendre le Wi-Fi indisponible. Refaire radio en pause, puis MP3 en pause. Garder le texte exact d'un refus et les logs `[NET]`, `[MAP]`, `[MEM][REFUS]`.
2. **Recherche et zoom.** Connecté : chercher un lieu Internet, puis un lieu local après échec/résultat vide. Sans connexion : chercher directement dans les données locales disponibles. Faire 30 changements de zoom avant la fin des chargements ; l'image doit correspondre à la dernière vue. Revenir dans Fichiers après utilisation intense : la SD doit rester accessible.
3. **Mémoire.** Relever `m`, ouvrir/fermer Cartes dix fois, puis démarrer/arrêter la radio dix fois. Comparer aussi après retour des tâches `[STACK]`, pas uniquement immédiatement au clic. Une baisse persistante du plus grand bloc interne malgré retour au même état est pertinente ; PSRAM libre seule ne suffit pas.
4. **Dessin.** 30 clics isolés puis 30 traits, sur un puis plusieurs calques. Gomme, déformation, sélection à progressivité positive/négative ; annuler/rétablir après déplacement/suppression de calque et redimensionnement. En cas de flash, comparer `d` avant/après. Ne pas écraser un document de travail pour ce test.
5. **Média et vidéo.** Passer entre Musique/Radio/Enregistrements/Vidéo, puis recliquer l'onglet actif. Tester une vidéo locale et une vidéo stream avec musique en cours. Noter la transition audio et conserver le journal complet de tout reset. Pour le claquement MP3, relever fichier, format, volume et sortie utilisée.
6. **Repos.** Accueil visible pendant trois minutes, puis Média sans lecture pendant trois minutes. Si flash : noter l'heure et comparer compteurs de rendu/publication/underrun. Une hausse des underruns avec observation active indique une sous-alimentation du DSI, sans identifier à elle seule sa cause.
7. **Réglages série.** Modifier le débit/port ; l'interface doit continuer à répondre et le succès apparaître après application. Refaire pendant un envoi : refus cohérent, aucune perte de paramètres. Éviter de changer le port portant le journal en cours sans avoir un autre moyen de le récupérer.

## Informations utiles en cas d'échec

Version exacte, type de dalle indiqué au démarrage, action précise, application précédente, lecture/pause/arrêt, source locale ou distante, taille/dimensions/format du fichier, texte d'erreur, `m` et `d` avant/après. Pour un reset : garder le journal **avant** le redémarrage et le backtrace complet, pas seulement la dernière ligne. Les essais imprimante USB et Meshtastic nécessitent les appareils correspondants et restent distincts.
