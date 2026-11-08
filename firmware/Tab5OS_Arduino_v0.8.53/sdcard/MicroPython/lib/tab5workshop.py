"""Tab5 on-device tools: radios, themes and SD recovery. No PC modules."""
import json
import tab5fs as fs

from tab5factory import VERSION, FILES, PREVIOUS
FACTORY = '/Factory/Tab5OS-' + VERSION
COLOR_KEYS = ('bg','surface','raised','border','text','muted','active','cursor','selected')
GREEN = {'bg':'#CFE8CC','surface':'#E0EFDD','raised':'#BCD6B8','border':'#71916C','text':'#172517','muted':'#42563F','active':'#A3C39D','cursor':'#183418','selected':'#A3C39D'}


def identifier(value, maximum=23):
    if not value or len(value) > maximum or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_' for c in value):
        raise ValueError('Identifiant invalide')
    return value


def color(value):
    if not isinstance(value,str) or len(value)!=7 or value[0]!='#' or any(c not in '0123456789abcdefABCDEF' for c in value[1:]):
        raise ValueError('Couleur : #RRGGBB')
    return value.upper()


def contrast(background, foreground):
    def luminance(value):
        rgb = [int(value[i:i+2],16)/255 for i in (1,3,5)]
        linear = [v/12.92 if v<=0.04045 else ((v+0.055)/1.055)**2.4 for v in rgb]
        return linear[0]*0.2126+linear[1]*0.7152+linear[2]*0.0722
    a,b=luminance(color(background)),luminance(color(foreground))
    return (max(a,b)+0.05)/(min(a,b)+0.05)


def themes():
    name='/System/themes.json'
    if not fs.exists(name):
        return []
    doc=fs.read_json(name,32768)
    if not isinstance(doc,dict) or not isinstance(doc.get('themes'),list):
        raise ValueError('Fichier de thèmes invalide')
    return doc['themes']


def save_theme(theme):
    theme=dict(theme)
    identifier(theme.get('id',''))
    name=theme.get('name','').strip()
    if not name or len(name.encode())>63 or '\n' in name:
        raise ValueError('Nom du thème invalide')
    theme['name']=name
    for key in COLOR_KEYS:
        theme[key]=color(theme.get(key,''))
    items=themes()
    index=len(items)
    for i,item in enumerate(items):
        if item.get('id')==theme['id']:
            index=i;break
    if index<len(items):
        items[index]=theme
    else:
        if len(items)>=16:
            raise ValueError('Maximum : 16 thèmes personnels')
        items.append(theme)
    payload={'version':1,'themes':items}
    if len(json.dumps(payload).encode())>32768:
        raise ValueError('Fichier de thèmes trop volumineux')
    with fs.Transaction() as tx:
        if fs.exists('/System/themes.json'):
            fs.copy('/System/themes.json',tx.allocate('/.tab5-backups/System/themes.json.bak'))
        tx.json('/System/themes.json',payload)
        tx.commit()
    return theme


def _station(item):
    if not isinstance(item,dict):
        raise ValueError('Station invalide')
    name=str(item.get('name','')).strip()
    url=str(item.get('url','')).strip()
    style=str(item.get('style') or item.get('age') or 'Autres').strip()
    if not name or len(name.encode())>128 or len(style.encode())>160 or len(url.encode())>512 or not url.startswith(('https://','http://')):
        raise ValueError('Nom ou adresse de radio invalide')
    if any(ord(c)<32 for c in name+style+url):
        raise ValueError('Caractère invalide dans une station')
    enabled=item.get('enabled',True)
    if isinstance(enabled,str):
        enabled=enabled.lower() not in ('false','0','no','non')
    return {'name':name,'url':url,'style':style,'enabled':bool(enabled)}


def _csv_row(line, separator):
    # CSV quotes and doubled quotes, one station per physical line.
    values=[]; current=''; quoted=False; i=0
    while i<len(line):
        c=line[i]
        if c=='"':
            if quoted and i+1<len(line) and line[i+1]=='"':
                current+='"';i+=1
            else:
                quoted=not quoted
        elif c==separator and not quoted:
            values.append(current.strip());current=''
        else:
            current+=c
        i+=1
    if quoted:
        raise ValueError('CSV : guillemets non fermés')
    values.append(current.strip())
    return values


def read_radios(filename, allow_empty=False):
    if fs.stat(filename)[1]>262144:
        raise ValueError('Catalogue radio trop volumineux')
    if filename.lower().endswith('.json'):
        data=fs.read_json(filename)
        if isinstance(data,dict):
            data=data.get('radios')
        if not isinstance(data,list):
            raise ValueError('Catalogue radio invalide')
        items=[_station(v) for v in data]
    else:
        items=[]
        with open(filename) as f:
            if filename.lower().endswith('.csv'):
                first=f.readline().lstrip('\ufeff').strip()
                sep=';' if ';' in first else ','
                keys=[k.lower() for k in _csv_row(first,sep)]
                if 'name' not in keys or 'url' not in keys:
                    raise ValueError('CSV : colonnes name, url, style')
                for line in f:
                    if not line.strip():
                        continue
                    row=_csv_row(line.strip(),sep)
                    if len(row)!=len(keys):
                        raise ValueError('CSV : nombre de colonnes incorrect')
                    items.append(_station(dict(zip(keys,row))))
                    if len(items)>800:
                        raise ValueError('Maximum : 800 radios')
            else:
                name='';style='Autres'
                for line in f:
                    line=line.strip().lstrip('\ufeff')
                    if line.startswith('#EXTINF:'):
                        name=line.rsplit(',',1)[-1].strip()
                        group='group-title="'
                        if group in line:
                            style=line.split(group,1)[1].split('"',1)[0]
                    elif line.startswith('#EXTGRP:'):
                        style=line[8:].strip()
                    elif line and not line.startswith('#'):
                        items.append(_station({'name':name or line,'url':line,'style':style}))
                        name='';style='Autres'
                        if len(items)>800:
                            raise ValueError('Maximum : 800 radios')
    if len(items)>800 or (not items and not allow_empty):
        raise ValueError('Catalogue vide ou supérieur à 800 radios')
    return items


