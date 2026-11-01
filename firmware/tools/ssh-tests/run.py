from pathlib import Path
import sys,os,socket,threading,time,subprocess,json,hashlib,base64,traceback,logging
here=Path(__file__).resolve().parent
work=here.parents[2]
sys.path.insert(0,os.environ.get('TAB5_SSH_TEST_DEPS',str(work/'ssh-test-deps-0822')))
import paramiko
output=Path(os.environ.get('TAB5_SSH_TEST_DIR','C:/workspace/tab5-ssh-0822-host'))
paramiko.util.log_to_file(str(output/'server.log'),logging.DEBUG)
client=output/'client.exe'
native_build=json.loads((output/'build-provenance.json').read_text())
assert native_build['mbedtlsVersion']=='3.6.5' and native_build['mbedtlsCommit']=='ffb280bb63c78bfec1e1ab55040671768c85c923','Build against the pinned ESP-IDF 5.5.4 mbedTLS source.'
assert native_build['clientSha256']==hashlib.sha256(client.read_bytes()).hexdigest(),'Client executable changed after build.'
for name,digest in native_build['appInputs'].items():assert hashlib.sha256((here.parent.parent/name).read_bytes()).hexdigest()==digest,('Rebuild changed client source',name)
root=output/'fixture';root.mkdir(exist_ok=True)
volume=root/'volume';volume.mkdir(exist_ok=True)
remote=root/'remote';remote.mkdir(exist_ok=True)
# Fixtures stay isolated; old pins belong only to this generated loopback test.
if (volume/'pins').exists():
 for p in (volume/'pins').glob('host??'):p.unlink()
for p in [volume/'download.bin',volume/'download.bin.tab5-sftp-part',volume/'cancel.bin',volume/'cancel.bin.tab5-sftp-part',remote/'upload.bin',remote/'upload.bin.tab5-sftp-part',remote/'failclose.bin',remote/'failclose.bin.tab5-sftp-part']:
 if p.exists():p.unlink()
(volume/'password').write_text('correct\n');(volume/'wrong').write_text('wrong\n')
key=paramiko.ECDSAKey.generate();key.write_private_key_file(str(volume/'key.pem'))
key.write_private_key_file(str(volume/'encrypted.pem'),password='fixture passphrase')
for name in ['key.pem.pub','encrypted.pem.pub']:(volume/name).write_text(key.get_name()+' '+key.get_base64()+' fixture\n')
(volume/'passphrase').write_text('fixture passphrase\n')
(volume/'broken.pem').write_text('-----BEGIN RSA PRIVATE KEY-----\ninvalid key\n-----END RSA PRIVATE KEY-----\n')
key.write_private_key_file(str(volume/'no-public.pem'))
rsakey=paramiko.RSAKey.generate(2048);rsakey.write_private_key_file(str(volume/'rsa.pem'))
hostkey=paramiko.RSAKey.generate(2048)
fingerprint='SHA256:'+base64.b64encode(hashlib.sha256(hostkey.asbytes()).digest()).decode().rstrip('=')
payload=bytes(range(256))*4096
(remote/'source.bin').write_bytes(payload);(remote/'slow.bin').write_bytes(payload);(volume/'upload.bin').write_bytes(payload)
auth=[];server_errors=[];connections=[]
class Server(paramiko.ServerInterface):
 def check_auth_password(self,user,password):auth.append(('password',user));return paramiko.AUTH_SUCCESSFUL if user=='tester' and password=='correct' else paramiko.AUTH_FAILED
 def check_auth_publickey(self,user,public):auth.append(('key',user));return paramiko.AUTH_SUCCESSFUL if user=='tester' and public in (key,rsakey) else paramiko.AUTH_FAILED
 def get_allowed_auths(self,user):return 'password,publickey'
 def check_channel_request(self,kind,chanid):return paramiko.OPEN_SUCCEEDED if kind=='session' else paramiko.OPEN_FAILED_ADMINISTRATIVELY_PROHIBITED
 def check_channel_exec_request(self,channel,command):
  def execute():
   try:
    if command==b'both':channel.sendall(b'output\n');channel.sendall_stderr(b'diagnostic\n')
    elif command==b'cat':
     while True:
      data=channel.recv(4096)
      if not data:break
      channel.sendall(data)
    elif command==b'bulk':channel.sendall(b'x'*200000);channel.sendall_stderr(b'y'*70000)
    elif command==b'stall':
     while not channel.closed:time.sleep(.01)
     return
    elif command!=b'exit7':channel.sendall(command)
    channel.send_exit_status(7 if command==b'exit7' else 0);channel.shutdown_write();channel.close()
   except (EOFError,OSError):pass
  threading.Thread(target=execute,daemon=True).start();return True
