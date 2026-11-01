# Étude ergonomique de l’explorateur — 0.8.38

Cette révision applique les derniers gestes demandés : **dossiers à gauche, contenu à droite ; un clic à droite sélectionne, un double-clic ouvre**. Elle remplace les règles de la 0.8.37 lorsque la demande les a corrigées. L’étude combine revue du contrat, inspection du code, essais des événements LVGL et inspection de rendus. Ce n’est pas un essai d’usage sur tablette physique.

## Contrat et cohérence des gestes

| Situation | Comportement retenu | Vérification |
|---|---|---|
| Premier accès de la session | Racine SD ouverte dans les deux panneaux ; USB si la SD est absente | Initialisation et routage des racines dans le test d’interaction ; rendu de la racine |
| Retour dans l’explorateur | Navigation de la session conservée, y compris après éviction des métadonnées ; état en RAM, sans nouvelle écriture NVS | Helpers de session et branche de sortie de page exécutés ; arbre testé après libération des métadonnées |
| Panneau gauche | Dossiers uniquement ; SD et USB monté sont des racines, sans préfixe « Racine carte SD » | Modèle d’arbre de production, données mixtes dossiers/fichiers, montage/retrait USB |
| Clic sur un dossier à gauche | Le contenu de ce dossier remplace la liste droite ; chemin et contenu suivent la même navigation | Événements de pointeur LVGL et callbacks de production |
| Clic droit sur une ligne du panneau droit | Menu contextuel de cette cible, sans ouverture implicite | Sélection et dispatch des commandes |
| Clic simple dans le panneau droit | Sélection uniquement ; ni liste remplacée, ni chemin de l’en-tête changé, ni décodage d’aperçu | Événements LVGL ; conservation des objets/lignes et du chemin |
| Double-clic sur un dossier à droite | Ouverture à droite ; révélation et focus du dossier correspondant à gauche | Ouverture et révélation explicite des ancêtres |
| Double-clic sur un fichier à droite | Une activation vers l’application associée | Cible et nombre d’activations enregistrés par le test ; application destinataire simulée |
| Chevron à gauche | Présent lorsqu’un sous-dossier a été détecté ; déplie/replie la branche, sans remplacer le contenu droit | Sondages progressifs, cache, dossiers sans sous-dossier, annulation et révision du stockage |
| Recherche | Loupe dédiée ; résultats sélectionnables au clic et ouverts au double-clic | Pagination, focus, reprise des résultats terminés et redémarrage d’une recherche interrompue |

Le nom et le chemin de l’en-tête droit décrivent le **dossier actuellement affiché**. Ils ne deviennent pas ceux d’une ligne simplement sélectionnée. Ce point supprime l’incohérence précédente entre en-tête, chemin et contenu. Une ligne fine sépare les deux panneaux ; elle indique la frontière sans encombrer la navigation.

## Organisation des commandes

| Emplacement | Rôle | Décision ergonomique |
|---|---|---|
| Fichier | Création | Séparer la création de la modification d’un élément existant |
| Édition | Copier, couper, coller, renommer, supprimer, sélection | Rassembler les opérations sur la sélection ; aucune suppression implicite du document d’une autre application |
| Actions | Ouvrir, ouvrir avec, informations, actions propres à la sélection | Dépendre du type d’élément et de son contexte |
| Sous-menu Git | Opérations adaptées au dossier ordinaire ou au dépôt sélectionné | Retirer le volet Git autonome et les entrées redondantes ; le raccourci Git n’apparaît que pour les dossiers reconnus comme Git |
| Affichage | Présentation et fichiers cachés | Conserver les réglages de vue dans un groupe stable |
| Icône de tri | Ordre de classement | Commande compacte, distincte du choix d’un support |
| Loupe | Recherche | Action immédiate et identifiable ; SD/USB se choisissent dans l’arbre |

Les racines restent sélectionnables pour consulter leur contenu, mais les commandes copier/couper/renommer/supprimer les refusent. Les tests vérifient également qu’une tentative de copie/coupe d’une racine ne détruit pas le presse-papiers existant. Annuler une suppression conserve la liste affichée et le parcours en cours.

## Réactivité : ce qui est établi

