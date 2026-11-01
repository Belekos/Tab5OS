# Tab5OS 0.8.22

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.22 réussis.** Preuve : [firmware-verification.json](tools/verification/firmware-verification.json).
<!-- /build-status -->

Les tests logiciels, la compilation cible et les essais physiques sont distingués dans [VERIFICATION.md](VERIFICATION.md). Aucun appareil n’est flashé par les outils de préparation.

## Cartes

Le curseur permet un zoom visuel jusqu’à 22. Lorsque la source s’arrête plus tôt, le lecteur agrandit ses dernières tuiles : il n’invente pas de nouveaux détails et ne demande pas des tuiles inexistantes. L’import PC et tablette applique le même calcul et indique le niveau effectivement téléchargé. La fréquence des requêtes est réglable dans les limites du fournisseur, avec temporisation des reprises et arrêt sur limitation HTTP 429. Les restrictions de téléchargement des fournisseurs publics sont conservées.

Le mode informations donne les coordonnées du point touché, l’échelle et la source, ainsi que les lieux enregistrés à proximité. Le raccourci I vise le centre de la carte. Une image cartographique ne permet pas d’identifier automatiquement tous les bâtiments ou commerces : aucun géocodage caché n’est lancé. Voir [CARTES.md](CARTES.md) et [CARTES_PC.md](Tab5OS_Arduino_v0.8.22_Ecriture/tools/CARTES_PC.md).

## Lecture

Wikipédia compose le texte, les petits tableaux et les images dans l’espace disponible. Un défaut interrompait le tableau après la première cellule : les cellules restantes sont désormais conservées et les grands tableaux peuvent défiler. Les allocations de composition sont bornées et les anciens caches d’image sont libérés avant remplacement. Les liens et polices arabe, chinoise, japonaise et coréenne sont inclus dans les essais de rendu hôte.

Le lecteur ebook dispose d’un curseur global. Pendant le glissement, il indique la page qui sera ouverte ; il ne lit pas un autre chapitre et n’enregistre pas de progression. Au relâchement, il prépare progressivement la destination. Le curseur est activé après la fin du comptage exact du livre. Un échec de préparation ou de rendu conserve le document et le marque-page précédents ; la possibilité de repeindre les pixels dépend encore de la mémoire disponible. Voir [LECTURE_EBOOK.md](LECTURE_EBOOK.md).

## Manettes et interface

HID et XInput partagent toujours l’hôte USB existant. Les corrections portent sur les interfaces composites, les endpoints, les erreurs d’allocation, la suspension/reprise et la déconnexion. Elles évitent notamment de libérer le périphérique parent avant la fermeture de ses interfaces. Les tests reproduisent les défauts de la version précédente ; ils ne remplacent pas un essai de la SN30 Pro physique. Les modes filaires sont décrits dans [MANETTES_USB.md](MANETTES_USB.md).

L’icône Thème est retirée de l’accueil. Le crayon présent dans les réglages Interface reste le point d’accès à l’éditeur.

## Git, SSH et ressources

Git utilise libgit2 pour travailler sur de vrais dépôts, y compris les objets compressés. Le terminal et l’Explorateur proposent les opérations prises en charge sur un dossier de projet. L’interface recycle ses lignes d’affichage et permet d’accéder à tous les fichiers par tranches ; le nombre total reste exact. Les imports et exports réseau utilisent HTTPS et le contrôle réseau de l’OS.

SSH utilise libssh2 et le mbedTLS existant du SDK. Il permet les commandes distantes, les pipes et les transferts SFTP. L’empreinte du serveur doit être vérifiée avant de lui accorder confiance ; une clé modifiée est refusée. Les identifiants se lisent depuis un fichier, pas depuis un argument contenant le secret. Le terminal PTY interactif n’est pas inclus. Les syntaxes et limites sont dans [TERMINAL.md](TERMINAL.md).

Le travail de résilience corrige des ressources conservées à la fin du worker Pentest et ajoute des diagnostics de tâches. Les démarrages en échec, annulations, fermetures et allocations insuffisantes sont exercés. Le partage de fichiers et la recherche réseau d’imprimantes passent par l’admission commune.

La fuite Pentest est reproduite puis corrigée sur le banc hôte. Le reset immédiat signalé n’a pas été reproduit ; une trace de phase conservée après redémarrage facilite son diagnostic sur tablette. Le bilan complet précise les limites restantes : processus coopératifs, ELF de confiance partageant l’espace mémoire, allocations de bibliothèques et absence de mesure physique des piles et heaps. Voir [AUDIT_RESSOURCES_0.8.22.md](AUDIT_RESSOURCES_0.8.22.md).

## Développement et langues

Le paquet contient les sources, les notices, un guide de compilation, les règles de contribution et des exemples ELF/Python vérifiés. Le script PowerShell détecte la version du sketch au lieu d’en sélectionner une ancienne. Le compilateur d’exemples ELF dispose d’une voie Clang/LLD et refuse les imports hors API compatible.

La préparation GitHub n’attribue pas arbitrairement une licence au code principal. [LICENCES.md](LICENCES.md) inventorie les composants et restrictions, dont celles du cœur SNES. Le choix de licence et la destination de publication restent à résoudre avant une mise en ligne.

<!-- resource-summary -->
Les 14 packs couvrent les 2859 clés de référence contrôlées. Factory contient 93 ressources officielles ; le secours firmware en contient 39. Les grandes polices et les dictionnaires de saisie sont distribués sur SD. Preuves : [languages-0822.json](tools/verification/languages-0822.json), [ime-0819.json](tools/verification/ime-0819.json), [resources-0822.json](tools/verification/resources-0822.json).
<!-- /resource-summary -->

Mettre à jour ensemble le firmware et les ressources officielles SD en préservant les données personnelles. Suivre [LISEZ_MOI.md](LISEZ_MOI.md).
