"""Bounded XYZ region downloads; spherical circle coverage and durable resume."""
import math
import json
from time import sleep_ms, ticks_ms, ticks_diff
from _tab5 import call
import tab5fs as fs
import tab5maps as maps
import tab5ui as ui
import tab5gui as gui
from tab5mappolicy import native_range, request_rate

tr = ui.tr
CONFIG = '/System/map_download.json'
JOB = '/System/maps/download.json'
REQUEST = '/System/maps/region_request.json'
KINDS = ('opentopomap', 'osm')
NAMES = ('OpenTopoMap', 'OpenStreetMap')
CATALOG = '/System/map_catalog.json'
TEMP = '/MicroPython/.map-city.json'
EARTH_KM = 6371.0088
MAX_LAT = 85.0511287798066
RAD = math.pi / 180
_last_search = None


def clean(value, limit):
    value = ''.join(c for c in str(value) if ord(c) >= 32).strip()
    while len(value.encode()) > limit:
        value = value[:-1]
    return value


def quote(value):
    return ''.join(chr(c) if 48 <= c <= 57 or 65 <= c <= 90 or 97 <= c <= 122 or c in (45,46,95,126) else '%%%02X' % c for c in str(value).encode())


def _hash(value):
    result = 2166136261
    for c in value.encode():
        result = ((result ^ c) * 16777619) & 0xffffffff
    return '%08x' % result


def validate(region):
    lat = float(region['lat']); lon = float(region['lon'])
    radius = float(region['radius']); low = int(region['min_zoom']); high = int(region['max_zoom'])
    if not -MAX_LAT <= lat <= MAX_LAT or not -180 <= lon <= 180:
        raise ValueError('Coordonnées hors de la carte mondiale')
    if not 0.1 <= radius <= 5000 or not 0 <= low <= high <= 22:
        raise ValueError('Rayon : 0,1 à 5 000 km ; zooms : 0 à 22')
    return {'lat': lat, 'lon': ((lon+180)%360)-180, 'radius': radius, 'min_zoom': low, 'max_zoom': high, 'name': clean(region.get('name',''), 120)}


def _tile_y(lat, n):
    phi = max(-MAX_LAT, min(MAX_LAT, lat)) * RAD
    return max(0, min(n-1, int((1-math.log(math.tan(math.pi/4+phi/2))/math.pi)*n/2)))


def _row_lat(y, n):
    e = math.exp(math.pi*(1-2*y/n))
    return math.atan((e-1/e)/2)


def rows(region, zoom, first_y=0):
    """Yield y, disjoint inclusive x spans, covering every circle/tile intersection."""
    n = 1 << zoom
    phi = region['lat']*RAD; rho = region['radius']/EARTH_KM
    sp = math.sin(phi); cp = math.cos(phi); cr = math.cos(rho)
    north = min(MAX_LAT*RAD, phi+rho); south = max(-MAX_LAT*RAD, phi-rho)
    top = max(first_y, _tile_y(north/RAD, n)); bottom = _tile_y(south/RAD, n)
    critical = math.asin(max(-1, min(1, sp/cr))) if abs(sp) <= cr else None
    for y in range(top, bottom+1):
        lo = max(south, _row_lat(y+1, n)); hi = min(north, _row_lat(y, n))
        if lo > hi + 1e-12:
            continue
        # At a fixed latitude the boundary satisfies cos(distance)=cos(radius).
        # The extrema occur at the row edges or sin(latitude)=sin(center)/cos(radius).
        ratio = min((cr-sp*math.sin(lo))/(cp*math.cos(lo)), (cr-sp*math.sin(hi))/(cp*math.cos(hi)))
        if critical is not None and lo <= critical <= hi:
            ratio = min(ratio, (cr-sp*math.sin(critical))/(cp*math.cos(critical)))
        if ratio <= -1:
            spans = ((0,n-1),)
        else:
            reach = math.acos(max(-1, min(1, ratio))) / RAD
            left = int(math.floor((region['lon']-reach+180)*n/360))
            right = int(math.floor((region['lon']+reach+180)*n/360))
            if right-left+1 >= n:
                spans = ((0,n-1),)
            else:
                left %= n; right %= n
                spans = ((left,right),) if left <= right else ((0,right),(left,n-1))
        yield y, spans


