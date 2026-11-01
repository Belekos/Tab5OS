"""Exercise production map tile validation/raster with the shared PNG decoder.

Generated 256px fixtures cover every legal PNG colour/depth pair, all five row
filters, Adam7 and transparency. No HTTP, provider, SD cache or device is used.
"""
from pathlib import Path
import argparse,datetime,hashlib,json,os,struct,subprocess,zlib

ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--m5gfx',type=Path,required=True)
ap.add_argument('--build',type=Path,required=True)
a=ap.parse_args();here=Path(__file__).resolve().parent;release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.35_Ecriture';work=a.build.resolve();work.mkdir(parents=True,exist_ok=True)
fixtures=work/'fixtures';fixtures.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def chunk(tag,data):return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
def paeth(a,b,c):
 p=a+b-c;pa=abs(p-a);pb=abs(p-b);pc=abs(p-c)
 return a if pa<=pb and pa<=pc else b if pb<=pc else c
def filter_row(row,prev,bpp,kind):
 out=bytearray()
 for i,v in enumerate(row):
  left=row[i-bpp] if i>=bpp else 0;above=prev[i] if prev else 0;corner=prev[i-bpp] if prev and i>=bpp else 0
  predictor=(0,left,above,(left+above)//2,paeth(left,above,corner))[kind]
  out.append((v-predictor)&255)
 return bytes([kind])+out
specs=[(0,d) for d in (1,2,4,8,16)]+[(3,d) for d in (1,2,4,8)]+[(c,d) for c in (2,4,6) for d in (8,16)]
manifest={};names=[]
for colour,depth in specs:
 channels={0:1,2:3,3:1,4:2,6:4}[colour];bpp=max(1,(channels*depth+7)//8);levels=(1<<depth)-1
 palette=[((i*53)&255,(i*97)&255,(i*149)&255,(i*71)&255) for i in range(1<<min(depth,8))]
 def pixel(x,y):
  v=(x*13+y*29)&255;r=(x*17+y*3)&255;g=(x*5+y*19)&255;b=(x*7+y*11)&255;alpha=(x*23+y*31)&255
  if colour==3:
   index=(x*3+y*5)&levels;return [index],palette[index]
  if colour==0:
   value=v if depth>=8 else (x+y)&levels;grey=value if depth>=8 else value*255//levels;return [value],(grey,grey,grey,255)
  if colour==2:return [r,g,b],(r,g,b,255)
  if colour==4:return [v,alpha],(v,v,v,alpha)
  return [r,g,b,alpha],(r,g,b,alpha)
 expected=bytearray()
 for y in range(256):
  for x in range(256):
   _,rgba=pixel(x,y);r,g,b,alpha=rgba;r,g,b=[(v*alpha+255*(255-alpha)+127)//255 for v in (r,g,b)]
   expected+=struct.pack('<H',(r&248)<<8|(g&252)<<3|b>>3)
 for interlace in (0,1):
  passes=[(0,0,1,1)] if not interlace else [(0,0,8,8),(4,0,8,8),(0,4,4,8),(2,0,4,4),(0,2,2,4),(1,0,2,2),(0,1,1,2)]
  scan=bytearray()
  for px,py,dx,dy in passes:
   previous=b''
   for row_index,y in enumerate(range(py,256,dy)):
    samples=[v for x in range(px,256,dx) for v in pixel(x,y)[0]]
    if depth==16:row=b''.join(struct.pack('>H',v*257) for v in samples)
    elif depth==8:row=bytes(samples)
    else:
     row=bytearray((len(samples)*depth+7)//8)
     for i,v in enumerate(samples):row[i*depth//8]|=v<<(8-depth-i*depth%8)
     row=bytes(row)
    scan+=filter_row(row,previous,bpp,row_index%5);previous=row
  data=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',256,256,depth,colour,0,0,interlace))
  if colour==3:data+=chunk(b'PLTE',bytes(v for p in palette for v in p[:3]))+chunk(b'tRNS',bytes(p[3] for p in palette))
  compressed=zlib.compress(scan)
  data+=b''.join(chunk(b'IDAT',compressed[i:i+257]) for i in range(0,len(compressed),257))+chunk(b'IEND',b'')
  name=f'tile-c{colour}-d{depth}-i{interlace}';names.append(name)
  for ext,content in [('.png',data),('.rgb565',expected)]:
   p=fixtures/(name+ext);p.write_bytes(content);manifest[p.name]=sha(p)
(fixtures/'names.txt').write_text('\n'.join(names)+'\n')
miniz_source=a.m5gfx.resolve()/'src/lgfx/utility/lgfx_miniz.c'
miniz=work/'lgfx_miniz_portable.c';original=miniz_source.read_bytes();old=b'\n#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 1\n';new=b'\n#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0\n'
assert original.count(old)==1;miniz.write_bytes(original.replace(old,new))
production=[sketch/x for x in ['Tab5TileValidation.cpp','Tab5TileValidation.h','Tab5TileRaster.cpp','Tab5TileRaster.h','Tab5TilePolicy.h','Tab5Cooperate.h','src/tab5png/tab5_pngle.c','src/tab5png/tab5_pngle.h']]
inputs=[*production,*[p for p in here.iterdir() if p.is_file()]]
hashes={p.relative_to(release).as_posix():sha(p) for p in inputs}
flags=['-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-I'+str(sketch),'-I'+str(a.m5gfx.resolve()/'src')]
objects=[];commands=[]
for i,p in enumerate([sketch/'src/tab5png/tab5_pngle.c',miniz,a.m5gfx.resolve()/'src/lgfx/utility/lgfx_tjpgd.c']):
 target=work/f'{i}.o';command=['gcc',*flags,'-DTAB5PNG_TEST_ALLOC','-c',str(p),'-o',str(target)];subprocess.run(command,check=True);commands.append(command);objects.append(target)
exe=work/'map-png';command=['g++',*flags,'-no-pie','-DTAB5_TILE_TEST_ALLOC','-std=c++17',str(here/'test.cpp'),str(sketch/'Tab5TileValidation.cpp'),str(sketch/'Tab5TileRaster.cpp'),*map(str,objects),'-o',str(exe)];subprocess.run(command,check=True);commands.append(command)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
run=subprocess.run([str(exe),str(fixtures)],capture_output=True,text=True,env=env,timeout=120)
(work/'test.log').write_text(run.stdout+run.stderr)
print(run.stdout+run.stderr)
if run.returncode:raise SystemExit(run.returncode)
assert hashes=={p.relative_to(release).as_posix():sha(p) for p in inputs},'Sources changed during map PNG tests'
result=json.loads(run.stdout.strip().splitlines()[-1]);report={'version':'0.8.35','passed':True,'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'sourceHashes':hashes,'result':result,'sanitizers':['address','undefined'],'fixturesSha256':manifest,'decoderSourceSha256':sha(a.m5gfx.resolve()/'src/lgfx/utility/lgfx_tjpgd.c'),'minizHostPortablePath':{'sourceSha256':sha(miniz_source),'compiledSha256':sha(miniz),'reason':'Execute portable loads used on RISC-V; x86-only unaligned casts disabled solely in isolated host copy.'},'commands':commands,'physicalDeviceTested':False,'method':__doc__,'limitations':['Production decoder/default allocator, tile policy, validation and raster exercised.','No provider HTTP, SD cache, image lock contention, ESP32 heap capabilities or display hardware exercised.','LeakSanitizer cannot inspect /proc in this host; all default PNG allocations are independently tracked and required to return to zero after each case.']}
proof=release/'tools/verification/map-png-0835.json';proof.write_text(json.dumps(report,indent=2)+'\n');print(proof)
