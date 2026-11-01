# Tab5OS 0.8.23 — corrections après retour sur tablette

Cette livraison reprend les défauts signalés après installation de la 0.8.22 : sortie du menu Git, création de dossier, commandes de carte peu accessibles, manette détectée seulement depuis le menu de l’émulateur et mémoire disponible après les applications ELF.

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.23 réussis.** Preuve : [firmware-verification.json](tools/verification/firmware-verification.json).
<!-- /build-status -->

## Changements

- **Explorateur et Git** : les fermetures de fenêtres et dialogues sont différées hors des callbacks. Les essais passent par les vrais boutons et les raccourcis ; la fermeture Git reproduisait une corruption mémoire en 0.8.22. Les actions secondaires passent dans un menu commun au clic droit. Le projet Git concerne un dossier sélectionné déjà configuré ; l'initialisation d'un dossier ordinaire sélectionné reste une action distincte.
- **Cartes** : le téléchargement ouvre une fenêtre native avec niveaux de zoom et cadence, sans dépendre de Python. Annuler conserve les tuiles terminées pour une reprise. Le bouton de position actuelle dans les lieux enregistrés tient compte de la largeur du texte.
- **Pentest** : l'application est fournie dans `Applications/Pentest.app.elf`, avec son profil de lancement, ses sources C et une interface utilisant les services communs de l'OS. Elle se lance depuis Applications ELF. L'analyse et l'affichage sont libérés à la fermeture effective du programme.
- **Consoles** : bouton d'effacement, Ctrl+L et clic droit agissent sur la sortie mémorisée du terminal, de Git, de Python et des consoles ELF compatibles. Une tâche peut continuer à produire de nouvelles lignes après l'effacement ; l'ancienne sortie ne réapparaît pas au rafraîchissement.
- **Manettes** : état et configuration dans les réglages hors F1, requête d'initialisation XInput et ancien paquet HID 8BitDo pris en charge. Les diagnostics distinguent identification, réception et décodage. La SN30 Pro reste à essayer physiquement ; les tests utilisent des rapports simulés.
- **Mémoire ELF** : restitution de l'audio après expiration d'une tentative de libération, nettoyage des images et allocations suivies et restitution des réservations après terminaison. L'audit distingue budgets logiques, blocs alloués et mémoire réelle : voir [AUDIT_ELF_0.8.23.md](AUDIT_ELF_0.8.23.md).
- **Interface et langues** : Accueil est masqué dans le menu principal ; les actions contextuelles suivent les thèmes et les commandes existantes. Les quatorze packs comprennent les nouveaux libellés, l'aide des menus/consoles et la correction de l'aide terminal sur les tubes et substitutions.

Les résultats sont consignés dans [VERIFICATION.md](VERIFICATION.md). Une réussite sur le banc hôte ne prouve pas la disparition d’un reset matériel ni le fonctionnement physique d’une manette. Les sources historiques restent accompagnées de leurs preuves d’origine ; aucune ancienne exécution n’est renommée comme un nouvel essai.

<!-- resource-summary -->
Les 14 packs couvrent les 2896 clés de référence contrôlées. Factory contient 95 ressources officielles ; le secours firmware en contient 39. Les grandes polices et les dictionnaires de saisie sont distribués sur SD. Preuves : [languages-0823.json](tools/verification/languages-0823.json), [ime-0819.json](tools/verification/ime-0819.json), [resources-0823.json](tools/verification/resources-0823.json).
<!-- /resource-summary -->

L’installation demande de mettre à jour ensemble firmware et ressources SD : voir [LISEZ_MOI.md](LISEZ_MOI.md). Les documents métier décrivent les commandes effectives et leurs limites.