class SlowHandle(paramiko.SFTPHandle):
 def read(self,offset,length):time.sleep(.1);return super().read(offset,length)
class Sftp(paramiko.SFTPServerInterface):
 def target(self,path):
  p=(remote/path.lstrip('/')).resolve()
  if not p.is_relative_to(remote.resolve()):raise OSError(13,'outside fixture')
  return p
 def stat(self,path):
  try:return paramiko.SFTPAttributes.from_stat(self.target(path).stat())
  except OSError as e:return paramiko.SFTPServer.convert_errno(e.errno)
 lstat=stat
 def list_folder(self,path):
  try:
   result=[]
   for p in self.target(path).iterdir():a=paramiko.SFTPAttributes.from_stat(p.stat());a.filename=p.name;result.append(a)
   return result
  except OSError as e:return paramiko.SFTPServer.convert_errno(e.errno)
 def open(self,path,flags,attr):
  try:
   fd=os.open(self.target(path),flags|os.O_BINARY,0o600);mode='r+b' if flags&os.O_RDWR else 'wb' if flags&os.O_WRONLY else 'rb';file=os.fdopen(fd,mode)
   handle=(SlowHandle if path.endswith('slow.bin') else paramiko.SFTPHandle)(flags);handle.readfile=file;handle.writefile=file;handle.failclose='failclose' in path;return handle
  except OSError as e:return paramiko.SFTPServer.convert_errno(e.errno)
 def remove(self,path):
  try:self.target(path).unlink();return paramiko.SFTP_OK
  except OSError as e:return paramiko.SFTPServer.convert_errno(e.errno)
 def rename(self,old,new):
  try:
   if self.target(new).exists():return paramiko.SFTP_FAILURE
   self.target(old).rename(self.target(new));return paramiko.SFTP_OK
  except OSError as e:return paramiko.SFTPServer.convert_errno(e.errno)
class SftpServer(paramiko.SFTPServer):
 def _process(self,t,request_number,msg):
  if t==paramiko.sftp.CMD_CLOSE:
   # A real protocol failure after consuming a handle exercises the client's
   # ownership transition; Paramiko's default implementation always says OK.
   name=msg.get_string();handle=self.file_table.get(name)
   if handle is not None:
    self.file_table.pop(name);handle.close();self._send_status(request_number,paramiko.SFTP_FAILURE if handle.failclose else paramiko.SFTP_OK);return
   if name in self.folder_table:
    del self.folder_table[name];self._send_status(request_number,paramiko.SFTP_OK);return
   self._send_status(request_number,paramiko.SFTP_BAD_MESSAGE);return
  return super()._process(t,request_number,msg)
listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(8);port=listener.getsockname()[1];shutdown=False
def handle(connection):
 transport=paramiko.Transport(connection);connections.append(transport);transport.add_server_key(hostkey);transport.set_subsystem_handler('sftp',SftpServer,Sftp)
 try:
  transport.start_server(server=Server())
  while transport.is_active() and not shutdown:time.sleep(.01)
 except (EOFError,OSError,paramiko.SSHException):pass
 finally:transport.close()
def accept():
 while not shutdown:
  try:connection,_=listener.accept()
  except OSError:return
  threading.Thread(target=handle,args=(connection,),daemon=True).start()
