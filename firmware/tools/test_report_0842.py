"""Attach reproducible test sources and execution output to a successful test run."""
from pathlib import Path
import hashlib,json

def save_report(report,path,here,output):
    path=Path(path);here=Path(here);release=here.parent.parent
    digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    report['passed']=True  # Reached only after checked compiler/executable return codes.
    report.setdefault('sourceHashes',{'Tab5OS_Arduino_v0.8.42_Ecriture/'+n:h for n,h in report.get('sources',{}).items()})
    scripts=[p for p in here.rglob('*') if p.is_file() and p.suffix in ('.py','.cpp','.h')]
    scripts.append(Path(__file__).resolve())
    report['testSourceHashes']={str(p.relative_to(release)):digest(p) for p in scripts}
    log=path.with_suffix('.log');log.write_text(output)
    report['evidenceHashes']={str(log.relative_to(release)):digest(log)}
    path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
