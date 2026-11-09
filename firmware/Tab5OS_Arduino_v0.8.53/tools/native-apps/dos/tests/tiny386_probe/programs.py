"""CC0: original PM32/486 .COM probe generated without an x86 assembler."""

def protected_program():
    code = bytearray(); labels = {}; fixes = []
    def emit(s): code.extend(bytes.fromhex(s))
    def mark(n): labels[n] = len(code) + 0x100
    def ptr(n, add=0, width=2):
        fixes.append((len(code), n, add, width, False)); code.extend(bytes(width))
    def rel(n):
        fixes.append((len(code), n, 0, 1, True)); code.append(0)
    emit('0e 1f 8c 16'); ptr('saved_ss')
    emit('89 26'); ptr('saved_sp')
    emit('9c 58 a3'); ptr('saved_flags')
    emit('8c c8 a3'); ptr('real_cs')
    emit('66 31 c0 8c c8 66 c1 e0 04 66 89 c3 66 05'); ptr('gdt', width=4)
    emit('66 a3'); ptr('gdtr', 2)
    emit('89 1e'); ptr('gdt', 10)
    emit('89 1e'); ptr('gdt', 26)
    emit('66 c1 eb 10 88 1e'); ptr('gdt', 12)
    emit('88 1e'); ptr('gdt', 28)
    emit('fa 0f 01 16'); ptr('gdtr')
    emit('e4 92 0c 02 e6 92 0f 20 c0 66 83 c8 01 0f 22 c0 66 ea'); ptr('pm32', width=4); emit('08 00')
    mark('pm32')
    emit('66 b8 10 00 8e d8 8e c0 8e d0 bc 00 f0 07 00 31 ed')
    emit('b8 78 56 34 12 0f c8 bb 02 00 00 00 0f c1 d8 3d 14 34 56 78 75'); rel('failure')
    emit('89 c1 ba ef be ad de 0f b1 d1 81 f9 ef be ad de 75'); rel('failure')
    # Distinct low/high memory verifies A20 and >1 MiB addressing. Restore low RAM.
    emit('8b 3d 00 00 07 00 c7 05 00 00 07 00 78 56 34 12 c7 05 00 00 17 00 ef be ad de')
    emit('81 3d 00 00 07 00 78 56 34 12 75'); rel('restore_fail')
    emit('81 3d 00 00 17 00 ef be ad de 75'); rel('restore_fail')
    emit('89 3d 00 00 07 00 eb'); rel('leave_pm')
    mark('restore_fail'); emit('89 3d 00 00 07 00')
    mark('failure'); emit('bd 01 00 00 00')
    mark('leave_pm'); emit('ea'); ptr('pm16', width=4); emit('18 00')
    mark('pm16')
    emit('b8 20 00 8e d8 8e c0 8e d0 0f 20 c0 66 83 e0 fe 0f 22 c0 ea'); ptr('real')
    mark('real_cs'); emit('00 00')
    # Do not use PUSH/POP before restoring the original SS:SP: the temporary
    # protected-mode stack descriptor still has base zero at this point.
    mark('real'); emit('8c c8 8e d8 8e c0 8b 16'); ptr('saved_ss'); emit('8e d2 66 31 e4 8b 26'); ptr('saved_sp')
    emit('ff 36'); ptr('saved_flags'); emit('9d 85 ed 75'); rel('dos_fail')
    emit('ba'); ptr('success'); emit('b4 09 cd 21 b8 00 4c cd 21')
    mark('dos_fail'); emit('ba'); ptr('failed'); emit('b4 09 cd 21 b8 01 4c cd 21')
    mark('saved_ss'); emit('00 00'); mark('saved_sp'); emit('00 00'); mark('saved_flags'); emit('00 00')
    mark('gdtr'); emit('27 00 00 00 00 00')
    mark('gdt')
    emit('00 00 00 00 00 00 00 00')
    emit('ff ff 00 00 00 9a 40 00') # program-base code32, 64 KiB
    emit('ff ff 00 00 00 92 cf 00') # flat data32, 4 GiB
    emit('ff ff 00 00 00 9a 00 00') # program-base code16, 64 KiB
    emit('ff ff 00 00 00 92 00 00') # base-zero data16, 64 KiB
    mark('success'); code.extend(b'TAB5 PM32 RETURN OK\r\n$')
    mark('failed'); code.extend(b'TAB5 PM32 DOS FAIL\r\n$')
    for pos, name, add, width, relative in fixes:
        value = labels[name] + add
        if relative:
            value -= pos + 0x101
            assert -128 <= value < 128, (name, value)
        code[pos:pos+width] = value.to_bytes(width, 'little', signed=relative)

    return bytes(code)
