#!/usr/bin/env python3
"""Decrypt a Tab5OS T5VAULT1 or T5VAULT2 file without a Tab5 (Python 3 + cryptography).
Usage: python dechiffrer_coffre.py source.t5v destination
The password is requested privately; existing destinations are never overwritten.
"""
import argparse
import getpass
import hashlib
import hmac
import os
from pathlib import Path
import struct
import tempfile
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.exceptions import InvalidTag

HEADER, CHUNK, ITERATIONS, MAX_SIZE = 48, 4096, 600000, 128*1024*1024

def backup_key(source, password):
    data = Path(source).read_bytes()
    if len(data) != 48+106+16 or data[:8] != b'T5VAULT1' or struct.unpack_from('<IIQ',data,8) != (ITERATIONS,CHUNK,106):
        raise ValueError('Invalid key backup / Sauvegarde de clé invalide')
    derived = hashlib.pbkdf2_hmac('sha256',password.encode('utf-8'),data[24:40],ITERATIONS,32)
    payload = AESGCM(derived).decrypt(data[40:48]+bytes(4),data[48:],data[:48]+struct.pack('<II',0,106))
    if payload[:8] != b'T5KEY001' or payload[8] != 1 or b'\0' not in payload[57:]:
        raise ValueError('Invalid key record / Clé invalide')
    return payload[9:25],payload[25:57]

def decrypt(source, destination, password, key_backup=None):
    source, destination = Path(source), Path(destination)
    if destination.exists():
        raise FileExistsError('Destination already exists / La destination existe déjà')
    temporary = None
    try:
        with source.open('rb') as stream:
            header = stream.read(HEADER)
            if len(header) != HEADER or header[:8] not in (b'T5VAULT1',b'T5VAULT2'):
                raise ValueError('Invalid vault header / En-tête de coffre invalide')
            keyed = header[:8] == b'T5VAULT2'
            if keyed: header += stream.read(16)
            header_size = 64 if keyed else 48
            if len(header) != header_size: raise ValueError('Truncated header / En-tête tronqué')
            iterations, chunk, size = struct.unpack_from('<IIQ', header, 8)
            if iterations != (0 if keyed else ITERATIONS) or chunk != CHUNK or size > MAX_SIZE:
                raise ValueError('Unsupported vault format / Format non pris en charge')
            count = max(1, (size+CHUNK-1)//CHUNK)
            if os.fstat(stream.fileno()).st_size != header_size+size+16*count:
                raise ValueError('Vault truncated or altered / Coffre tronqué ou altéré')
            if keyed:
                if key_backup is None: raise ValueError('Use --cle sauvegarde.t5key / Clé de secours requise')
                key_id, master = backup_key(key_backup,password)
                if key_id != header[24:40]: raise ValueError('Wrong key / Cette clé ne correspond pas au fichier')
                key = hmac.new(master,b'T5VAULT2'+header[40:56],hashlib.sha256).digest()
            else:
                key = hashlib.pbkdf2_hmac('sha256', password.encode('utf-8'), header[24:40], ITERATIONS, 32)
            cipher = AESGCM(key)
            def chunks():
                stream.seek(header_size)
                for index in range(count):
                    length = min(CHUNK, size-index*CHUNK)
                    nonce = header[-8:] + struct.pack('>I', index)
                    aad = header + struct.pack('<II', index, length)
                    yield cipher.decrypt(nonce, stream.read(length+16), aad)
            # Authenticate the entire file before writing any plaintext.
            for _ in chunks():
                pass
            fd, temporary = tempfile.mkstemp(prefix='.tab5-decrypt-', dir=destination.parent)
            with os.fdopen(fd, 'wb') as output:
                for data in chunks():
                    output.write(data)
                output.flush()
                os.fsync(output.fileno())
            # Atomic publication without replacing a concurrently created file.
            os.link(temporary, destination)
    finally:
        if temporary is not None:
            os.unlink(temporary)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('destination',type=Path)
    parser.add_argument('--cle',type=Path,help='Sauvegarde de la clé .t5key pour un coffre V2')
    args=parser.parse_args()
    try:
        decrypt(args.source,args.destination,getpass.getpass('Mot de passe du coffre ou de la sauvegarde de clé / Password: '),args.cle)
    except InvalidTag:
        parser.exit(1,'Mot de passe incorrect ou coffre altéré / Incorrect password or altered vault.\n')
    except (OSError,ValueError) as error:
        parser.exit(1,str(error)+'\n')
    print('Copie déchiffrée / Decrypted copy:',args.destination)

if __name__=='__main__':
    main()
