# SRAM — vérifications 0.8.53

La référence est l’ELF et les sources de la 0.8.52 livrée. Le relevé lié contient 94 128 octets de DATA+BSS. Il ne représente pas à lui seul l’utilisation totale de SRAM : les piles, allocations SDK, buffers DMA et tas internes sont distincts des données statiques.

L’audit a retenu des données sans contrainte ISR, DMA ou cache désactivé :

- L’index de pagination EPUB occupait 2 052 octets internes en permanence. Son propriétaire ne conserve désormais qu’un pointeur. Le même algorithme et les mêmes capacités restent utilisés, avec allocation lors de l’ouverture, réutilisation lors d’un changement de mise en page et libération à la fermeture. La PSRAM est essayée d’abord, puis une allocation interne bornée de même taille si nécessaire.
- Trois tables de quantification JPEG, 384 octets au total, sont uniquement lues. Leur déclaration et le paramètre privé qui les lit deviennent `const`.
- La table de gains FC0013, 192 octets, est uniquement lue par son pilote et n’est pas exposée dans l’API ELF. Elle devient `const`. Les licences d’origine sont conservées.

Le gain brut attendu est de 2 624 octets de données statiques internes. La vérification du firmware final doit confirmer le placement Flash réel des tables et la réduction du symbole `readerBookPages` de 2 052 à 4 octets. Le bilan net de toute la .53 dépend aussi des autres évolutions. Aucun gain de tas libre observé sur Tab5 physique n’est revendiqué.

La double impossibilité d’allocation n’efface ni ne ferme le livre : les méthodes de l’index deviennent neutres, le statut signale explicitement le manque de mémoire pour la pagination et aucune nouvelle allocation n’est tentée au repos. Une ouverture ou un changement de mise en page permet de réessayer. Le saut par numéro de page reste désactivé tant qu’aucun total exact n’est disponible, selon la logique préexistante.

## Exécution

```sh
python3 tools/sram-tests-0853/run.py --baseline /chemin/Tab5OS_Ecriture_v0.8.52 --output /chemin/resultats
```

Le banc compile les vrais fichiers avec ASan et UBSan :

- 84 112 contrôles d’équivalence et de cycle de vie de l’index ; 1 004 allocations et libérations comptées, aucun bloc suivi restant. Les fonctions d’ouverture, fermeture et statut sont extraites du code livré. Les limites rendu, cache fichiers et destruction LVGL sont simulées.
- 1 440 encodages JPEG couvrant qualités, dimensions impaires, canaux, sous-échantillonnages, tables standard/alternatives et une/deux passes ; les fichiers concaténés obtenus avec .52 et .53 sont identiques octet par octet.
- 64 032 scénarios FC0013 couvrant gains, bits préservés et erreurs de lecture/écriture ; traces simulées comparées à la .52. Cela ne teste pas la radio physique.

LSan ne peut pas inspecter les threads `/proc` dans l’environnement utilisé. Il est désactivé, sans désactiver ASan/UBSan ; le suivi explicite des allocations concerne le propriétaire EPUB. Il ne prouve pas l’absence globale de fuite dans tout l’OS.

L’ancien `tools/epub-pagination-tests/test_book_index.cpp` échoue sur une assertion de géométrie antérieure : il attend `y=111` pour 720×1280, tandis que la fonction, identique dans .52 et .53, retourne `y=94`. Les deux exécutions et cet échec préexistant sont consignés. Ni le test historique ni la géométrie de l’interface n’ont été modifiés pour le contourner.

Les piles ISR, la rétention SDK, les buffers audio/écran et le grand état permanent de Dessin n’ont pas été déplacés pour obtenir artificiellement un meilleur chiffre. Leur modification exigerait des mesures et une validation séparées.
