# Projets Git sur Tab5OS 0.8.35

Git permet de conserver l'historique d'un projet, de préparer des modifications et de les échanger avec un dépôt distant. Il est accessible depuis l'explorateur et le terminal. Un projet peut se trouver sur la carte SD ou sur un disque USB connecté.

## Ouvrir ou créer un projet dans l'explorateur

Ouvrir **Actions → Git : état et commandes**, l’icône Git ou le menu du clic droit. La dernière sélection dans l’un des deux panneaux de l’explorateur détermine la cible. Sans sélection, la cible est le dossier du panneau actif. Un dossier sélectionné devient la cible ; un fichier sélectionné est conservé comme cible des opérations de fichier. La recherche du dépôt remonte jusqu’à sa racine : être dans un sous-dossier fonctionne également.

Git occupe le volet droit de l’explorateur. La liste des fichiers à gauche reste utilisable et le panneau suit le contexte choisi après la fin d’une commande en cours. **Revenir à l’aperçu · Échap** ferme le volet sans perdre le dossier parcouru. Le dossier courant et la racine du dépôt sont conservés séparément.

Le menu **Fichier** propose **Créer un dossier Git** (nom du nouveau sous-dossier) et **Cloner un dépôt Git** (adresse HTTPS et nom du nouveau sous-dossier). Le clonage refuse une destination déjà existante. Pour transformer un dossier ordinaire existant en projet, le sélectionner et choisir **Actions → Initialiser ce dossier avec Git**. Son contenu reste en place et l'initialisation crée les métadonnées Git dans ce dossier.

Les commandes ordinaires de l'explorateur restent accessibles : copier/couper/coller et sélection multiple dans **Édition** ; renommer, masquer, supprimer, informations et outils de fichier dans **Actions** ; affichage des fichiers cachés et actualisation dans **Affichage**. Un clic droit sur une ligne cible cet élément et conserve une sélection multiple lorsqu'il en fait déjà partie. Un clic droit dans le fond d’un panneau vise son dossier courant.

Les commandes de dépôt deviennent disponibles lorsque le dossier contient un dépôt reconnu. Les commandes réseau apparaissent lorsque le dépôt distant de la branche amont est configuré (ou `origin` en l’absence d’amont). La mise à jour et l'envoi exigent une branche ayant déjà un commit ; ils sont masqués dans un dépôt vide ou lorsque le dépôt est détaché d'une branche. Le bouton d'arrêt apparaît pendant une opération.

| Action | Effet |
| --- | --- |
| Actualiser | Relit la branche et les fichiers modifiés. |
| Préparer le fichier / ce dossier | Prépare le fichier ciblé ou les changements du dossier courant, sans englober les autres sous-dossiers du dépôt. |
| Tout préparer | Prépare explicitement toutes les modifications du dépôt. |
| Enregistrer un commit | Enregistre tout l’index du dépôt, avec le message saisi : un commit n’est pas limité au fichier affiché. |
| Voir les différences | Propose les modifications locales ou préparées du fichier ciblé ou du dossier courant. Un fichier non suivi n’a pas de diff Git avant son ajout à l’index. |
| Historique | Affiche les 20 derniers commits. |
| Récupérer les commits distants | Télécharge les nouveaux commits sans changer les fichiers de travail. |
| Mettre à jour sans fusion | Télécharge puis applique une avancée directe de la branche, si les fichiers de travail sont propres. |
| Envoyer les commits | Demande confirmation, puis envoie les commits de la branche courante vers sa branche amont configurée (ou le même nom sur `origin` si aucun amont n’est défini). |
| Configurer | Règle l’identité de l’auteur, l’adresse du dépôt distant de la branche amont (ou `origin` en l’absence d’amont) et les noms des identifiants du coffre. |
| Ouvrir le terminal ici | Ouvre le terminal dans le dossier de travail conservé par l’explorateur. |

**Préparer** et **Commit** enregistrent localement. **Envoyer** publie les commits sur le serveur. **Récupérer** ne modifie pas les fichiers locaux : utiliser **Mettre à jour** pour appliquer les commits reçus. La sortie indique les références modifiées et leurs anciens et nouveaux identifiants ; l’envoi affiche aussi sa branche de destination. Une récupération sans nouvelle référence ou une branche déjà à jour est signalée explicitement. Un rejet du serveur reste une erreur, même si le transport s’est terminé correctement.

