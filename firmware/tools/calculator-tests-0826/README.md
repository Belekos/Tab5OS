# Calculatrice 0.8.21

La saisie et le pavé de 40 touches sont conservés. La colonne de lecture présente chaque calcul puis son résultat ; toucher une ligne ou la valider avec Entrée/Espace recopie son contenu dans la saisie. Les flèches, Page précédente/suivante, Début/Fin et Tab/Maj+Tab parcourent aussi les lignes hors écran sans confondre les objets recyclés.

Après un résultat, un opérateur continue le calcul ; un chiffre ou une fonction commence une nouvelle expression. Une modification volontaire du curseur permet d'éditer le nombre affiché. Les résultats utilisent 17 chiffres significatifs pour préserver l'aller-retour des nombres doubles. Un résultat négatif est regroupé avant une puissance. C efface la saisie, sans effacer l'historique.

Conversions ouvre une fenêtre indépendante. Entrée calcule, Utiliser reprend le résultat dans la calculatrice, Échap referme d'abord une liste d'unités ouverte puis la fenêtre. Les commandes d'édition standard fonctionnent dans les champs et le résultat reste copiable, sans être modifiable.

L'historique est limité aux 128 derniers calculs et à 64 Kio de textes, avec remplacement des plus anciens. Descripteurs et textes utilisent la PSRAM sur la tablette ; l'interface ne crée que 24 lignes LVGL recyclées. Tous ces blocs et les objets graphiques sont libérés à la fermeture de la calculatrice. Aucun fichier d'historique n'est écrit sur la carte SD.

## Vérification

`python -X utf8 tools/calculator-tests/run.py` utilise le compilateur hôte MSVC, la bibliothèque LVGL 9.6 native déjà construite dans `C:/workspace/tab5-ui-0819-host/lvgl.lib`, les sources LVGL de `C:/workspace/tab5-deps-0815/lvgl` et la police livrée sur la SD. Les chemins de travail sont dans le script.

Les tests exécutent les composants de production : géométrie, touches par pointeur, moteur, historique et recyclage des lignes, navigation clavier, fenêtre de conversion, édition et mémoire. Ils vérifient notamment la reprise de 1/3 sans arrondi intermédiaire, les puissances de résultats négatifs, le refus d'une sélection après recyclage sous le doigt, Tab/Maj+Tab sur toutes les lignes, les échecs d'allocation, le plafond mémoire et la restitution de chaque allocation. Les captures sont des rendus hôte, pas des photos de la tablette.

La preuve `tools/verification/calculator-0821.json` contient les empreintes exactes des sources testées. La syntaxe cible de `Tab5Tools.cpp` et la compilation intégrale vérifient séparément son intégration au firmware. Aucun essai physique n'est revendiqué par ces tests.
