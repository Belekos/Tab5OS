"""Current task snapshots and real RTC diagnostics; only SDK boundaries mocked."""
from pathlib import Path
import datetime,hashlib,json,os,subprocess,sys
here=Path(__file__).resolve().parent
release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.27_Ecriture'
build=Path('C:/workspace/tab5-task-diagnostics-0827');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd'
    cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
production=(sketch/'Tab5Memory.cpp').read_text(encoding='utf-8')
production='#ifdef ARDUINO\n'+production[production.index('namespace {'):production.index('Tab5Resources::MemoryTotals tab5MemoryTotals')]
task_fixture=release/'tools/continuity-tests-0826/tasks.cpp'
(build/'tasks.cpp').write_text(task_fixture.read_text(encoding='utf-8').replace('/* DIAGNOSTIC_SOURCE */',production),encoding='utf-8')
stubs=build/'stubs';(stubs/'freertos').mkdir(parents=True,exist_ok=True)
(stubs/'esp_attr.h').write_text('#pragma once\n#define RTC_NOINIT_ATTR\n',encoding='utf-8')
(stubs/'esp_system.h').write_text('#pragma once\nenum esp_reset_reason_t { ESP_RST_UNKNOWN,ESP_RST_POWERON,ESP_RST_EXT,ESP_RST_SW,ESP_RST_PANIC,ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT,ESP_RST_DEEPSLEEP,ESP_RST_BROWNOUT,ESP_RST_SDIO,ESP_RST_USB,ESP_RST_JTAG,ESP_RST_EFUSE,ESP_RST_PWR_GLITCH,ESP_RST_CPU_LOCKUP };\nesp_reset_reason_t esp_reset_reason();\n',encoding='utf-8')
(stubs/'freertos/FreeRTOS.h').write_text('#pragma once\n',encoding='utf-8')
(stubs/'freertos/portmacro.h').write_text('#pragma once\n#include <mutex>\nusing portMUX_TYPE=std::mutex;\n#define portMUX_INITIALIZER_UNLOCKED {}\n#define portENTER_CRITICAL(p) ((p)->lock())\n#define portEXIT_CRITICAL(p) ((p)->unlock())\n',encoding='utf-8')
results=[]
for name,source in [('tasks',build/'tasks.cpp'),('rtc',here/'rtc.cpp')]:
    compile=subprocess.run(['cl','/nologo','/utf-8','/EHsc','/std:c++17','/D_CRT_SECURE_NO_WARNINGS','/I'+str(stubs),'/I'+str(sketch),str(source),'/Fe:'+str(build/(name+'.exe'))],cwd=build,capture_output=True,encoding='utf-8',errors='replace')
    if compile.returncode:
        print(compile.stdout+compile.stderr);raise SystemExit(compile.returncode)
    run=subprocess.run([str(build/(name+'.exe'))],capture_output=True,encoding='utf-8',errors='replace',timeout=60)
    print(run.stdout+run.stderr,flush=True)
    results.append(dict(name=name,exitCode=run.returncode,output=run.stdout+run.stderr))
report=dict(version='0.8.27',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=all(r['exitCode']==0 for r in results),output=''.join(r['output'] for r in results),results=results,method='Actual production task snapshot and PSRAM registry, plus full Tab5Diagnostics.cpp RTC record implementation. SDK task status/allocator/reset-reason and critical-section boundaries mocked. Warm/cold reset, invalidated old RTC format, every record byte corruption, no-change writes, seven-slot capacity and vault detail exclusion. UI/worker marker ownership is tested separately; not a hardware reset reproduction.',physicalDeviceTested=False,sources={n:hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in ['Tab5Memory.cpp','Tab5Memory.h','Tab5Task.h','Tab5ResourceManager.cpp','Tab5Diagnostics.cpp','Tab5Diagnostics.h']},tests={str(p.relative_to(release)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),here/'rtc.cpp',task_fixture]},rtcRecordBytes=968,rtcFormat=4,rtcSlots=7)
(release/'tools/verification/task-diagnostics-0827.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
