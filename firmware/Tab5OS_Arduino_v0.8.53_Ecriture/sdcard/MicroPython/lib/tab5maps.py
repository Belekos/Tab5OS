"""Streaming map import for Tab5OS: T5MAP packs, PNG XYZ cache.
Full tile decoding stays native; only verified existing cache tiles are kept.
"""
import struct
from _tab5 import call
import tab5fs as fs
from tab5workshop import identifier

PNG=b'\x89PNG\r\n\x1a\n'
MAX_TILE_BYTES=512*1024
OFFLINE='/System/maps/offline'
CACHE='/System/maps/cache'


def crc(data, value=0):
    return struct.unpack('<I',call(68,0,0,0,struct.pack('<I',value)+data,4))[0]


def exact(file,size):
    value=file.read(size)
    if len(value)!=size:
        raise ValueError('Carte tronquée')
    return value


def png(file, size):
    """Check the PNG envelope only; validate_tile is required before installing."""
    start=file.tell()
    if size<57 or size>MAX_TILE_BYTES or exact(file,8)!=PNG:
        raise ValueError('Tuile PNG invalide')
    first=True;image=False;ended=False
    while file.tell()-start+12<=size:
        length=struct.unpack('>I',exact(file,4))[0]
        kind=exact(file,4)
        chunk_length=length
        if length>size-(file.tell()-start)-4:
            raise ValueError('Tuile PNG tronquée')
        digest=crc(kind)
        if first:
            if kind!=b'IHDR' or length!=13:
                raise ValueError('En-tête PNG invalide')
            header=exact(file,13)
            if struct.unpack('>II',header[:8])!=(256,256):
                raise ValueError('Tuiles PNG : 256 × 256 requis')
            depth=header[8];color=header[9]
            valid_depth=(color==0 and depth in (1,2,4,8,16)) or (color==3 and depth in (1,2,4,8)) or (color in (2,4,6) and depth in (8,16))
            if not valid_depth or header[10] or header[11] or header[12]>1:
                raise ValueError('En-tête PNG invalide')
            digest=crc(header,digest);length=0;first=False
        elif kind==b'IHDR':
            raise ValueError('En-tête PNG invalide')
        while length:
            data=exact(file,min(4092,length))
            digest=crc(data,digest);length-=len(data)
        if digest!=struct.unpack('>I',exact(file,4))[0]:
            raise ValueError('Tuile PNG corrompue')
        if kind==b'IDAT' and chunk_length:
            image=True
        if kind==b'IEND':
            if chunk_length:
                raise ValueError('Tuile PNG invalide')
            ended=True
            break
    if not ended or not image or file.tell()-start!=size:
        raise ValueError('Tuile PNG incomplète')


def valid_tile(filename, offset=0, length=0):
    """Decode a file or pack slice natively, without a VM-sized image buffer.
    Missing/invalid data can be replaced; unavailable SD/decoder memory cannot.
    """
    filename=fs.path(filename)
    if not 0<=offset<=0xffffffff or not 0<=length<=MAX_TILE_BYTES:
        raise ValueError('Limites de tuile invalides')
    # The bridge has signed 32-bit arguments; packs may reach 4 GiB.
    position=offset if offset<0x80000000 else offset-0x100000000
    try:
        return call(97,position,length,0,filename.encode(),0)==1
    except OSError as error:
        if error.args and error.args[0]==2:
            return False
        raise


def validate_tile(filename, offset=0, length=0):
    if not valid_tile(filename,offset,length):
        raise ValueError('Tuile PNG/JPEG invalide ou incomplète')


