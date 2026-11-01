# Restitution des ressources — corrections v0.8.49

Ce lot part du diagnostic `memory-lifecycle-initial.md`. Il ne touche pas aux partitions, documents, fichiers, secrets ou préférences. Le contrat de sortie audio acquittée est corrigé séparément par le lot Audio ; les vérifications ci-dessous portent sur vidéo, radio/admission et caches Média.

## Modifications

**Vidéo (`Tab5Video.cpp/.h`).** Lorsque le producteur est réellement arrêté, `service()` restitue les frames de travail. Cette opération appartient à la tâche UI : le worker ne peut pas libérer l'ancienne image entre `frame()` et son adoption par LVGL. Une frame finale publiée mais pas encore adoptée et l'ancienne frame empruntée sont conservées ensemble ; le service UI suivant l'adoption ne conserve que l'image finale. Fermer le viewer détache l'image avant de rendre toutes les frames. Un nouveau décodage ne reconstitue que les slots manquants ; tout échec d'allocation conserve l'image empruntée et libère les nouveaux slots partiels. Un refus d'admission ou de création de tâche ne transforme plus un ancien emprunt affiché en frame libérable.

À 1280 × 720 RGB565, après adoption de la dernière image, le coût des frames passe de **5 529 600 à 1 843 200 octets**, soit **3 686 400 octets restituables**. Calcul de taille vérifié dans le code ; ce n'est pas un échantillon matériel du tas. L'intervalle où deux images sont conservées est nécessaire pour respecter la propriété de l'image affichée. Les workspaces JPEG/entrée sont toujours libérés à la fin effective du worker.

**Radio (`Tab5Audio.cpp`, `Tab5ResourceManager.cpp/.h`).** Le ring prévu est choisi une fois, puis transmis au constructeur de transport. Le budget compte ce ring, la préallocation du codec effectivement choisi, le buffer de lecture de 16 Kio, le PCM lorsque requis, les workspaces TLS selon l'allocateur actif et une marge de métadonnées. Le minimum contigu demandé pour le ring devient **64 Kio + 1 octet**, ou davantage si le codec a un besoin supérieur. Il n'exige plus 1 Mio avant que le ring ait la possibilité de se réduire. L'allocation effective peut toujours retenter à une capacité plus petite. Les ressources I²S déjà allouées lors d'une transition immédiate ne sont pas facturées une seconde fois. Le choix du ring ne parcourt plus le tas pour demander son plus grand bloc.

Ce budget reste une admission du pic prévu, avec des marges ; il ne garantit pas qu'une allocation concurrente réussisse. Les branches d'échec continuent à fermer les ressources déjà ouvertes. Les codecs et allocations TLS réels ne sont pas remplacés.

**Caches Média (`.ino`).** La sortie de `Page::Audio` mémorise la navigation existante, masque la vue puis appelle le même helper de restitution de liste que le collecteur sous pression. Les lignes virtuelles, leur modèle et les groupes album/catégorie sont détruits. La réouverture utilise déjà `refreshAudioList()` et reconstruit ces données. Le moteur audio tente de restituer ses catalogues inutilisés avec ses gardes existantes ; il conserve le catalogue nécessaire à une lecture ou une pause. Enregistrement, commandes en attente, priorité et garde de stockage/état indisponible peuvent refuser ce trim. Le catalogue vidéo est rendu si son worker et son affichage sont inactifs.

Ce comportement est événementiel, sans timer de purge. Il n'arrête pas une lecture de fond, ne réinitialise pas la pause et ne supprime pas de document ou d'historique. Un trim refusé reste disponible ultérieurement au collecteur sous pression : il ne faut pas annoncer une purge immédiate et totale dans tous les modes.

## Vérifications exécutées

| Test | Chemin réellement exécuté | Résultat |
| --- | --- | --- |
| `video-lifecycle-tests-0849/run.py` | Méthodes de production `playSource`, worker `taskEntry`, allocation/décodage/publication, `service`, `frame`, stop/détachement ; corps d'entrée/parser et DMA remplacés aux frontières matérielles | 100 cycles publics, 200 cycles décodeur, 6543 assertions, ASan/UBSan, zéro buffer/moteur JPEG final. Inclut échecs de chaque slot manquant, refus de budget/création de tâche et image empruntée conservée. |
| Même fixture sur les sources .48 | Même chemin de lecture/fin/service | Échec attendu et enregistré : les trois frames restent présentes à la fin au lieu de la seule frame finale. |
| `radio-lifecycle-tests-0849/run.py` | `startRadioNow`, manager de ressources complet, ring/source tamponnée de production, receiver et sémaphores adaptés en threads hôte | 25 cycles, 425 vérifications, repli réel à 256 puis 64 Kio sous limite de bloc simulée, refus à 64 Kio sans l'octet de sentinelle, échec amont, zéro allocation/connexion restante. |
| `media-hide-tests-0849/run.py` | `showPage` exact, helper exact, destructeur MediaList et `releaseIdleCatalogues` exacts ; frontières objets UI/stockage contrôlées | 104 sorties de page, 704 vérifications, conservation des modes lecture/pause/enregistrement et veto ; zéro buffer explicite restant. |
| `media-hide-tests-0849/native.py` | MediaList, Focus, TextInput, UiStyle et WriterDocument réels liés à LVGL 9.6 hôte | 14446 vérifications : 800 références, tri, souris/clavier, génération des clics, puis 100 cycles masquage/destruction/recréation avec focus mémorisé et événements scroll/size après destruction. |

Les rapports et empreintes sont dans `tools/verification/*-0849.json`, avec le résultat témoin `video-lifecycle-baseline-0848.json`. Les tests radio contrôlent les codecs/HTTP/sortie aux frontières : ils ne mesurent ni le son ni le Wi-Fi réel. Le test de menu natif utilise la bibliothèque LVGL hôte existante avec l'allocateur hôte ; le nouvel allocateur LVGL de l'OS est validé séparément. LeakSanitizer n'est pas disponible dans cet environnement : ASan/UBSan et compteurs explicites d'allocations sont utilisés.

## Limites et tests Tab5

- La compilation cible complète et son bilan Flash sont fournis par la validation globale du firmware ; ce lot ne revendique pas à lui seul cette compilation.
- Vérifier dix lectures vidéo successives, dont échec réseau et retour au menu : une image finale conservée, puis retour des frames après fermeture. Capturer mémoire et tâches avant lecture, après fin/adoption, après fermeture et après un tour idle.
- Pendant une lecture ou pause locale/radio, quitter Média puis revenir : mode, sélection, recherche, navigation et volume doivent demeurer identiques. Les listes peuvent être reconstruites ; le catalogue de la lecture reste détenu.
- Une pause radio conserve volontairement I²S/PCM dans le nouveau contrat Audio et ferme le réseau. Si son worker s'est retiré, une demande de priorité vidéo requiert de recréer ce worker de 16 Kio avant libération acquittée. La pénurie interne peut encore faire refuser ce démarrage ; il ne s'agit pas d'une fuite prouvée. Aucune fermeture I²S bloquante n'a été déplacée sur l'UI pour masquer ce cas.
- Aucun essai physique ne confirme encore l'absence de flash ou de claquement. Ces symptômes font l'objet de corrections et mesures transversales distinctes ; ce lot garantit uniquement les chemins et invariants explicitement testés.
