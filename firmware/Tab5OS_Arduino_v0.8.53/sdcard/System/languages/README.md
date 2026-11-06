# Tab5OS languages

The JSON packs in this directory customize interface text. The supplied
languages are `ar`, `de-DE`, `en-US`, `es-ES`, `fr-FR`, `id-ID`,
`it-IT`, `ja-JP`, `ko-KR`, `nl-NL`, `pt-BR`, `pt-PT`, `ru-RU` and
`zh-CN`.

Install the packs in `/System/languages/` on the microSD card, select the
language in Settings → Language, then restart to apply it to all pages.
Also restart after changing the files.

To prepare a card, use the [complete SD package](../../../../../downloads/Tab5OS_0.8.53_SD.zip),
which also contains the required fonts. Interface language and keyboard
layout are configured separately.

## Customize a pack

Duplicate a JSON file, set its `id` and `name`, then translate the values in
`translations`. Preserve the keys exactly, including accents, spaces,
punctuation and line breaks. Personal filenames, documents, stations and
place names remain data and are not translated by the interface.

Templates containing `%s`, `%u`, `%ld` or another printf placeholder must
retain the same placeholder types in the same order. Lists separated by
`\n` must retain the same number of options in the same order. Save as
UTF-8 without duplicate JSON keys.

The installed font must contain the language's characters. Text rendering
inside an ELF application depends on that application.
