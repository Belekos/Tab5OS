from pathlib import Path
import argparse,hashlib,subprocess,tempfile,shutil,json
p=argparse.ArgumentParser();p.add_argument('sketch',type=Path);a=p.parse_args();sketch=a.sketch.resolve();here=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='tab5-http-',dir=here) as tmp:
 d=Path(tmp)
 for file in ['HttpPlatform.h','http.cpp']:shutil.copy2(here/file,d/file)
 source=(sketch/'Tab5NetworkHttp.cpp').read_text(encoding='utf-8')
 # Replace only POSIX write's qualified call, preserving Stream::write names.
 source=source.replace('::write(fd,','::fakeFileWrite(fd,')
 (d/'Tab5NetworkHttp.cpp').write_text(source,encoding='utf-8')
 for header in ['Tab5SecureClient.h','Tab5CooperativeClient.h','Tab5NetworkGate.h','Tab5Storage.h','Tab5VolumeFS.h','Tab5UsbStorage.h','HTTPClient.h','NetworkClient.h','unistd.h']:
  (d/header).write_text('#include "HttpPlatform.h"\n',encoding='utf-8')
 subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/W4','/utf-8','/I'+str(d),'/I'+str(sketch),'http.cpp','/Fe:http.exe'],cwd=d,check=True)
 subprocess.run([str(d/'http.exe'),str(d/'volume')],cwd=d,check=True)
 print('Production HTTP source SHA256:',hashlib.sha256((sketch/'Tab5NetworkHttp.cpp').read_bytes()).hexdigest())
