#!/usr/bin/env python3
"""Revalidate exact tested UI excerpts when only unrelated main-sketch sections changed."""
from pathlib import Path
import datetime,hashlib,importlib.util,json
root=Path(__file__).resolve().parents[2]
sketch=root/'Tab5OS_Arduino_v0.8.42_Ecriture';main=sketch/(sketch.name+'.ino');text=main.read_text();digest=hashlib.sha256(main.read_bytes()).hexdigest();rel=str(main.relative_to(root))
spec=importlib.util.spec_from_file_location('settings_extract',root/'tools/settings-ui-tests-0842/extract.py');extractor=importlib.util.module_from_spec(spec);spec.loader.exec_module(extractor)
updates=[]
for name in ['maps-input-0842','settings-ui-0842']:
 file=root/'tools/verification'/(name+'.json');report=json.loads(file.read_text());assert report['passed']
 if name=='maps-input-0842':
  start=text.index('static bool imageMouseEvent(int x,int y,uint8_t buttons,int wheel) {');end=text.index('\n}\n',start)+3
  assert hashlib.sha256(text[start:end].encode()).hexdigest()==report['extractedRouteSha256']
  start=text.index('  Tab5MapWheel::View nextWheelView;');end=text.index('  // La tuile',start)
  assert hashlib.sha256(text[start:end].encode()).hexdigest()==report['extractedServiceSha256']
 else:
  for function,expected in report['mainFunctionHashes'].items():
   source=(sketch/'Tab5GamepadSettings.inc').read_text() if function=='createGamepadSettings' else (sketch/'Tab5SerialSettings.inc').read_text() if function in ['serialSettingsRefresh','createSerialSettings'] else text
   assert hashlib.sha256(extractor.extract(source,function).encode()).hexdigest()==expected,function
 for group in ['sourceHashes','testSourceHashes','integrationSourceHashes']:
  for other,expected in report.get(group,{}).items():
   if other!=rel:assert hashlib.sha256((root/other).read_bytes()).hexdigest()==expected,(name,group,other)
 old={}
 for group in ['sourceHashes','integrationSourceHashes']:
  if rel in report.get(group,{}):old[group]=report[group][rel];report[group][rel]=digest
 report['mainIntegrationRevalidation']={'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'previousMainHashes':old,'currentMainSha256':digest,'exactExecutedExcerptsUnchanged':True,'fullMainExecutedOnHost':False,'method':__doc__,'reviewToolSha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
 updates.append((file,report))
assert text.count('lv_timer_handler()')==2
assert text.count('Tab5DisplayRefresh::service(lvDisplay)')==2
assert 'lv_tick_inc(' not in text and 'lv_tick_set_cb([]()->uint32_t{return millis();});' in text
for file,report in updates:file.write_text(json.dumps(report,indent=2)+'\n')
print('PASS: current main integration and byte-identical tested input/settings excerpts; full main remains target-build validation.')
