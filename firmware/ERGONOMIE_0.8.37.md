# Étude ergonomique — Explorateur 0.8.37

Cette étude compare le comportement de la 0.8.36 au contrat demandé pour la 0.8.37. Elle comprend une revue indépendante du sketch, des menus, de la construction des panneaux et des styles de lignes. Les constats issus du code, les essais hôtes et les validations encore nécessaires sur Tab5 sont distingués. Une capture ou un test de focus isolé ne suffit pas à valider un parcours de navigation.

## Défauts constatés dans la 0.8.36

| Constat vérifié par lecture du code | Conséquence pour l’utilisateur |
|---|---|
| `previewSelectedFile()` renseigne les métadonnées et les commandes, mais ne remplit plus le titre, le texte ni l’image d’aperçu. | Un clic change l’en-tête secondaire sans afficher le fichier sélectionné. |
| Le clic gauche sélectionne seulement ; le dossier droit attend la fenêtre de double-clic de 400 ms avant de s’ouvrir. | L’affichage semble ignorer certains clics et retarde systématiquement les autres. |
| Le chemin droit dépend de l’ouverture du dossier, tandis que les informations dépendent de la sélection. | Le nom, le chemin, le contenu et la cible des commandes peuvent désigner des objets différents sans explication. |
| L’actualisation initialise automatiquement l’arbre racine et le contenu droit. | L’explorateur charge un dossier qui n’a pas été demandé. |
| Le bouton parent agit différemment selon le panneau actif ; à gauche il peut seulement replier l’arbre. | La même icône ne produit pas toujours un déplacement vers le dossier parent. |
| La fermeture de Git réouvre un dossier, même si un fichier était consulté auparavant. | Le retour ne restitue pas nécessairement la vue quittée. |
| L’en-tête droit réserve une largeur fixe pour les raccourcis. | Le chemin est tronqué même lorsque peu d’actions sont affichées. |

Les ralentissements et scintillements sont rapportés par l’utilisateur ; une capture ne permet pas de les mesurer.

## Règles de navigation retenues

La gauche sert à parcourir l’arborescence. La droite montre un dossier ou l’aperçu d’un fichier. Une sélection explicite détermine les commandes ; déplacer le focus ne doit pas, à lui seul, modifier leur cible.

| Geste | Affichage à droite | Effet à gauche |
|---|---|---|
| Premier accès à l’explorateur, sans demande de dossier | Aucun contenu ouvert automatiquement | Arbre vide ; aucun parcours de la racine |
| Choisir volontairement Carte SD ou USB dans Navigation | Vide, jusqu’au clic sur un objet | Chargement de l’arbre correspondant |
| Clic sur un dossier à gauche | Contenu et chemin de ce dossier | Sélection de sa ligne, sans dépliage automatique |
| Clic sur un fichier à gauche | Aperçu du fichier et son chemin | Sélection de sa ligne |
| Clic sur un dossier à droite | Contenu et chemin de ce dossier | Arbre conservé |
| Clic sur un fichier à droite | Aperçu du fichier et son chemin | Arbre conservé |
| Double-clic sur un dossier à droite | Le même dossier, une seule navigation | Révélation et focus sur sa ligne |
| Double-clic sur un fichier à droite | Le même fichier | Révélation du dossier qui le contient ; le fichier reste la cible des commandes |
| Double-clic lorsque le dossier à révéler est la racine SD/USB | Vue droite conservée | Focus sur le conteneur gauche et retour en haut ; aucune fausse ligne racine ajoutée |
| Chevron à gauche | Aucun changement | Dépliage/repli uniquement |
| Ctrl/Maj, appui long ou mode de sélection multiple | Pas de navigation qui détruirait la sélection en cours | Sélection multiple dans le panneau utilisé |
| Clic droit | Pas d’ouverture involontaire | Menu portant sur la cible du clic ou la sélection multiple déjà concernée |

Aucune racine n’est ouverte au premier accès. Le choix du support reste accessible par Navigation. Un retour ultérieur conserve la vue de session si le support reste disponible.

## Cohérence de l’affichage et des commandes

Le panneau droit est vide, affiche un dossier, un aperçu, Git ou une erreur. Son chemin et son contenu changent ensemble. Une erreur de lecture remplace l’ancien contenu. Un fichier inconnu affiche ses informations ; sa sélection n’exécute aucun programme.

Depuis un aperçu, Parent, Retour arrière et Alt+Haut reviennent au dossier contenant le fichier, puis au parent. Les flèches gauche/droite dans l’arbre servent au repli/dépliage. Fermer Git restitue le mode et le chemin précédents, aperçu compris.

Copier, couper, renommer, supprimer et ouvrir portent sur la dernière sélection explicite. Coller, créer, cloner et le terminal utilisent son dossier ou, après une remontée, le dossier affiché à droite. Révéler un parent ne le sélectionne pas. Sans sélection, Suppr ne récupère aucun document ouvert ailleurs.

