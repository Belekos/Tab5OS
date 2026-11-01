"""Applications run entirely in the Tab5 MicroPython VM; UI is rendered by Tab5OS."""
import tab5ui as ui
import tab5gui as gui
import tab5fs as fs
from time import sleep_ms
tr=ui.tr


def packages():
    import tab5libraryui
    tab5libraryui.main(packages_tab5)


def packages_tab5():
    import mip
    source='';version=''
    while True:
        back=gui.start('Modules pour cette Tab5')
        ui.label(tr('Nom du paquet ou adresse HTTPS / github:'),16,70,1060,40)
        name=ui.entry(source,16,116,1064,48)
        ui.label(tr('Version (vide : dernière version)'),16,180,620,42)
        release=ui.entry(version,680,176,400,48)
        install=ui.button(tr('Installer'),16,248,328,58)
        catalog=ui.button(tr('Catalogue'),384,248,328,58)
        installed=ui.button(tr('Installées'),752,248,328,58)
        refresh=ui.button(tr('Actualiser le catalogue'),16,340,510,56)
        ui.label(tr('Installation globale pour les scripts de cette Tab5. Pour une autre carte, revenir au catalogue MicroPython.'),16,420,1060,74)
        key,value=gui.event();source=name.text().strip();version=release.text().strip()
        if key==back.id:
            return
        try:
            if key==catalog.id:
                items=mip.catalog()
                if not items:
                    gui.message('Catalogue',tr('Actualiser le catalogue avec le Wi-Fi connecté.'))
                else:
                    selected=gui.choose('Catalogue MicroPython',items,[p['name']+' · '+p.get('version','') for p in items],True)
                    if selected:
                        source=selected['name'];version=''
            elif key==refresh.id:
                p=gui.progress('Catalogue MicroPython');items=mip.catalog(True,p)
                gui.message('Catalogue',str(len(items))+' '+tr('paquets référencés'))
            elif key==install.id:
                if not source:
                    raise ValueError('Saisir un nom de paquet')
                if gui.confirm('Installer une bibliothèque',source+'\n'+tr('Les sources seront installées avec leurs dépendances.')):
                    p=gui.progress('Installation');changed=mip.install(source,version or None,progress=p)
                    gui.message('Installation',tr('Bibliothèques installées')+'\n'+'\n'.join(r['name']+' '+r['version'] for r in changed))
            elif key==installed.id:
                manage_packages(mip)
        except (OSError,ValueError,KeyError,TypeError,MemoryError) as e:
            gui.failure(e)


def manage_packages(mip):
    while True:
        records=mip.list()
        if not records:
            gui.message('Bibliothèques',tr('Aucune bibliothèque installée par le gestionnaire.'));return
        record=gui.choose('Bibliothèques installées',records,[p['name']+' · '+p['version'] for p in records],True)
        if record is None:
            return
        back=gui.start('Bibliothèque')
        ui.label(record['name']+'\n'+record['version'],16,70,1060,92)
        ui.label(record['source'][-300:],16,172,1060,90)
        ui.label(str(len(record['files']))+' '+tr('fichiers')+' · '+str(len(record['deps']))+' '+tr('dépendances'),16,276,1060,48)
        update=ui.button(tr('Mettre à jour'),16,370,500,60)
        remove=ui.button(tr('Désinstaller'),580,370,500,60)
        key,value=gui.event()
        if key==update.id and gui.confirm('Mettre à jour',record['name']):
            p=gui.progress('Mise à jour');mip.update(record['source'],progress=p)
            gui.message('Mise à jour',tr('Bibliothèques mises à jour'))
        elif key==remove.id and gui.confirm('Désinstaller',record['name']+'\n'+tr('Les dépendances et les fichiers personnels sont conservés.')):
            mip.uninstall(record['source'])
            gui.message('Bibliothèques',tr('Bibliothèque désinstallée'))


def radios():
    import tab5catalogs
    tab5catalogs.radios(radios_manual)


def radios_manual():
    import tab5workshop as work
    source=''
    while True:
        back=gui.start('Importer des radios')
        ui.label(tr('Fichier SD ou adresse HTTPS (JSON, CSV, M3U)'),16,70,1060,48)
        entry=ui.entry(source,16,132,1064,48)
        browse=ui.button(tr('Parcourir'),16,214,400,56)
        load=ui.button(tr('Importer'),680,214,400,56)
        key,value=gui.event();source=entry.text().strip()
        if key==back.id:
            return
        try:
            if key==browse.id:
                chosen=gui.pick()
                if chosen:
                    source=chosen
            elif key==load.id:
                local=source;temp=None
                try:
                    if source.startswith('https://'):
                        suffix=source.split('?',1)[0].rsplit('.',1)[-1].lower()
                        if suffix not in ('json','csv','m3u','m3u8'):
                            raise ValueError('Adresse terminant par .json, .csv ou .m3u requise')
                        temp='/MicroPython/.import-radio.'+suffix
                        if fs.exists(temp):
                            fs.remove(temp)
                        fs.download(source,temp,262144,gui.progress('Téléchargement'))
                        local=temp
                    data,added,updated,old=work.radio_preview(local)
                    del data
                    if gui.confirm('Importer des radios',str(added)+' '+tr('nouvelles')+' · '+str(updated)+' '+tr('mises à jour')):
                        work.import_radios(local)
                        gui.message('Radios',tr('Import terminé. Recharger Musique ou redémarrer pour actualiser la liste.'))
                finally:
                    if temp and fs.exists(temp):
                        fs.remove(temp)
        except (OSError,ValueError,KeyError,TypeError,MemoryError) as e:
            gui.failure(e)


