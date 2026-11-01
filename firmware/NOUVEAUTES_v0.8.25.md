# Tab5OS 0.8.25

<!-- build-status -->
**Compilation complète et contrôles de l’image 0.8.25 réussis.** Preuve : [firmware-verification.json](tools/verification/firmware-verification.json).
<!-- /build-status -->

Cette version corrige les retours obtenus sur la 0.8.24. La prise en charge des manettes reste conservée.

- **Radio SDR** : correction du traitement FM qui dégradait le signal utile, filtrage audio mono, désaccentuation 50/75 µs et correction de fréquence en ppm. La précision de saisie reste indépendante de la modulation. Les boutons suivent le thème ; les commandes de pas et les champs numériques sont corrigés.
- **Explorateur** : traductions des menus et raccourcis, terminal accessible depuis Actions, ouverture directe des scripts `.sh`, second toucher sur la sélection sans délai imposé, parent à gauche, surbrillance au déplacement de la souris. Fermer un menu conserve l'aperçu.
- **Réglages** : blocs plus compacts et adaptatifs, contour du bouton des interrupteurs respectant le thème.
- **Coffre et clés** : gestion native des secrets, accès par PIN et suppression des clés, y compris la clé initiale. La suppression d'une clé ne déchiffre pas les fichiers déjà protégés avec elle : conserver une copie utile avant suppression.
- **Variables et Git** : variables publiques de session et références vers les secrets du coffre pour le terminal, les scripts, MicroPython et les applications ELF/natives. Git peut utiliser des références vers un identifiant et un jeton sans les enregistrer dans sa configuration.
- **Langues et aide** : les quatorze packs sont mis à jour ; l'audit couvre désormais les titres et raccourcis transmis indirectement aux menus.

Les descriptions techniques se trouvent dans [RTL_SDR.md](RTL_SDR.md), [EXPLORATEUR.md](EXPLORATEUR.md), [SECURITE.md](SECURITE.md) et [ENVIRONNEMENT_ET_COFFRE.md](ENVIRONNEMENT_ET_COFFRE.md).

<!-- resource-summary -->
Les 14 packs couvrent les 3060 clés de référence contrôlées. Factory contient 174 ressources officielles ; le secours firmware en contient 40. Les grandes polices et les dictionnaires de saisie sont distribués sur SD. Preuves : [languages-0825.json](tools/verification/languages-0825.json), [ime-0825.json](tools/verification/ime-0825.json), [resources-0825.json](tools/verification/resources-0825.json).
<!-- /resource-summary -->

Les vérifications logicielles utilisent le code de production et des scénarios reproductibles. Elles ne remplacent pas une écoute SDR avec l'antenne et la clé réelles, ni un essai de saisie sur l'écran physique. Les fichiers `.sh`, Python, ELF et applications natives auxquels un secret est rendu accessible doivent être des programmes de confiance.
