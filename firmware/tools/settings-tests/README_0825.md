# Vérifications Réglages et coffres — 0.8.25

Les commandes se lancent avec Python sous Windows et utilisent l'installation MSVC ainsi que LVGL 9.6 déjà présents dans l'environnement de compilation. Aucun accès à une tablette n'est utilisé.

## Mise en page

`python tools/settings-tests/run_layout.py`

Le test extrait la fonction actuelle de création de Réglages, les helpers et la création de la carte manette. Il exécute les appels réels à LVGL ; seuls les callbacks vers les services et le matériel sont supprimés ou simulés. Il utilise la police distribuée à 24 pixels et les 14 traductions, vérifie les limites horizontales et verticales des lignes, les cibles tactiles, l'affichage d'un rapport de manette de quatre lignes, les informations de partage et les changements de largeur 1216 → 1280 → 1216 pixels. Le rapport est `tools/verification/settings-layout-0825.json`.

## Interface des clés, du PIN et des identifiants

`python tools/settings-tests/run_credentials.py`

Le test inclut directement `Tab5CredentialUi.inc` et `Tab5TextInput.cpp`, extrait les helpers de `Tab5Tools.cpp` et déclenche de vrais événements de clic LVGL. Il couvre notamment :

- suppression de la dernière clé, création dans un emplacement libéré et sélection de la bonne clé pour la sauvegarde ;
- 200 cycles sauvegarde, fermeture et réouverture ; conservation de la sélection à la fin d'une sauvegarde ;
- PIN incorrect, confirmation, changement et migration vers le coffre appareil ; refus d'un ancien mot de passe CJK dépassant 256 octets ;
- création, affichage, sélection et suppression d'identifiants ; impossibilité de les copier par l'action ordinaire ou de les retrouver dans l'historique d'édition après affichage ;
- copie explicite expirant après 30 secondes, passage du compteur temporel par zéro et effacement immédiat au verrouillage ; observation de l'effacement des buffers avant libération ;
- textes de boutons contenus dans leurs limites et absence de chevauchement des contrôles, instructions et statuts dans les pages des 14 langues.

Le stockage, le chiffrement et l'environnement sont simulés : ce test ne démontre pas leur fonctionnement. L'hôte externe d'ouverture/fermeture est réduit, avec vérification et empreinte de l'ordre utilisé par le code réel. Les API et callbacks de l'interface des coffres sont réels. Le rapport est `tools/verification/credential-ui-0825.json`.

Les contrôles généraux des interrupteurs et de l'aperçu du thème sont dans `tools/ui-tests/run.py`, rapport `ui-controls-0825.json`. Les trois rapports comprennent les empreintes des sources exercées et doivent être régénérés si ces sources ou leurs packs linguistiques changent.
