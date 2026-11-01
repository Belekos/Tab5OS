# Contribuer à Tab5OS

Une correction doit expliquer le symptôme, sa cause et les effets attendus. Garder un changement centré sur un problème ; identifier les interactions avec l'audio, les supports, le réseau et les applications déjà actives.

## Rapporter un défaut

Indiquer la version exacte du firmware et des ressources SD, le modèle du périphérique, le chemin pour reproduire, le résultat attendu et observé. Pour un redémarrage, joindre le journal série et la trace d'exception quand ils sont disponibles. Retirer mots de passe, jetons et données personnelles des captures. Préciser les essais réussis et ceux qui échouent, plutôt que de remplacer un journal par « ça plante ».

## Modifier le code

Respecter les services communs de l'OS, les couleurs du thème, les raccourcis clavier, la navigation Retour et les traductions. Ne pas ajouter une nouvelle pile d'affichage, audio, USB ou réseau dans une application. Les longues opérations acceptent l'annulation et limitent leur mémoire ; les ressources se libèrent après la fin effective du worker.

Les nouveaux libellés doivent être présents dans chaque pack concerné et utilisables avec les claviers associés. Tester une traduction longue, les caractères CJK/arabe si concernés, le clavier physique et la navigation tactile. Éviter de mettre un bouton destructif contre une touche fréquemment utilisée.

Conserver les mentions de droits, licences et fichiers de provenance des composants tiers. Une modification d'un composant doit décrire son origine, les changements locaux et une révision amont vérifiable. Ne pas ajouter de ROM, BIOS propriétaire, identifiant personnel ou clé privée.

## Présenter une modification

La proposition indique : problème corrigé, comportement obtenu, tests exécutés, limites des essais et changements éventuels de format sur SD. Donner les commandes utiles pour reproduire les vérifications. Distinguer les tests sur ordinateur des observations réelles sur le Tab5.

La licence principale doit encore être choisie par le titulaire du projet avant d'organiser les contributions externes. Aucune cession de droits ni licence universelle n'est créée par ce document ; voir [LICENCES.md](LICENCES.md).
