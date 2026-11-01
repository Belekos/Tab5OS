# Lecture EPUB — 0.8.22

La glissière du bas permet de choisir une page dans l'ensemble du livre. Pendant le déplacement, le numéro de la page visée et le nombre total de pages remplacent momentanément le statut. La page affichée ne change qu'après relâchement. Le lecteur prépare alors la destination progressivement et conserve la page courante pendant cette préparation.

Au clavier, utiliser Tab pour sélectionner la glissière, Entrée pour la régler, les flèches pour choisir et Entrée pour ouvrir la page. Échap annule le réglage. Page précédente/suivante continue de tourner les pages.

Le nombre de pages dépend de la taille du texte et de la mise en page. Lors de la première ouverture ou après un changement de ces paramètres, le lecteur calcule l'index du livre en arrière-plan. La glissière devient disponible une fois le total exact connu ; le statut « Mise en page. » et la progression approximative restent visibles en attendant. Un pourcentage précédé de `~` est une estimation, jamais un nombre de pages présenté comme exact.

L'index est enregistré sur le stockage lorsqu'il est complet et réutilisé aux ouvertures suivantes. La version 0.8.22 recalcule une fois les index antérieurs, car la zone de lecture réserve désormais l'espace de la glissière. Les marque-pages par section et position de texte restent compatibles.

Pour rejoindre un chapitre non chargé, le lecteur réutilise le document de travail de l'indexeur ; il ne charge pas plusieurs chapitres de fond simultanément. Il traite au plus une page de mise en page par passage du service, puis transfère le document prêt à la lecture sans le parser une seconde fois. Une destination déjà paginée dans le chapitre courant est immédiate. La destination ne remplace le marque-page qu'après un rendu réussi. Une allocation, une mesure ou un rendu impossible conserve le document et le marque-page précédents. Si la mémoire manque aussi pour repeindre l'ancienne page, son affichage peut rester incomplet ; sa position sauvegardée reste intacte.