La branche locale et sa branche amont peuvent avoir des noms différents. Les commandes utilisent maintenant la configuration `branch.<nom>.remote` et `branch.<nom>.merge` et les références de récupération du dépôt distant. La confirmation d’envoi affiche la destination exacte. Les compteurs **à envoyer** et **à recevoir (dernier relevé)** comparent l’état local aux dernières références récupérées ; Actualiser seul ne contacte pas le serveur. La configuration de l’adresse distante vise également ce dépôt amont.

Le nom et l'adresse e-mail de l'auteur doivent être renseignés avant le premier commit. Si ces valeurs manquent, l’action Commit ouvre Configurer avec une instruction ; renseigner le nom puis l’e-mail et relancer Commit. Ces réglages sont enregistrés dans le projet.

Les actions sans effet sont grisées : **Préparer** si le fichier sélectionné (ou le dossier sans sélection) n'a plus de modification à préparer ; **Enregistrer un commit** tant que rien n'est préparé, si un conflit subsiste  ; **Voir les différences** si le projet est propre ; la corbeille si la sortie est vide. **Mettre à jour sans fusion** exige un projet sans modification locale. Les changements et conflits des autres pages sont également pris en compte. Pendant une opération, les commandes qui modifieraient le dépôt restent désactivées.

La liste indique son intervalle et le nombre réel de fichiers modifiés, par exemple `129–256 / 300`. Elle défile à l'intérieur d'une page de 128 fichiers ; les flèches de page donnent accès à la suite. Cette pagination ne limite pas le nombre total de fichiers accessibles. La sélection et la page sont conservées après la préparation d'un fichier. Les indicateurs des lignes sont : `+` pour une modification préparée, `M` pour une modification locale, `?` pour un nouveau fichier, `D` pour une suppression et `!` pour un conflit signalé par le dépôt.

La zone de sortie du volet contient le résultat de la commande et son état final. Pour un résultat très long, les dernières lignes sont conservées et l'interface le signale. Ce texte reste sélectionnable et copiable. La corbeille de sortie, **Ctrl+L** ou **clic droit → Effacer la sortie** efface uniquement ce texte ; aucun fichier, commit ou processus n’est supprimé.

Le bouton **Nouveau** crée un dépôt local. **Cloner** télécharge un dépôt HTTPS dans un nouveau sous-dossier. **Configurer** distingue l’identité de l’auteur des accès au serveur : le nom et l’e-mail signent les commits ; les sélecteurs **Identifiant du coffre** et **Jeton du coffre** servent à l’authentification. Un dépôt public peut être cloné sans les renseigner. Pour un dépôt privé, utiliser **Déverrouiller le coffre** dans le parcours Git, saisir le PIN, puis choisir les deux secrets déjà enregistrés dans le coffre. Leur création se fait dans les réglages du coffre ; le déverrouillage depuis Git permet de les utiliser sans quitter l’explorateur. Le fichier d’identifiants reste une option avancée. Aucun jeton n’est placé dans l’URL du dépôt.

## Clavier et saisie

| Raccourci | Action |
| --- | --- |
| Tab / Maj+Tab | Déplacer le focus entre les commandes et les champs. |
| Flèche haut / bas | Parcourir les fichiers, y compris au-delà des lignes actuellement visibles. |
| Alt+Gauche / Alt+Droite | Page précédente / suivante de fichiers. |
| F5 | Actualiser le projet. |
| Ctrl+L | Effacer la sortie Git, hors fenêtre de saisie. |
| Ctrl+. | Demander l'arrêt de l'opération en cours. |
| Entrée | Valider un champ à une ligne ; dans Cloner, avancer de l’adresse au dossier, puis valider. |
| Ctrl+Entrée | Valider le formulaire depuis n’importe quel champ. |
| Échap | Fermer d'abord une liste déroulante ouverte, puis la fenêtre de saisie ou le projet. |
| Ctrl+A/C/X/V/Z/Y | Sélectionner, copier, couper, coller, annuler ou rétablir dans un champ éditable. |

