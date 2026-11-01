"""Production importers, with only OS/HTTP/clock boundaries substituted."""
from pathlib import Path
import contextlib,importlib,io,json,math,sys,tempfile,types,unittest.mock as mock
sketch=Path(sys.argv[1]);sys.path[:0]=[str(sketch/'tools'),str(sketch/'sdcard/MicroPython/lib')]
import time
time.sleep_ms=lambda ms:None;time.ticks_ms=lambda:0;time.ticks_diff=lambda a,b:a-b
for name in ('_tab5','tab5fs','tab5maps','tab5ui','tab5gui'):
    sys.modules[name]=types.ModuleType(name)
sys.modules['_tab5'].call=lambda *args:None
sys.modules['tab5ui'].tr=lambda text:text
import tab5regions as tab
import download_t5map as pc
import tab5_map_policy as policy
checks=0
def check(value,why):
    global checks
    checks+=1
    if not value:raise AssertionError(why)

for provider,native in [('osm',19),('opentopomap',17)]:
    item=tab.source({'provider':provider,'url':'https://local.example/{z}/{x}/{y}.png','requests_per_second':4})
    for low in range(23):
        for high in range(low,23):
            region={'lat':47.7,'lon':-3.4,'radius':0.1,'min_zoom':low,'max_zoom':high}
            actual=tab.native_region(region,item)
            check((actual['min_zoom'],actual['max_zoom'])==policy.native_range(low,high,native),'PC/tablet identical zoom mapping')
    region=tab.native_region({'lat':47.7,'lon':179.999,'radius':0.1,'min_zoom':21,'max_zoom':22},item)
    check(list(tab.tiles(region))==list(pc.geometry.tiles(region)),'same parent tiles including dateline')
    check(tab.estimate(region)==pc.geometry.estimate(region),'same tile counts')
    check(all(z==native and x<1<<z and y<1<<z for z,x,y in tab.tiles(region)),'no requests beyond native maximum')

for v in [-1,0,float('nan'),float('inf'),101]:
    try:policy.request_rate(v)
    except ValueError:check(True,'bad rate rejected')
    else:check(False,'bad rate accepted')
for v in [.1,.5,1,4]:
    source=tab.source({'provider':'opentopomap','requests_per_second':v})
    check(source['delay_ms']>=2000,'public topomap rate floor')
    check(1000/source['delay_ms']==policy.request_rate(v,True,4),'PC/tablet public rate identical')
for url in ['https://tile.openstreetmap.org/{z}/{x}/{y}.png','https://a.tile.openstreetmap.org/{z}/{x}/{y}.png']:
    for call in [lambda:tab.source({'provider':'osm','url':url}),lambda:pc.validate_template(url,'')]:
        try:call()
        except ValueError:check(True,'public OSM offline blocked')
        else:check(False,'public OSM offline accepted')

class Transaction:
    def __enter__(self):return self
    def __exit__(self,*args):pass
    def allocate(self,path):return path+'.part'
    def commit(self):self.committed=True
tab.fs.Transaction=Transaction;tab.maps.MAX_TILE_BYTES=512*1024;tab.maps.valid_tile=lambda path:True
calls=[];clock=[0]
def sleep(ms):clock[0]+=ms
tab.sleep_ms=sleep
def flaky(*args):
    calls.append(clock[0])
    if len(calls)<3:raise OSError(1503)
    return 100
tab.fs.download=flaky
check(tab._download_tile('https://local.example/x','/tile',delay_ms=2000)==100,'retries finish valid transfer')
check(calls==[0,2000,4000],'retry requests respect configured frequency')
calls.clear();tab.fs.download=lambda *args:(_ for _ in ()).throw(OSError(1429))
before=clock[0]
try:tab._download_tile('https://local.example/x','/tile',delay_ms=2000)
except OSError as error:check(error.args==(1429,),'HTTP429 stops without hidden retry')
else:check(False,'HTTP429 ignored')
check(clock[0]==before,'HTTP429 no retry delay')
before=clock[0]
check(not tab._wait(10000,lambda:clock[0]>=before+100),'slow request pause is responsive')
check(clock[0]-before==100,'pause checked in50ms slices')

