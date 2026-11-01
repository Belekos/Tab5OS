# Tab5OS 0.8.40

Cette version reprend la livraison 0.8.39 sans retirer ses composants. Le firmware, les quatorze packs de langues et les ressources SD doivent être mis à jour ensemble. Les rapports anciens restent historiques ; les essais .40 se trouvent dans `tools/verification`.

## Réglages et informations système

Les sections des réglages se disposent dans une grille adaptative, avec une hauteur propre à chaque section. **Polices et texte…** ouvre une fenêtre dédiée. Les informations système montrent séparément SRAM interne, PSRAM externe et occupation de la partition de l’application, avec des barres et les valeurs libres/utilisées. L’alimentation, les connexions et les supports restent visibles ; les détails techniques s’ouvrent à la demande.

Les jauges présentent de vraies capacités mémoire et stockage. Elles ne prétendent pas mesurer une charge CPU instantanée. Les compteurs d’écran aident à documenter un défaut observé.

## Éditeur

La coloration utilise des vocabulaires et règles distincts pour chaque langage. Le choix **Langage du document…** permet de conserver la détection par extension ou d’imposer un langage au document. Les mots-clés, commentaires, chaînes, nombres et directives suivent leurs règles propres ; les noms d’un autre langage ne sont plus regroupés dans une liste universelle.

C/C++, Python, JSON/JSON avec commentaires, JavaScript/TypeScript, CSS, HTML/XML, YAML, TOML, INI, Shell, Lua, SQL, Rust, Go, Java, Kotlin, Swift, Ruby, PHP, PowerShell, CMake et Makefile ont leurs profils. Il s’agit de coloration syntaxique, pas d’un compilateur ni d’un validateur complet.

## Dessin

Les sélections comprennent rectangle, ellipse, lasso libre, baguette contiguë et plage de couleurs. Une sélection peut remplacer, compléter, soustraire ou croiser la précédente ; elle peut être inversée et ses bords adoucis. La tolérance, la progressivité et l’échantillonnage des calques visibles sont réglables.

Le remplissage peut s’appliquer à une zone contiguë ou à toute la sélection. Il propose couleur unie, dégradés linéaire/radial, hachures, damier et points, avec densité, direction et taille de motif. L’arrêt aux contours et son seuil limitent la propagation.

Les calculs longs restent coopératifs et annulables. Les refus d’allocation préservent l’image ; les sélections utilisent des masques bornés à l’image. Ces outils sont intégrés à Dessin, dont le dossier par défaut reste `/Picture`.

## Cartes hors ligne et Meshtastic

La recherche locale utilise l’index GeoNames livré sur SD : **171 590 lieux**, avec noms et variantes. La couverture porte sur les localités de plus de 1 000 habitants et les sièges administratifs présents dans la source ; elle ne constitue pas un répertoire de toutes les rues, adresses ou lieux touristiques. Une recherche peut trouver un lieu sans connexion réseau ; l’affichage du fond de carte dépend toujours des tuiles locales ou en ligne disponibles.

L’index de 37 831 158 octets est lu par petites fenêtres de 4 Kio, avec au plus huit résultats à la fois. Il n’est ni chargé entièrement en RAM, ni embarqué en flash, ni haché au démarrage. Sa provenance, ses empreintes et l’attribution CC BY 4.0 sont dans `System/maps/gazetteer/README.txt` et `cities.json`. Le miroir Factory permet une réparation explicitement demandée.

Les waypoints et positions Mesh peuvent être marqués sur la carte. L’affichage des nœuds Meshtastic est optionnel. Un message contenant une position permet de rejoindre Cartes. Les appareils connus et les préférences Mesh sont conservés ; la reconnexion automatique et la veille en arrière-plan sont des options. L’historique borne le nombre de messages et conserve horodatages et positions disponibles. Une pastille du bandeau indique l’état Mesh et les messages non lus.

L’interface Mesh reprend la palette de l’OS, les icônes de recherche/envoi et le focus de saisie. Entrée envoie le message. Le contrôle mémoire Bluetooth utilise un budget SRAM estimé à partir de la configuration NimBLE/ESP-Hosted du SDK : la présence de PSRAM libre ne suffit pas à garantir le démarrage du transport.

## Bibliothèque ELF et stockage USB

La bibliothèque d’applications surveille `/Applications` et `/Apps` sur SD, ainsi que leurs équivalents USB. Elle se rafraîchit après les notifications de fichiers et lance une vérification toutes les cinq secondes quand l’accueil ou la bibliothèque est visible, hors fenêtre superposée et personnalisation. Le focus et le défilement sont conservés. Un ELF ajouté apparaît et un ELF retiré disparaît sans redémarrer. Le catalogue est remplacé seulement après un parcours terminé ; un parcours annulé, incomplet ou sans mémoire conserve le dernier catalogue valide. Une liste inchangée ne déclenche pas de reconstruction inutile. Les limites restent 64 applications et 384 entrées parcourues ; les autres fichiers restent accessibles depuis l’Explorateur.

La fin du formatage USB publie désormais une notification après la libération du worker. Cela corrige l’absence de rafraîchissement lorsque l’interface avait consommé la notification précédente pendant l’opération, et rend de nouveau visible l’état final permettant le remontage. Ce défaut confirmé n’explique pas nécessairement tous les échecs de remontage matériels. Les contrôles hôtes font réellement créer, monter, écrire, lire, démonter, reformater et remonter un volume FAT avec les sources FatFs de l’ESP-IDF utilisé. Ces contrôles ne remplacent pas l’essai du contrôleur USB et d’une clé physique.

## Scintillements, mémoire et vérification

Les compteurs de rafraîchissement permettent d’identifier les grosses mises à jour et leur origine sans forcer de redessin périodique. Les tests couvrent l’échange des tampons, les délais et les erreurs injectées. Aucun déclencheur de réinitialisation d’écran toutes les 90 secondes n’a été confirmé. La disparition de tous les scintillements n’est donc pas annoncée ; les diagnostics sont destinés à relier une observation matérielle à l’activité réelle.

Les quatorze langues conservent toutes leurs traductions antérieures. Les nouveaux textes, formats numériques et sauts de ligne sont audités. Les packs restent sous 512 Kio et 4 096 entrées ; aucune augmentation de ces plafonds n’est nécessaire.

La compilation cible, les tests de production sur ordinateur avec ASan/UBSan, les captures LVGL et les contrôles de préservation constituent les preuves distribuées. **Aucun essai sur Tab5 physique, appareil Meshtastic ou imprimante USB n’est revendiqué.** Le temps de démarrage, la fluidité tactile, la radio réelle et les clés USB doivent être mesurés sur le matériel.

Les chantiers TCC/dépendances, Bitchat et adaptations d’outils Flipper ne sont pas déclarés terminés dans cette version. Leur périmètre précédent est conservé dans `EVOLUTIONS_0.8.39.md`.
