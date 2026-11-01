# Dessin et liste Média — audit 0.8.46

## Constats établis

Le clic pinceau/gomme lançait `ie_brush_begin()` dans le contexte graphique. Cette fonction mettait à zéro un masque de couverture de toute l'image, puis clonait tous les calques et le masque de sélection pour conserver l'annulation. Les pixels des calques non modifiés étaient recopiés à chaque clic. `ie_warp_begin()` utilisait aussi ce clone complet. Les copies sont en PSRAM et le callback coopératif laisse le processeur aux autres tâches, mais il ne rend pas la main à la boucle graphique : cette charge synchrone explique une partie de la latence, sans établir à elle seule l'origine physique des flashs.

Le rendu d'un trait est déjà régional : `Tab5ImageEditor::render()` recompose et invalide seulement son rectangle. `service()` ne recompose pas l'image au repos sans changement de thème/police. Il n'existe pas de timer de clignotement de la sélection. Aucune réinitialisation de dalle ou série de rafraîchissements n'a été ajoutée.

La liste Média conservait ses références, clés de tri et noms temporaires avec les allocateurs génériques. L'OS active déjà `heap_caps_malloc_extmem_enable(0)` : cela privilégie la PSRAM ; il serait donc faux de présenter tous ces noms comme des allocations SRAM dans le cas normal. Le repli implicite en RAM interne restait possible sous pression PSRAM.

## Modifications

- Les instantanés de pinceau/gomme/déformation capturent les pixels du seul calque modifié, avec les métadonnées complètes et la sélection. Les autres calques restent la propriété du document courant. Leurs pointeurs ne sont pas partagés entre propriétaires : l'historique marque les calques inchangés par un pointeur nul et les transferts d'annulation/rétablissement conservent leur unique propriétaire. La compatibilité dimensions/nombre de calques est vérifiée avant restauration. Les opérations multicalques ou géométriques gardent leurs instantanés complets.
- Le masque de couverture du pinceau initialise seulement les blocs de 256 pixels touchés. Un petit bitmap initialisé à la création garantit qu'aucune couverture non initialisée n'est lue. Le masque et ce bitmap sont alloués avant la modification des pixels ; un échec laisse l'image intacte. Ils sont libérés à validation/annulation du geste.
- `Tab5MediaList` demande explicitement `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` pour les références du catalogue et les clés/noms retenus pendant le tri. Un échec du tri libère ses temporaires et conserve l'ordre existant. Il n'est plus permis d'emprunter de la RAM interne pour ces données. La politique d'admission de l'OS, les tailles de piles, le nombre de lignes visibles et le comportement de lecture restent inchangés.

Aucun format de document, ABI de structure, limite de calques, fonction de sélection, progressivité, opacité, annulation ou politique d'enregistrement n'a changé. Les historiques se conservent jusqu'aux mêmes plafonds de nombre et d'octets ; un geste multicalque en consomme moins. L'éviction d'anciens historiques en cas de manque de mémoire conserve la politique préexistante.

## Mesures exécutées sur le code C

Les sources 0.8.45 ont été extraites de l'archive livrée (SHA256 `46fd5da89ebb693a61d3ab34e2d3f3480a714aff76c095ce339f290ee8641573`). Les deux implémentations exécutent le même scénario instrumenté. Ces chiffres comptent les appels C `memcpy/memset`, pas le trafic du bus ni les millisecondes sur Tab5.

| Début d'un geste, image 640 × 360 | 0.8.45 | 0.8.46 |
|---|---:|---:|
| Copie, un calque (métadonnées hôte incluses) | 921 968 octets | 921 968 octets |
| Initialisation du masque, un calque | 230 400 octets | 113 octets |
| Copie, huit calques | 7 373 168 octets | 921 968 octets |
| Zéros au premier tampon de pinceau 2 px | 0 | 512 octets |
| Surcoût du bitmap par geste à cette dimension | 0 | 113 octets |

Le calque actif reste copié intégralement : le cas un calque n'est pas présenté comme une copie supprimée. Les valeurs globales d'allocation des essais incluent l'éviction d'anciens historiques ; elles ne constituent pas une mesure de RAM libre du firmware.

## Vérifications exécutées

- Noyau historique : 34 777 vérifications, 400 cycles, 336 scénarios d'échec d'allocation.
- Sélection/progressivité/remplissage : 157 059 vérifications, 17 586 étapes de tâches, 150 cas aléatoires de remplissage, 72 injections d'échec supplémentaires. Ce total réexécute aussi le noyau historique ; ne pas additionner les deux totaux comme des tests tous indépendants.
- Régression 0.8.45/0.8.46 : 4 149 / 3 932 assertions, même empreinte finale des pixels, métadonnées de calques et couverture de sélection. Mélanges de pinceau, gomme, déformation, suppression de calque, changement de calque, redimensionnement, annulation/rétablissement, annulation d'un geste en présence de redo. Allocateur empoisonné avant usage ; chaque scénario finit à zéro octet possédé.
- Interface Dessin : véritable module natif, noyau et LVGL 9.6 ; tests gestes, clavier, options, cadres, sélection, texte, ouverture/fermeture et suspension. Voir `image-native-0846.json` pour le résultat figé et les limites des adaptateurs hôte.
- Média : LVGL réel, 800 entrées et pool de 24 lignes ; 5 048 vérifications, tri, focus, clic périmé, favoris, destruction et échecs à cinq étapes du tri. Zéro allocation explicite PSRAM restante ; maximum hôte de ces seules allocations 50 592 octets.
- Compilation des unités P4 : noyau, éditeur et liste Média, dans `drawing-target-0846.json`.

ASan et UBSan sont actifs dans les essais hôte. LSan est désactivé pour la limitation de collecte `/proc` du conteneur ; un allocateur de test contrôle indépendamment la restitution des allocations concernées.

## Attentes et limites restantes

`Tab5ImageEditor::cooperate()` conserve son `delay(1)` au plus une fois toutes les 16 ms. Ce passage rend le processeur à FreeRTOS sans réentrer dans LVGL ou modifier le document. Le retirer rendrait les opérations longues moins coopératives. Il ne supprime pas le blocage graphique d'une opération synchrone : certaines importations, transformations globales et sauvegardes restent à mesurer sur appareil. Il serait risqué de les déplacer sur un autre cœur sans transférer explicitement la propriété du document, des handles de fichiers et des résultats ; cette modification n'a pas été simulée.

Le tri Média par taille/date demande encore ses métadonnées dans le contexte appelant. Le rendu et le défilement ne relisent pas les métadonnées ; cependant le premier tri d'une très grande liste sur SD lente reste un point de latence à mesurer.

L'éditeur caché libère son aperçu dérivé, conserve volontairement le document et l'historique non enregistrés, puis les restitue à la fermeture confirmée. Cette conservation n'est pas une fuite et n'a pas été supprimée pour afficher un meilleur chiffre mémoire.

Aucun essai physique n'a été exécuté. Les flashs ne sont pas déclarés résolus par ces changements. À vérifier sur Tab5 : 30 clics isolés et 30 traits, pinceau/gomme/déformation, un puis huit calques ; sélection avec progressivité positive/négative ; annuler/rétablir après suppression de calque et redimensionnement ; suspendre/reprendre/fermer ; observer flashs et compteurs de sous-alimentation DSI, latence et mémoire avant/après. Comparer aux relevés sans dessin actif.
