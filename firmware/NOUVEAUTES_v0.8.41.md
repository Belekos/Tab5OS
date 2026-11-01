# Tab5OS 0.8.41

Cette version reprend la livraison 0.8.40 et corrige Cartes, l’utilisation et la présentation des mémoires, Meshtastic et les sélections de Dessin. Les preuves de compilation et les essais sur ordinateur sont détaillés dans [VERIFICATION.md](VERIFICATION.md). Aucun essai sur le Tab5 physique n’a été effectué.

## Cartes

- La molette zoome autour du pointeur lorsque Cartes possède le focus. Les événements rapprochés sont regroupés ; la molette ne traverse pas les fenêtres, le clavier ou une opération d’impression/export. Les limites de zoom du style sont respectées.
- La recherche locale utilise l’index de lieux sur SD, les favoris et les points Meshtastic affichables, sans activer le Wi-Fi. Une recherche en ligne déjà demandée revient à la recherche locale si la connexion a disparu avant son exécution.
- L’index fourni couvre 171 590 lieux peuplés et sièges administratifs GeoNames. Il faut copier `System/maps/gazetteer/cities.t5geo` sur la carte ; ce fichier ne remplace pas les tuiles cartographiques et ne fournit pas toutes les adresses ni tous les points d’intérêt.
- Une reprise réseau ou audio conserve le canevas déjà visible si le centre, le zoom, le style et ses dimensions sont identiques. Elle ne l’efface plus brièvement avant de remettre les mêmes tuiles. Les changements de vue continuent à reconstruire l’image normalement.

## Mémoire et informations système

Le P4 possède **768 Kio de SRAM HP physique**. Cette capacité inclut la zone consacrée au cache L2, au code et aux données fixes : elle n’est pas intégralement disponible pour les allocations. Le `275/347 Kio` observé dans la version précédente était une mesure du tas interne alloué/disponible, pas un bilan de toute la SRAM physique.

Les informations système distinguent maintenant le tas de SRAM HP, la PSRAM, les **8 Kio de SPM/TCM** — deux noms pour la même mémoire — et les **32 Kio de SRAM LP**. Le cache L2 configuré par ce SDK occupe 256 Kio. Les détails affichent séparément les espaces utilisables et leur plus grand bloc ; l’aperçu périodique ne parcourt plus le tas pour obtenir ce dernier.

Trois tables constantes SHA1/XZ totalisant **14 424 octets** passent de la SRAM à la flash. Les calculs SHA1, la détection des collisions et les CRC sont conservés et vérifiés. Les charges utiles des files Cartes passent en PSRAM, avec leurs structures de contrôle conservées en mémoire interne. Ce nombre de 14 424 octets décrit les tables déplacées ; le gain statique net, après les nouveaux compteurs et caches, figure dans le rapport de compilation.

L’ancien `Application/partition OTA : 6,0/6,2` désignait la **taille du firmware dans sa partition flash active**, en Mio. Ce n’était pas de la RAM : le firmware utilisait environ 6 Mio sur une partition de 6,25 Mio. Le nouvel intitulé est **Firmware · partition flash**, avec deux décimales et la place restante.

La taille du firmware est désormais lue une seule fois à partir de ses métadonnées. L’aperçu système ne relance plus une vérification complète d’environ 6 Mio toutes les deux secondes. Une erreur de lecture est également mémorisée pour éviter les tentatives répétées.

## Affichage et Meshtastic

L’audit des rafraîchissements inclut une simulation de 180 secondes au repos : les tests n’y constatent aucune publication, copie ni synchronisation de cache de l’image sans activité. Cela ne permet pas d’affirmer que les flashs physiques signalés sur la tablette ont disparu.

Les détails système disposent d’un compteur des manques de données du pont DSI, avec l’heure du dernier événement depuis le démarrage. Le relais d’interruption observe le statut puis appelle le gestionnaire du SDK ; il n’acquitte pas l’interruption et ne change pas les paramètres de la dalle. Le compteur distingue explicitement une observation indisponible d’un compteur à zéro. Les procédures de compilation fournies activent ce relais.

Les réglages Meshtastic sont accessibles depuis **Réglages → Système → Connexions**. Leur ouverture seule ne lance pas le Bluetooth. La pastille supérieure apparaît uniquement lorsque le service Meshtastic est effectivement prêt et actif ; elle conserve l’indication des messages reçus. Le titre du bandeau reste limité à une ligne, y compris avec la police de 24 pixels.

Le refus Bluetooth pouvait se produire avec environ 72 Kio libres car l’ancienne admission réservait 88 à 92 Kio de mémoire interne. Pour le SDK fourni, les allocations génériques de l’hôte NimBLE utilisent désormais son mode PSRAM officiel ; les allocations internes/DMA, le transport et les piles des tâches restent internes. L’admission distingue ces besoins et conserve les réserves de sécurité. Les erreurs et annulations sont testées, mais une connexion à un appareil Meshtastic réel reste à vérifier.

## Dessin

La progressivité accepte les valeurs **−32 à +32**. Une valeur négative adoucit uniquement vers l’intérieur de la sélection ; une valeur positive conserve l’adoucissement autour de son bord. Le comportement s’applique aux sélections et au remplissage de la sélection entière.

Après une désélection, le premier tracé crée maintenant la zone dessinée même si le mode précédent était Soustraire. Il ne soustrait plus ce tracé à une sélection implicitement pleine, ce qui produisait son complément. La soustraction entre sélections existantes et la commande d’inversion volontaire restent disponibles. Les essais couvrent notamment les deux sens du lasso, souris/tactile, annulation et échecs d’allocation.

## Installation et portée

Mettre à jour le firmware et les ressources SD ensemble suivant [LISEZ_MOI.md](LISEZ_MOI.md). Les quatorze packs de langue, les thèmes et les noms de fichiers anglais sont conservés. Les anciennes traductions sont préservées ; les nouveaux libellés sont ajoutés à chaque pack.

Les applications ELF restent détectées automatiquement comme en 0.8.40. Cette version ne remplace pas les développements matériels restant à réaliser, détaillés dans [EVOLUTIONS_0.8.39.md](EVOLUTIONS_0.8.39.md). Les résultats historiques gardent leur numéro de version et ne sont pas présentés comme de nouveaux essais.
