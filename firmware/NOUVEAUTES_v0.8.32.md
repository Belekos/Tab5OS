# Tab5OS 0.8.32

Cette version poursuit la 0.8.31 : Git dans l’explorateur, déverrouillage du coffre depuis le parcours de travail, clavier des dialogues et nouvelle application ELF Atelier Image. Les corrections précédentes de Cartes, d’admission mémoire Git, de récupération des caches et de cycle de vie audio sont conservées.

## Git dans l’explorateur

Le panneau Git occupe la partie droite de l’explorateur. La navigation dans les fichiers reste disponible et le chemin concerné reste visible. L’ouverture fonctionne sur le dossier courant, sur un dossier sélectionné et depuis un fichier appartenant à un dépôt ; le dépôt parent est recherché sans confondre sa racine et le sous-dossier de travail.

Les actions précisent leur portée. Un ajout ciblé concerne le fichier ou dossier indiqué ; l’ajout de tout le dépôt est explicite. La commande `git add .` du terminal respecte également le sous-dossier courant. Les différences peuvent être ciblées sur un chemin littéral. Les résultats et les erreurs restent visibles ; une identité Git manquante doit être renseignée avant un commit.

Des essais sur de vrais dépôts locaux contrôlent que les modifications d’index, les commits et la configuration sont effectivement écrits. Ils sont ensuite lus et vérifiés par Git sur ordinateur. Ces essais ne constituent pas un clone, fetch, pull ou push réseau sur la tablette.

## Coffre et clavier

Un dialogue de PIN réutilisable permet de déverrouiller le coffre depuis l’explorateur, puis de reprendre l’action en cours. Annuler ou fermer le dialogue invalide la demande ; un déverrouillage en retard ne doit pas réactiver le coffre après l’annulation. Le PIN n’est pas gardé dans le champ après le démarrage du traitement.

Entrée valide les saisies à une ligne ou avance au champ suivant dans les formulaires de PIN. Échap annule le dialogue actif ; une liste déroulante ouverte se referme d’abord. Les dialogues de l’explorateur et les formulaires des réglages/coffre utilisent cette navigation, avec restauration du focus et protection contre les callbacks d’objets détruits.

## Atelier Image, application ELF

`Applications/AtelierImage.app.elf` est un programme natif RISC-V chargé à la demande. Il se lance depuis Applications ELF, depuis un projet `.t5i` via le lanceur habituel, ou par **Modifier l’image** sur un BMP dans l’explorateur.

- Pinceau rond/carré : taille, opacité, dureté du bord (contraste) et diffusion ; gomme et pipette.
- Calques : ajout, duplication, suppression, ordre, visibilité, opacité et fusion ; annuler/rétablir.
- Sélection rectangulaire, déplacement, redimensionnement de la sélection, inclinaison et rotation par quarts de tour.
- Ligne, rectangle et ellipse, remplissage et/ou contour ; insertion de texte bitmap.
- Luminosité, contraste, saturation et gains rouge/vert/bleu ; réglages sur le calque actif ou plusieurs calques.
- Résolution, redimensionnement de l’image et recadrage sur la sélection.

Formats : import BMP 24/32 bits non compressé, export BMP 24 bits aplati sur fond blanc, et projet T5I conservant les calques et leur transparence. PNG/JPEG ne sont pas pris en charge dans cette application. Les opérations géométriques utilisent le plus proche voisin. La police de texte est une petite police bitmap ; ce n’est pas une composition typographique vectorielle.

L’image est limitée à 1 024 pixels par côté, huit calques et un budget de pixels inférieur à 8 Mio. Ces maxima ne sont pas simultanés : la taille et le nombre de calques se partagent le budget. L’historique retient au plus 16 états et 8 Mio, selon la mémoire disponible ; les états les plus anciens sont libérés au besoin. Une opération qui ne peut pas préserver l’image en cours échoue avec un message. Le travail reste en mémoire jusqu’à une sauvegarde explicite.

La PSRAM est demandée par blocs à mesure des besoins, avec un plafond de 16 Mio pour le moteur de document et d’historique ; les blocs entièrement libérés sont immédiatement rendus à l’OS, y compris pendant la session.

Les sorties sont écrites par blocs dans `/Images`. La validation remplace le fichier précédent après fermeture du fichier temporaire. Un échec de renommage conserve l’ancienne copie pour récupération à la prochaine ouverture/sauvegarde ; cela ne garantit pas l’atomicité d’une coupure électrique au niveau de la carte FAT.

## Services de l’OS

L’ABI ELF 6 ajoute un canevas tactile avec une file bornée d’événements clavier/pointeur, et les écritures progressives d’images. L’OS possède les objets LVGL, copie les pixels et les événements, contrôle l’allocation PSRAM et nettoie les ressources après l’arrêt effectif de l’application. Les ABI 1 à 5 et leurs contrôles de jeux sont conservées.

La SRAM interne et la PSRAM externe restent deux domaines d’allocation distincts. Les marges du gestionnaire mémoire ne sont pas abaissées. La désactivation de l’ancien verrou Wi-Fi sur le numéro de version C6 et l’absence de sauvegarde forcée sur batterie critique sont conservées.

Les preuves et limites d’essai figurent dans [VERIFICATION.md](VERIFICATION.md). Aucun essai sur un Tab5 physique n’est revendiqué.
