# Nouveautés 0.8.19

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.19 réussis.** Preuve : [firmware-verification.json](tools/verification/history-0819/firmware-verification.json).
<!-- /build-status -->

Cette livraison répond aux régressions signalées après la 0.8.18. Les détails des commandes et de leurs paramètres figurent dans [TERMINAL.md](TERMINAL.md) et dans l’aide intégrée.

- Retour Accueil depuis les outils, dont la calculatrice ; commandes de correction éloignées de la fermeture.
- Choix du support dans l’explorateur : carte SD et disque USB monté, retour vers la SD possible depuis l’USB ; ouverture avec une application par icône.
- Personnalisation via Réglages → Interface → Affichage → Accueil et bandeau, sans onglet Accueil séparé ; réglage des arrondis, contrôles assortis au thème et séparation fine des onglets.
- Chargement des catalogues de langues et de claviers au moment de leur utilisation.
- Installation de firmware possible sans analyse préalable séparée, avec contrôle de compatibilité et d’intégrité pendant l’installation.
- Terminal enrichi, aide détaillée et actions d’édition cohérentes ; application Pentest distincte.
- Repli des caractères arabes des polices inclinées vers la police normale de même taille ; contrôles des packs, polices et dispositions de clavier.

Les saisies chinoise et japonaise utilisent les lectures exactes du dictionnaire : elles ne font pas d’analyse automatique de phrases complètes. Le coréen compose le hangul moderne en 2-beolsik, sans conversion hanja. Ctrl+Espace alterne entre saisie latine et méthode de saisie ; Espace convertit, 1–6 ou Entrée choisit, Échap annule.

La liste précise des tests figure dans [VERIFICATION.md](VERIFICATION.md). Aucun essai sur le Tab5 physique n’a été réalisé.

<!-- resource-summary -->
Les 14 packs couvrent les 2711 clés de référence contrôlées. Factory contient 93 ressources officielles ; le secours firmware en contient 38. Les grandes polices et les dictionnaires de saisie sont distribués sur SD. Preuves : [languages-0819.json](tools/verification/languages-0819.json), [ime-0819.json](tools/verification/ime-0819.json), [resources-0819.json](tools/verification/resources-0819.json).
<!-- /resource-summary -->

Le VPN et Tor ne sont pas annoncés comme disponibles. L’audit des accès réseau et des projets proposés est fourni dans [RESEAU_DIAGNOSTIC.md](RESEAU_DIAGNOSTIC.md).
