"""Add the bounded-admission format while preserving every existing translation."""
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[2]
SKETCH=ROOT/'Tab5OS_Arduino_v0.8.49_Ecriture'
OLD='%s\nRAM : %u Kio libres ; %u nécessaires (%u démarrage + %u réservés + 32 marge).\nBloc interne : %u / %u Kio ; DMA : %u / %u Kio.\nPSRAM : %u Kio libres ; %u nécessaires ; bloc %u / %u Kio.'
NEW=OLD.replace('%u / %u','%s %u')

def main():
    paths=sorted((SKETCH/'sdcard/System/languages').glob('*.json'))
    assert len(paths)==14
    for path in paths:
        data=json.loads(path.read_text());before=dict(data['translations'])
        original=before[OLD]
        assert original.count('%u / %u')==3,path.name
        translated=original.replace('%u / %u','%s %u')
        assert len(translated.encode())<384,path.name
        if NEW in before:assert before[NEW]==translated,path.name
        data['translations'][NEW]=translated
        assert all(data['translations'][key]==value for key,value in before.items())
        path.write_text(json.dumps(data,ensure_ascii=False,separators=(',',':'))+'\n')
    print('14 admission templates added; all existing translations preserved.')

if __name__=='__main__':main()
