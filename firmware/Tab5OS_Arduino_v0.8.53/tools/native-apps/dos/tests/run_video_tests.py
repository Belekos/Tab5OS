"""Verify exact presentation pixels and synchronous VGA snapshot equivalence."""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--msvc',action='store_true');p.add_argument('--cc',default='cc');p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];repo=root.parents[4]
with tempfile.TemporaryDirectory(prefix='tab5-dos-video-') as temporary:
 out=Path(temporary);suffix='.exe' if os.name=='nt' else ''
 base=['cl','/nologo','/std:c11','/O2','/D_CRT_SECURE_NO_WARNINGS','/DEMUDBG_OFF','/FI'+str(root/'tests/host-compat/platform.h'),'/I'+str(root/'tests/host-compat')] if a.msvc else [a.cc,'-std=gnu11','-O2','-DEMUDBG_OFF','-include',str(root/'tests/host-compat/platform.h')]
 for component in ('ibmpc','x86','upd765','diskspace','emudbg'):base += [('/I' if a.msvc else '-I')+str(root/'core'/component)]
 def build(name,sources):
  exe=out/(name+suffix);cmd=base+[str(s) for s in sources]+(['/Fe:'+str(exe)] if a.msvc else ['-lm','-o',str(exe)])
  subprocess.run(cmd,cwd=out,check=True,capture_output=True);return exe
 def run(exe,*args):
  result=subprocess.run([str(exe),*args],capture_output=True,text=True,timeout=120)
  if result.returncode:raise RuntimeError(result.stdout+result.stderr)
  return json.loads(result.stdout.strip().splitlines()[-1])
 presentation=run(build('presentation',[root/'tests/test_presentation.c']))
 pacing=run(build('pacing',[root/'tests/test_pacing.c']))
 sources=[s for s in (root/'core/ibmpc').glob('*.c') if s.name!='sound.c']+[root/'core/x86/x86.c',root/'core/upd765/upd765.c',root/'core/diskspace/fat.c',root/'x87_disabled.c',root/'sound_disabled.c',root/'tests/test_video.c']
 exe=build('video',sources);snapshot=run(exe,'0');synchronous=run(exe,'1');same=snapshot['frameChecksums']==synchronous['frameChecksums']
 # Recorded before the palette/clock optimisations, using the c08b0fa renderer
 # and these deterministic VRAM patterns in BIOS modes 03/04/0D/10/12/13.
 baseline=[3043387023,3210283913,2153722205,2153722205,975084657,975084657,733485832,733485832,211425557,211425557,688705845,688705845]
 same=same and snapshot['frameChecksums']==baseline
 phase_checks={key: snapshot[key] and synchronous[key] for key in ('staticGraphicsStable','textBlinkAndCursor','rasterPaletteAndMode','animatedGraphicsRefresh','palette255Refresh','displayStartRefresh')}
 sources += [root/'presentation.h',root/'tests/test_presentation.c',root/'pacing.h',root/'tests/test_pacing.c',root/'tests/run_video_tests.py',root/'core/ibmpc/pc.h',root/'core/ibmpc/video.h']
 report={'schema':1,'passed':same and all(phase_checks.values()) and presentation['passed'] and pacing['passed'],'presentation':presentation,'pacing':pacing,'snapshot':snapshot,'synchronous':synchronous,'checks':{**phase_checks,'same_pixels_across_six_bios_modes_and_blink':same,'downscale_matches_original':presentation['passed'],'bounded_fractional_frame_pacing':pacing['passed'],'synchronous_skips_vram_copy':synchronous['vramCopiesPerCapturedFrame']==0},'sourceHashes':{s.relative_to(repo).as_posix():hashlib.sha256(s.read_bytes()).hexdigest() for s in sources},'limitations':['Host timings and simulated scheduling costs are diagnostics, not Tab5 performance predictions. CPU interpretation, audio and physical display are not benchmarked by this test.']}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in report.items() if k!='sourceHashes'},indent=2));raise SystemExit(not report['passed'])
