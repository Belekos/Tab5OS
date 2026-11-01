#!/usr/bin/env python3
"""Compile the actual map translation unit with official M5Stack Tab5 / ESP32-P4 GCC.
No target link or physical execution is claimed; final firmware proof is separate.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess,tempfile
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.40_Ecriture';here=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--sdk-root',required=True,type=Path);p.add_argument('--build',type=Path);p.add_argument('--final',action='store_true');a=p.parse_args();build=a.build or Path(tempfile.mkdtemp(prefix='map-target-'));build.mkdir(exist_ok=True,parents=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
sources=[sketch/x for x in ('Tab5Map.cpp','Tab5Map.h','Tab5GeoIndex.h','Tab5GeoFold.h','Tab5Meshtastic.h')]
before={x.relative_to(root).as_posix():sha(x) for x in sources}
command=[s.replace('${SDK_ROOT}',str(a.sdk_root)).replace('${SKETCH}',str(sketch)).replace('${BUILD}',str(build)) for s in json.loads((here/'target-command.json').read_text())]
r=subprocess.run(command,capture_output=True,text=True);proof=root/'tools/verification/maps-0840';proof.mkdir(exist_ok=True,parents=True);(proof/'target-build.log').write_text(r.stdout+r.stderr);r.check_returncode()
stack=(build/'Tab5Map.su').read_text();(proof/'target-stack.txt').write_text(stack)
selected=[line for line in stack.splitlines() if any(x in line for x in ('localSearch(', 'taskLoop(', 'requestSearch(', 'marker(', 'Tab5GeoIndex::search'))]
stable=before=={x.relative_to(root).as_posix():sha(x) for x in sources};assert stable
report={'version':'0.8.40','passed':a.final,'draft':not a.final,'sourceStableDuringTests':stable,'sourceHashes':before,'testSourceHashes':{str(x.relative_to(root)):sha(x) for x in (here/'target.py',here/'target-command.json')},'evidenceHashes':{str(x.relative_to(root)):sha(x) for x in (proof/'target-build.log',proof/'target-stack.txt')},'compiler':subprocess.check_output([command[0],'--version'],text=True).splitlines()[0],'compilerSha256':sha(Path(command[0])),'command':command,'objectSha256':sha(build/'Tab5Map.o'),'selectedStackFrames':selected,'physicalDeviceTested':False,'limits':[__doc__,'Static stack frames are individual functions, not measured total task high-water marks.']}
(root/'tools/verification/maps-target-0840.json').write_text(json.dumps(report,indent=2)+'\n');print('\n'.join(selected));print('P4 compilation PASS')
