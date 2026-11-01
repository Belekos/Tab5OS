#!/usr/bin/env python3
"""Reconcile only unrelated source edits; refuse any changed executed code."""
from pathlib import Path
import hashlib,json
from extract import extract,closing
r=Path(__file__).resolve().parents[2];p=r/'tools/verification/root-integration-0843.json';d=json.loads(p.read_text());s=r/'Tab5OS_Arduino_v0.8.43_Ecriture';m=s/(s.name+'.ino');source=m.read_text();audio=(s/'Tab5Audio.cpp').read_text()
def h(s):return hashlib.sha256(s.encode()).hexdigest()
def block(text,start):
 a=text.index(start);return text[a:closing(text,text.index('{',a))]
for name,digest in d['after']['functionHashes'].items():assert h(extract(source,name))==digest,name
for name,digest in d['after']['audioMethods'].items():assert h(block(audio,'bool Tab5AudioEngine::'+name+'('))==digest,name
line=source.index('toolsHooks.runExecutable=');start=source.index('[]',line);end=closing(source,source.index('{',start));assert h('static auto runExecutable='+source[start:end]+';')==d['after']['hookSha256']
header=(s/'Tab5Audio.h').read_text();structs=block(header,'enum class Tab5AudioMode')+';\n'+block(header,'struct Tab5AudioSnapshot')+';'
assert h(structs)==d['after']['audioStructsSha256']
assert hashlib.sha256((s/'Tab5ImageFormats.h').read_bytes()).hexdigest()==d['imageFormatsSha256']
current=h(source);old=d['after']['mainSha256'];aCurrent=h(audio);aOld=d['after']['audioSha256']
if current!=old or aCurrent!=aOld:d.setdefault('integrationReviews',[]).append({'oldMainSha256':old,'currentMainSha256':current,'oldAudioSha256':aOld,'currentAudioSha256':aCurrent,'executedMainFunctionsUnchanged':len(d['after']['functionHashes']),'executedAudioMethodsUnchanged':len(d['after']['audioMethods']),'hookUnchanged':True,'method':'Exact function/hook byte comparison, no reuse if executed code changes; unrelated edits are not asserted executed on host.'})
d['after']['mainSha256']=current;d['after']['audioSha256']=aCurrent
d['testSourceHashes']={str(x.relative_to(r)):hashlib.sha256(x.read_bytes()).hexdigest() for x in Path(__file__).parent.glob('*') if x.is_file()}
p.write_text(json.dumps(d,indent=2)+'\n');print(json.dumps({'passed':True,'testedMainFunctions':len(d['after']['functionHashes']),'testedAudioMethods':len(d['after']['audioMethods']),'mainSha256':current}))
