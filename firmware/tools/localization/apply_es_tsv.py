import json, sys
from pathlib import Path
folder=Path(__file__).resolve().parent
reference=json.loads((folder/'reference-complete-0817.json').read_text(encoding='utf-8-sig'))
path=folder.parents[1]/'Tab5OS_Arduino_v0.8.18_Ecriture/sdcard/System/languages/es-ES.json'
pack=json.loads(path.read_text(encoding='utf-8-sig'))
text=sys.stdin.read()
count=0
for row in text.splitlines():
 if not row.strip():continue
 index,value=row.split('\t',1)
 entry=reference[int(index)]
 assert entry['index']==int(index)
 pack['translations'][entry['source']]=value.replace('\\n','\n')
 count+=1
path.write_text(json.dumps(pack,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
with (folder/'es-ES-translation.tsv').open('a',encoding='utf-8') as out:out.write(text+'\n')
print('Applied',count,'Spanish translations')