def maps():
    import tab5catalogs
    tab5catalogs.maps(maps_manual)


def maps_manual():
    import tab5maps as carto
    source='';cache='osm'
    while True:
        back=gui.start('Importer des cartes')
        ui.label(tr('Paquet .t5map, dossier XYZ ou URL HTTPS'),16,66,1060,46)
        entry=ui.entry(source,16,118,1064,48)
        file=ui.button(tr('Fichier'),16,188,330,52)
        folder=ui.button(tr('Dossier XYZ'),386,188,330,52)
        load=ui.button(tr('Importer'),756,188,324,52)
        ui.label(tr('Identifiant du cache XYZ'),16,278,630,46)
        identity=ui.entry(cache,680,272,400,48)
        key,value=gui.event();source=entry.text().strip();cache=identity.text().strip()
        if key==back.id:
            return
        try:
            if key in (file.id,folder.id):
                chosen=gui.pick(folder=key==folder.id)
                if chosen:
                    source=chosen
            elif key==load.id and gui.confirm('Importer des cartes',source):
                p=gui.progress('Import des cartes')
                if source.startswith('https://') or source.lower().endswith('.t5map'):
                    result=carto.import_pack(source,progress=p)
                    gui.message('Cartes',tr('Paquet importé. Ouvrir Cartes pour le consulter.')+'\n'+tr('Identifiant du cache : ')+result['cache_id'])
                elif fs.isdir(source):
                    added,skipped=carto.import_xyz(source,cache,p)
                    gui.message('Cartes',str(added)+' '+tr('tuiles importées')+' · '+str(skipped)+' '+tr('déjà présentes'))
                else:
                    raise ValueError('Format de carte non pris en charge')
        except (OSError,ValueError,KeyError,TypeError,MemoryError) as e:
            gui.failure(e)


def themes():
    from _tab5 import call
    call(102)
    raise SystemExit


def repair():
    import tab5workshop as work
    try:
        gui.progress('Vérifier la carte SD')
        plan=work.repair_plan()
        if not plan:
            gui.message('Carte SD',tr('Les fichiers de référence sont à jour.'))
            return
        actions=['Réparer et mettre à jour','Afficher les différences']
        if any(e[2]=='modified' for e in plan):
            actions.append('Restaurer les fichiers modifiés')
        kinds={'missing':'Absent','update':'Mise à jour','language':'Traductions manquantes','corrupt':'JSON invalide','modified':'Modifié — conservé','conflict':'Dossier — conservé'}
        while True:
            choice=gui.choose('Vérifier la carte SD',actions,[tr(a) for a in actions])
            if choice is None:
                return
            if choice=='Afficher les différences':
                gui.choose('Différences',plan,[tr(kinds[e[2]])+' · '+e[1] for e in plan])
                continue
            restore=choice=='Restaurer les fichiers modifiés'
            count=sum(1 for e in plan if e[2]!='conflict' and (restore or e[2]!='modified'))
            if not count:
                gui.message('Carte SD',tr('Aucun fichier à réparer. Les fichiers modifiés sont conservés.'))
                continue
            notice=str(count)+' '+tr('fichiers à compléter ou réparer')+'\n'+tr('Les fichiers remplacés seront sauvegardés dans .tab5-backups/repair. Aucun fichier personnel supplémentaire ne sera supprimé.')
            if restore:
                notice+='\n'+tr('Les modifications des fichiers de référence seront remplacées. Leur sauvegarde sera conservée.')
            if gui.confirm(choice,notice):
                p=gui.progress('Mise à jour de la SD')
                work.repair_sd(p,plan,restore)
                gui.message('Carte SD',tr('Mise à jour terminée. Redémarrer pour charger les modifications.'))
                return
    except (OSError,ValueError,KeyError,TypeError,MemoryError) as e:
        gui.failure(e)


def main():
    fs.recover()
    while True:
        gui.start('Atelier Python · Tab5',False)
        actions={}
        for i,(label,action) in enumerate((('Cartes',maps),('Radios',radios),('Bibliothèques',packages),('Thèmes',themes),('Mise à jour de la SD',repair))):
            button=ui.button(tr(label),16+(i%2)*556,30+(i//2)*132,520,94)
            actions[button.id]=action
        ui.label(tr('Ces programmes Python s’exécutent sur la Tab5.'),16,448,1060,44)
        key,value=gui.event()
        if key in actions:
            actions[key]()


def run(app):
    try:
        fs.recover()
        app()
    except KeyboardInterrupt:
        pass
    finally:
        ui.close()
