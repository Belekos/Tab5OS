# Explorateur

Le panneau gauche est un arbre de **dossiers**. Le panneau droit présente les fichiers et dossiers du répertoire ouvert, ou une vue associée. La sélection détermine les commandes ; déplacer le focus ne change pas à lui seul leur cible.

## Démarrage et supports

La racine **SD** est ouverte au premier accès, ou **USB** si la SD est absente. **USB** apparaît comme un autre support de l’arbre uniquement lorsqu’un volume est monté. La navigation est conservée en mémoire pendant que l’OS reste allumé : quitter l’explorateur puis y revenir ne remet pas systématiquement la racine. Elle n’est pas écrite comme une nouvelle préférence persistante entre redémarrages.

## Gestes

| Geste | Résultat |
|---|---|
| Clic sur un dossier à gauche | Sélection du dossier et affichage de son contenu à droite |
| Clic sur un dossier ou fichier à droite | Sélection uniquement ; la liste et le chemin affichés restent ceux du dossier ouvert |
| Double-clic sur un dossier à droite | Ouverture du dossier dans le panneau droit et révélation/focus du dossier correspondant à gauche |
| Double-clic sur un fichier à droite | Ouverture avec l’application adaptée ; un fichier non reconnu ne s’exécute pas arbitrairement |
| Chevron d’un dossier à gauche | Dépliage/repli de cette branche uniquement |
| Ctrl/Maj ou sélection multiple | Modification de la sélection sans navigation |
| Clic droit | Menu correspondant à la sélection ou à la cible explicite |

Le chevron n’est affiché qu’après détection d’un sous-dossier. Cette détection se fait progressivement sur les lignes utiles à l’affichage et ne parcourt pas récursivement tout le support au démarrage. Un dossier contenant seulement des fichiers n’a pas de chevron.

Le chemin de l’en-tête droit désigne le contenu affiché. Sélectionner une ligne n’affiche pas son chemin à la place de celui du dossier ouvert. Les deux panneaux restent séparés par une ligne fine.

## Menus

- **Fichier** : créer les éléments proposés par l’OS.
- **Édition** : copier, couper, coller, renommer, supprimer et gérer la sélection.
- **Actions** : ouvrir, choisir une application, consulter les informations et accéder aux actions adaptées au type sélectionné.
- **Git** : un sous-menu contextuel d’Actions ou du menu de la sélection, adapté au dossier choisi ; un dossier ordinaire et un dépôt ne proposent pas les mêmes opérations. Le raccourci Git n’est visible que pour un dossier reconnu comme Git.
- **Affichage** : options de présentation et fichiers cachés.
- **Icône de tri** : choix de l’ordre de classement.
- **Loupe** : recherche. Le choix SD/USB se fait directement dans l’arbre.

Les commandes de modification visent la sélection explicite. Sans sélection, un Suppr dans l’explorateur ne vise pas un document ouvert dans une autre application. Annuler une suppression conserve le contenu consulté. Les destinations de création et de collage utilisent le contexte choisi ; après Parent, elles suivent le dossier affiché.

## Noms et validation

Les noms de fichiers et dossiers proposés par défaut sont en anglais, quelle que soit la langue d’affichage. Les fichiers personnels existants et les chemins configurés ne sont pas renommés automatiquement. Les anciens noms reconnus restent des chemins de compatibilité lorsqu’ils sont nécessaires.
