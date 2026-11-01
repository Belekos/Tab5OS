# Validation sur Tab5 — 0.8.49

Ces essais restent nécessaires : aucun écran, codec analogique, carte SD ou appareil Tab5 physique n’est disponible dans l’environnement de compilation. Ne pas présenter cette livraison comme une certification production.

Conserver les mêmes fichiers MP3/FLAC/vidéo et réglages que pour 0.8.48. La mise à jour ne nécessite ni formatage ni changement des partitions. Conserver le firmware précédent pour comparaison. Ne pas forcer une extinction pour les essais.

## Relevé initial

Sur la liaison série, envoyer `m` une fois avant la série pour la mémoire par catégorie, puis `p` pour activer le profilage UI et `d` pour son état et les compteurs DSI. `m` est une mesure détaillée volontairement coûteuse : ne pas l’envoyer en boucle ni pendant l’interaction dont on mesure le flash.

Noter la révision matérielle, les versions P4/C6 affichées, l’état Wi-Fi, la taille/résolution des fichiers et le dernier reset. Ne pas envoyer de secrets, PIN, mots de passe ou clés avec les journaux.

## Scénarios

1. **Repos** : accueil puis page Media, sans lecture, trois minutes chacune. Relever un flash éventuel et son heure ; envoyer ensuite `d`. Comparer les compteurs avant/après, pas leur seule valeur absolue.
2. **Navigation Media** : alterner radio, fichiers audio, enregistrement et vidéo vingt fois. Relever flashs et retard du pointeur ; puis `d`. Aucun changement de contenu ne doit entraîner une disparition durable des listes.
3. **Audio** : MP3 puis FLAC, pause/reprise, déplacement dans la piste, piste suivante, Stop. Distinguer le premier démarrage à froid des transitions. Après Stop et retour accueil, relever `m` ; refaire dix cycles avec les mêmes fichiers. Une lecture locale en pause conserve volontairement son décodeur pour reprendre.
4. **Vidéo** : lire une vidéo jusqu’à sa fin, puis une seconde ; fermer le lecteur ; répéter dix fois. Relever `m` aux mêmes étapes. L’image finale affichée conserve un tampon légitime ; les autres frames et les ressources de décodage doivent être rendues. Noter toute erreur mémoire, tout reset et l’URL/type de flux sans jetons privés.
5. **Dessin** : ouvrir la même image, premier trait, traits suivants, annuler/rétablir, changer d’outil. Relever si le flash apparaît au premier clic seulement et si `d` rapporte de nouveaux underruns. Vérifier que le dessin et l’historique restent corrects. Ne pas effacer l’historique pour améliorer artificiellement la mémoire libre.
6. **Coexistence** : lancer radio, ouvrir Cartes, zoomer pendant le chargement ; mettre radio en pause puis reprendre. Tester recherche Internet et repli local hors Wi-Fi. Vérifier accès SD après la série et l’enregistrement volontaire d’un document de test ; ne pas utiliser un document unique comme essai.

## Interprétation

- Des octets PSRAM libres ne prouvent pas qu’une pile interne ou un buffer DMA soit disponible. Comparer chaque catégorie séparément.
- Une frame encore affichée, un document et une lecture en pause ne sont pas des caches à purger arbitrairement.
- Une baisse progressive à étapes identiques sur plusieurs cycles est suspecte ; une réserve stable n’est pas à elle seule une fuite.
- Un flash avec hausse du compteur DSI oriente vers le transport/mémoire ; un flash sans hausse exige de vérifier aussi l’activité graphique et les limites de détection. Aucun de ces cas ne suffit seul à attribuer la cause.
- Le claquement au premier démarrage à froid reste une vérification acoustique/analogique distincte du nombre de redémarrages I²S lors des transitions.

Après la série, envoyer `d`, puis `m` ; désactiver le profilage avec `p`. En cas de reset, conserver la sortie de démarrage et le diagnostic précédent disponible via `d`.
