"""Apply reviewed .27 translations, with exact .26 help text as the baseline."""
from pathlib import Path
import hashlib
import json
import re
import zipfile

HERE = Path(__file__).resolve().parent
RELEASE = HERE.parents[1]
SKETCH = RELEASE / 'Tab5OS_Arduino_v0.8.27_Ecriture'
LOCALES = ['fr-FR', 'en-US', 'de-DE', 'es-ES', 'it-IT', 'nl-NL', 'pt-BR',
           'pt-PT', 'id-ID', 'ru-RU', 'ar', 'zh-CN', 'ja-JP', 'ko-KR']
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
write = lambda path, data: path.write_text(json.dumps(data, ensure_ascii=False, indent=2)+'\n', encoding='utf8')


def table(name):
    rows = {}
    for number, line in enumerate((HERE / name).read_text(encoding='utf8').splitlines(), 1):
        if not line.strip() or line.startswith('#'):
            continue
        values = [value.replace('\\n', '\n') for value in line.split('|')]
        assert len(values) == len(LOCALES), (name, number, len(values))
        assert values[0] not in rows and all(value.strip() == value and value for value in values), (name, number)
        rows[values[0]] = dict(zip(LOCALES, values))
    return rows


def main():
    packs = {locale: json.loads((SKETCH / 'sdcard/System/languages' / (locale+'.json')).read_text(encoding='utf8')) for locale in LOCALES}
    delta = {locale: {} for locale in LOCALES}
    inputs = ['ui-0827.tsv']
    for key, values in table('ui-0827.tsv').items():
        for locale, value in values.items():
            delta[locale][key] = value
    help_table = HERE / 'help-0827.tsv'
    if help_table.exists():
        inputs.append(help_table.name)
        parts = {**table('ui-0827.tsv'), **table(help_table.name)}
        pattern = re.compile(r'\{\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\}')
        archive_path = RELEASE.parent / 'Tab5OS_0.8.26_corrections.zip'
        assert sha(archive_path) == '90eaa4d4c7b9c772baddcea2ce9b204fb05a8b0a34b007da6fcf69adb3b035b2'
        with zipfile.ZipFile(archive_path) as archive:
            prefix = 'Tab5OS_Ecriture_v0.8.26/Tab5OS_Arduino_v0.8.26_Ecriture/'
            oldtext = archive.read(prefix+'Tab5Help.cpp').decode('utf8')
            original = {json.loads(a): {'source': json.loads(b), 'translations': {}} for a,b in pattern.findall(oldtext)}
            for locale in LOCALES:
                translations = json.loads(archive.read(prefix+'sdcard/System/languages/'+locale+'.json'))['translations']
                for row in original.values():
                    row['translations'][locale] = translations[row['source']]
        write(HERE / 'help-before-0827.json', original)
        # Each row records its target page and paragraph operation explicitly.
        operations = json.loads((HERE / 'help-operations-0827.json').read_text(encoding='utf8'))
        inputs.append('help-operations-0827.json')
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
        write(HERE / 'delta-help-0827.json', help_delta)
        inputs.append('delta-help-0827.json')
        path = SKETCH / 'Tab5Help.cpp'
        def replace(match):
            title = json.loads(match[1])
            return '{'+json.dumps(title, ensure_ascii=False)+', '+json.dumps(changed[title], ensure_ascii=False)+'}' if title in changed else match.group()
        path.write_text(pattern.sub(replace, oldtext), encoding='utf8')
        write(RELEASE / 'tools/verification/help-0827.json', {
            'version': '0.8.27', 'passed': True, 'physicalDeviceTested': False,
            'changedPages': list(changed), 'translatedLanguages': 14,
            'sourceSha256': sha(path),
            'sources': {name: sha(HERE/name) for name in ['help-before-0827.json','delta-help-0827.json','apply_delta_0827.py','help-0827.tsv','help-operations-0827.json']},
            'scope': 'Actual translated help paragraphs and exact .26 baseline. No physical device test.'})
    for locale, pack in packs.items():
        pack['translations'].update(delta[locale])
        write(SKETCH / 'sdcard/System/languages' / (locale+'.json'), pack)
    write(HERE / 'delta-0827.json', delta)
    write(HERE / 'reference-final-delta-0827.json', list(delta['fr-FR']))
    write(HERE / 'delta-inputs-0827.json', inputs)
    print('Applied', len(delta['fr-FR']), 'reviewed .27 keys in', len(LOCALES), 'languages')


if __name__ == '__main__':
    main()
