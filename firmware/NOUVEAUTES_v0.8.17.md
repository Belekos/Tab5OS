# Tab5OS 0.8.17 — corrections d’usage

## Accueil, applications et réglages

L’accueil distingue les outils intégrés, les applications ELF et les applications Python. La personnalisation passe dans **Réglages → Accueil** : visibilité, ordre dans chaque section, 24 symboles ou icône automatique, grille ou liste, trois tailles et libellés facultatifs. Les applications SD et USB sont reconnues, y compris les `.elf` compatibles.

Les lanceurs ELF/firmware et les écrans MicroPython utilisent des espaces de mise en page mieux délimités. Quitter une application Python revient à l’écran précédent de l’OS. Les lancements ELF peuvent mémoriser « demander un fichier », « sans fichier » ou « toujours ce fichier ». Les chemins publics sont `sd:/Dossier/fichier.ext` et `usb:/Dossier/fichier.ext` ; il ne s’agit pas de commandes de terminal.

**Stockage** devient un onglet des réglages. Les commandes du disque USB apparaissent lorsqu’un support est détecté. Le partage se nomme **Partage** ; sa commande d’arrêt n’est proposée qu’en cours de partage. Les informations techniques sans utilité pendant l’écoute sont retirées de la lecture audio. Le réglage des thèmes ajoute un curseur symétrique **Noir / Blanc** autour de la couleur choisie.

## Explorateur

La recherche retrouve sa loupe. Les actions copier, couper et coller sont regroupées ; la commande de dossier parent reste accessible au clavier. La croix de recherche n’apparaît que lorsqu’un filtre est actif. Les fichiers système se distinguent par leur couleur, et les éléments masqués sont atténués.

L’œil masque ou réaffiche l’élément sélectionné sans le renommer. Ce choix appartient à Tab5OS et reste mémorisé sur le support ; il ne change pas les attributs cachés de Windows. Ctrl+H montre les éléments cachés. Les modifications de fichiers déclenchent une actualisation, et F5 reste disponible.

L’aperçu des dossiers compte tous les éléments visibles au lieu d’afficher une limite « 7+ ». Sa liste se parcourt par pages de 48 entrées ; ce nombre limite la mémoire d’affichage, pas le nombre de fichiers accessibles. Les associations ELF et les commandes pertinentes suivent le type sélectionné.

## Wikipédia et EPUB

Wikipédia conserve les cellules des tableaux avec défilement horizontal. Les résultats locaux et en ligne partagent la même pagination. Les commandes de suppression d’une copie locale ou d’une archive sont accessibles depuis les résultats avec confirmation. La barre de lecture permet d’afficher ou masquer les images ; Ctrl+I assure le même rôle. Retour arrière, Échap et Alt+Gauche permettent de revenir ; les flèches et touches de page assurent la navigation.

Les EPUB retrouvent une progression et un numéro de page pour le livre entier. Le premier calcul parcourt progressivement les sections avec le moteur de mise en page utilisé à l’écran. Pendant ce calcul, `~` marque le pourcentage provisoire et le total n’est pas présenté comme exact. Le résultat est mémorisé pour le même livre, la même police et la même géométrie. Un changement de contenu ou de réglages invalide ce cache. La reprise reste attachée au chapitre, au bloc et à sa position, pas à un numéro de page devenu périmé.

## Langues et portée

Les traductions de l’interface, des aides et des erreurs sont complétées, y compris les nouvelles commandes et le partage dans le navigateur. L’indonésien, le russe et l’arabe portent à onze les packs livrés : français, anglais, allemand, espagnol, italien, néerlandais, portugais du Portugal, portugais du Brésil, indonésien, russe et arabe. Le contrôle final des clés, formats et glyphes figure dans VERIFICATION.md.

Cela ne couvre pas toutes les langues les plus parlées : le chinois et l’hindi nécessitent notamment des polices et un rendu adaptés qui ne sont pas livrés ici. L’arabe utilise la composition et le traitement bidirectionnel de LVGL ; sa lisibilité et les mélanges de texte/chiffres restent à vérifier sur l’écran physique. Les textes des documents, noms de fichiers et articles Wikipédia conservent leur langue. Sans police SD utilisable, l’anglais intégré reste temporaire et la langue choisie n’est pas effacée.

Mail, le navigateur Internet, Quake et Duke Nukem 3D restent retirés. Le formatage et la récupération de fichiers effacés ne sont pas ajoutés. Les fichiers personnels et les clés ne doivent pas être remplacés lors de la mise à jour des ressources.

**Compilation complète et contrôles du firmware 0.8.17 réussis.** [VERIFICATION.md](VERIFICATION.md) distingue les tests hôtes exécutés, les contrôles du firmware et les essais matériels restant à faire. Les corrections de mise en page et de stabilité demandent encore une vérification tactile, clavier et écran sur le Tab5 physique.