def estimate(region, progress=None):
    region = validate(region); totals = []; count = 0; steps = 0
    for z in range(region['min_zoom'], region['max_zoom']+1):
        level = 0
        for y, spans in rows(region, z):
            for lo, hi in spans:
                level += hi-lo+1
            steps += 1
            if steps % 128 == 0:
                if progress:
                    progress(z, count+level)
                sleep_ms(1)
        count += level; totals.append([z,level])
        if progress:
            progress(z, count)
    return count, totals


def tiles(region, cursor=None):
    region = validate(region)
    cursor = cursor or [region['min_zoom'],0,0]
    for z in range(max(region['min_zoom'],cursor[0]), region['max_zoom']+1):
        for y, spans in rows(region, z, cursor[1] if z == cursor[0] else 0):
            for lo, hi in spans:
                if z == cursor[0] and y == cursor[1]:
                    lo = max(lo, cursor[2])
                for x in range(lo,hi+1):
                    yield z,x,y


def city_search(query):
    global _last_search
    query = clean(query,160)
    if not query:
        raise ValueError('Saisir une ville')
    cache = '/System/maps/cities-'+_hash(query.lower())+'.json'
    if fs.exists(cache):
        return fs.read_json(cache,65536)
    if _last_search is not None:
        delay = 1100-ticks_diff(ticks_ms(),_last_search)
        if delay > 0:
            sleep_ms(delay)
    _last_search = ticks_ms()
    fs.makedirs('/System/maps')
    if fs.exists(TEMP):
        fs.remove(TEMP)
    try:
        fs.download('https://nominatim.openstreetmap.org/search?format=jsonv2&limit=8&q='+quote(query)+'&accept-language='+quote(ui.language()),TEMP,65536)
        data = fs.read_json(TEMP,65536)
        if not isinstance(data,list):
            raise ValueError('Réponse de recherche invalide')
        result = []
        for item in data[:8]:
            try:
                lat = float(item['lat']); lon = float(item['lon'])
                if -MAX_LAT <= lat <= MAX_LAT and -180 <= lon <= 180:
                    result.append({'name':clean(item.get('display_name',query),240),'lat':lat,'lon':lon})
            except (ValueError,KeyError,TypeError):
                continue
        with fs.Transaction() as tx:
            tx.json(cache,result); tx.commit()
        return result
    finally:
        if fs.exists(TEMP):
            fs.remove(TEMP)


def source(config):
    kind = config.get('provider','opentopomap')
    if kind not in ('osm','opentopomap'):
        raise ValueError('Choisir OpenStreetMap ou OpenTopoMap dans Source des cartes')
    url = config.get('url','').strip()
    if not url and kind == 'opentopomap':
        url = 'https://a.tile.opentopomap.org/{z}/{x}/{y}.png'
    if not url and kind == 'osm':
        raise ValueError('Préparer OpenStreetMap avec Cartes_Tab5 sur le PC, puis copier les paquets sur la SD')
    if not url.startswith('https://') or len(url.encode())>=512 or any(ord(c)<=32 for c in url) or any(v not in url for v in ('{z}','{x}','{y}')):
        raise ValueError('Adresse HTTPS XYZ avec {z}, {x} et {y} requise')
    authority = url.split('/',3)[2]
    host = authority.split(':',1)[0].lower().rstrip('.')
    if not host or '@' in authority:
        raise ValueError('Adresse HTTPS XYZ avec {z}, {x} et {y} requise')
    if host == 'tile.openstreetmap.org' or host.endswith('.tile.openstreetmap.org'):
        raise ValueError('Le serveur public OSM ne permet pas le téléchargement de zones')
    public_topo = host == 'tile.opentopomap.org' or host.endswith('.tile.opentopomap.org')
    rate = request_rate(config.get('requests_per_second',0.5), public_topo, 4.0)
    return {'id':kind,'cache_id':kind,'name':'OpenStreetMap' if kind=='osm' else 'OpenTopoMap', 'url':url,'attribution':'© OpenStreetMap contributors' if kind=='osm' else '© OpenStreetMap · SRTM · OpenTopoMap (CC-BY-SA)','min_zoom':0,'max_zoom':19 if kind=='osm' else 17,'delay_ms':int(math.ceil(1000/rate))}


def installed_sources():
    if not fs.exists(CATALOG):
        return []
    data = fs.read_json(CATALOG,16384)
    if not isinstance(data,dict) or not isinstance(data.get('styles'),list) or len(data['styles'])>6:
        raise ValueError('Catalogue cartographique invalide')
    return [s for s in data['styles'] if isinstance(s,dict) and not str(s.get('id','')).startswith(('ign-','tf-'))]


