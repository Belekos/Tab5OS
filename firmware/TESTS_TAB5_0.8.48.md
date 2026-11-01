# Validation sur Tab5 — 0.8.48

Ces essais n'ont pas été exécutés sur une tablette ici. Conserver une copie des fichiers et de la version précédente. Utiliser la mise à jour normale du firmware sans effacement global ni changement de partitions ; ne pas remplacer les données utilisateur par une copie du dossier Factory.

## Relevé commun

Capturer la liaison série depuis le démarrage. La commande `m` fournit les informations mémoire et `d` les compteurs d'affichage. Vérifier ces commandes dans l'aide série de la version installée. Relever aussi la révision matérielle, le type de connexion audio, les options Wi-Fi et la résolution des streams. Ne transmettre ni mots de passe ni clés ; masquer les adresses privées sensibles si elles apparaissent dans d'autres journaux.

Pour chaque essai : relever l'heure du flash/pop/erreur, la page et l'action, puis les commandes de diagnostic. Si possible filmer l'écran avec le journal horodaté. Une erreur DSI et un grand rafraîchissement LVGL sont des observations différentes.

## 1. Flashs au repos et dans Média

1. Après démarrage, laisser Accueil immobile 5 minutes avec Wi-Fi, puis une autre page 5 minutes. Noter tout flash sans interaction.
2. Dans Média, alterner MP3, Radio, Enregistrements et Vidéo 50 fois ; attendre aussi quelques secondes après certains changements pour couvrir la persistance des préférences.
3. Comparer les lignes `[DSI]`, `[DSI underrun]`, `[MEDIA PREFS]`, `[DISPLAY]` et `[LVGL]` avant/après. Le compteur d'erreurs est cumulatif ; les messages peuvent regrouper plusieurs événements.
4. Refaire avec musique en lecture puis en pause. Signaler un flash même sans incrément d'underrun : le compteur ne couvre pas nécessairement tous les défauts physiques.

## 2. Début des musiques

1. Après un démarrage à froid, lancer le fichier `tools/diagnostic-audio/silence_48k.wav` depuis une copie sur SD. Aucun son n'est attendu du fichier lui-même.
2. Lire une musique, l'arrêter complètement, puis relancer le fichier silencieux. Répéter 20 fois à volume modéré, puis avec plusieurs fichiers musicaux usuels.
3. Tester séparément pause/reprise, piste suivante et radio. Distinguer un pop au premier démarrage d'un résidu après une piste précédente ; vérifier que le début musical n'est pas tronqué.

## 3. Deuxième stream et cycles mémoire

1. Relever `m`, lancer un stream A, demander B alors que A charge encore, puis C rapidement. Le dernier choix doit être respecté.
2. Arrêter pendant le changement, fermer la page et la rouvrir. Aucun stream annulé ne doit redémarrer seul ; aucune image libérée ne doit être affichée.
3. Effectuer 20 cycles de deux streams. Après chaque fermeture, attendre que les tâches idle aient pu rendre leurs piles, puis relever mémoire interne libre/minimale/plus grand bloc et PSRAM.
4. Tester aussi B après qu'A soit entièrement fermé : cette arrivée tardive ne bénéficie pas forcément du même worker. En cas de refus, conserver la ligne d'admission mémoire exacte, pas seulement le total PSRAM.
5. Couper le réseau pendant le chargement, puis le rétablir. Vérifier retour utilisable et arrêt propre. Aucun reset ne doit être considéré comme une récupération acceptable.

## 4. Dessin et historique

Sur une copie d'image 1280 × 720 : premier point, traits longs, déformation, sélection active, annuler/rétablir et annuler un geste. Vérifier le contenu pixel à pixel visuellement aux bords, puis rouvrir le document enregistré volontairement. Répéter 20 ouvertures/fermetures et relever mémoire. Ne pas valider seulement la baisse du flash : l'exactitude de l'historique et des sélections est indispensable.

## 5. Coexistence et conservation

Radio en lecture puis en pause : zoomer/déplacer Cartes avant la fin des chargements, rechercher un lieu en ligne puis hors ligne, ouvrir des fichiers SD. Vérifier retour à Média, disponibilité SD et absence de message Wi-Fi erroné. Redémarrer normalement et vérifier préférences Média, paramètres, documents et déverrouillage. Aucun test de cette liste n'autorise à réinitialiser les secrets ou à formater un support.

## Critères

Pas de reset, de corruption, de perte de fichier ou de redémarrage d'un flux annulé. Pas de baisse continue de mémoire après les mêmes cycles stabilisés. Aucun flash ni pop reproductible. Si un symptôme subsiste, joindre le scénario minimal, le firmware exact et les journaux avant/après ; les résultats logiciels inclus ne remplacent pas cette qualification.
