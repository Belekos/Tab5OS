#!/usr/bin/env python3
"""Independent DWARF checks against the actual P4 heap archive, without running the probe."""
import argparse,hashlib,json,re,subprocess,tempfile
from pathlib import Path

parser=argparse.ArgumentParser();parser.add_argument('--sdk',type=Path,required=True)
parser.add_argument('--ar',type=Path,required=True);parser.add_argument('--readelf',default='readelf')
parser.add_argument('--report',type=Path,required=True);args=parser.parse_args()
here=Path(__file__).resolve().parent;release=here.parents[1]
archive=args.sdk/'lib/libheap.a'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def structures(text):
    result={}
    blocks=re.split(r'(?=^ <1><[0-9a-f]+>:)',text,flags=re.M)
    for block in blocks:
        if 'DW_TAG_structure_type' not in block.split('\n',1)[0]:continue
        head=block.split(' <2><',1)[0]
        name=re.search(r'DW_AT_name\s*:\s*(?:\(indirect string, offset: [^)]+\): )?(\w+)',head)
        size=re.search(r'DW_AT_byte_size\s*:\s*(\d+)',head)
        if not name or not size:continue
        members={}
        for member in re.split(r'(?=^ <2><[0-9a-f]+>:)',block,flags=re.M)[1:]:
            if 'DW_TAG_member' not in member.split('\n',1)[0]:continue
            key=re.search(r'DW_AT_name\s*:\s*(?:\(indirect string, offset: [^)]+\): )?(\w+)',member)
            offset=re.search(r'DW_AT_data_member_location:\s*(\d+)',member)
            if key and offset:members[key[1]]=int(offset[1])
        result[name[1]]={'bytes':int(size[1]),'members':members,'evidence':block}
    return result

expected={
 'heap_caps_base.c.obj':{'heap_t_':(36,{'caps':0,'start':12,'end':16,'heap_mux':20,'heap':28,'next':32})},
 'multi_heap.c.obj':{'multi_heap_info':(20,{'lock':0,'free_bytes':4,'minimum_free_bytes':8,'pool_size':12,'heap_data':16})},
 'tlsf.c.obj':{'block_header_t':(16,{'prev_phys_block':0,'size':4,'next_free':8,'prev_free':12}),'control_t':(36,{})}}
proof={};excerpts=[]
with tempfile.TemporaryDirectory(prefix='tab5-heap-abi-review-') as temporary:
    for member,types in expected.items():
        file=Path(temporary)/member
        file.write_bytes(subprocess.check_output([str(args.ar),'p',str(archive),member]))
        text=subprocess.check_output([args.readelf,'--debug-dump=info',str(file)],text=True)
        parsed=structures(text)
        for name,(size,members) in types.items():
            actual=parsed[name];assert actual['bytes']==size,(name,actual)
            for key,offset in members.items():assert actual['members'][key]==offset,(name,key,actual)
            excerpts.append(member+'\n'+actual.pop('evidence'))
            proof[name]=actual

hashes={str(archive):sha(archive)}
for name in ['tlsf_control_functions.h','tlsf_block_functions.h']:
    sdk=args.sdk/'include/heap/tlsf'/name
    upstream=here/'heap-walk/upstream'/name
    assert sdk.read_bytes()==upstream.read_bytes(),name+' differs from actual SDK'
    hashes[str(sdk)]=sha(sdk)
baseline=json.loads((here/'sdk-heap-critical-proof.json').read_text())
assert sha(archive)==baseline['sdkArchiveSha256'],'Heap archive differs from audited baseline'
args.report.parent.mkdir(parents=True,exist_ok=True)
args.report.with_suffix('.txt').write_text('\n\n'.join(excerpts))
args.report.write_text(json.dumps({'passed':True,'physicalDeviceTested':False,'nativeStructures':proof,'inputHashes':hashes,
    'scriptSha256':sha(Path(__file__)),'limits':['DWARF validates actual archived ABI; it does not execute the P4 probe.',
       'Host TLSF differential tests and target object compilation are separate validations.']},indent=2)+'\n')
print(json.dumps({'passed':True,'nativeStructures':list(proof),'report':str(args.report)}))