tab.fs.recover=lambda:None;tab.maps.CACHE='/cache';tab._valid_tile=lambda path:False
saved=[];tab.save=lambda value:saved.append(json.loads(json.dumps(value)))
region={'lat':47.7,'lon':-3.4,'radius':.1,'min_zoom':22,'max_zoom':22}
item=tab.source({'provider':'osm','url':'https://local.example/{z}/{x}/{y}.png'})
job=tab.new_job(region,item,1);old=json.loads(json.dumps(job))
try:tab.run(job,max_tiles=1)
except OSError:pass
else:check(False,'HTTP failure must propagate')
check(job==old and not saved,'failed transfer leaves cursor and checkpoint unchanged')
tab.fs.download=lambda *args:100
check(not tab.run(job,max_tiles=1),'one tile batch can stop')
check(job['done']==1 and len(saved)==1 and job['cursor']!=old['cursor'],'commit advances checkpoint once')
job['region']['max_zoom']=22
try:tab.run(job,max_tiles=1)
except ValueError:check(True,'tampered resume cannot request unavailable zoom')
else:check(False,'tampered resume accepted')

# Configuration persists selected frequency and migrates legacy files.
config={'provider':'opentopomap','urls':{},'requests_per_second':.2}
tab.fs.exists=lambda path:True;tab.fs.read_json=lambda *args:config
check(tab.load_config()['requests_per_second']==.2,'frequency preference loaded')
check(tab.config_source(tab.load_config())['delay_ms']==5000,'saved preference controls transfer')
config['requests_per_second']='broken'
check(tab.load_config()['requests_per_second']==.5,'old/corrupt rate safely defaults')

with tempfile.TemporaryDirectory() as tmp:
    cancel=pc.DownloadCancellation(Path(tmp)/'cancel')
    check(not cancel.is_set(),'PC starts unstopped')
    (Path(tmp)/'cancel').touch()
    check(cancel.wait(10),'PC file pause interrupts limiter waits')
    for provider,native in [('osm',19),('opentopomap',17)]:
        args=['download_t5map.py',str(Path(tmp)/'out.t5map'),'--provider',provider,'--center','47.7,-3.4','--radius-km','0.1','--min-zoom','22','--max-zoom','22','--estimate-only','--requests-per-second','0.2']
        with mock.patch.object(sys,'argv',args),contextlib.redirect_stdout(io.StringIO()) as output:
            code=pc.main()
        check(code==0 and f'source téléchargée : z{native}–{native}' in output.getvalue(),'actual PC CLI accepts display22 and imports parent')

print(f'PASS {checks} production importer checks')

# Execute the actual PC GUI controller, substituting only Tk widgets. Tkinter
# is not installed in the embedded-toolchain Python used by this host runner.
import ast
tree=ast.parse((sketch/'tools/cartes_tab5.py').read_text(encoding='utf-8'))
gui_code=compile(ast.Module(body=[next(n for n in tree.body if isinstance(n,ast.ClassDef) and n.name=='App')],type_ignores=[]),'cartes_tab5.py','exec')
namespace={'Path':Path,'json':json,'engine':pc,'sys':sys,'os':__import__('os'),'tempfile':tempfile,'__file__':str(sketch/'tools/cartes_tab5.py')}
exec(gui_code,namespace)
class Value:
    def __init__(self,value):self.value=value
    def get(self):return self.value
    def set(self,value):self.value=value
class Label:
    def configure(self,**kwargs):self.text=kwargs['text']
with tempfile.TemporaryDirectory() as tmp:
    app=namespace['App'].__new__(namespace['App']);app.process=None
    app.preferences=Path(tmp)/'pc-settings.json';app.selected_provider='OpenTopoMap';app.endpoints={}
    app.values={k:Value(v) for k,v in {'provider':'OpenTopoMap','endpoint':'','rate':'4','name':'zone','output':tmp,'lat':'47.7','lon':'-3.4','radius':'0.1','low':'20','high':'22','max_tiles':'1000','sd':''}.items()}
    app.rate_hint=Label();app.rate_changed()
    check('0.5' in app.rate_hint.text,'PC displays actual Topo rate ceiling')
    app.values['endpoint'].set('http://localhost:8080/{z}/{x}/{y}.png');app.values['rate'].set('1,7');app.rate_changed()
    check('1.7' in app.rate_hint.text and 'Limite' not in app.rate_hint.text,'PC live custom source rate preview')
    commands=[];app.start=lambda command:commands.append(command);app.download(True)
    command=commands.pop();check(command[command.index('--requests-per-second')+1]=='1.7','GUI forwards effective user rate to actual CLI')
    check(json.loads(app.preferences.read_text(encoding='utf-8'))['rate']=='1,7','GUI saves frequency preference')
    app.values['rate'].set('nan');app.rate_changed();check('Saisir' in app.rate_hint.text,'PC invalid rate explains correction')
print(f'PASS {checks} production importer and PC controller checks')
