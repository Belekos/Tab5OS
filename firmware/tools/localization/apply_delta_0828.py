"""Apply reviewed .28 translations, with exact .27 help text as the baseline."""
from pathlib import Path
import hashlib
import json
import re
import zipfile

HERE = Path(__file__).resolve().parent
RELEASE = HERE.parents[1]
SKETCH = RELEASE / 'Tab5OS_Arduino_v0.8.28_Ecriture'
LOCALES = ['fr-FR', 'en-US', 'de-DE', 'es-ES', 'it-IT', 'nl-NL', 'pt-BR',
           'pt-PT', 'id-ID', 'ru-RU', 'ar', 'zh-CN', 'ja-JP', 'ko-KR']
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
write = lambda path, data: path.write_text(json.dumps(data, ensure_ascii=False, indent=2)+'\n', encoding='utf8')


def table(name):
    rows = {}
    for number, line in enumerate((HERE / name).read_text(encoding='utf8').splitlines(), 1):
        if not line.strip() or line.startswith('#'):
            continue
        cells = line.split('|')
        assert all(value.strip() == value and value for value in cells), (name, number, 'unintentional whitespace')
        values = [value.replace('\\n', '\n') for value in cells]
        assert len(values) == len(LOCALES), (name, number, len(values))
        assert values[0] not in rows and all(values), (name, number)
        rows[values[0]] = dict(zip(LOCALES, values))
    return rows


def main():
    packs = {locale: json.loads((SKETCH / 'sdcard/System/languages' / (locale+'.json')).read_text(encoding='utf8')) for locale in LOCALES}
    delta = {locale: {} for locale in LOCALES}
    inputs = ['ui-0828.tsv']
    for key, values in table('ui-0828.tsv').items():
        for locale, value in values.items():
            delta[locale][key] = value
    help_table = HERE / 'help-0828.tsv'
    if help_table.exists():
        inputs.append(help_table.name)
        parts = {**table('ui-0828.tsv'), **table(help_table.name)}
        pattern = re.compile(r'\{\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\}')
        archive_path = RELEASE.parent / 'Tab5OS_0.8.27_corrections.zip'
        assert sha(archive_path) == '8e38c90607aa573da3d5c73ebc0cd862f58e09269c5051b6dffbb8ffc828e150'
        with zipfile.ZipFile(archive_path) as archive:
            prefix = 'Tab5OS_Ecriture_v0.8.27/Tab5OS_Arduino_v0.8.27_Ecriture/'
            oldtext = archive.read(prefix+'Tab5Help.cpp').decode('utf8')
            original = {json.loads(a): {'source': json.loads(b), 'translations': {}} for a,b in pattern.findall(oldtext)}
            for locale in LOCALES:
                translations = json.loads(archive.read(prefix+'sdcard/System/languages/'+locale+'.json'))['translations']
                for row in original.values():
                    row['translations'][locale] = translations[row['source']]
        write(HERE / 'help-before-0828.json', original)
        # Each row records its target page and paragraph operation explicitly.
        operations = json.loads((HERE / 'help-operations-0828.json').read_text(encoding='utf8'))
        inputs.append('help-operations-0828.json')
        changed = {}
        help_delta = {locale: {} for locale in LOCALES}
        for title, ops in operations.items():
            translated = {}
            for locale in LOCALES:
                paragraphs = original[title]['translations'][locale].split('\n\n')
                for op in ops:
                    value = parts[op['key']][locale]
                    if op['operation'] == 'append':
                        paragraphs.append(value)
                    else:
                        assert op['operation'] == 'replace'
                        paragraphs[op['paragraph']] = value
                translated[locale] = '\n\n'.join(paragraphs)
            body = translated['fr-FR']
            changed[title] = body
            for locale in LOCALES:
                delta[locale][body] = translated[locale]
                help_delta[locale][body] = translated[locale]
        write(HERE / 'delta-help-0828.json', help_delta)
        inputs.append('delta-help-0828.json')
        path = SKETCH / 'Tab5Help.cpp'
        def replace(match):
            title = json.loads(match[1])
            return '{'+json.dumps(title, ensure_ascii=False)+', '+json.dumps(changed[title], ensure_ascii=False)+'}' if title in changed else match.group()
        path.write_text(pattern.sub(replace, oldtext), encoding='utf8')
        write(RELEASE / 'tools/verification/help-0828.json', {
            'version': '0.8.28', 'passed': True, 'physicalDeviceTested': False,
            'changedPages': list(changed), 'translatedLanguages': 14,
            'sourceSha256': sha(path),
            'sources': {name: sha(HERE/name) for name in ['help-before-0828.json','delta-help-0828.json','apply_delta_0828.py','help-0828.tsv','help-operations-0828.json']},
            'scope': 'Actual translated help paragraphs and exact .27 baseline. No physical device test.'})
    for locale, pack in packs.items():
        pack['translations'].update(delta[locale])
        write(SKETCH / 'sdcard/System/languages' / (locale+'.json'), pack)
    write(HERE / 'delta-0828.json', delta)
    write(HERE / 'reference-final-delta-0828.json', list(delta['fr-FR']))
    write(HERE / 'delta-inputs-0828.json', inputs)
    print('Applied', len(delta['fr-FR']), 'reviewed .28 keys in', len(LOCALES), 'languages')


if __name__ == '__main__':
    main()
