# Wikipédia : pages et archives — 0.8.19

## Lecture en 0.8.22

Une page rassemble désormais les paragraphes, un tableau court et une illustration lorsque la place le permet. Les tableaux ne s'arrêtent plus après leur première cellule. Les liens des cellules restent accessibles, les tableaux larges défilent horizontalement et une cellule très haute se lit par défilement vertical.

Les articles restent chargés par fenêtres de 32 Kio. Une page contient au plus douze éléments de mise en page et une illustration décodée à la fois ; les autres éléments restent accessibles à la page suivante. Les tableaux sont regroupés par portions de lignes, avec au plus 128 cellules résidentes. L'index des tableaux est borné à 2 048 entrées : au-delà, le texte de l'article reste lisible. Ces limites protègent la mémoire sans supprimer le texte.

La hauteur du texte est mesurée sur le véritable widget LVGL, après la mise en forme arabe/persane. Les tests de rendu emploient les polices livrées, y compris les replis japonais et coréen. Les couleurs des liens et du bouton d'illustrations suivent la palette de l'OS.

## Lecture et recherche

Les résultats remplissent chaque page en continu : copies enregistrées, archives ZIM puis résultats en ligne. Une seule copie locale ne repousse plus tous les résultats suivants sur une autre page. Les reliquats et le curseur des deux sources sont conservés en revenant à la page précédente ; les titres répétés sont filtrés sur les 32 dernières pages.

Le bouton **…** d’un résultat hors ligne permet de supprimer sa copie enregistrée. Pour un résultat ZIM, il propose explicitement la suppression de l’archive entière, avec confirmation de son nom ou de son chemin. Les autres documents du dossier sont conservés. « Retirer de la liste » dans la bibliothèque continue de conserver le fichier.

L’icône image de la barre de lecture active ou masque les illustrations sans quitter l’article. **Ctrl I** fait la même chose. **Retour arrière**, **Échap** et **Alt Gauche** remontent l’historique de lecture, puis reviennent directement aux résultats. Les flèches, Page précédente/suivante et Espace naviguent ; dans un tableau défilant, les flèches déplacent d’abord le tableau. Les boutons de page sans destination disparaissent.

Les tableaux HTML sont désormais affichés en cellules alignées : en-têtes, bordures, cellules fusionnées horizontalement et verticalement, liens conservés. Les tableaux larges défilent horizontalement. Le lecteur charge les grands tableaux par portions ; les boutons précédent/suivant passent entre ces portions. Les tableaux imbriqués restent du texte à l’intérieur de leur cellule, et les grilles au-delà de 64 colonnes utilisent le texte de secours. Ce lecteur n’interprète pas toute la mise en page CSS de Wikipédia.

Les nouvelles extractions ZIM bénéficient de ces cellules hors ligne. Pour les anciennes **copies d’articles enregistrées** dont les tableaux avaient déjà été aplatis dans le texte, une ouverture avec connexion reconstruit la structure ; leur ancienne copie reste lisible en l’absence de réseau. Installer ensemble le firmware et les modules MicroPython 0.8.19.

Les tests sur ordinateur couvrent la fusion et le retour des résultats, les suppressions ciblées, les tableaux depuis HTML et ZIM brut/XZ/Zstandard, les cellules fusionnées, les offsets des liens et les erreurs de lecture. Les contrôles de syntaxe ESP32-P4 passent ; le rendu tactile sur l’appareil reste à vérifier.

## Fiabilité des chargements en 0.8.16

Les lectures ZIM sont découpées en blocs de 32 Kio maximum et tiennent compte de l'annulation. Le décodage XZ rend régulièrement la main, par étapes de 8 Kio de sortie compressée, tout en conservant le chemin compatible des anciennes archives annonçant un dictionnaire de 64 Mio. Les demandes de mémoire sont contrôlées avant les allocations importantes ; une mémoire insuffisante doit produire une erreur, pas un chargement prétendument réussi.

L'index de liens est lu par blocs de 512 octets et reprend à son dernier curseur entre les pages. Il est limité à 4 Mio, avec au plus 4 096 octets par ligne. La pagination mesure le voisinage de l'écran plutôt que de retraiter inutilement tout l'article.

Un cache JSON endommagé ne bloque plus à lui seul la lecture. Le nettoyage du cache reste limité aux noms de fichiers produits par le lecteur. Le chargement des illustrations respecte l'annulation, refuse les index trop volumineux et s'arrête en cas de mémoire insuffisante.

