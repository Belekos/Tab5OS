#!/usr/bin/env python3
"""Exercise original-resolution JPEG/PNG/WebP/BMP production codecs with allocator and IO fault injection."""
from pathlib import Path
import argparse, hashlib, importlib.util, json, os, random, subprocess, struct, zlib, concurrent.futures
from PIL import Image

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--m5gfx',required=True,type=Path)
p.add_argument('--build',required=True,type=Path)
a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.35_Ecriture';work=a.build.resolve();work.mkdir(parents=True,exist_ok=True)
helper=release/'tools/build_linux/prepare_m5gfx_jpeg.py'
spec=importlib.util.spec_from_file_location('jpeg_patch',helper);patch=importlib.util.module_from_spec(spec);spec.loader.exec_module(patch)
m5gfx=patch.prepare_m5gfx_jpeg(a.m5gfx,work)
fixtures=work/'fixtures';fixtures.mkdir(exist_ok=True)
gradient=Image.new('RGB',(127,95));gradient.putdata([((x*2)%256,(y*3)%256,((x+y)*5)%256)for y in range(95)for x in range(127)])
gradient.save(fixtures/'gradient.jpg',quality=94)
gradient.save(fixtures/'progressive.jpg',quality=94,progressive=True)
gradient.save(fixtures/'app.jpg',quality=94,exif=b'Exif\x00\x00'+bytes(range(256))*16)
rng=random.Random(834)
Image.frombytes('RGB',(129,131),bytes(rng.randrange(256)for _ in range(129*131*3))).save(fixtures/'noise.jpg',quality=95)
Image.new('L',(71,59),133).save(fixtures/'grey.jpg')
checker=Image.new('RGB',(127,127));checker.putdata([(255,0,30)if((x//3+y//3)&1)else(0,230,255)for y in range(127)for x in range(127)]);checker.save(fixtures/'low.jpg',quality=5)
for name,size in [('half',(1600,1200)),('fullhd',(1920,1080)),('quarter',(4001,3001)),('eighth',(6000,4000)),('overside',(8193,1))]:Image.new('RGB',size,(37,160,219)).save(fixtures/(name+'.jpg'),quality=95)
Image.new('L',(8192,16),107).save(fixtures/'thin.jpg')
(fixtures/'not.jpg').write_bytes(b'This is not a JPEG.')

alpha=Image.new('RGBA',(129,97));alpha.putdata([((x%4)*61,(y%4)*61,113,64+((x+y)%4)*63)for y in range(97)for x in range(129)])
alpha.save(fixtures/'alpha.png');(fixtures/'alpha.rgba').write_bytes(alpha.tobytes())
alpha.quantize(colors=64,method=Image.Quantize.FASTOCTREE).save(fixtures/'palette.png')
alpha.save(fixtures/'alpha.webp',lossless=True,exact=True);alpha.save(fixtures/'lossy.webp',quality=90)
alpha.save(fixtures/'animated.webp',save_all=True,append_images=[Image.new('RGBA',alpha.size,(250,10,70,255))],duration=[60,60],loop=0,lossless=True)
for ext in ['png','webp','bmp']:
 Image.new('RGB',(1600,1200),(37,160,219)).save(fixtures/('large.'+ext))
 if ext!='webp':Image.new('RGB',(8192,16),(37,160,219)).save(fixtures/('thin.'+ext))
 if ext!='bmp':Image.new('RGB',(3000,2000),(37,160,219)).save(fixtures/('too_large.'+ext))
Image.new('1',alpha.size,1).save(fixtures/'mono.png')
Image.new('I;16',alpha.size,32896).save(fixtures/'grey16.png')
# Encode Adam7 directly; Pillow's PNG saver does not write interlace.
def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
payload=bytearray()
for x0,y0,dx,dy in [(0,0,8,8),(4,0,8,8),(0,4,4,8),(2,0,4,4),(0,2,2,4),(1,0,2,2),(0,1,1,2)]:
 for y in range(y0,alpha.height,dy):
  payload.append(0)
  for x in range(x0,alpha.width,dx):payload.extend(alpha.getpixel((x,y)))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',alpha.width,alpha.height,8,6,0,0,1))+chunk(b'IDAT',zlib.compress(payload))+chunk(b'IEND',b'')
(fixtures/'interlace.png').write_bytes(png)
assert Image.open(fixtures/'interlace.png').tobytes()==alpha.tobytes()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
inputs=[helper,*[sketch/x for x in ['Tab5ImageCodec.cpp','Tab5ImageCodec.h','Tab5ImageWrite.h','Tab5WebPPort.cpp','Tab5WebPPort.h','Tab5ImageDecode.cpp','Tab5ImageDecode.h','src/tab5png/tab5_pngle.c','src/tab5png/tab5_pngle.h','src/jpeg/jpge.cpp','src/jpeg/jpge.h','Tab5ImageCore.c','tools/native-apps/image-editor/image_core.c','tools/native-apps/image-editor/image_core.h']],*list((sketch/'src/tab5webp').rglob('*.*')),*[x for x in here.iterdir()if x.is_file()]]
before={str(x.relative_to(release)):sha(x)for x in inputs}
flags=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror']
includes=['-I'+str(sketch),'-I'+str(m5gfx/'src')]
def run(cmd,ok=True):
 r=subprocess.run([str(x)for x in cmd],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
 if ok and r.returncode: print(r.stdout+r.stderr);raise SystemExit(r.returncode)
 return r
run(['cc','-std=c11',*flags,*includes,'-c',sketch/'Tab5ImageCore.c','-o',work/'core.o'])
run(['cc','-std=c11',*flags,*includes,'-c',m5gfx/patch.DECODER,'-o',work/'decoder.o'])

# Upstream enables integer unaligned accesses on x86 only. Match the firmware's
# RISC-V portable byte-access path while retaining every UBSan check.
miniz_source=m5gfx/'src/lgfx/utility/lgfx_miniz.c'
miniz_portable=work/'miniz_portable.c'
miniz_bytes=miniz_source.read_bytes();needle=b'\n#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 1\n'
assert miniz_bytes.count(needle)==1
miniz_portable.write_bytes(miniz_bytes.replace(needle,b'\n#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0\n'))
more_sources=[sketch/'src/tab5png/tab5_pngle.c',miniz_portable,*sorted((sketch/'src/tab5webp').rglob('*.c'))]
more_objects=[work/(str(i)+'_'+source.stem+'.o')for i,source in enumerate(more_sources)]
def compile_extra(pair):
 source,obj=pair
 # Vendored C libraries retain their baseline warning policy; sanitizers remain
 # enabled for every translation unit.
 run(['cc','-std=c11',*[f for f in flags if f!='-Werror'],*includes,'-c',source,'-o',obj])
with concurrent.futures.ThreadPoolExecutor(max_workers=4)as pool:list(pool.map(compile_extra,zip(more_sources,more_objects)))
extra_cpp=[sketch/'Tab5WebPPort.cpp',sketch/'Tab5ImageDecode.cpp']
common=[here/'codec.cpp',sketch/'Tab5ImageCodec.cpp',sketch/'src/jpeg/jpge.cpp',*extra_cpp,work/'core.o',*more_objects]
run(['c++','-std=c++17',*flags,'-Wno-misleading-indentation','-Wno-missing-field-initializers',*includes,*common,work/'decoder.o','-o',work/'codec'])
result=run([work/'codec',fixtures]);print(result.stdout,end='')
proof=release/'tools/verification/image-codec-evidence-0835';proof.mkdir(parents=True,exist_ok=True);(proof/'run.log').write_text(result.stdout+result.stderr)
# Reproduce each independent upstream defect without weakening sanitizer flags.
regressions={}
for name,decoder in [('baseline',(a.m5gfx/patch.DECODER).read_bytes()),('overlap',patch.patch_decoder((a.m5gfx/patch.DECODER).read_bytes()).replace(b'memmove(d += rx * 3, s_ += mx * 3, rx * 3);',b'memcpy(d += rx * 3, s_ += mx * 3, rx * 3);'))]:
 source=work/(name+'.c');source.write_bytes(decoder)
 run(['cc','-std=c11',*flags,*includes,'-I'+str(m5gfx/'src/lgfx/utility'),'-c',source,'-o',work/(name+'.o')])
 run(['c++','-std=c++17',*flags,'-Wno-misleading-indentation','-Wno-missing-field-initializers',*includes,*common,work/(name+'.o'),'-o',work/name])
 r=run([work/name,fixtures,'smoke'],ok=False);log=r.stdout+r.stderr;(proof/(name+'.log')).write_text(log)
 expected='left shift of negative value'if name=='baseline'else'memcpy-param-overlap'
 assert r.returncode and expected in log,(name,log)
 regressions[name]={'reproduced':True,'diagnostic':expected,'decoderSha256':sha(source)}
wide=Image.open(fixtures/'export_wide.bmp').convert('RGB');assert wide.size==(8192,16) and wide.getpixel((8191,15))==(37,160,219)
export=Image.open(fixtures/'export.jpg').convert('RGB');assert export.size==(65,47)
errors=[]
for y in range(47):
 for x in range(65):
  expected=tuple((v*128+255*127+127)//255 for v in (x*3,y*5,80))
  errors.extend(abs(a-b)for a,b in zip(export.getpixel((x,y)),expected))
assert sum(errors)/len(errors)<2.5 and max(errors)<13,(max(errors),sum(errors)/len(errors))
assert before=={str(x.relative_to(release)):sha(x)for x in inputs},'Sources changed while testing; rerun when stable'
report={'version':'0.8.35','passed':True,'physicalDeviceTested':False,'method':__doc__,'sourceHashes':before,'result':json.loads(result.stdout),'decoderSourceSha256':sha(m5gfx/patch.DECODER),'minizHostConfiguration':{'reason':'Use firmware RISC-V portable byte-access path on x86; all sanitizers remain enabled','upstreamSha256':sha(miniz_source),'hostPortableSha256':sha(miniz_portable),'change':'MINIZ_USE_UNALIGNED_LOADS_AND_STORES=0'},'regression':regressions,'sanitizers':['address','undefined'],'dependencies':{'M5GFX':{'version':'0.2.30','sources':{patch.DECODER:sha(m5gfx/patch.DECODER),'src/lgfx/utility/lgfx_tjpgd.h':sha(m5gfx/'src/lgfx/utility/lgfx_tjpgd.h')}}},'externalDecoder':{'name':'Pillow/libjpeg','exportMeanChannelError':sum(errors)/len(errors),'exportMaxChannelError':max(errors)},'evidenceHashes':{str(x.relative_to(release)):sha(x)for x in proof.iterdir()if x.is_file()},'limitations':['Host memory/IO only; no physical SD or P4 timing measured.','JPEG baseline/grayscale supported; progressive is refused.','No EXIF metadata preservation or automatic orientation.','Original resolution only; <=8192 pixels per side and <8MiB of RGBA layer pixels; no automatic resampling.','PNG alpha/depth/palette/Adam7; WebP lossy/lossless and first animated frame only, encoded <=2MiB.','The18MiB appbudget can refuse imports or gestures; previousimage remains intact.']}
(release/'tools/verification/image-codec-0835.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print('JPEG verification passed')
