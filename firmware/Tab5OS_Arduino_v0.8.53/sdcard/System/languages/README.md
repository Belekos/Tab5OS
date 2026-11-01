# Langues de Tab5OS

Les packs JSON de ce dossier personnalisent les textes de l'interface. Les langues
fournies sont `ar`, `de-DE`, `en-US`, `es-ES`, `fr-FR`, `id-ID`,
`it-IT`, `ja-JP`, `ko-KR`, `nl-NL`, `pt-BR`, `pt-PT`, `ru-RU` et
`zh-CN`.

Installer les packs dans `/System/languages/` sur la microSD, choisir la langue
dans Réglages → Langue, puis redémarrer pour appliquer le choix à toutes les pages.
Redémarrer également après une modification des fichiers.

Pour préparer une carte, utiliser le [paquet SD complet](../../../../../downloads/Tab5OS_0.8.53_SD.zip),
qui contient aussi les polices nécessaires. La langue de l'interface et la
disposition du clavier se règlent séparément.

## Personnaliser un pack

Dupliquer un JSON, renseigner son `id` et son `name`, puis traduire les valeurs de
`translations`. Conserver les clés exactement, avec leurs accents, espaces,
ponctuation et retours à la ligne. Les noms de fichiers, documents, stations et
lieux personnels restent des données et ne sont pas traduits par l'interface.

Les modèles comportant `%s`, `%u`, `%ld` ou un autre champ printf doivent
conserver les mêmes types de champs et le même ordre. Les listes séparées par
`\n` doivent conserver le même nombre d'options dans le même ordre. Enregistrer
en UTF-8 sans clés JSON en double.

La police installée doit contenir les caractères de la langue. Le rendu des textes
dans une application ELF dépend de cette application.
