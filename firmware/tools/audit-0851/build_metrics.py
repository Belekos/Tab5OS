#!/usr/bin/env python3
"""Record measured linked resources without claiming device runtime measurements."""
import argparse, hashlib, json, re, shutil
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p, d): p.write_text(json.dumps(d, indent=2, ensure_ascii=False) + '\n')
def stack_frame(path, name):
    matches = [int(line.split('\t')[1]) for line in path.read_text().splitlines() if line.split('\t')[0].endswith(':' + name)]
    assert len(matches) == 1, (path, name, matches)
    return matches[0]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--environment',type=Path,required=True);a=p.parse_args()
    env=a.environment.resolve(); proof=env/'qa/firmware-verification-0851.json'; data=json.loads(proof.read_text())
    assert data['version']=='0.8.51' and data['passed'] and data['sourcesUnchanged'] and data['buildExitCode']==0
    for name,digest in data['sourceHashes'].items(): assert sha(ROOT/name)==digest, name
    baseline=json.loads((Path(__file__).parent/'build-baseline.json').read_text()); sections=data['memoryLinkChecks']['allSections']; old=baseline['sections']
    metrics={}
    def add(name,before,after):metrics[name]={'before':before,'after':after,'delta':after-before,'unit':'bytes'}
    add('applicationBin',baseline['firmwareBytes'],data['firmwareBytes']);add('otaFree',baseline['otaFreeBytes'],data['otaFreeBytes'])
    names=('.dram0.data','.dram1.data','.dram0.bss','.dram1.bss');add('staticDataBss',sum(old[n] for n in names),sum(sections.get(n,0) for n in names))
    for n in ('.iram0.text','.tcm.text','.tcm.data','.rtc_noinit'):add(n,old[n],sections.get(n,0))
    stack_name='ie_job_step';add('ieJobStepGccFrame',stack_frame(env/'warning-baseline/Tab5ImageCore.c.su',stack_name),stack_frame(env/'build/sketch/Tab5ImageCore.c.su',stack_name))
    stack_evidence={}
    for name,source in [('core-stack-baseline-0850.su',env/'warning-baseline/Tab5ImageCore.c.su'),('core-stack-current-0851.su',env/'build/sketch/Tab5ImageCore.c.su')]:
        target=ROOT/'tools/audit-0851'/name;shutil.copy2(source,target);stack_evidence[str(target.relative_to(ROOT))]=sha(target)
    local=ROOT/'tools/verification/firmware-verification-0851.json';shutil.copy2(proof,local)
    wanted=('Tab5ImageCore.c','Tab5ImageEditor.cpp','Tab5Help.cpp','image_core.c','image_core.h','image_selection.h')
    result={'version':'0.8.51','baselineVersion':'0.8.50','passed':True,'physicalDeviceTested':False,'scope':'Actual complete ESP32-P4 linked firmware sections and GCC per-function static frame estimates, compared with authenticated 0.8.50 build.', 'measurements':metrics,'otaPartitionBytes':data['otaPartitionBytes'],'partitionsUnchanged':data['partitionsUnchangedFromBaseline'],'objectFiles':data['objectFiles'],'sourceFilesChecked':data['sourceFilesChecked'],'sourceHashes':{n:h for n,h in data['sourceHashes'].items() if Path(n).name in wanted or n.endswith('.ino')},'testSourceHashes':{str(Path(__file__).relative_to(ROOT)):sha(Path(__file__))},'evidenceHashes':{str(local.relative_to(ROOT)):sha(local),'tools/audit-0851/build-baseline.json':sha(Path(__file__).parent/'build-baseline.json'),**stack_evidence},'limitations':['No physical Tab5 test was performed.','Static DATA/BSS and IRAM sections do not measure free runtime SRAM or PSRAM, fragmentation, startup time or frame latency.','GCC per-function frames exclude cumulative caller/callee and interrupt/task stack demand.','Host model and GUI peaks are measured separately and exclude unmodelled OS/device services.']}
    write(ROOT/'tools/verification/build-resources-0851.json',result);print(json.dumps(metrics,indent=2))
if __name__=='__main__': main()