def radio_preview(filename):
    incoming=read_radios(filename)
    old='/radios.json' if fs.exists('/radios.json') else '/System/radios.json'
    existing=read_radios(old,True) if fs.exists(old) else []
    merged={r['url']:r for r in existing}
    added=0
    for item in incoming:
        if item['url'] not in merged:
            added+=1
        merged[item['url']]=item
    if len(merged)>800:
        raise ValueError('Maximum : 800 radios après fusion')
    return {'radios':list(merged.values())},added,len(incoming)-added,old


def import_radios(filename):
    data,added,updated,old=radio_preview(filename)
    with fs.Transaction() as tx:
        if fs.exists(old):
            fs.copy(old,tx.allocate('/.tab5-backups/radios.json.bak'))
        tx.json('/radios.json',data)
        tx.commit()
    return added,updated


def repair_plan():
    """Read-only audit. Unknown files and valid personal configuration are kept."""
    manifest=fs.read_json(FACTORY+'/manifest.json')
    entries=manifest.get('files')
    if manifest.get('version')!=VERSION or not isinstance(entries,dict) or set(entries)!=set(FILES):
        raise ValueError('Référence de réparation incompatible')
    plan=[]
    for relative,digest in entries.items():
        fs.relative(relative)
        if not isinstance(digest,str) or len(digest)!=64 or any(c not in '0123456789abcdef' for c in digest):
            raise ValueError('Empreinte de référence invalide')
        source=FACTORY+'/'+relative
        if fs.sha256(source)!=digest:
            raise ValueError('Référence de réparation corrompue : '+relative)
        dest='/'+relative
        previous=None
        kind=None
        if not fs.exists(dest):
            kind='missing'
        elif fs.isdir(dest):
            kind='conflict'
        else:
            previous=fs.sha256(dest)
            if previous==digest:
                continue
            if relative.startswith('System/languages/') and relative.endswith('.json') and previous in PREVIOUS.get(relative,()):
                # Known official packs may exceed the JSON parser budget. Their
                # exact hashes permit replacement without parsing user content.
                kind='update'
            elif relative.startswith('System/') and relative.endswith('.json') and fs.stat(dest)[1]>262144:
                # A resource above the parser budget is not evidence of corruption.
                kind='modified'
            elif relative.startswith('System/languages/') and relative.endswith('.json'):
                base=fs.read_json(source)
                try:
                    current=fs.read_json(dest)
                    if not isinstance(current,dict) or not isinstance(current.get('translations'),dict):
                        raise ValueError('Langue invalide')
                except ValueError:
                    kind='corrupt'
                else:
                    if any(k not in current['translations'] for k in base['translations']):
                        kind='language'
            elif relative.startswith('System/') and relative.endswith(('.json','.ini')):
                if relative.endswith('.json'):
                    try:
                        fs.read_json(dest)
                    except ValueError:
                        kind='corrupt'
            elif previous in PREVIOUS.get(relative,()):
                kind='update'
            else:
                kind='modified'
        if kind:
            plan.append((source,dest,kind,previous))
    return plan


def _repair_backup(dest):
    base='/.tab5-backups/repair/'+VERSION+dest
    for i in range(1000):
        candidate=base+'.'+str(i)
        if not fs.exists(candidate):
            return candidate
    raise ValueError('Trop de sauvegardes de récupération')


def repair_sd(progress=None, plan=None, restore_modified=False):
    current=repair_plan()
    if plan is not None and plan!=current:
        raise ValueError('Les fichiers ont changé. Relancer la vérification.')
    plan=[e for e in current if e[2]!='conflict' and (e[2]!='modified' or restore_modified)]
    if not plan:
        return 0
    # All sources and destinations were checked before the first write.
    with fs.Transaction() as tx:
        for source,dest,kind,previous in plan:
            if fs.exists(dest)!=(previous is not None) or (previous is not None and fs.sha256(dest)!=previous):
                raise ValueError('Fichier modifié pendant la préparation')
            if progress:
                progress.text(dest)
            if previous is not None:
                fs.copy(dest,tx.allocate(_repair_backup(dest)))
            if kind=='language':
                base=fs.read_json(source)
                current=fs.read_json(dest)
                base['translations'].update(current['translations'])
                for key,value in current.items():
                    if key!='translations':
                        base[key]=value
                tx.json(dest,base)
            else:
                fs.copy(source,tx.allocate(dest))
        tx.commit()
    return len(plan)