threading.Thread(target=accept,daemon=True).start()
checks=[];peak=0
def run(name,args,rc=0,data=b'',env=None):
 global peak
 environment=os.environ.copy();environment.update(SSH_VOLUME=str(volume),SSH_REPORT=str(root/'result.json'))
 if env:environment.update(env)
 start=time.monotonic()
 try:p=subprocess.run([str(client)]+args,input=data,capture_output=True,env=environment,timeout=45)
 except subprocess.TimeoutExpired as e:print('TIMEOUT OUT',e.stdout,'ERR',e.stderr[-9000:] if e.stderr else '',flush=True);raise
 stats=json.loads((root/'result.json').read_text());peak=max(peak,stats['peakSessionBytes'])
 if rc is not None:assert p.returncode==rc,(name,p.returncode,p.stdout[:1000],p.stderr[-7500:])
 assert stats['prematureTcpWrites']==0 and stats['retainedCrtBytes']==0,(name,stats)
 assert stats['retainedSessionBytes']==0 and stats['liveAllocations']==0,(name,stats)
 checks.append({'name':name,'returnCode':p.returncode,'elapsedSeconds':round(time.monotonic()-start,3),**stats})
 print(name,'PASS',flush=True);return p
base=['-p',str(port),'--password-file','/password','tester@127.0.0.1']
try:
 p=run('keyscan',['ssh','keyscan','-p',str(port),'127.0.0.1']);assert fingerprint.encode() in p.stdout and not auth
 before=len(auth);run('unknown host fails before authentication',['ssh']+base+['both'],1);assert len(auth)==before
 run('explicit trust',['ssh','trust','-p',str(port),'127.0.0.1',fingerprint])
 run('TCP waits through 10 empty readiness polls',['ssh']+base+['both'],env={'SSH_CONNECT_EMPTY_POLLS':'10'})
 p=run('stdout and stderr remain distinct',['ssh']+base+['both']);assert p.stdout==b'output\n' and p.stderr==b'diagnostic\n'
 p=run('pipeline stdin 200KiB',['ssh']+base+['cat'],data=b'z'*200000);assert p.stdout==b'z'*200000
 p=run('simultaneous streams above receive window',['ssh']+base+['bulk']);assert p.stdout==b'x'*200000 and p.stderr==b'y'*70000
 run('remote exit status',['ssh']+base+['exit7'],7)
 run('wrong password',['ssh','-p',str(port),'--password-file','/wrong','tester@127.0.0.1','both'],1)
 run('PEM ECDSA private key',['ssh','-p',str(port),'-i','/key.pem','tester@127.0.0.1','both'])
 run('encrypted PEM ECDSA private key',['ssh','-p',str(port),'-i','/encrypted.pem','--passphrase-file','/passphrase','tester@127.0.0.1','both'])
 run('PEM RSA private key with SHA2 signature',['ssh','-p',str(port),'-i','/rsa.pem','tester@127.0.0.1','both'])
 run('malformed PEM is rejected',['ssh','-p',str(port),'-i','/broken.pem','tester@127.0.0.1','both'],1)
 run('wrong PEM passphrase is rejected',['ssh','-p',str(port),'-i','/encrypted.pem','--passphrase-file','/wrong','tester@127.0.0.1','both'],1)
 run('missing ECDSA public companion is rejected',['ssh','-p',str(port),'-i','/no-public.pem','tester@127.0.0.1','both'],1)
 run('changed fingerprint cannot overwrite trust',['ssh','trust','-p',str(port),'127.0.0.1','SHA256:'+'A'*43],1)
 run('forget',['ssh','forget','-p',str(port),'127.0.0.1'])
 run('pin wrong fingerprint',['ssh','trust','-p',str(port),'127.0.0.1','SHA256:'+'A'*43])
 before=len(auth);run('changed server key blocks credentials',['ssh']+base+['both'],1);assert len(auth)==before
 run('forget wrong pin',['ssh','forget','-p',str(port),'127.0.0.1'])
 run('restore correct pin',['ssh','trust','-p',str(port),'127.0.0.1',fingerprint])
 run('cancellation of silent server',['ssh']+base+['stall'],130,env={'SSH_CANCEL_MS':'300'})
 run('idle timeout',['ssh','--timeout','1']+base+['stall'],1)
 run('network disabled',['ssh']+base+['both'],130,env={'SSH_NETWORK_OFF':'1'})
 run('network gate busy',['ssh']+base+['both'],1,env={'SSH_GATE_BUSY':'1'})
 p=run('SFTP listing',['sftp','ls']+base+['/']);assert b'source.bin' in p.stdout
 run('SFTP receive 1MiB',['sftp','get']+base+['/source.bin','/download.bin']);assert (volume/'download.bin').read_bytes()==payload
 run('SFTP refuses overwrite local',['sftp','get']+base+['/source.bin','/download.bin'],1);assert (volume/'download.bin').read_bytes()==payload
 run('SFTP send 1MiB',['sftp','put']+base+['/upload.bin','/upload.bin']);assert (remote/'upload.bin').read_bytes()==payload
 run('SFTP refuses overwrite remote',['sftp','put']+base+['/upload.bin','/upload.bin'],1);assert (remote/'upload.bin').read_bytes()==payload
 run('SFTP CLOSE failure consumes handle once',['sftp','put']+base+['/failclose.bin','/upload.bin'],1);assert not (remote/'failclose.bin').exists() and not (remote/'failclose.bin.tab5-sftp-part').exists()
 run('SFTP cancelled receive removes owned partial',['sftp','get']+base+['/slow.bin','/cancel.bin'],130,env={'SSH_CANCEL_MS':'2000'});assert not (volume/'cancel.bin').exists() and not (volume/'cancel.bin.tab5-sftp-part').exists()
 run('50 sessions in one process retain no CRT allocation',['ssh']+base+['both'],env={'SSH_REPEAT':'50'})
 password_calls=checks[-1]['allocationCalls']
 run('20 ECDSA sessions retain no CRT allocation',['ssh','-p',str(port),'-i','/key.pem','tester@127.0.0.1','both'],env={'SSH_REPEAT':'20'})
 key_calls=checks[-1]['allocationCalls']
 # Every observed session allocation boundary is failed in turn. Repeating
 # each fault after a warm-up also checks SDK/CRT allocations, not just the
 # app's PSRAM adapter. TCP packet coalescing may move a boundary slightly.
 for n in range(1,password_calls+1):run('password session allocation failure '+str(n),['ssh']+base+['both'],rc=None,env={'SSH_FAIL_AT':str(n),'SSH_REPEAT':'2'})
 for n in range(30,key_calls+1):run('key import/signature allocation failure '+str(n),['ssh','-p',str(port),'-i','/key.pem','tester@127.0.0.1','both'],rc=None,env={'SSH_FAIL_AT':str(n),'SSH_REPEAT':'2'})
 report={'passed':True,'version':'0.8.22','method':'Production Tab5Ssh.cpp + official libssh2 1.11.2_DEV pinned at 18aebe08962c1878d84c373ad85caff25ec67771 + actual mbedTLS 3.6.5 pinned at ffb280bb63c78bfec1e1ab55040671768c85c923 from ESP-IDF v5.5.4 sources, compiled with native Windows configuration (not target hardware/configuration), over loopback TCP against Paramiko 4.0.0. Host NVS adapter; actual target SDK separately compiled, no physical hardware claim.','physicalDeviceTested':False,'nativeBuild':native_build,'checks':checks,'peakSessionBytes':peak,'sources':{str(p.relative_to(here.parent.parent)):hashlib.sha256(p.read_bytes()).hexdigest() for p in list((here.parent.parent/'Tab5OS_Arduino_v0.8.22_Ecriture').glob('Tab5Ssh*'))+list((here.parent.parent/'Tab5OS_Arduino_v0.8.22_Ecriture/src/tab5ssh').glob('*'))+list(here.glob('*.py'))+list(here.glob('*.h'))+list(here.glob('*.cpp')) if p.is_file()}}
 (here.parent/'verification/ssh-0822.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('ALL PASS',len(checks),'peak',peak)
finally:
 shutdown=True;listener.close()
 for t in connections:t.close()