La sélection droite ne déclenche plus le remplacement d’un aperçu ni le rechargement de la liste. Dans le scénario LVGL testé, changer de sélection écrit deux styles de ligne ; 200 mises à jour répétées de cette même sélection n’en réécrivent pas. La séquence immobile de 500 ms produit zéro rafraîchissement écran. Ces compteurs portent sur le scénario instrumenté, pas sur l’ensemble du firmware ni sur son débit d’images physique.

L’arbre ignore les fichiers et sonde progressivement les sous-dossiers utiles à l’affichage, avec un résultat mémorisé. Les tests exercent 8 193 dossiers, une profondeur de 129, six tris, l’annulation, les erreurs mémoire et le retrait USB. Le scénario de 200 000 appels à l’état immobile ne produit ni lecture ni ouverture de fichier. Le benchmark hôte ne permet pas de convertir ce résultat en millisecondes sur ESP32-P4 ni de chiffrer un gain face à l’ancien arbre, dont la structure diffère.

Le correctif de chargement des langues de la **0.8.37 est conservé à l’identique** : lecture regroupée du pack et analyse dans son tampon, au lieu de multiplier les petits accès. Les preuves historiques restent nommées `locale-0837.json` et `locale-parser-0837.json`. Le premier rapport avait mesuré, sur hôte simulé, 103 appels de lecture pour les 418 993 octets du pack français et quatre allocations. Le contrôle 0.8.38 confirme l’intégrité du moteur et des 14 packs ; il ne prétend pas avoir remesuré le démarrage réel ni résolu sur tablette les 2,8 s signalées. Aucun rapport historique n’est présenté comme une exécution actuelle.

## Noms de stockage

Les valeurs proposées par défaut sont anglaises, indépendamment de la langue d’affichage : par exemple `New folder`, `Novel.md`, `Project`, le suffixe ` - copy`, ainsi que les dossiers MicroPython `Libraries` et `Projects`. Les six scripts et cinq notices fournis sous un nom français sont livrés avec leur nom anglais. Le contenu des onze fichiers renommés est conservé par comparaison au ZIP 0.8.37 ; les anciennes entrées ne figurent plus dans la nouvelle Factory.

Les chemins personnels configurés et les fichiers d’une carte existante ne sont pas renommés. La compatibilité des anciens chemins est distincte du nom proposé pour une nouvelle création. [ENGLISH_DEFAULTS_0.8.38.md](ENGLISH_DEFAULTS_0.8.38.md) détaille les choix et leurs tests.

## Preuves disponibles et limites

| Preuve 0.8.38 | Portée effective |
|---|---|
| [Interaction](tools/verification/explorer-0838.json) — 4 867 contrôles | Événements LVGL réels, fonctions de production extraites, sélection et menus ; stockage, parcours de recherche et frontières applicatives simulés |
| [Arbre](tools/verification/explorer-tree-0838.json) — 269 112 contrôles | Modèle de production non modifié, système de fichiers et allocations contrôlés ; aucune exécution UI ou matérielle |
| [Présentation](tools/verification/explorer-ui-0838.json) — 258 contrôles, 10 rendus, polices 22/24 | Construction et styles de production, police livrée, 32 cycles de focus ; contenu des dossiers, disponibilité des actions et état des supports injectés |
| [Noms anglais](tools/verification/english-defaults-0838.json) | Code Python et scans C++ de production sur adaptateurs hôtes ; fragments d’intégration du sketch contrôlés statiquement |
| [Conservation](tools/verification/preserved-components-0838.json) — 1 841 fichiers | Identité binaire avec la livraison 0.8.37 des composants hors périmètre modifié, dont le moteur de langues |

Les rendus contrôlent la lisibilité, la séparation des panneaux, les menus compacts, les résultats de recherche et le contexte Git. Ils ne sont pas des captures d’une tablette. Les tests de session exécutent les helpers et la branche de sortie de l’explorateur, pas tout le changement d’application ni le transport Git. La compilation complète et la validation du binaire figurent séparément dans [VERIFICATION.md](VERIFICATION.md).

La validation matérielle restante concerne la latence tactile et USB, la vitesse de la carte SD, la stabilité visuelle sous charge, le démarrage avec une langue sélectionnée et les allers-retours réels entre applications. Aucun taux d’images, temps de démarrage ni disparition totale du scintillement n’est affirmé sans cet essai.