def inspect_pack(filename, progress=None):
    size=fs.stat(filename)[1]
    with open(filename,'rb') as f:
        h=struct.unpack('<8s8I2Q24s40s8s',exact(f,128))
        magic,version,header,count,blocks,per,directory,index,data,first,last,cache,name,reserved=h
        cache=cache.split(b'\0',1)[0].decode();identifier(cache)
        if magic!=b'T5MAP01\0' or version!=1 or header!=128 or not count or not 64<=per<=4096 or first>last:
            raise ValueError('En-tête T5MAP invalide')
        if blocks!=(count+per-1)//per or directory<128 or directory+blocks*16>index or index+count*16>data or data>=size or size>0xffffffff or blocks*16>32768:
            raise ValueError('Index T5MAP invalide ou trop volumineux')
        previous=-1
        for i in range(count):
            f.seek(index+i*16)
            key,offset,length=struct.unpack('<QII',exact(f,16))
            z=key>>58;x=(key>>29)&0x1fffffff;y=key&0x1fffffff
            if z>22 or x>=(1<<z) or y>=(1<<z) or key<=previous or offset<data or not length or length>MAX_TILE_BYTES or offset+length>size:
                raise ValueError('Entrée T5MAP invalide')
            if (i==0 and key!=first) or (i==count-1 and key!=last):
                raise ValueError('Bornes T5MAP incorrectes')
            if i%per==0:
                f.seek(directory+(i//per)*16)
                blockkey,start,n=struct.unpack('<QII',exact(f,16))
                if (blockkey,start,n)!=(key,i,min(per,count-i)):
                    raise ValueError('Répertoire T5MAP invalide')
            validate_tile(filename,offset,length);previous=key
            if progress and (i%16==0 or i==count-1):
                progress.text(str(i+1)+' / '+str(count))
        return {'cache_id':cache,'name':name.split(b'\0',1)[0].decode(),'count':count,'bytes':size}


def import_pack(source, name=None, progress=None):
    name=name or source.rsplit('/',1)[-1]
    fs.relative(name)
    if '/' in name or not name.lower().endswith('.t5map'):
        raise ValueError('Nom de paquet .t5map requis')
    destination=OFFLINE+'/'+name
    if fs.exists(destination):
        raise ValueError('Le paquet existe déjà')
    with fs.Transaction() as tx:
        staged=tx.allocate(destination)
        if source.startswith('https://'):
            fs.download(source,staged,512*1024*1024,progress)
        else:
            fs.copy(source,staged,lambda done,total: progress.text(str(done//1024)+' / '+str(total//1024)+' KiB') if progress else None)
        result=inspect_pack(staged,progress)
        tx.commit()
    return result


def import_xyz(source, cache_id, progress=None):
    identifier(cache_id)
    source=fs.path(source)
    destination=CACHE+'/'+cache_id
    if source==destination or source.startswith(destination+'/') or destination.startswith(source+'/'):
        raise ValueError('Dossiers source et destination imbriqués')
    imported=skipped=0
    # Commit each tile individually: cancellation retains valid completed tiles, never a partial PNG.
    for zname in fs.ilistdir(source):
        if not zname.isdigit():
            continue
        z=int(zname)
        if z>22:
            continue
        zdir=source+'/'+zname
        if not fs.isdir(zdir):
            continue
        for xname in fs.ilistdir(zdir):
            if not xname.isdigit() or int(xname)>=(1<<z):
                continue
            xdir=zdir+'/'+xname
            if not fs.isdir(xdir):
                continue
            for leaf in fs.ilistdir(xdir):
                if not leaf.lower().endswith('.png') or not leaf[:-4].isdigit() or int(leaf[:-4])>=(1<<z):
                    continue
                src=xdir+'/'+leaf
                if fs.isdir(src):
                    continue
                dest=destination+'/'+str(z)+'/'+str(int(xname))+'/'+str(int(leaf[:-4]))+'.png'
                if valid_tile(dest):
                    skipped+=1
                    continue
                with fs.Transaction() as tx:
                    staged=tx.allocate(dest)
                    fs.copy(src,staged)
                    validate_tile(staged)
                    tx.commit()
                imported+=1
                if progress:
                    progress.text(str(imported)+' PNG · '+str(skipped)+' =')
    return imported,skipped
