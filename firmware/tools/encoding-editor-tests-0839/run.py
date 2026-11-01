from pathlib import Path
import datetime,hashlib,json,os,subprocess,tempfile
here=Path(__file__).resolve().parent
release=here.parent.parent
sketch=release/'Tab5OS_Arduino_v0.8.39_Ecriture'
build=Path(tempfile.mkdtemp(prefix='tab5-encoding-0839-'))
proof=release/'tools/verification/encoding-editor-0839';proof.mkdir(parents=True,exist_ok=True)
production=['WriterEncoding.cpp','WriterEncoding.h','WriterDocument.cpp','WriterDocument.h','WriterStorage.cpp','WriterStorage.h','WriterJson.cpp','WriterJson.h','WriterSyntax.cpp','WriterSyntax.h','WriterRich.cpp','WriterRich.h','WriterAllocator.h','Tab5Writing.cpp','Tab5Writing.h']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
hashes={str((sketch/n).relative_to(release)):sha(sketch/n) for n in production}
testHashes={str(p.relative_to(release)):sha(p) for p in here.glob('*') if p.is_file()}
cmd=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(sketch),str(here/'test.cpp'),*[str(sketch/n) for n in ['WriterEncoding.cpp','WriterDocument.cpp','WriterStorage.cpp','WriterJson.cpp','WriterSyntax.cpp','WriterRich.cpp']],'-o',str(build/'test')]
r=subprocess.run(cmd,capture_output=True,text=True);(proof/'build.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr
r=subprocess.run([str(build/'test')],capture_output=True,text=True,timeout=120,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:abort_on_error=1'})
(proof/'run.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr)
# Integration guards cover UI wiring; the full firmware build validates types and linkage.
s=(sketch/'Tab5Writing.cpp').read_text()
integration={
'chooser_for_invalid_auto_input':'encodingDialog(file,ref,project)' in s,
'explicit_reopen_valid_ambiguous_input':'TextReopenEncoding' in s and 'path.length() && !document.dirty()' in s,
'code_creation':'case CodeNewC: create("/Projects",true,".c")' in s and 'case CodeNewJson: create("/Documents",true,".json")' in s,
'json_validation':'writing::validateJson(document)' in s,
'python_only_execution':'if (!writing::pythonFile(syntaxPath()))' in s,
'no_corrupted_non_utf8_print':'document.encoding()!=writing::TextEncoding::Utf8' in s,
'chooser_can_display_after_explorer_open':'return loaded || impl_->encodingOpenPath.length()' in s,
'output_encoding_control':'document.setEncoding(encoding)' in s,
}
stable=hashes=={str((sketch/n).relative_to(release)):sha(sketch/n) for n in production}
report=dict(version='0.8.39',passed=r.returncode==0 and stable and all(integration.values()),physicalDeviceTested=False,sourceStableDuringTests=stable,checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),sourceHashes=hashes,testSourceHashes=testHashes,evidenceHashes={str(p.relative_to(release)):sha(p) for p in proof.glob('*') if p.is_file()},integrationChecks=integration,method='Production streaming codecs, Document, transactional Storage, JSON validator, RichText and Syntax execute unchanged under ASan/UBSan; filesystem is an explicit fault-injection double. Writer UI wiring is statically checked; full firmware build separately validates integration.',limitations=['No physical SD/USB test.','Windows ANSI support is explicitly Windows-1252, not every Windows codepage.','UTF-32 and undefined CP1252 bytes are refused.','Print pipeline still requires a UTF-8 copy for other encodings.','LeakSanitizer is disabled because this sandbox cannot inspect /proc threads; AddressSanitizer and UBSan remain enabled.'])
if r.returncode==0:report['checks']=json.loads(r.stdout.strip())['checks']
(release/'tools/verification/encoding-editor-0839.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
raise SystemExit(0 if report['passed'] else 1)