def install_source(item, replace=None):
    styles = installed_sources()
    for i,s in enumerate(styles):
        if s.get('id') == item['id']:
            replace = i
    if replace is not None:
        styles[replace] = item
    elif len(styles)<6:
        styles.append(item)
    else:
        raise ValueError('Choisir une carte à remplacer')
    with fs.Transaction() as tx:
        tx.json(CATALOG,{'version':1,'styles':styles}); tx.commit()


def save(job):
    with fs.Transaction() as tx:
        tx.json(JOB,job); tx.commit()


def load():
    if not fs.exists(JOB):
        return None
    job = fs.read_json(JOB,16384)
    if job.get('version') != 1 or not isinstance(job.get('source'),dict):
        raise ValueError('Téléchargement de cartes invalide')
    job['region'] = validate(job['region'])
    maps.identifier(job['source']['cache_id'])
    if job['source']['cache_id'] not in ('osm','opentopomap'):
        raise ValueError('Choisir OpenStreetMap ou OpenTopoMap dans Source des cartes')
    cursor = job.get('cursor')
    if not isinstance(cursor,list) or len(cursor)!=3 or any(not isinstance(v,int) or v<0 for v in cursor) or cursor[0]>23 or cursor[1]>(1<<22) or cursor[2]>(1<<22):
        raise ValueError('Reprise de téléchargement invalide')
    return job


def native_region(region,item):
    region = validate(region)
    region['min_zoom'],region['max_zoom'] = native_range(region['min_zoom'],region['max_zoom'],item['max_zoom'])
    return region


def new_job(region,item,count):
    region = native_region(region,item)
    return {'version':1,'region':region,'source':item,'cursor':[region['min_zoom'],0,0],'total':count,'done':0,'downloaded':0,'reused':0,'bytes':0,'complete':False}


def _valid_tile(path):
    return maps.valid_tile(path)


def _retryable_download(error):
    code=error.args[0] if error.args else None
    # The HTTPS service maps HTTP status to 1000+status and transport to 5004.
    # EIO here can mean a short transfer. Validation/commit SD errors bypass this.
    return code in (5,104,110,5002,5003,5004,1408) or (isinstance(code,int) and 1500<=code<=1599)


def _wait(delay_ms, cancelled=None):
    # Keep Pause responsive between requests/backoffs, even at 0.1 request/s.
    while delay_ms > 0:
        if cancelled and cancelled():
            return False
        step = min(50,delay_ms); sleep_ms(step); delay_ms -= step
    return not (cancelled and cancelled())


def _download_tile(url, path, cancelled=None, delay_ms=250):
    for attempt in range(4):
        if cancelled and cancelled():
            return None
        with fs.Transaction() as tx:
            staged=tx.allocate(path)
            retry=False
            try:
                size=fs.download(url,staged,maps.MAX_TILE_BYTES)
            except OSError as error:
                if not _retryable_download(error) or attempt==3:
                    raise
                retry=True
            if not retry:
                # Resource/SD failures are not evidence of corrupt image data.
                # They propagate immediately, including ENOMEM (no OOM retry).
                if maps.valid_tile(staged):
                    tx.commit()
                    return size
                if attempt==3:
                    raise ValueError('Tuile PNG/JPEG invalide ou incomplète')
        # Failed staging has been rolled back before another transfer starts.
        if cancelled and cancelled():
            return None
        if not _wait(max(delay_ms,(400,1200,2500)[attempt]),cancelled):
            return None