Annuler une suppression conserve la vue. Renommer ou déplacer un dossier remappe les chemins affichés, descendants compris. Ces cas ont été signalés pendant la revue puis corrigés dans le code.

Une ligne verticale fine sépare les panneaux. Les fichiers restent sans contour individuel, avec leur nom sur une ligne. Le chemin occupe sa propre ligne au-dessus des commandes. La surbrillance indique la sélection ; un repère discret distingue le focus clavier.

## Réactivité

Le clic simple agit sans attendre le délai du double-clic. Le deuxième clic conserve la cible initiale même si sa ligne a disparu ; il ne doit pas ouvrir un enfant apparu au même endroit. Le code évite de réutiliser une ligne après destruction de sa liste.

Les styles communs ne sont recalculés que si le thème ou la police change. Une sélection inchangée ne réécrit pas les lignes. Le chemin et la révision permettent de réutiliser les métadonnées et le contenu. Les lignes existantes restent affichées pendant les scans. Ces réductions de travail ne mesurent pas la fluidité sur Tab5.

## Matrice de validation

| Parcours et résultat attendu | Vérification adaptée |
|---|---|
| Premier accès : aucun arbre ni contenu racine ; Navigation SD/USB charge seulement l’arbre demandé. | Gestes et compte des lectures dans le harnais explorateur ; capture hôte de l’état vide. |
| Clic gauche puis droit sur dossier, texte, image et type inconnu : chemin et contenu correspondent à la cible dès le premier clic. | Événements LVGL et données de stockage injectées ; texte réellement lu par le code de production ; rendu des widgets vérifié séparément. |
| Double-clic droit malgré le remplacement de la ligne : une seule navigation ; le focus gauche révèle la cible d’origine. | Deux événements aux mêmes coordonnées, puis contrôle du chemin, du focus et de la sélection. Inclure les parents racines SD et USB. |
| Double-clic droit d’un fichier : son parent reçoit le focus, Renommer/Supprimer restent ciblés sur le fichier. | Contrôler séparément focus, sélection et chemin visible ; ne pas déduire la cible d’une surbrillance. |
| Chevron, Ctrl/Maj, sélection multiple, appui long et clic droit : aucune navigation involontaire. | Harnais des gestes et menus ; contrôler les états avant et après fermeture du menu. |
| Tab et flèches parcourent les contrôles ; Entrée active la ligne ; Parent, Retour arrière et Alt+Haut remontent depuis la vue droite. | Vérifier le focus et le dossier cible de Nouveau/Coller/Git après la remontée, notamment après une sélection gauche. |
| Ouvrir puis fermer Git restitue le mode, le chemin et le contenu précédents. | Contrôler une liste, un texte et une image ; le scope Git doit être fermé avant la restauration. |
| Suppr sans sélection ne cible aucun autre document ; annuler une suppression conserve le contenu. | Revue des branches de suppression et essai du dialogue sur un dossier affiché ; ne pas se limiter au bouton désactivé. |
| Renommer ou déplacer une entrée affichée conserve la cohérence du chemin ; un fichier supprimé ou un support retiré ne laisse pas d’aperçu périmé. | Revue des remappages de chemins et essais de mutation/retrait ; inclure un descendant du dossier renommé ou déplacé. |
| Ligne fine, chemin lisible au-dessus des commandes, noms sur une ligne, focus distinct de la sélection. | Captures LVGL avec la police livrée aux deux tailles prises en charge. |
| Au repos, aucune reconstruction ni écriture de style inutile ; les opérations de langue ne parcourent pas plusieurs fois les mêmes données. | Comptes de travail et rapports hôtes ; mesures SD, entrée physique, démarrage et scintillement à refaire sur Tab5. |

## Résultats et limites

La lecture du code 0.8.37 confirme les états séparés, l’affichage depuis les deux panneaux, la reconnaissance du double-clic après remplacement de la liste, le démarrage sans parcours implicite et le séparateur de 1 pixel. Les corrections supplémentaires signalées pendant la revue ont été relues : Parent cible désormais la droite ; renommage et déplacement remappent les chemins ; demander puis annuler une suppression conserve la vue ; Suppr sans sélection ne cible plus un document externe.

Les rapports `tools/verification/explorer-0837.json` et `tools/verification/explorer-ui-0837.json` identifient les sources testées. Les résultats hôtes consultés sont réussis ; leur régénération sur les sources finales appartient au bilan de livraison. Les captures de l’entrée vide et de l’aperçu texte ont été inspectées : panneaux distincts, chemin lisible et commandes sans chevauchement. Leurs données sont injectées ; elles ne prouvent pas le déclenchement d’une action.

Le bilan de la livraison, sur les sources finales, est consigné dans [VERIFICATION.md](VERIFICATION.md). Aucun essai sur le Tab5 physique n’a été effectué dans cette étude. Les durées réelles de démarrage, les performances de la carte SD et l’absence de scintillement restent à confirmer sur l’appareil.
