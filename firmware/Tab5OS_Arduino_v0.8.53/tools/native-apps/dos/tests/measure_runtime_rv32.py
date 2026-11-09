#!/usr/bin/env python3
"""Count actual RV32 instructions for old/new DOS primitives (not host timing)."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

import unicorn as u
from unicorn import riscv_const as r

HERE = Path(__file__).resolve().parent
RUNTIME = HERE.parents[1] / 'common/runtime.c'
PIT = HERE.parent / 'core/ibmpc/pit.c'
REPO = HERE.parents[5]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--toolchain', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = RUNTIME.read_text(encoding='utf-8')


def region(label):
    start = source.index('/* ' + label + '_BEGIN')
    end = source.index('/* ' + label + '_END */', start)
    return source[start:end]


def function(text, signature):
    start = text.index(signature)
    position = text.index('{', start)
    depth = 1
    while depth:
        position += 1
        if text[position] == '{':
            depth += 1
        elif text[position] == '}':
            depth -= 1
    return text[start:position + 1]


pit_state = '''#include <stdint.h>
#include <stdbool.h>
typedef struct {uint64_t start;uint32_t reload;bool counting,gate,frozen;int mode;} chan_t;
static chan_t s_ch[3];
static bool out_at(const chan_t *c,uint64_t t){(void)c;(void)t;return false;}
'''
# Only mode 3 is benchmarked. Its former expression is preserved literally;
# other modes are verified by the audio functional tests, not this benchmark.
pit_before = '''uint32_t pit_high2(uint64_t a,uint64_t b){
 const chan_t *c=&s_ch[2];
 if(b<=a)return 0;
 if(c->mode==3&&c->counting&&c->gate&&!c->frozen){
  uint32_t r=c->reload,half=(r+1)/2;
  uint64_t sa=a>c->start?a-c->start:0,sb=b>c->start?b-c->start:0;
  uint64_t ha=sa/r*half+((sa%r)<half?sa%r:half);
  uint64_t hb=sb/r*half+((sb%r)<half?sb%r:half);
  uint64_t before=(a<c->start?(c->start<b?c->start:b)-a:0);
  return (uint32_t)(hb-ha+before);
 }
 uint32_t n=0;for(uint64_t t=a;t<b;t++)n+=out_at(c,t);return n;
}
'''
pit_wrapper = '''uint32_t bench_pit(uint64_t a,uint64_t b,uint32_t reload,uint32_t start){
 s_ch[2]=(chan_t){.start=start,.reload=reload,.mode=3,.counting=true,.gate=true};
 return pit_high2(a,b);
}
'''


# These scalar reference implementations are exactly the c08b0fa runtime logic.
# Build them with the same target flags as the new production functions.
baseline = r'''
void* old_memset(void* p,int c,size_t n){unsigned char* d=p;while(n--)*d++=(unsigned char)c;return p;}
void* old_memcpy(void* p,const void* s,size_t n){unsigned char* d=p;const unsigned char* b=s;while(n--)*d++=*b++;return p;}
void* old_memmove(void* p,const void* s,size_t n){if((uintptr_t)p<(uintptr_t)s)return old_memcpy(p,s,n);unsigned char* d=p;const unsigned char* b=s;while(n){--n;d[n]=b[n];}return p;}
unsigned long long old_udiv(unsigned long long n,unsigned long long d){if(!d)abort();unsigned long long q=0,r=0;for(int i=63;i>=0;--i){unsigned top=(unsigned)(r>>63);r=(r<<1)|((n>>i)&1);if(top||r>=d){r-=d;q|=1ULL<<i;}}return q;}
unsigned long long old_umod(unsigned long long a,unsigned long long b){return a-old_udiv(a,b)*b;}
'''
compiled = '''#include <stdint.h>
#include <stddef.h>
void abort(void){__asm__ volatile("ebreak");for(;;){}}
#define TAB5_DOS 1
''' + region('DOS_MEMORY_PRIMITIVES') + '\n' + region('DOS_DIVISION_PRIMITIVES') + baseline

with tempfile.TemporaryDirectory(prefix='tab5-dos-rv32-primitives-') as temporary:
    work = Path(temporary)
    unit, elf, binary = work / 'primitives.c', work / 'primitives.elf', work / 'primitives.bin'
    unit.write_text(compiled, encoding='utf-8')
    suffix = '.exe' if (args.toolchain / 'riscv32-esp-elf-gcc.exe').exists() else ''

    def tool(name):
        return str(args.toolchain / ('riscv32-esp-elf-' + name + suffix))

    flags = ['-march=rv32imafc_zicsr_zifencei', '-mabi=ilp32', '-O3', '-fPIC', '-fvisibility=hidden',
             '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-nostdlib',
             '-fno-unwind-tables', '-fno-asynchronous-unwind-tables']
    objects = []
    for label, body in [('before', pit_before), ('after', function(PIT.read_text(encoding='utf-8'), 'uint32_t pit_high2('))]:
        path, obj = work / ('pit_' + label + '.c'), work / ('pit_' + label + '.o')
        path.write_text(pit_state + body + pit_wrapper, encoding='utf-8')
        subprocess.run([tool('gcc'), *flags, '-Dpit_high2=high_' + label,
                        '-Dbench_pit=pit_' + label, '-c', str(path), '-o', str(obj)], check=True)
        if label == 'before':
            # Match the old PIT's compiler-generated arithmetic calls to the
            # old runtime, instead of accidentally measuring it with new math.
            subprocess.run([tool('objcopy'), '--redefine-sym', '__udivdi3=old_udiv',
                            '--redefine-sym', '__umoddi3=old_umod', str(obj)], check=True)
        objects.append(str(obj))
    subprocess.run([tool('gcc'), *flags, '-Wl,-Ttext=0x100000,-e,memcpy,--build-id=none',
                    str(unit), *objects, '-o', str(elf)], check=True)
    subprocess.run([tool('objcopy'), '-O', 'binary', str(elf), str(binary)], check=True)
    symbols = {}
    for line in subprocess.check_output([tool('nm'), '-n', str(elf)], text=True).splitlines():
        parts = line.split()
        if len(parts) == 3:
            symbols[parts[2]] = int(parts[0], 16)
    code = binary.read_bytes()
    rows = []

    def run(name, registers, memory=None):
        cpu = u.Uc(u.UC_ARCH_RISCV, u.UC_MODE_RISCV32)
        cpu.mem_map(0x80000, 0x1000)
        cpu.mem_map(0x100000, 0x100000)
        cpu.mem_write(0x100000, code)
        cpu.mem_map(0x200000, 0x100000)
        cpu.mem_map(0x300000, 0x100000)
        cpu.mem_map(0x400000, 0x10000)
        cpu.reg_write(r.UC_RISCV_REG_SP, 0x40fff0)
        cpu.reg_write(r.UC_RISCV_REG_RA, 0x80000)
        if '__global_pointer$' in symbols:
            cpu.reg_write(r.UC_RISCV_REG_GP, symbols['__global_pointer$'])
        if memory:
            for address, data in memory:
                cpu.mem_write(address, data)
        for index, value in enumerate(registers):
            cpu.reg_write(r.UC_RISCV_REG_A0 + index, value & 0xffffffff)
        instructions = 0
        block_counts = {}

        def block(uc, address, size, user):
            nonlocal instructions
            key = (address, size)
            if key not in block_counts:
                data = uc.mem_read(address, size)
                cursor = count = 0
                while cursor < len(data):
                    cursor += 4 if data[cursor] & 3 == 3 else 2
                    count += 1
                block_counts[key] = count
            instructions += block_counts[key]

        cpu.hook_add(u.UC_HOOK_BLOCK, block)
        cpu.emu_start(symbols[name], 0x80000, timeout=30_000_000, count=20_000_000)
        if cpu.reg_read(r.UC_RISCV_REG_PC) != 0x80000:
            raise RuntimeError(name + ' did not return within the execution budget')
        value = cpu.reg_read(r.UC_RISCV_REG_A0) | cpu.reg_read(r.UC_RISCV_REG_A1) << 32
        return instructions, value, cpu

    for n, d in [(32767, 1193), (64, 65536), (0xffffffff, 257), (1 << 45, 65535),
                 ((1 << 64) - 1, (1 << 63) + 1)]:
        regs = (n, n >> 32, d, d >> 32)
        for operator, old, new, answer in [('divide', 'old_udiv', '__udivdi3', n // d),
                                           ('remainder', 'old_umod', '__umoddi3', n % d)]:
            before, a, _ = run(old, regs)
            after, b, _ = run(new, regs)
            assert a == b == answer, (operator, n, d, a, b, answer)
            rows.append({'case': operator, 'numerator': n, 'denominator': d,
                         'beforeInstructions': before, 'afterInstructions': after})

    for start, a, span, reload in [(65536, 65536 + 2300, 27, 1193),
                                    (65536, 65536 + 2370, 54, 1193),
                                    (65536, 65536 + 55, 54, 36),
                                    (65536, (1 << 40) + 55, 54, 1193)]:
        b = a + span
        regs = (a, a >> 32, b, b >> 32, reload, start)
        before, old, _ = run('pit_before', regs)
        after, new, _ = run('pit_after', regs)
        half = (reload + 1) // 2
        def integral(t):
            t = max(t - start, 0)
            return t // reload * half + min(t % reload, half)
        expected = integral(b) - integral(a) + max(min(start, b) - a, 0)
        assert old & 0xffffffff == new & 0xffffffff == expected & 0xffffffff
        rows.append({'case': 'pit-square-wave-sample', 'start': start, 'a': a,
                     'span': span, 'reload': reload, 'beforeInstructions': before, 'afterInstructions': after})

    count = 128 * 1024
    data = bytes((i * 31 + i // 7) & 255 for i in range(count + 64))
    for name, old, new, regs, initial, output_address, expected in [
            ('aligned-copy', 'old_memcpy', 'memcpy', (0x300000, 0x200000, count),
             [(0x200000, data)], 0x300000, data[:count]),
            ('same-unaligned-copy', 'old_memcpy', 'memcpy', (0x300001, 0x200001, count),
             [(0x200000, data)], 0x300001, data[1:count + 1]),
            ('different-alignment-copy', 'old_memcpy', 'memcpy', (0x300001, 0x200002, count),
             [(0x200000, data)], 0x300001, data[2:count + 2]),
            ('aligned-fill', 'old_memset', 'memset', (0x300000, 0x5b, count),
             [], 0x300000, b'\x5b' * count),
            ('backward-overlap', 'old_memmove', 'memmove', (0x200004, 0x200000, count),
             [(0x200000, data)], 0x200004, data[:count])]:
        before, _, a = run(old, regs, initial)
        after, _, b = run(new, regs, initial)
        assert bytes(a.mem_read(output_address, count)) == expected
        assert bytes(b.mem_read(output_address, count)) == expected
        rows.append({'case': name, 'bytes': count, 'beforeInstructions': before, 'afterInstructions': after})

report = {'component': 'dos-runtime-rv32-instructions', 'passed': True,
          'baselineCommit': 'c08b0facb6db1c6bc136299444de13bbe81a4648',
          'rows': rows,
          'sourceHashes': {p.relative_to(REPO).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in [RUNTIME, PIT, Path(__file__)]},
          'limitations': ['Instruction counts from Unicorn are not CPU cycles, PSRAM timing or a game FPS prediction.']}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
for row in rows:
    print(row['case'], row['beforeInstructions'], '->', row['afterInstructions'], 'RV32 instructions')
