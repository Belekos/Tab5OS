from pathlib import Path
import hashlib,json,subprocess
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.28_Ecriture'
build=Path('C:/workspace/tab5-git-target-0828');build.mkdir(exist_ok=True)
database=Path('C:/workspace/tab5-build-0827-clang/compile_commands.json');commands=json.loads(database.read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();results=[]
for name in ['Tab5Git.cpp','Tab5GitPort.cpp','Tab5GitView.cpp','Tab5Focus.cpp']:
    row=next(r for r in commands if Path(r['file']).name==name)
    args=[arg.replace('Tab5OS_Ecriture_v0.8.27','Tab5OS_Ecriture_v0.8.28').replace('Tab5OS_Arduino_v0.8.27','Tab5OS_Arduino_v0.8.28') for arg in row['arguments']]
    args[args.index(row['file'])]=str(sketch/name);args[-1]=str(build/(name+'.o'))
    result=subprocess.run(args,capture_output=True,text=True,errors='replace');print(name,result.returncode,result.stdout+result.stderr)
    frames=[]
    if not result.returncode:
        for line in (build/(name+'.su')).read_text().splitlines():
            parts=line.split('\t')
            if len(parts)==3:frames.append(dict(symbol=parts[0].rsplit(':',1)[-1],bytes=int(parts[1]),kind=parts[2]))
    results.append(dict(source=name,exitCode=result.returncode,frames=frames,arguments=args,diagnostic=result.stdout+result.stderr))
report=dict(version='0.8.28',passed=all(r['exitCode']==0 for r in results),physicalDeviceTested=False,wholeStackBound=False,method='Actual ESP32-P4 Clang20.1.1 compilation with SDK3.3.9 flags and -fstack-usage. UI entry frame sizes are measured static frames, not a total call-chain bound or physical free-stack measurement.',results=results,sources={n:sha(sketch/n) for n in ['Tab5Git.cpp','Tab5Git.h','Tab5GitPort.cpp','Tab5GitPort.h','Tab5GitView.cpp','Tab5GitView.h','Tab5Focus.cpp','Tab5Focus.h']},tests={Path(__file__).name:sha(Path(__file__))})
(release/'tools/verification/git-target-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
