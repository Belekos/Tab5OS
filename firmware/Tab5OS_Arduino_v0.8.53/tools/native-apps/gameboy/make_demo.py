from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
s=Path(__file__).resolve().parents[3]
rom=bytearray(32768);rom[0x100:0x104]=bytes([0,0xc3,0x50,1]);rom[0x134:0x143]=b'TAB5 POCKET'.ljust(15,b'\0')
code=bytearray();labels={};fix=[]
def emit(*b):code.extend(b)
def label(n):labels[n]=0x150+len(code)
def jr(op,n):emit(op,0);fix.append((len(code)-1,n,True))
def jp(n):emit(0xc3,0,0);fix.append((len(code)-2,n,False))
def copy(source,dest,size,name):
 emit(0x21,dest&255,dest>>8,0x11,source&255,source>>8,1,size&255,size>>8);label(name);emit(0x1a,0x22,0x13,0x0b,0x78,0xb1);jr(0x20,name)
emit(0xf3,0x31,0xfe,0xff);label('ready');emit(0xf0,0x44,0xfe,144);jr(0x38,'ready');emit(0xaf,0xe0,0x40)
copy(0x1000,0x8000,96*16,'tiles');copy(0x1800,0x9800,1024,'map')
emit(0x3e,0xe4,0xe0,0x47,0xaf,0xe0,0x42,0xe0,0x43,0x3e,0x91,0xe0,0x40)
label('frame');emit(0xf0,0x44,0xfe,144);jr(0x38,'frame')
emit(0x3e,0x20,0xe0,0,0xf0,0,0xf0,0,0x47)
# BIT b,B; JR NZ skip; LDH A,(scroll); INC/DEC A; LDH(scroll),A
for i,(reg,inc) in enumerate([(0x43,0x3c),(0x43,0x3d),(0x42,0x3d),(0x42,0x3c)]):
 emit(0xcb,0x40+8*i);jr(0x20,'skip'+str(i));emit(0xf0,reg,inc,0xe0,reg);label('skip'+str(i))
emit(0x3e,0x10,0xe0,0,0xf0,0,0xf0,0,0x47,0x3e,0xe4,0xcb,0x40);jr(0x20,'palette');emit(0x3e,0x1b);label('palette');emit(0xe0,0x47,0xcb,0x48);jr(0x20,'rest');emit(0xaf,0xe0,0x42,0xe0,0x43);label('rest')
label('vblank');emit(0xf0,0x44,0xfe,144);jr(0x30,'vblank');jp('frame')
for at,n,relative in fix:
 if relative:
  rel=labels[n]-(0x150+at+1);assert -128<=rel<=127;code[at]=rel&255
 else:code[at:at+2]=labels[n].to_bytes(2,'little')
rom[0x150:0x150+len(code)]=code
import sys
font=ImageFont.truetype(sys.argv[1] if len(sys.argv)>1 else str(s/'sdcard/System/fonts/regular.ttf'),8)
for i in range(96):
 im=Image.new('1',(8,8));draw=ImageDraw.Draw(im);draw.text((1,-1),chr(i+32),font=font,fill=1)
 for y in range(8):
  b=sum((1<<(7-x)) for x in range(8) if im.getpixel((x,y)))
  rom[0x1000+i*16+y*2:0x1000+i*16+y*2+2]=bytes([b,b])
lines=['+------------------+','|   TAB5 POCKET    |','|                  |','|  GAME BOY .ELF   |','|                  |','|  ARROWS: SCROLL  |','|                  |','|  Z / A: COLOURS  |','|  X / B: RESET    |','|                  |','|  ESC: QUIT       |','|                  |','|   OPEN YOUR ROM  |','|    FROM FILES    |','|                  |','|   FREE HOMEBREW  |','|                  |','+------------------+']
for y,line in enumerate(lines):
 for x,c in enumerate(line):rom[0x1800+y*32+x]=ord(c)-32
checksum=0
for b in rom[0x134:0x14d]:checksum=(checksum-b-1)&255
rom[0x14d]=checksum;total=sum(rom)&65535;rom[0x14e:0x150]=total.to_bytes(2,'big')
d=s/'sdcard/Games/GB';d.mkdir(parents=True,exist_ok=True);(d/'Tab5_Pocket.gb').write_bytes(rom)