Les champs disposent aussi des icônes d'édition de l'OS. À l'ouverture d'une fenêtre, le premier champ nécessaire reçoit le focus. Lors de l'affichage du clavier tactile, le formulaire défile et la validation reste au-dessus du clavier.

## Parcours dans le terminal

Les chemins `sd:/…` désignent la carte SD et `usb:/…` le disque USB. Les chemins relatifs partent du dossier courant. Entourer de guillemets les noms contenant des espaces.

```text
cd sd:/Projects
git init demo
cd demo
git config user.name "Votre nom"
git config user.email "vous@example.org"
git status
git add -- programme.py
git diff --cached
git commit -m "Premier programme"
git log -n 20
```

Pour agir depuis un autre dossier, placer `-C` avant le nom de commande :

```text
git -C usb:/Projects/demo status
```

Pour cloner un dépôt public dans un nouveau dossier :

```text
git clone https://example.org/equipe/projet.git projet
```

Le clonage récupère par défaut le dernier état avec une profondeur de 1. Pour demander l'historique complet, dans les limites du périphérique :

```text
git clone --depth 0 https://example.org/equipe/projet.git projet-complet
```

Pour configurer un dépôt distant et échanger les changements :

```text
git remote set-url origin https://example.org/equipe/projet.git
git remote -v
git fetch
git pull --ff-only
git push
```

`remote set-url origin` crée aussi `origin` s'il n'existe pas. `fetch` récupère les objets et références distants ; `pull --ff-only` met également à jour les fichiers locaux ; `push` publie les commits locaux. Une archive de fichiers seule ne remplace pas un dépôt Git : copier l'ensemble du dossier, y compris `.git`, conserve son historique local.

## Dépôts privés et identification

Les échanges Git utilisent HTTPS. Le formulaire **Cloner** permet de choisir **Identifiant du coffre** et **Jeton du coffre**, après déverrouillage par PIN si nécessaire. Les deux noms sont mémorisés dans le dépôt créé pour les échanges suivants. Dans un dépôt déjà ouvert, ces choix se règlent avec **Configurer**.

En option avancée, un fichier texte peut contenir deux lignes : le nom d’utilisateur, puis le jeton fourni par l’hébergeur. Ce fichier contient un secret : le garder hors du dossier du projet, pour ne pas l’ajouter à un commit. Par exemple, conserver son chemin dans `sd:/Secrets/git-host.txt`. Dans un dépôt déjà ouvert, le saisir dans **Configurer → Fichier d’identifiants (avancé)**. Pour utiliser ce fichier dès le premier clonage, passer par le terminal avec `--credential-file` ci-dessous.

Le terminal reçoit uniquement le chemin du fichier, jamais le jeton lui-même :

```text
git clone --credential-file sd:/Secrets/git-host.txt https://example.org/equipe/prive.git prive
```

Le clonage mémorise le chemin du fichier d'identification dans le dépôt créé pour les échanges suivants. Seul ce chemin est enregistré dans la configuration, pas le contenu secret.

Le fichier ne doit pas contenir d'autre ligne. Une lecture échouée produit une erreur explicite. Les identifiants dans l'URL sont refusés. Les certificats et le nom du serveur sont vérifiés ; les redirections sont refusées. Les connexions passent par la supervision réseau de l'OS.

## Arrêt et reprise

Le bouton d'arrêt et `Ctrl+.` demandent une annulation coopérative. Une lecture réseau déjà commencée peut durer jusqu'à son délai d'expiration. Fermer le projet pendant une commande demande aussi son arrêt. Le contexte reste conservé jusqu'au dernier accès du travailleur ; FreeRTOS libère ensuite sa pile.

Un clonage utilise d'abord un dossier temporaire portant le suffixe `.git-import`, renommé à la fin. Après un clonage interrompu, ce dossier peut rester présent et une nouvelle tentative vers le même nom est refusée. Inspecter ce dossier sur ordinateur ou choisir une autre destination pour recommencer ; la reprise décrite ci-dessous concerne une mise à jour par `pull`, pas un clonage incomplet.

