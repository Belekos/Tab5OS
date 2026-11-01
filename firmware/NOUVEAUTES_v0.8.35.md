# Tab5OS 0.8.35 — Dessin et sélection des fichiers

## Explorateur

Un fichier choisi dans l’aperçu de dossier à droite devient la sélection réelle de l’explorateur. Son dossier devient le dossier courant et sa ligne est retrouvée à gauche, y compris lorsque la liste doit être chargée ou paginée. Les actions travaillent sur cette sélection commune.

Les images prises en charge disposent des choix Visualisateur et Dessin dans Ouvrir avec. Les projets T5I conservent leur ouverture directe dans Dessin et peuvent aussi être consultés avec le visualisateur.

## Dessin

- Bouton gauche : première couleur ; bouton droit : seconde couleur, sans menu contextuel dans l’image.
- Texte saisi directement à la position choisie dans le dessin ; Entrée valide, Échap annule.
- Volet d’options fermé au départ et superposé à l’image : son ouverture ne change pas le cadrage.
- Taille initiale de 2 px ; coordonnées visibles au survol et pourcentage de zoom lisible aux fortes valeurs.
- Suppression du titre répété dans le bandeau, du second bouton de permutation et des intitulés redondants. Transformation de l’image entière distinguée de la déformation locale.
- Import aux dimensions originales, sans réduction automatique des JPEG ou BMP. Un manque de mémoire est signalé sans remplacer le document déjà ouvert.

Voir [DESSIN.md](DESSIN.md) pour les formats et limites, [EXPLORATEUR.md](EXPLORATEUR.md) pour la sélection et [VERIFICATION.md](VERIFICATION.md) pour les résultats effectifs.

La politique mémoire, Git et les corrections d’affichage des versions précédentes sont conservés. Aucun nouvel effacement de mémoire périodique n’est ajouté.
