# Nouveautés 0.8.18

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.18 réussis.** Preuve : [firmware-verification.json](tools/verification/firmware-verification.json).
<!-- /build-status -->

## Langues et saisie

Quatorze packs sont livrés : français, anglais, allemand, espagnol, italien, néerlandais, portugais du Portugal et du Brésil, indonésien, russe, arabe, chinois simplifié, japonais et coréen. Les dispositions associées sont livrées avec les ressources SD. L’anglais britannique reste également disponible comme disposition de clavier.

| Langue | Saisie | Limite actuelle |
|---|---|---|
| Chinois simplifié | Pinyin et candidats issus du dictionnaire hors ligne | Lecture exacte de syllabes, mots ou expressions présents ; pas de segmentation ou de prédiction automatique de phrases |
| Japonais | Romaji vers hiragana/katakana, petits kana et conversion par lecture exacte | Pas d’analyse automatique de phrase ni de flexion complète ; composer les formes fléchies par segments ou en kana |
| Coréen | Disposition 2-beolsik, assemblage et correction des syllabes modernes | Pas de conversion Hanja |

**Ctrl+Espace** bascule entre saisie latine et méthode de saisie. **Espace** demande la conversion ou parcourt les candidats ; **1 à 6** ou **Entrée** choisissent un candidat, **Échap** annule. Les flèches et Page précédente/suivante parcourent les candidats. Le clavier tactile expose les mêmes candidats.

Les raccourcis avec **Ctrl**, **Alt** ou **Meta** conservent leurs commandes avec les dispositions russes, arabes et CJK, ainsi qu’avec les claviers français et allemand. La saisie ordinaire conserve les caractères de la disposition choisie.

Installer également les nouvelles polices et les dictionnaires : la mise à jour du BIN seule ne suffit pas. Les polices couvrent les caractères nécessaires aux packs et dictionnaires livrés ; cela ne garantit pas tous les caractères de tout document extérieur. Le retour anglais avec police intégrée reste disponible si la police choisie est inaccessible.

## Accueil, navigation et apparence

Les applications ELF et Python disposent chacune d’une entrée et d’un menu dédiés. Les applications natives restent directement accessibles sur l’accueil. Les positions de défilement sont conservées entre les menus et les lancements.

Le bandeau latéral devient facultatif. Ses raccourcis peuvent être masqués et réordonnés ; l’ordre de tabulation suit leur ordre visuel. Sans bandeau, Accueil reste présent dans la barre supérieure et les pages utilisent l’espace libéré. Les champs et consoles ELF, firmware et Terminal suivent cette largeur ; les interfaces Python sont recentrées.

La liste de thèmes applique le choix immédiatement, avec conservation des saisies et du défilement. Une apparence accentuée facultative ajoute de la couleur aux commandes. Les réglages de couleurs conservent leur ajustement symétrique vers le noir ou le blanc.

La sélection initiale des fenêtres et le routage clavier sont revus. Les commandes d’une fenêtre contextuelle prennent la priorité sur la navigation de la page située derrière elle.

## Explorateur

Les commandes pour afficher les éléments cachés et masquer la sélection ont des présentations distinctes. La sélection multiple permet les actions groupées ; les sélections sur plusieurs pages sont conservées. Les raccourcis de sélection, copie, déplacement et suppression restent utilisables. Les opérations prennent un instantané de la sélection pour éviter qu’un changement visuel modifie la tâche en cours.

## USB et manettes

Le choix **USB rapide** avait été placé dans un panneau masqué sans disque : il redevient un réglage global visible. Son activation nécessite un redémarrage. Les commandes propres au stockage restent conditionnées à la présence du disque.

Le transport filaire XInput de type Xbox 360 s’ajoute au HID générique. Cela répond au cas d’une SN30 Pro démarrée en mode Windows, ignorée auparavant car son interface n’était pas HID. Les noms, commandes reçues et associations sont consultables depuis l’aide du jeu. Aucun appairage Bluetooth, initialisation Switch, retour de force ni compatibilité de toutes les variantes n’est annoncé. Voir [MANETTES_USB.md](MANETTES_USB.md).

## Réseau

Le Terminal ajoute `netinfo`, `dns`, `ping` et `tcp`. Les commandes sont bornées et annulables ; `tcp` teste un port précis. Elles ne constituent pas une suite de pentest. VPN et Tor restent des développements distincts, non livrés dans cette version ; voir [RESEAU_DIAGNOSTIC.md](RESEAU_DIAGNOSTIC.md).

## Validation

<!-- resource-summary -->
Les 14 packs couvrent les 2563 clés de référence contrôlées. Factory contient 93 ressources officielles ; le secours firmware en contient 38. Les grandes polices et les dictionnaires de saisie sont distribués sur SD. Preuves : [languages-0818.json](tools/verification/languages-0818.json), [ime-0818.json](tools/verification/ime-0818.json), [resources-0818.json](tools/verification/resources-0818.json).
<!-- /resource-summary -->

Les tests sur ordinateur et leurs preuves sont dans [VERIFICATION.md](VERIFICATION.md). Aucun appareil n’a été flashé ni testé physiquement ; les alignements, caractères, périphériques et actions simultanées restent à vérifier sur le Tab5.