def run(job, progress=None, cancelled=None, max_tiles=None):
    """One tile and one small checkpoint at a time; no queue proportional to area."""
    fs.recover()
    item = job['source']; batch = 0
    # Revalidate persisted jobs too; their edited rate cannot bypass a public limit.
    safe = source({'provider':item['cache_id'],'url':item['url'],
                   'requests_per_second':min(4.0,max(0.1,1000/max(1,int(item.get('delay_ms',2000)))))})
    if job['region']['min_zoom'] < safe['min_zoom'] or job['region']['max_zoom'] > safe['max_zoom']:
        raise ValueError('La source ne fournit pas tous les zooms choisis')
    delay_ms = safe['delay_ms']
    if job.get('complete'):
        return True
    for z,x,y in tiles(job['region'],job['cursor']):
        if cancelled and cancelled():
            return False
        path = maps.CACHE+'/'+item['cache_id']+'/'+str(z)+'/'+str(x)+'/'+str(y)+'.png'
        if _valid_tile(path):
            reused = True; size = fs.stat(path)[1]
        else:
            url = item['url'].replace('{z}',str(z)).replace('{x}',str(x)).replace('{y}',str(y))
            # Failed transfer/decode never advances the checkpoint or replaces old data.
            size = _download_tile(url,path,cancelled,delay_ms)
            if size is None:
                return False
            reused = False
        job['cursor'] = [z,y,x+1]
        job['done'] += 1
        job['reused' if reused else 'downloaded'] += 1
        job['bytes'] += size
        save(job)
        if progress:
            progress(job,z)
        batch += 1
        if max_tiles is not None and batch>=max_tiles:
            return False
        if not _wait(1 if reused else delay_ms,cancelled):
            return False
    job['complete'] = True; save(job)
    return True


def size_text(size):
    return ('%.1f GiB' % (size/(1024*1024*1024))) if size>=1024*1024*1024 else ('%.1f MiB' % (size/(1024*1024)))


