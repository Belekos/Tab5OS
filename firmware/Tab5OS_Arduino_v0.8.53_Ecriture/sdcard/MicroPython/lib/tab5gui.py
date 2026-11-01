"""Small paged, translated touch/USB forms for the Tab5's native UI bridge."""
import tab5ui as ui
import tab5fs as fs
from time import sleep_ms

tr=ui.tr


def start(title, back=True):
    ui.open(tr(title))
    return ui.shortcut(ui.button(tr('Retour'),0,0,140,46), 'back') if back else None


def event():
    while ui.is_open():
        value=ui.poll()
        if value:
            return value
        sleep_ms(20)
    raise KeyboardInterrupt


def message(title, text):
    start(title,False)
    # Native labels are bounded; wrap into up to four labels without cutting UTF-8.
    text=str(text)
    chunks=[]
    while text:
        cut=min(150,len(text))
        if len(text)>cut and '\n' in text[:cut]:
            cut=text.rfind('\n',0,cut)+1
        chunks.append(text[:cut]);text=text[cut:]
    for i,part in enumerate(chunks[:4]):
        ui.label(part,16,20+i*92,1060,86)
    ok=ui.shortcut(ui.button(tr('OK'),820,420,260,60), 'enter')
    while event()[0]!=ok.id:
        pass


def confirm(title, text):
    start(title,False)
    text=str(text)
    for i in range(4):
        part=text[i*150:(i+1)*150]
        if part:
            ui.label(part,20,24+i*76,1040,72)
    no=ui.shortcut(ui.button(tr('Annuler'),20,370,480,64), 'back')
    yes=ui.shortcut(ui.button(tr('Confirmer'),580,370,480,64), 'enter')
    while True:
        key,value=event()
        if key in (no.id,yes.id):
            return key==yes.id


def progress(title):
    start(title,False)
    ui.label(tr('Opération en cours. La croix permet d’annuler.'),20,20,1030,80)
    return ui.label('',20,140,1030,200)


def choose(title, items, labels=None, searchable=False):
    labels=labels or [str(v) for v in items]
    page=0;query=''
    while True:
        back=start(title)
        selected=[i for i,label in enumerate(labels) if query.lower() in label.lower()]
        page=max(0,min(page,max(0,(len(selected)-1)//6)))
        entry=search=clear=None
        if searchable:
            entry=ui.entry(query,156,0,652,46)
            search=ui.button(tr('Rechercher'),822,0,258,46)
        rows={}
        for position,i in enumerate(selected[page*6:page*6+6]):
            row=ui.button(labels[i][-140:],16,74+position*56,1064,50)
            rows[row.id]=i
        ui.label(str(page+1)+' / '+str(max(1,(len(selected)+5)//6))+' · '+str(len(selected)),350,432,380,48)
        prev=ui.shortcut(ui.button('<',16,428,180,50), 'previous');nxt=ui.shortcut(ui.button('>',900,428,180,50), 'next')
        while True:
            key,value=event()
            if key==back.id:
                return None
            if key in rows:
                return items[rows[key]]
            if search and key==search.id:
                query=entry.text();page=0;break
            if key==prev.id:
                page-=1;break
            if key==nxt.id:
                page+=1;break


def pick(directory='/', folder=False):
    directory=fs.path(directory);page=0
    while True:
        back=start('Choisir un dossier' if folder else 'Choisir un fichier')
        address=ui.entry(directory,158,0,698,46)
        go=ui.button(tr('Ouvrir'),870,0,210,46)
        parent=ui.button('↑',16,62,94,46)
        select=ui.button(tr('Ce dossier'),700,62,380,46) if folder else None
        # Read only the requested page; huge directories don't become a Python list.
        rows=[];count=0
        try:
            for leaf in fs.ilistdir(directory):
                if page*5<=count<page*5+5:
                    full=directory.rstrip('/')+'/'+leaf
                    rows.append((leaf,full,fs.isdir(full)))
                count+=1
        except OSError as e:
            message('Erreur',tr('Dossier inaccessible')+'\n'+str(e));directory='/';page=0;continue
        if page and not rows:
            page=max(0,(count-1)//5);continue
        buttons={}
        for i,(leaf,full,isdir) in enumerate(rows):
            row=ui.button(('▸ ' if isdir else '')+leaf[-130:],16,122+i*54,1064,48)
            buttons[row.id]=(full,isdir)
        ui.label(str(page+1)+' / '+str(max(1,(count+4)//5)),330,430,380,44)
        prev=ui.shortcut(ui.button('<',16,426,180,50), 'previous');nxt=ui.shortcut(ui.button('>',900,426,180,50), 'next')
        while True:
            key,value=event()
            if key==back.id:
                return None
            if select and key==select.id:
                return directory
            if key==parent.id:
                directory=directory.rsplit('/',1)[0] or '/';page=0;break
            if key==go.id:
                try:
                    requested=fs.path(address.text())
                    if fs.isdir(requested):
                        directory=requested;page=0
                    elif not folder:
                        return requested
                    break
                except (OSError,ValueError) as e:
                    message('Erreur',tr(str(e)));break
            if key in buttons:
                full,isdir=buttons[key]
                if isdir:
                    directory=full;page=0;break
                if not folder:
                    return full
            if key==prev.id:
                page=max(0,page-1);break
            if key==nxt.id:
                if (page+1)*5<count:
                    page+=1
                break


def failure(error):
    code=12 if isinstance(error,MemoryError) else error.args[0] if isinstance(error,OSError) and error.args else None
    details={12:'Mémoire insuffisante',5002:'Réseau occupé : arrêter la radio puis réessayer',28:'Espace SD insuffisant',5001:'Régler la date et l’heure pour vérifier les certificats',5005:'Adresse HTTPS requise',5003:'Connecter le Wi-Fi dans Réglages',5004:'Connexion HTTPS impossible : vérifier le réseau, la date et les certificats',5006:'Opération annulée'}
    text=tr(details[code]) if code in details else ('HTTP '+str(code-1000) if isinstance(code,int) and 1100<=code<=1599 else translate_error(str(error)))
    message('Erreur',text)


def translate_error(text):
    translated = tr(text)
    if translated != text:
        return translated
    # Error messages often append a user filename to a translated prefix.
    # Translate only a known dictionary entry, never the user's path itself.
    pos = text.find(': ')
    if pos >= 0:
        prefix = text[:pos+2]
        translated = tr(prefix)
        if translated != prefix:
            return translated + text[pos+2:]
    return text
