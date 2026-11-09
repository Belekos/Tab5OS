"""Create original public-domain 8086 COM/MZ examples; no external assembler."""
from pathlib import Path
import argparse,struct
def create(out):
 out.mkdir(parents=True,exist_ok=True)
 # COM: MOV DX,message; MOV AH,9; INT21; MOV AX,4C00; INT21.
 message=b'TAB5 COM OK\r\n$'
 code=b'\xba\x0c\x01\xb4\x09\xcd\x21\xb8\x00\x4c\xcd\x21'+message
 (out/'HELLO.COM').write_bytes(code)
 # EXE: DS=CS, print, open adjacent DATA.TXT, read it, write to stdout.
 # The generated code is original 8086 machine code with no relocations.
 code=bytearray(b'\x0e\x1f');fixups=[];labels={}
 def addr(op,label):
  code.extend(op);fixups.append((len(code),label));code.extend(b'\0\0')
 addr(b'\xba','message');code.extend(b'\xb4\x09\xcd\x21')
 addr(b'\xba','filename');code.extend(b'\xb8\x00\x3d\xcd\x21\x89\xc3\xb9\x40\x00')
 addr(b'\xba','buffer');code.extend(b'\xb4\x3f\xcd\x21\x89\xc1\xbb\x01\x00')
 addr(b'\xba','buffer');code.extend(b'\xb4\x40\xcd\x21\xb8\x00\x4c\xcd\x21')
 for label,data in [('message',b'TAB5 MZ EXE OK\r\n$'),('filename',b'DATA.TXT\0'),('buffer',b'\0'*64)]:labels[label]=len(code);code.extend(data)
 for offset,label in fixups:struct.pack_into('<H',code,offset,labels[label])
 size=32+len(code)
 header=struct.pack('<14H',0x5a4d,size%512,(size+511)//512,0,2,32,0xffff,0,0xfffe,0,0,0,0x1c,0)+b'\0'*4
 (out/'HELLO.EXE').write_bytes(header+code)
 (out/'DATA.TXT').write_bytes(b'TAB5 ADJACENT DATA OK\r\n')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('output',type=Path);create(p.parse_args().output)