La mise à jour des fichiers par `pull` est journalisée. Après une interruption, le diagnostic peut demander :

```text
git recover --continue
```

Lancer cette commande dans le dépôt concerné, puis vérifier `git status`. Aucune reprise de fichiers n'est effectuée automatiquement au démarrage. Tant que la reprise est nécessaire, les commandes de modification sont bloquées ; le statut, l'historique, les différences et la lecture de la configuration restent consultables.

Le marqueur `.git/tab5-pull.pending` conserve l'ancien commit, le commit cible et la référence à avancer. La référence reste sur l'ancien commit tant que la mise à jour des fichiers, leur synchronisation et l'index ne sont pas achevés. La reprise refuse d'écraser un fichier divergent, un index modifié ou un fichier modifié manquant ; elle refuse aussi une branche changée entre-temps. Les fichiers non suivis et les modifications non concernées sont conservés.

Un journal corrompu ou un fichier partiellement écrit nécessite une inspection sur ordinateur. Conserver les fichiers et le journal pour analyser le diagnostic ; ne pas supprimer le marqueur pour contourner le blocage. La synchronisation des écritures réduit le risque, mais ne garantit pas l'intégrité du système de fichiers FAT lors d'une coupure électrique. Les tests de reprise tuent le processus aux transitions du moteur ; ils ne simulent pas une coupure physique de la carte SD.

## Périmètre et limites de cette version

L’interface exécute ses opérations hors de la boucle d’affichage et recycle ses lignes visibles. Une consultation, une préparation, un commit, une configuration ou un fetch ne reconstruit plus la liste de l’explorateur. Les créations et mises à jour de fichiers signalent leur fin à l’OS, y compris depuis le terminal, afin d’invalider la liste et la détection du dépôt au bon moment.

Les comparaisons du contenu ne se fient plus uniquement à la taille et à la date du fichier : deux sauvegardes de même taille dans la résolution d’horodatage FAT sont détectées par Status, Diff et Préparer. Le cache de date de l’index est invalidé en mémoire au début de ces opérations, puis le contenu est comparé. Les consultations ne réécrivent pas l’index sur disque et il n’y a pas de scan périodique Git. Son état est borné à 128 entrées par page, avec un rapport de 32 Kio. Le moteur Git est sérialisé : une seule opération Git est exécutée à la fois, depuis l'interface ou le terminal.

L'allocateur du moteur Git est limité à 8 Mio de PSRAM, sans repli vers la SRAM pour ces allocations. Son cache est limité à 256 Kio ; les fenêtres de lecture des packs utilisent 128 Kio chacune, 256 Kio au total et au plus quatre fichiers. Un pack est limité à 32 768 objets et une réception à 128 Mio. L'affichage textuel des différences traite des fichiers de 2 Mio au plus. Un dépôt dépassant ces limites peut être refusé : la profondeur réduite du clonage diminue l'historique téléchargé, mais ne réduit pas la taille de son état courant.

Le `pull` exige un arbre de travail propre pour appliquer une avancée directe de la branche courante. Un état déjà à jour ou une branche locale uniquement en avance est indiqué explicitement ; une divergence ou une branche amont supprimée sur le serveur est refusée sans déplacer la branche locale. Les fusions, rebase, changements de branche, sous-modules, LFS, dépôts avec alternates et worktrees liés ne sont pas pris en charge. Les racines des volumes ne peuvent pas devenir des dépôts. Le transport Git par SSH n'est pas pris en charge par ce moteur ; la commande SSH du terminal est distincte.

## Corrections 0.8.26

`git`, `git help`, `git --help` et `git --version` répondent sans démarrer de tâche Git ni initialiser TLS. Les commandes locales ne réservent plus le budget TLS et n’initialisent plus son générateur aléatoire. Le transport réseau libère chaque socket une seule fois, y compris si la création de la session TLS manque de mémoire. Les compteurs de pile sur l’appareil restent la référence pour les limites matérielles.

Après Tab, toucher ou cliquer un autre champ change immédiatement la cible du clavier physique et tactile. Le même routage est partagé avec les autres formulaires de l’OS.