def duration_text(seconds):
    minutes = max(1, int((seconds+59)//60))
    return ('%d h %02d min' % (minutes//60,minutes%60)) if minutes>=60 else ('%d min' % minutes)


def free_space():
    try:
        return int(json.loads(call(85,0,0,0,None,256).decode())['free'])
    except (OSError,KeyError,ValueError):
        return None


def load_config():
    config = fs.read_json(CONFIG,8192) if fs.exists(CONFIG) else {}
    if not isinstance(config,dict):
        raise ValueError('Configuration cartographique invalide')
    kind = config.get('provider','opentopomap')
    if kind not in KINDS:
        kind = 'opentopomap'
    urls = config.get('urls',{})
    if not isinstance(urls,dict):
        urls = {}
    urls = {k:str(urls.get(k,'')) for k in KINDS}
    if config.get('provider') in KINDS and 'url' in config:
        urls[kind] = str(config.get('url') or '')
    for k in KINDS:
        url = urls[k]
        if url.startswith('https://'):
            host = url.split('/',3)[2].split(':',1)[0].lower().rstrip('.')
            if host=='tile.openstreetmap.org' or host.endswith('.tile.openstreetmap.org'):
                urls[k]=''
    rate = config.get('requests_per_second',0.5)
    try:
        rate = request_rate(rate,False,4.0)
    except (ValueError,TypeError):
        rate = 0.5
    return {'provider':kind,'urls':urls,'requests_per_second':rate}


def save_config(config):
    with fs.Transaction() as tx:
        tx.json(CONFIG,config); tx.commit()


def config_source(config):
    kind = config['provider']
    return source({'provider':kind,'url':config['urls'].get(kind,''),'requests_per_second':config.get('requests_per_second',0.5)})


def load_request():
    if not fs.exists(REQUEST):
        return None
    request = fs.read_json(REQUEST,2048)
    fs.remove(REQUEST)
    if not isinstance(request,dict) or request.get('version') != 1:
        raise ValueError('Demande de zone invalide')
    region = validate({'lat':request['lat'],'lon':request['lon'],'radius':100,
                       'min_zoom':0,'max_zoom':max(14,min(22,int(request['zoom'])))})
    if request.get('provider') not in KINDS:
        raise ValueError('Demande de zone invalide')
    url = str(request.get('url',''))
    if len(url.encode()) >= 512:
        raise ValueError('Demande de zone invalide')
    return {'region':region,'provider':request['provider'],'url':url}


def configure(config=None):
    config = config or load_config()
    selected = KINDS.index(config['provider'])
    while True:
        back = gui.start('Source des cartes hors ligne')
        choose = ui.dropdown(list(NAMES),16,70,1064,48,selected)
        ui.label(tr('URL HTTPS personnelle (facultatif)'),16,140,1064,38)
        address = ui.entry(config['urls'].get(KINDS[selected],''),16,186,1064,48)
        if KINDS[selected] == 'osm':
            ui.label(tr('OpenStreetMap : renseigner un serveur autorisant les zones hors ligne, ou préparer la zone sur PC. Le serveur public standard sert à la consultation.'),16,252,1064,120)
        else:
            ui.label(tr('OpenTopoMap fonctionne directement. Une tuile toutes les deux secondes au maximum ; les grandes zones peuvent prendre plusieurs jours.'),16,252,1064,120)
        ui.label(tr('Requêtes par seconde (0,1 à 4)'),16,366,700,38)
        rate = ui.entry(str(config.get('requests_per_second',0.5)),750,360,330,48)
        done = ui.button(tr('Enregistrer'),750,436,330,58)
        k,v = gui.event()
        config['urls'][KINDS[selected]] = address.text().strip()
        requested_rate = rate.text().strip().replace(',','.')
        if k==back.id:
            return None
        if k==choose.id:
            selected=max(0,min(1,v)); config['provider']=KINDS[selected]; continue
        if k==done.id:
            config['provider']=KINDS[selected]
            try:
                config['requests_per_second'] = request_rate(requested_rate,False,4.0)
                if KINDS[selected]!='osm' or config['urls']['osm']:
                    config_source(config)
                save_config(config)
                return config
            except (OSError,ValueError,TypeError) as error:
                gui.failure(error)


def pc_help():
    gui.message('Cartes sur PC',tr('Ouvrir tools/Cartes_Tab5.cmd sur le PC. Choisir OpenStreetMap ou OpenTopoMap, une ville, un rayon et les zooms. Copier les fichiers .t5map dans System/maps/offline sur la SD.'))


def download_screen(job):
    gui.start('Téléchargement de la zone',False)
    ui.label(job['region']['name'] or ('%.4f, %.4f' % (job['region']['lat'],job['region']['lon'])),16,22,1064,70)
    status = ui.label('',16,110,1064,120)
    bar = ui.slider(16,250,1064,28,100,0)
    pause = ui.button(tr('Mettre en pause'),330,358,440,64)
    def stopped():
        if not ui.is_open():
            return True
        event = ui.poll()
        return bool(event and event[0]==pause.id)
    def progress(state,z):
        status.text(str(state['done'])+' / '+str(state['total'])+' '+tr('tuiles')+' · z'+str(z)+'\n'+size_text(state['bytes'])+' · '+str(state['reused'])+' '+tr('déjà présentes'))
        bar.value(min(100,int(state['done']*100/max(1,state['total']))))
    progress(job,job['cursor'][0])
    try:
        finished = run(job,progress,stopped)
        gui.message('Cartes hors ligne',tr('Zone téléchargée. Ouvrir Cartes et choisir cette source.') if finished else tr('Téléchargement en pause. Les tuiles terminées sont conservées.'))
    except (OSError,ValueError,KeyError,TypeError,MemoryError) as error:
        gui.failure(error)


def new_region(request=None):
    config = load_config()
    region = {'name':'','lat':0,'lon':0,'radius':100,'min_zoom':0,'max_zoom':14}
    if request:
        region.update(request['region'])
        config['provider'] = request['provider']
        # Preserve a personal OSM endpoint when the native view uses the public one.
        url = request['url']
        if '://' in url:
            host = url.split('/',3)[2].split(':',1)[0].lower().rstrip('.')
            if not (host=='tile.openstreetmap.org' or host.endswith('.tile.openstreetmap.org')):
                config['urls'][request['provider']] = url
    query = ''
    while True:
        selected = KINDS.index(config['provider'])
        back = gui.start('Télécharger une zone')
        choose = ui.dropdown(list(NAMES),190,0,560,48,selected)
        settings = ui.button(tr('Source'),780,0,300,48)
        city = ui.entry(query,16,68,700,48)
        find = ui.button(tr('Chercher une ville'),750,68,330,48)
        ui.label(tr('Latitude / longitude'),16,138,480,34)
        lat = ui.entry(str(region['lat']),570,134,240,48)
        lon = ui.entry(str(region['lon']),840,134,240,48)
        ui.label(tr('Rayon autour du centre (km)'),16,206,690,40)
        radius = ui.entry(str(region['radius']),750,202,330,48)
        ui.label(tr('Zoom minimum / maximum'),16,274,480,40)
        low = ui.entry(str(region['min_zoom']),570,270,240,48)
        high = ui.entry(str(region['max_zoom']),840,270,240,48)
        ui.label(tr('© OpenStreetMap contributors · Recherche Nominatim'),16,334,1064,34)
        prepare = ui.button(tr('Estimer le téléchargement'),570,414,510,58)
        key,value = gui.event()
        query = city.text().strip()
        if key==back.id:
            return
        try:
            region.update(lat=lat.text(),lon=lon.text(),radius=radius.text(),min_zoom=low.text(),max_zoom=high.text())
            if key==choose.id:
                config['provider'] = KINDS[max(0,min(1,value))]
            elif key==settings.id:
                changed = configure(config)
                if changed:
                    config = changed
            elif key==find.id:
                gui.progress('Recherche de ville')
                result = city_search(query)
                if not result:
                    gui.message('Rechercher',tr('Aucune ville trouvée'))
                else:
                    chosen = gui.choose('Choisir le centre',result,[r['name'] for r in result])
                    if chosen:
                        region.update(chosen);query=chosen['name'][:100]
            elif key==prepare.id:
                if config['provider']=='osm' and not config['urls']['osm'].strip():
                    action = gui.choose('Source OpenStreetMap', ['source','topo','pc'],
                        [tr('Configurer une source OpenStreetMap'),tr('Choisir OpenTopoMap sur la tablette'),tr('Préparer OpenStreetMap sur PC')])
                    if action=='source':
                        changed=configure(config)
                        if changed:
                            config=changed
                    elif action=='topo':
                        config['provider']='opentopomap'
                    elif action=='pc':
                        pc_help()
                    continue
                item = config_source(config)
                region = validate(region)
                native = native_region(region,item)
                p = gui.progress('Calcul de la zone')
                def estimating(z,count):
                    p.text('z'+str(z)+' · '+str(count)+' '+tr('tuiles'))
                    if not ui.is_open():
                        raise KeyboardInterrupt
                count,levels = estimate(native,estimating)
                free = free_space()
                text = item['name']+' · %.1f km · z%d–%d\n%d ' % (region['radius'],region['min_zoom'],region['max_zoom'],count)+tr('tuiles')+'\n'+tr('Estimation à 32 KiB/tuile : ')+size_text(count*32768)
                text += '\n'+tr('Zoom des tuiles téléchargées')+' : z%d–%d' % (native['min_zoom'],native['max_zoom'])
                text += '\n'+tr('Requêtes par seconde')+' : '+str(1000/item['delay_ms'])
                text += '\n'+tr('Taille réelle variable ; maximum : ')+size_text(count*maps.MAX_TILE_BYTES)
                text += '\n'+tr('Temps minimal hors cache : ')+duration_text(count*max(250,int(item.get('delay_ms',2000)))/1000)
                if free is not None:
                    text += '\n'+tr('SD libre : ')+size_text(free)
                if gui.confirm('Télécharger cette zone',text):
                    styles = installed_sources(); replace = None
                    if len(styles)>=6 and not any(s.get('id')==item['id'] for s in styles):
                        replace = gui.choose('Choisir une carte à remplacer',list(range(len(styles))),[s['name'] for s in styles])
                        if replace is None:
                            continue
                    install_source(item,replace)
                    save_config(config)
                    job = new_job(region,item,count);save(job);download_screen(job)
                    return
        except (OSError,ValueError,KeyError,TypeError,MemoryError) as error:
            gui.failure(error)


def main(manual):
    fs.recover()
    try:
        request = load_request()
        if request:
            new_region(request)
    except (OSError,ValueError,KeyError,TypeError,MemoryError) as error:
        gui.failure(error)
    while True:
        if fs.exists(REQUEST):
            try:
                request = load_request()
                if request:
                    new_region(request)
            except (OSError,ValueError,KeyError,TypeError,MemoryError) as error:
                gui.failure(error)
        back = gui.start('Cartes hors ligne')
        fresh = ui.button(tr('Télécharger une zone'),16,80,1064,66)
        resume = ui.button(tr('Reprendre le téléchargement'),16,170,1064,66)
        settings = ui.button(tr('Source des cartes'),16,260,510,66)
        local = ui.button(tr('Import manuel'),570,260,510,66)
        pc = ui.button(tr('Préparer sur PC'),16,350,1064,66)
        key,value = gui.event()
        if key==back.id:
            return
        try:
            if key==fresh.id:
                new_region()
            elif key==resume.id:
                job = load()
                if job:
                    download_screen(job)
                else:
                    gui.message('Cartes hors ligne',tr('Aucun téléchargement à reprendre'))
            elif key==settings.id:
                configure()
            elif key==local.id:
                manual()
            elif key==pc.id:
                pc_help()
        except (OSError,ValueError,KeyError,TypeError,MemoryError) as error:
            gui.failure(error)
