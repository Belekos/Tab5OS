# Langues de Tab5OS

Copier les fichiers JSON de ce dossier dans `/System/languages/` sur la SD.
La langue se choisit dans Réglages → Langue. Redémarrer pour appliquer
le choix à toutes les pages déjà construites. Après avoir modifié les fichiers SD,
redémarrer pour les recharger.

Les packs fournis `fr-FR.json` et `en-US.json` couvrent les mêmes clés, y compris
l’aide, les menus d’Écriture et de projets, Wikipédia, Cartes, les thèmes, les
réglages et les messages des autres applications. Le français peut lui aussi être
personnalisé sur la SD. L’anglais dispose aussi d’un repli intégré au firmware ;
une clé absente des packs et de ce repli revient au libellé français du programme.
Le démarrage reste en anglais, indépendamment de la langue choisie pour l’interface.

Packs supplémentaires : `de-DE`, `es-ES`, `it-IT`, `pt-PT`, `pt-BR`, `nl-NL`,
`id-ID`, `ru-RU`, `ar`. La livraison 0.8.17 couvre 2 507 clés de référence
par pack, y compris les aides, les erreurs et commandes du terminal, ainsi que
les commandes du partage dans le navigateur.
Les noms propres, chemins et commandes restent inchangés. Les formats printf,
les listes multilignes et les glyphes sont vérifiés avant distribution.

L’arabe utilise la composition contextuelle et le texte bidirectionnel de LVGL.
Les 2 507 traductions ont été vérifiées après composition et réordonnancement,
y compris les chemins latins et les glyphes de la police fournie. Ces contrôles
logiciels ne remplacent pas une vérification visuelle sur le Tab5. Les textes des
interfaces MicroPython fournies et les commandes natives utilisent des objets
LVGL ; le contenu dessiné dans le framebuffer d’une application ELF dépend de
cette application.

Le chinois, l’hindi et le bengali ne sont pas proposés dans cette version : la
police fournie ne contient pas leurs caractères. L’hindi et le bengali nécessitent
également une composition des écritures indiennes absente du moteur actuel.
Ajouter seulement un JSON ne suffirait pas à les afficher correctement.

Pour ajouter une langue, dupliquer un JSON, lui donner un autre `id` (par exemple
`es-ES`) et un `name`, puis traduire seulement les valeurs de `translations`.
Conserver les clés exactement : accents, espaces, ponctuation et `\n` compris.
Les noms de fichiers, documents, stations et lieux personnels restent des données
et ne sont pas traduits par l’interface. Pour les noms des résultats de recherche
cartographique, le service reçoit l’identifiant de langue sélectionné.

Les modèles comportant `%s`, `%u`, `%ld` ou un autre champ printf doivent garder
les mêmes types de champs et le même ordre. Leur largeur numérique peut changer.
Un pack dont un modèle change les arguments est refusé en conservant la langue
précédente. Les arguments positionnels et `%n` sont refusés. Les listes d’options
séparées par `\n` doivent garder le même nombre d’options, dans le même ordre.

Les fichiers sont en UTF-8, sans clés JSON en double, de 512 Kio maximum, avec au
plus 4096 entrées. Leur contenu et la table de recherche sont conservés en PSRAM.
La police installée doit contenir les caractères de la langue. La langue de
l’interface ne change pas la disposition du clavier : celle-ci se règle séparément.

Les rapports reproductibles sont dans `tools/verification/languages-0817.json`
et `tools/verification/arabic-0817.json`, à la racine de la livraison. L’audit
contrôle aussi les longs textes laissés en français ou en anglais ; les noms
propres, unités et termes réellement communs à plusieurs langues sont conservés.
