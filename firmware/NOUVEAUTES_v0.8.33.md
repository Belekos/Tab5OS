# Corrections 0.8.33

Cette version traite en priorité la régression mémoire signalée après la 0.8.32. Les tests sur ordinateur reproduisent des comportements fautifs de la version livrée ; aucun essai physique sur Tab5 n’est revendiqué.

## Mémoire et stabilité de l’affichage

La purge après 30 secondes d’inactivité et le passage périodique de maintenance sont supprimés. Un échec d’allocation déjà géré par son appelant ne déclenche plus, plus tard, une collecte générale. L’OS vérifie d’abord la demande effective d’une opération ; seulement si elle ne tient pas, il peut récupérer des caches rechargeables d’autres applications, dans le domaine mémoire déficient.

Les ressources de l’application au premier plan et celles de l’opération demandeuse sont protégées. Cartes refuse également toute récupération de ses tuiles pendant son affichage ou une impression. Les tampons temporaires restent libérés à la fin ou à l’annulation des opérations. Les marges de sécurité ne sont pas abaissées.

Le pilote de bascule d’écran reste inchangé. Ses tests ne reproduisent ni effacement ni rafraîchissement déclenché par la seule inactivité. Les flashs signalés doivent encore être vérifiés sur tablette ; les tests hôtes ne mesurent pas la bande passante ni les interruptions réelles. Voir [MEMOIRE_0833.md](MEMOIRE_0833.md).

## Git

Les commandes de synchronisation utilisent la branche amont configurée, même si son nom diffère du nom local. Les commandes indiquent leur résultat et leur destination. Une connexion réseau non bloquante est vérifiée avant d’être considérée comme établie, ce qui permet d’essayer une autre adresse quand la première refuse la connexion.

Les commandes de statut, de différence et de préparation vérifient le contenu modifié malgré une taille et une date inchangées, cas possible sur FAT. Les changements du répertoire de travail notifient l’explorateur, y compris après une commande lancée au terminal. Les commandes de consultation ne reconstruisent plus inutilement la liste et l’aperçu masqué.

Rappel : préparer ajoute à l’index, valider crée un commit local, publier envoie les commits. Récupérer met à jour les références distantes ; tirer met à jour la branche locale. Aucune publication automatique de modifications non validées n’est ajoutée. Voir [GIT.md](GIT.md).

## Atelier Image intégré

L’éditeur utilise désormais les widgets LVGL, les couleurs, la police et le clavier de l’OS. L’accueil lui donne une entrée native. Un BMP s’édite par **Modifier l’image** ; un projet T5I s’ouvre directement. L’ancien ELF n’est plus distribué.

Le dessin met à jour la seule zone touchée dans un tampon RGB565 adapté à l’aperçu. Les contrôles ne sont plus redessinés dans une trame bitmap complète à chaque mouvement. Les tampons sont alloués à la demande auprès du gestionnaire mémoire. Masquer l’application libère l’aperçu dérivé, tout en conservant le document et son historique ; le retour recalcule cet aperçu une fois.

Les outils comprennent pinceau rond/carré, taille/opacité/contraste/diffusion, gomme, pipette, calques, annuler/rétablir, sélection rectangulaire, déplacement et transformation affine, texte bitmap, lignes/rectangles/ellipses pleins ou en contour, luminosité/contraste/saturation/gains RVB, résolution et recadrage.

Les projets T5I conservent les calques et la transparence. Import BMP non compressé 24/32 bits ; export BMP 24 bits aplati sur blanc. PNG/JPEG restent hors périmètre de cet éditeur. Limite de 1024 pixels par côté et de 8 calques, sous un budget commun de moins de 8 Mio de pixels ; ces maxima ne se cumulent pas librement. Historique borné à 16 états et 8 Mio, selon mémoire disponible.

L’enregistrement reste explicite. Fermer ou remplacer une image modifiée demande confirmation. Les sauvegardes utilisent un fichier temporaire et conservent l’ancien fichier en cas d’échec pris en charge ; cela ne garantit pas une écriture FAT atomique face à une coupure électrique.

## Installation et limites

Mettre à jour firmware et ressources SD ensemble selon [LISEZ_MOI.md](LISEZ_MOI.md), en préservant les données personnelles. Aucun changement ne réintroduit le refus Wi-Fi fondé sur une version C6, ni la sauvegarde forcée sur batterie critique. Les corrections de durée de vie Cartes/IME, le PIN depuis l’explorateur et les dialogues clavier restent en place.

Les preuves courantes, la compilation et les limites de validation sont indiquées dans [VERIFICATION.md](VERIFICATION.md). Aucun chiffre de fluidité réelle sur Tab5 n’est annoncé sans mesure matérielle.