Ces changements conservent les limites préexistantes de taille des articles et des décodeurs. Ils ne garantissent pas la compatibilité de chaque archive Kiwix. Les tests sur ordinateur comprennent un article synthétique de 3 420 046 octets en brut, XZ et Zstandard, avec erreurs de lecture, annulation et manque de mémoire.  La fluidité sur le Tab5 et vos archives réelles restent à vérifier.

## Utilisation des archives

La recherche reste commune aux copies enregistrées, aux ZIM et au réseau. Une annotation indique la provenance ; les articles locaux sont recherchés d’abord.

**Ajouter une archive déjà présente sur la SD** : Options → Ajouter une archive ZIM de la carte SD → choisir `.zim` ou `.zimaa`, puis sa langue. Pour les téléchargements segmentés Tab5, choisir leur `manifest.json`. L’index de titres est lu et doit fournir un article avant l’enregistrement. L’ajout est rapide parce qu’il conserve le chemin de l’archive ; il ne copie ni ne décompresse toute l’encyclopédie.

**Retrouver un ZIM** : Archives disponibles sur l’accueil Wikipédia. Les ajouts de toutes les langues sont affichés avec leur langue et leur nom. Lire ouvre directement son index. Les fichiers ZIM externes proposent Changer la langue et Retirer de la liste ; retirer ne supprime jamais le fichier. Un chemin devenu introuvable reste visible. Les archives téléchargées et complètes de la langue courante y figurent aussi ; leurs manifestes conservent la langue de l’édition.

Articles disponibles montre les résultats locaux de la langue choisie. Une erreur de lecture de ZIM ne doit plus se transformer en simple « Aucun article trouvé » : elle est indiquée, tandis que les autres sources restent consultables. Les ZIM ajoutés sous une mauvaise langue dans une ancienne version peuvent être corrigés depuis Archives disponibles. Le registre conserve jusqu’à 32 ZIM externes au lieu de quatre.

Formats de base inchangés : ZIM 5/6, fichiers uniques et segments pris en charge par le lecteur existant, décompression XZ/Zstandard bornée. Tous les ZIM du catalogue Kiwix n’ont pas été validés. Une archive dont l’index utilise un format non pris en charge affiche son erreur. Quatre petites archives réelles issues des jeux de référence libzim ont été utilisées pour vérifier l’ajout, la persistance et la recherche ; ce contrôle ne vaut pas essai de tous les gros Wikipédia.

Le lecteur conserve des limites de mémoire et ne remplace pas un navigateur HTML/CSS complet. Les corrections anciennes de la bibliothèque et du téléchargement ci-dessous sont conservées ; le rendu des tableaux a été ajouté en 0.8.17.

## Démarrage corrigé depuis la 0.8.9

Le pied de page dépassait de dix pixels la limite de la surface graphique. Sa création déclenchait `OSError: 22` avant l’affichage du menu. Les coordonnées sont corrigées et les dimensions sont désormais validées côté Python. Installer les nouveaux modules `tab5ui.py` et `tab5wikiui.py` ; aucune suppression des archives ou des pages enregistrées n’est nécessaire.

## Mémoire du décodeur en 0.8.11

Les blocs Zstandard à grande fenêtre sont décodés en C++ par blocs, jusqu’à la fin de l’article demandé. Un tampon contigu conserve l’historique nécessaire et est libéré aussitôt. Le cas Sodium de l’archive française nopic 2026-05 passe avec un budget de 1,5 Mio sur ordinateur, pour un pic suivi de 1 059 419 octets. La limite du tampon de compatibilité reste de 8 Mio. Une erreur restante indique la demande, la PSRAM libre et le plus grand bloc libre. Le test physique de votre édition reste nécessaire.

Mettre à jour le firmware, en plus des ressources SD : le décodeur est dans le firmware.  Aucun effacement des archives n’est requis.

## Téléchargement et actualisation en 0.8.12

Le catalogue permet de choisir la langue, puis l’édition avec ou sans illustrations. L’icône ↻ sur une édition déjà présente permet de recommencer son téléchargement après confirmation ; elle est aussi disponible dans Archives disponibles pour les éditions Wikipédia reconnues. La nouvelle copie d’un ZIM importé est téléchargée séparément et conserve le fichier original.

Les morceaux sont téléchargés sur un miroir HTTPS mémorisé. Un autre miroir est essayé si le serveur refuse la reprise. Taille exacte et SHA-256 sont lus dans les métadonnées Kiwix. Après réception, l’ensemble de l’archive est relu et vérifié avant d’être annoncé complet ; cette lecture peut être longue et son avancement est visible. Une pause pendant la vérification conserve les morceaux : la reprise recommence seulement leur vérification. Les erreurs de réseau, de taille, d’identifiant du miroir et de contenu sont distinguées.
