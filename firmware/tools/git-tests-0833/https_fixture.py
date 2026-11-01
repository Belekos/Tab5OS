"""Real smart HTTPS fixture backed by the installed PC Git, loopback only."""
from pathlib import Path
from http.server import ThreadingHTTPServer,BaseHTTPRequestHandler
from datetime import datetime,timedelta,timezone
import ssl,threading,subprocess,os,base64,socket,sys

def certificates(folder):
 try:import cryptography
 except ImportError:
  dependencies=Path(__file__).resolve().parents[3]/'ssh-test-deps-0822'
  if dependencies.is_dir():sys.path.insert(0,str(dependencies))
 from cryptography import x509
 from cryptography.x509.oid import NameOID
 from cryptography.hazmat.primitives import hashes,serialization
 from cryptography.hazmat.primitives.asymmetric import rsa
 folder=Path(folder);folder.mkdir(parents=True,exist_ok=True)
 if (folder/'server.pem').exists():return
 key=rsa.generate_private_key(public_exponent=65537,key_size=2048)
 name=x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'Tab5 Git local test CA')]);now=datetime.now(timezone.utc)
 ca=x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key()).serial_number(x509.random_serial_number()).not_valid_before(now-timedelta(days=1)).not_valid_after(now+timedelta(days=30)).add_extension(x509.BasicConstraints(ca=True,path_length=0),critical=True).sign(key,hashes.SHA256())
 server_key=rsa.generate_private_key(public_exponent=65537,key_size=2048)
 server=x509.CertificateBuilder().subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'localhost')])).issuer_name(name).public_key(server_key.public_key()).serial_number(x509.random_serial_number()).not_valid_before(now-timedelta(days=1)).not_valid_after(now+timedelta(days=30)).add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost')]),critical=False).add_extension(x509.BasicConstraints(ca=False,path_length=None),critical=True).sign(key,hashes.SHA256())
 (folder/'ca.pem').write_bytes(ca.public_bytes(serialization.Encoding.PEM));(folder/'server.pem').write_bytes(server.public_bytes(serialization.Encoding.PEM));(folder/'server.key').write_bytes(server_key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.TraditionalOpenSSL,serialization.NoEncryption()))

class Fixture:
 def __init__(self,root,certs):
  self.root=Path(root).resolve();self.requests=[];self.reject=False;self.delay=0
  self.disconnect_upload_pack=False;self.interruptions=0;self.errors=[]
  owner=self
  class Handler(BaseHTTPRequestHandler):
   protocol_version='HTTP/1.1'
   def log_message(self,*args):pass
   def do_GET(self):self.handle_git()
   def do_POST(self):self.handle_git()
   def handle_git(self):
    import time
    path,_,query=self.path.partition('?');owner.requests.append((self.command,path,query))
    self.connection.settimeout(15)
    if self.headers.get('Authorization')!='Basic '+base64.b64encode(b'test:fixture-token').decode():
     self.send_response(401);self.send_header('WWW-Authenticate','Basic realm="Git fixture"');self.send_header('Content-Length','0');self.end_headers();return
    if owner.delay:time.sleep(owner.delay)
    body=b''
    if self.headers.get('Transfer-Encoding','').lower()=='chunked':
     while True:
      size=int(self.rfile.readline().strip().split(b';')[0],16)
      if not size:self.rfile.readline();break
      if len(body)+size>16*1024*1024:self.send_error(413);return
      block=self.rfile.read(size)
      if len(block)!=size:return
      body+=block;self.rfile.read(2)
    elif self.headers.get('Content-Length'):
     amount=int(self.headers['Content-Length'])
     if amount>16*1024*1024:self.send_error(413);return
     body=self.rfile.read(amount)
    if len(body)>16*1024*1024:self.send_error(413);return
    env=dict(os.environ,GIT_PROJECT_ROOT=str(owner.root),GIT_HTTP_EXPORT_ALL='1',REQUEST_METHOD=self.command,PATH_INFO=path,QUERY_STRING=query,CONTENT_TYPE=self.headers.get('Content-Type',''),CONTENT_LENGTH=str(len(body)),REMOTE_USER='test',SERVER_PROTOCOL='HTTP/1.1')
    if 'Git-Protocol' in self.headers:env['HTTP_GIT_PROTOCOL']=self.headers['Git-Protocol']
    r=subprocess.run(['git','http-backend'],input=body,capture_output=True,env=env,timeout=20)
    if r.returncode:owner.errors.append({'exitCode':r.returncode,'stderr':r.stderr.decode(errors='replace')})
    headers,_,data=r.stdout.partition(b'\r\n\r\n')
    if not _:headers,_,data=r.stdout.partition(b'\n\n')
    pairs=[line.decode().split(':',1) for line in headers.splitlines() if b':' in line]
    status=next((int(v.strip().split()[0]) for k,v in pairs if k.lower()=='status'),200)
    self.send_response(status)
    for k,v in pairs:
     if k.lower() not in ('status','content-length','connection'):self.send_header(k,v.strip())
    self.send_header('Content-Length',str(len(data)));self.end_headers()
    try:
     if owner.disconnect_upload_pack and self.command=='POST' and path.endswith('/git-upload-pack'):
      self.wfile.write(data[:max(1,min(128,len(data)//2))]);self.wfile.flush();owner.interruptions+=1
      self.close_connection=True;self.connection.shutdown(socket.SHUT_RDWR);return
     self.wfile.write(data)
    except (BrokenPipeError,ConnectionResetError,ssl.SSLError):pass
  self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler);context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(str(Path(certs)/'server.pem'),str(Path(certs)/'server.key'));self.server.socket=context.wrap_socket(self.server.socket,server_side=True)
  self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start();self.url=f'https://localhost:{self.server.server_port}/remote.git'
 def close(self):self.server.shutdown();self.server.server_close();self.thread.join()
