#!/usr/bin/env python3
"""Execute the actual image_actions function and ZIM image branch against temporary files."""
from pathlib import Path
import ast,hashlib,json,shutil,sys,tempfile,types
r=Path(__file__).resolve().parents[2];source=r/'Tab5OS_Arduino_v0.8.50_Ecriture/sdcard/MicroPython/lib/tab5wikiui.py'
code=source.read_text();tree=ast.parse(code);source_hash=hashlib.sha256(source.read_bytes()).hexdigest();checks=0

def check(ok,msg):
 global checks
 checks+=1
 assert ok,msg

class FS:
 def __init__(self,root):self.root=root;self.downloads=[];self.created=[]
 def p(self,path):return self.root/path.lstrip('/')
 def exists(self,path):return self.p(path).exists()
 def makedirs(self,path):self.p(path).mkdir(parents=True,exist_ok=True);self.created.append(path)
 def read_json(self,path,limit):return json.loads(self.p(path).read_text())
 def download(self,url,path,limit,progress):self.downloads.append((url,path));self.p(path).write_bytes(b'image bytes')
 def rename(self,src,dst):self.p(src).rename(self.p(dst))
 def remove(self,path):self.p(path).unlink()
 def copy(self,src,dst):shutil.copyfile(self.p(src),self.p(dst))
class GUI:
 def __init__(self):self.choices=iter(['save',None]);self.messages=[]
 def choose(self,*args):return next(self.choices)
 def message(self,*args):self.messages.append(args)
images=types.ModuleType('tab5wikiimages');images.original=lambda x:x;images.source=lambda x:x
markup=types.ModuleType('tab5wikimarkup');markup.unquote=lambda x:x
sys.modules['tab5wikiimages']=images;sys.modules['tab5wikimarkup']=markup
with tempfile.TemporaryDirectory(prefix='tab5-wiki-') as temp:
 fs=FS(Path(temp));gui=GUI();fs.makedirs('/Pictures/Wikipedia');fs.p('/Pictures/Wikipedia/old.jpg').write_bytes(b'old data');fs.makedirs('/Wiki')
 fs.p('/Wiki/item.images.json').write_text(json.dumps([{'index':1,'url':'https://example.test/image.jpg','source':'https://example.test/image.jpg'}]))
 namespace={'fs':fs,'gui':gui,'wiki':types.SimpleNamespace(record_paths=lambda x:('/Wiki/item',)), 'tr':lambda x:x,'progress':lambda x:None,'clip':lambda x:x,'failure':lambda x:(_ for _ in ()).throw(x),'show_image':lambda x:None}
 function=next(x for x in tree.body if isinstance(x,ast.FunctionDef) and x.name=='image_actions')
 exec(compile(ast.Module(body=[function],type_ignores=[]),str(source),'exec'),namespace)
 namespace['image_actions']({},1)
 check(fs.p('/Picture/Wikipedia/image.jpg').read_bytes()==b'image bytes','download uses Picture')
 check(fs.p('/Pictures/Wikipedia/old.jpg').read_bytes()==b'old data','old Pictures data preserved')
 check(len(fs.downloads)==1,'one download published')
 check(not fs.exists('/Picture/Wikipedia/image.jpg.download'),'temporary download renamed')
 gui.choices=iter(['save',None]);namespace['image_actions']({},1);check(len(fs.downloads)==1,'existing canonical image reused')
 # Execute unchanged ZIM image branch in one-iteration loop (its continue belongs to reader loop).
 branch=next(x for x in ast.walk(tree) if isinstance(x,ast.If) and ast.unparse(x.test)=="target.startswith('@image:')")
 loop=ast.For(target=ast.Name(id='_once',ctx=ast.Store()),iter=ast.Tuple(elts=[ast.Constant(value=1)],ctx=ast.Load()),body=[branch],orelse=[])
 module=ast.fix_missing_locations(ast.Module(body=[loop],type_ignores=[]));zim=compile(module,str(source),'exec');shown=[]
 def open_file(path,*args,**kwargs):return open(fs.p(path),*args,**kwargs)
 namespace.update({'open':open_file,'show_image':shown.append,'current':{'kind':'zim','id':123},'article':{'path':'/Wiki/zim'},'target':'@image:4'})
 for signature,ext in [(b'\x89PNG\r\n\x1a\nxxx','.png'),(b'RIFF12345678','.webp'),(b'\xff\xd8\xffxxxxxxxxx','.jpg')]:
  fs.p('/Wiki/zim.img4').write_bytes(signature);exec(zim,namespace);path='/Picture/Wikipedia/zim-123-4'+ext
  check(shown[-1]==path,'ZIM viewer receives canonical image path');check(fs.p(path).read_bytes()==signature,'ZIM bytes preserved')
 check(fs.p('/Pictures/Wikipedia/old.jpg').read_bytes()==b'old data','old path still preserved')
assert source_hash==hashlib.sha256(source.read_bytes()).hexdigest()
report={'version':'0.8.50','passed':True,'checks':checks,'physicalDeviceTested':False,'method':__doc__,'sourceHashes':{str(source.relative_to(r)):source_hash},'testSourceHashes':{str(Path(__file__).resolve().relative_to(r)):hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},'limits':['MicroPython functions executed in CPython with filesystem/network/UI boundaries controlled; no Wi-Fi or SD latency test.']}
(r/'tools/verification/wiki-picture-paths-0850.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
