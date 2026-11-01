# Tab5OS 0.8.37 — Explorateur et langues

La 0.8.36 présentait des défauts de parcours : un clic pouvait modifier les informations sans afficher l’aperçu, le panneau droit retardait l’ouverture de dossier de 400 ms, et le retour de Git ne retrouvait pas toujours la vue précédente. L’[étude ergonomique](ERGONOMIE_0.8.37.md) décrit ces constats et le contrat de correction.

Un clic sur un objet, dans l’un ou l’autre panneau, demande son contenu ou son aperçu à droite. Le chemin et le contenu changent ensemble ; un échec d’ouverture doit être affiché pour le chemin demandé. Le clic simple n’attend plus systématiquement la fenêtre de double-clic. Le deuxième clic garde la cible initiale même si la première ouverture a remplacé les lignes.

À droite, le double-clic sur un dossier révèle aussi sa ligne à gauche. Sur un fichier, il révèle son dossier parent tout en conservant le fichier comme cible des commandes. Les chevrons ne font que déplier ou replier l’arbre. Les sélections multiples, Ctrl/Maj et le clic droit ne déclenchent pas de navigation involontaire.

Au premier accès, aucun contenu racine n’est chargé sans demande. Le menu Navigation permet de choisir Carte SD ou USB et charge l’arbre à gauche ; la droite reste vide jusqu’au choix d’un objet. Une session déjà utilisée retrouve sa vue si le support est disponible. Parent revient du fichier vers son dossier, puis vers le dossier parent. Fermer Git retrouve la vue précédente.

Les travaux sur le chargement de langue et sur les listes visent à réduire les lectures et reconstructions inutiles. Les résultats réellement mesurés sur ordinateur, leur périmètre et les limites de validation sont consignés dans [VERIFICATION.md](VERIFICATION.md). Aucune vitesse ni absence de scintillement sur le Tab5 n’est déduite des tests hôtes.

Les moteurs Dessin, Git, mémoire et affichage sont conservés depuis la livraison 0.8.36. Installer firmware et ressources SD ensemble selon [LISEZ_MOI.md](LISEZ_MOI.md).
