# Tab5OS 0.8.36 — Explorateur

La dernière sélection à gauche ou à droite devient la cible des actions. À droite, un clic sur un dossier ouvre son contenu uniquement à droite ; un double-clic l’ouvre et place aussi le focus sur ce dossier dans l’arbre gauche. Le simple clic attend la fenêtre de double-clic de 400 ms pour éviter qu’un deuxième clic ouvre un enfant apparu à la même position. Ctrl et Maj restent des gestes de sélection et ne lancent pas cette navigation.

Les dossiers de l’arborescence gauche disposent d’un chevron sans contour. Il développe ou replie leurs enfants sans changer la sélection ni le dossier affiché à droite. Un clic sur la ligne sélectionne ; un double-clic ou Entrée ouvre le dossier à droite. Un double-clic ou Entrée sur un fichier ouvre son application. Lors d’un double-clic sur un dossier à droite, l’arbre révèle sa ligne en développant ses ancêtres et en changeant de page si nécessaire.

La surbrillance provient d’un état de sélection commun aux deux panneaux. Restaurer le focus après un menu ne sélectionne pas à nouveau une ancienne ligne. Le clic droit choisit sa cible avant de montrer les commandes. Ctrl et Maj servent à sélectionner plusieurs objets.

L’en-tête gauche occupe moins de hauteur. Les fichiers et dossiers du panneau droit sont présentés sur des lignes plates, avec les couleurs, polices et icônes de l’OS.

Les dossiers sont parcourus progressivement et seulement lorsqu’ils sont demandés. Les métadonnées utilisent la PSRAM ; le nombre de widgets reste borné. Replier un dossier libère son sous-arbre lors de cette action. Il n’y a pas de vidage périodique des applications.

Les corrections Dessin 0.8.35 et les moteurs Git, mémoire, cartes et affichage sont conservés. Voir [EXPLORATEUR.md](EXPLORATEUR.md) pour l’usage, [VERIFICATION.md](VERIFICATION.md) pour les preuves et limites, et [LISEZ_MOI.md](LISEZ_MOI.md) pour installer firmware et ressources SD ensemble.
