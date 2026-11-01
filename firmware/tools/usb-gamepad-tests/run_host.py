"""Build actual USB driver function bodies against allocation/USB boundary doubles."""
from pathlib import Path
import argparse,datetime,hashlib,json,re,subprocess,tempfile,os
p=argparse.ArgumentParser();p.add_argument('sketch',type=Path);p.add_argument('--baseline',type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;sketch=a.sketch.resolve()
vcvars=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
functions=['get_hid_device_by_handle','is_interface_in_list','get_iface_by_handle',
 'hid_host_user_interface_callback','hid_host_add_interface','_hid_host_remove_interface',
 'hid_host_device_disconnected','hid_host_interface_claim_and_prepare_transfer',
 'hid_host_interface_release_and_free_transfer','hid_host_disable_interface',
 'hid_host_disable_interface_disconnect','hid_host_install_device','hid_host_uninstall_device',
 'in_xfer_done','hid_host_device_close_impl','hid_host_device_close','hid_host_device_close_disconnect','hid_host_device_start','ctrl_xfer_done','hid_control_transfer',
 'hid_host_suspend_interface','hid_host_resume_interface','hid_host_device_suspended','hid_host_device_resumed']
def extract(text,name):
 m=re.search(r'^(?:static\s+)?(?:inline\s+)?(?:esp_err_t|bool|void|hid_device_t|hid_iface_t)\s+\*?'+re.escape(name)+r'\([^;{}]*\)\s*\{',text,re.M)
 if not m:raise ValueError(name)
 start=m.start();at=text.index('{',m.start());depth=1;end=at+1
 # Strip comments before brace counting: braces in driver comments are rare,
 # but the extraction must still preserve every original body byte.
 lexical=re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\{|\}',re.S)
 for token in lexical.finditer(text,at+1):
  t=token.group()
  if t=='{':depth+=1
  elif t=='}':
   depth-=1
   if depth==0:return text[start:token.end()]
 raise ValueError('unterminated '+name)
def typedef(text,start,end):
 return text[text.index(start):text.index(end,text.index(start))+len(end)]
def run(command,directory):
 result=subprocess.run(command,cwd=directory,capture_output=True,text=True,encoding='utf-8',errors='replace')
 if result.returncode:print((result.stdout+result.stderr)[-12000:])
 return result
def host(source,directory,label):
 text=(source/'src/usb_hid/hid_host.c').read_text(encoding='utf-8')
 structures=[typedef(text,'typedef struct hid_host_device {','} hid_device_t;'),
  typedef(text,'typedef enum {','} hid_iface_state_t;'),
  typedef(text,'typedef struct hid_interface {','} hid_iface_t;')]
 driver='''typedef struct {STAILQ_HEAD(devices,hid_host_device) hid_devices_tailq;STAILQ_HEAD(interfaces,hid_interface) hid_ifaces_tailq;void *client_handle,*open_close_mutex,*user_arg;hid_host_driver_event_cb_t user_cb;} hid_driver_t;static hid_driver_t *s_hid_driver;'''
 current_only=['hid_interface_present','hid_host_user_device_callback','hid_host_notify_interface_connected','hid_host_interface_list_create','hid_host_device_init_attempt'] if '#include "input_interface.h"' in text else []
 extra=['hid_device_try_lock','hid_device_unlock','hid_host_xinput_initialize'] if 'static esp_err_t hid_host_xinput_initialize' in text else []
 bodies=[extract(text,name) for name in functions+current_only+extra]
 prototypes=[body[:body.index('{')].strip()+';' for body in bodies]
 output='\n'.join(['#include "host_stub.h"', '#define CURRENT_USB_DRIVER 1' if current_only else '',*structures,driver,*prototypes,*bodies,'#include "test_host.c"'])
 (directory/(label+'.c')).write_text(output,encoding='utf-8')
 cmd=f'call "{vcvars}" >nul\ncl /nologo /TC /std:c17 /utf-8 /W3 /I"{here}" /I"{sketch}" {label}.c /Fe:{label}.exe >{label}-compile.log 2>&1\nif errorlevel 1 exit /b 1\n{label}.exe >{label}-run.log 2>&1\n'
 (directory/(label+'.cmd')).write_text(cmd)
 result=run(['cmd','/c',str(directory/(label+'.cmd'))],directory)
 compilelog=(directory/(label+'-compile.log')).read_text(errors='replace')
 log=(directory/(label+'-run.log')).read_text(errors='replace') if (directory/(label+'-run.log')).exists() else ''
 if not log:print(compilelog[-12000:]);raise RuntimeError('host harness did not build')
 print(label,log[-400:].strip())
 return dict(exitCode=result.returncode,result=log.splitlines()[-1],sourceSha256=hashlib.sha256((source/'src/usb_hid/hid_host.c').read_bytes()).hexdigest(),extractedFunctions=functions+current_only+extra,compileWarnings=compilelog)
with tempfile.TemporaryDirectory(prefix='tab5-usb-0824-') as name:
 d=Path(name)
 current=host(sketch,d,'current')
 baseline=host(a.baseline.resolve(),d,'baseline') if a.baseline else None
 cmd=f'call "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 /utf-8 /I"{sketch}" "{here / "test_interfaces.cpp"}" /Fe:interfaces.exe\nif errorlevel 1 exit /b 1\ninterfaces.exe\n'
 (d/'interfaces.cmd').write_text(cmd);enumeration=run(['cmd','/c',str(d/'interfaces.cmd')],d)
 print(enumeration.stdout[-500:])
 packet=run(['cmd','/c',str(here/'run_msvc.cmd'),str(sketch)],d);print(packet.stdout[-500:])
 files=['WriterUsb.cpp','WriterUsb.h','WriterGamepad.h','WriterGamepad.cpp','WriterHid.cpp','WriterMouse.cpp','src/usb_hid/hid_host.c','src/usb_hid/input_interface.h','src/usb_hid/gamepad_transport.h']
 report=dict(version='0.8.24',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=current['exitCode']==0 and enumeration.returncode==0 and packet.returncode==0 and (baseline is None or baseline['exitCode']!=0),physicalDeviceTested=False,method='MSVC native: production USB lifecycle bodies with USB/RTOS boundary doubles; actual bounded enumeration and HID/XInput decoders',current=current,baseline=baseline,enumeration=enumeration.stdout,packets=packet.stdout,files={f:hashlib.sha256((sketch/f).read_bytes()).hexdigest() for f in files},harness={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in here.iterdir() if f.is_file() and f.suffix in ['.h','.c','.cpp','.py','.cmd']})
 (sketch.parent/'tools/verification/gamepad-0824.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 raise SystemExit(0 if report['passed'] else 1)
