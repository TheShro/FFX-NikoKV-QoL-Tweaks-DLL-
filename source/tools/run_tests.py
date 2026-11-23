#!/usr/bin/env python3
"""Execute the SHIPPED cave bytes natively (32-bit, no libc) against simulated game memory."""
import sys, os, struct, subprocess, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_caves as g

# fixed test addresses (section .mydata is linked at 0x0A000000)
TEST = {'A_AREA':0x0A000000,'A_STREAK':0x0A000004,'A_LGFLAG':0x0A000008,'A_SCHEME':0x0A00000C,'A_ROUND':0x0A000010,
        'A_ARRAY':0x0A000200,'A_SLOT0':0x0A000100,'A_SLOT1':0x0A000101,'A_SLOT2':0x0A000102,'A_SLOT3':0x0A000103,
        'A_SLOT4':0x0A000104,'A_SLOT5':0x0A000105,'A_MEP':0x0A000110,'A_NIM':0x0A000111,'A_ZAL':0x0A000112}

def relocated(inc):
    code, _ = g.assemble(inc, g.PLACEHOLDERS)          # exactly what caves.h ships
    code = bytearray(code)
    for pos, which in g.relocs(bytes(code)):
        code[pos:pos+4] = struct.pack('<I', TEST[g.WHICH[which]])
    return bytes(code)

dodger = relocated('dodger.inc'); league = relocated('league.inc'); choco = relocated('chocobo.inc')
lvl = relocated('blitzlvl.inc'); learn = relocated('blitzlearn.inc')
def byts(b): return '\n'.join('    .byte ' + ','.join('0x%02X'%x for x in b[i:i+16]) for i in range(0,len(b),16))

# (name, area, sig, ax, streak, counter_in, expect)
D = [
 ('South, fail (ax=0) is ignored',      0x8C,0xDD2C,0,7,3,3),
 ('South, dodge, streak==counter',      0x8C,0xDD2C,5,7,7,8),
 ('South, dodge, restores streak 7',    0x8C,0xDD2C,5,7,3,8),
 ('North, fail (ax=0) is ignored',      0xA2,0x1078,0,2,9,9),
 ('North, dodge, streak==counter',      0xA2,0x1078,5,2,2,3),
 ('North, dodge, counter 0 -> streak 2',0xA2,0x1078,5,2,0,3),
 ('South, wrong signature -> original', 0x8C,0x1078,9,7,3,9),
 ('other area -> original',             0x50,0xDD2C,9,7,3,9),
 ('North, wrong signature -> original', 0xA2,0xDD2C,4,2,3,4),
 ('word-size ops keep the high word',   0x8C,0xDD2C,5,7,0xABCD0007,0xABCD0008),
]
# (name, area, flag, sel(1=round var), al, expect dword at the written location)
L = [
 ('round 5 -> 10',                      0x15B,0,1,5,0xEEEEEE0A),
 ('round 4 untouched',                  0x15B,0,1,4,0xEEEEEE04),
 ('flag set -> original',               0x15B,1,1,5,0xEEEEEE05),
 ('other area -> original',             0x100,0,1,5,0xEEEEEE05),
 ('different address -> original',      0x15B,0,0,5,0xEEEEEE05),
 ('round 6 untouched',                  0x15B,0,1,6,0xEEEEEE06),
 ('writes of 0 pass through',           0x15B,0,1,0,0xEEEEEE00),
]
# (name, area, eax, ball47_initial, scheme, expect47, expect4A)
C = [
 ('preset 1: Tidus balls +3',           0xDF,100,0,0,3,0),
 ('preset 2: Tidus balls +4',           0xDF,100,0,1,4,0),
 ('preset 3: +30 / +30',                0xDF,100,0,2,30,30),
 ('invalid preset adds nothing',        0xDF,100,0,3,0,0),
 ('other area adds nothing',            0x50,100,0,0,0,0),
 ('value != 100 adds nothing',          0xDF,99, 0,0,0,0),
 ('balls already set: untouched',       0xDF,100,5,0,5,0),
]
ARR = 0x0A000200
M32 = 0xFFFFFFFF
# (name, area, flag, ecx address, ebx level, expect eax)      slots hold the player ids 5..10
V = [
 ('LF slot (index 5) +10',                     0x3E,2,ARR+5,20,30),
 ('MF slot (index 7) +10',                     0x3E,2,ARR+7,20,30),
 ('GL slot (index 10) +10',                    0x3E,2,ARR+10,20,30),
 ('non-member (index 11)',                     0x3E,2,ARR+11,20,20),
 ('non-member (index 0)',                      0x3E,2,ARR+0,20,20),
 ('wrong area',                                0x3F,2,ARR+5,20,20),
 ('not an Exhibition match',                   0x3E,1,ARR+5,20,20),
 ('al matches but index 0x105 (upper bound)',  0x3E,2,ARR+0x105,20,20),
 ('al matches but index below array (-251)',   0x3E,2,(ARR-251)&M32,20,20),
 ('level 99 +10 (no clamp here, as original)', 0x3E,2,ARR+6,99,109),
]
# (name, mep, nim, zal initial, expect mep, nim, zal)
N = [
 ('sets the three skill bits, keeps others',   0x10,0x20,0x01,0x11,0x21,0x05),
 ('already set stays set',                     0x01,0x01,0x04,0x01,0x01,0x04),
 ('all other bits preserved',                  0xFE,0xFE,0xFB,0xFF,0xFF,0xFF),
]
rows = lambda tbl: ''.join('    .long ' + ','.join('0x%X'%v for v in r[1:]) + '\n' for r in tbl)

asm = f'''.intel_syntax noprefix
.set AREA,0x0A000000
.set STREAK,0x0A000004
.set LGFLAG,0x0A000008
.set SCHEME,0x0A00000C
.set ROUND,0x0A000010
.set OTHER,0x0A000014
.set BUF,0x0A000020
.set RESULTS,0x0A0000C0
.set SLOT0,0x0A000100
.set MEP,0x0A000110
.set NIM,0x0A000111
.set ZAL,0x0A000112
.set WORDV,0x0A000118
.section .mydata,"aw",@progbits
.space 0x400
.section .rodata
dcases:
{rows(D)}    .long 0xFFFFFFFF,0,0,0,0,0
lcases:
{rows(L)}    .long 0xFFFFFFFF,0,0,0,0
casesC:
{rows(C)}    .long 0xFFFFFFFF,0,0,0,0,0
casesV:
{rows(V)}    .long 0xFFFFFFFF,0,0,0,0
casesN:
{rows(N)}    .long 0xFFFFFFFF,0,0,0,0,0,0
.text
.globl _start
_start:
    xor edi, edi
    mov esi, offset dcases
d_loop:
    cmp dword ptr [esi], 0xFFFFFFFF
    je  d_done
    mov eax, [esi]
    mov word ptr [AREA], ax
    mov eax, [esi+4]
    mov word ptr [BUF], ax
    mov eax, [esi+12]
    mov dword ptr [STREAK], eax
    mov eax, [esi+16]
    mov dword ptr [BUF+0x26], eax
    mov edx, offset BUF+0x26
    mov ecx, 0x11223344
    mov eax, [esi+8]
    push offset d_ret
    push 0x1234
    jmp prod_dodger
d_ret:
    mov bl, 0x50
    cmp ebp, 0x1234
    jne d_fail
    cmp ecx, 0x11223344
    jne d_fail
    mov eax, dword ptr [BUF+0x26]
    cmp eax, [esi+20]
    jne d_fail
    jmp d_store
d_fail:
    mov bl, 0x46
d_store:
    mov [RESULTS+edi], bl
    inc edi
    add esi, 24
    jmp d_loop
d_done:
    mov esi, offset lcases
l_loop:
    cmp dword ptr [esi], 0xFFFFFFFF
    je  l_done
    mov eax, [esi]
    mov word ptr [AREA], ax
    mov eax, [esi+4]
    mov byte ptr [LGFLAG], al
    mov dword ptr [ROUND], 0xEEEEEE77
    mov dword ptr [OTHER], 0xEEEEEE77
    mov edx, offset ROUND
    cmp dword ptr [esi+8], 0
    jne l_go
    mov edx, offset OTHER
l_go:
    mov ecx, 0x11223344
    mov eax, [esi+12]
    push offset l_ret
    push 0x1234
    jmp prod_league
l_ret:
    mov bl, 0x50
    cmp ebp, 0x1234
    jne l_fail
    cmp ecx, 0x11223344
    jne l_fail
    mov eax, [edx]
    cmp eax, [esi+16]
    jne l_fail
    jmp l_store
l_fail:
    mov bl, 0x46
l_store:
    mov [RESULTS+edi], bl
    inc edi
    add esi, 20
    jmp l_loop
l_done:
    mov esi, offset casesC
c_loop:
    cmp dword ptr [esi], 0xFFFFFFFF
    je  c_done
    mov eax, [esi]
    mov word ptr [AREA], ax
    mov eax, [esi+12]
    mov byte ptr [SCHEME], al
    push edi
    mov edi, offset BUF
    mov ecx, 24
    mov eax, 0xEEEEEEEE
    cld
    rep stosd
    pop edi
    mov eax, [esi+8]
    mov byte ptr [BUF+0x47], al
    mov byte ptr [BUF+0x4A], 0
    mov edx, offset BUF
    mov eax, [esi+4]
    push offset c_ret
    push 0x1234
    jmp prod_choco
c_ret:
    mov bl, 0x50
    cmp ebp, 0x1234
    jne c_fail
    cmp eax, [edx]
    jne c_fail
    movzx eax, byte ptr [BUF+0x47]
    cmp eax, [esi+16]
    jne c_fail
    movzx eax, byte ptr [BUF+0x4A]
    cmp eax, [esi+20]
    jne c_fail
    cmp byte ptr [BUF+0x48], 0xEE
    jne c_fail
    cmp byte ptr [BUF+0x49], 0xEE
    jne c_fail
    cmp byte ptr [BUF+0x4B], 0xEE
    jne c_fail
    jmp c_store
c_fail:
    mov bl, 0x46
c_store:
    mov [RESULTS+edi], bl
    inc edi
    add esi, 24
    jmp c_loop
c_done:
    mov byte ptr [SLOT0],   5
    mov byte ptr [SLOT0+1], 6
    mov byte ptr [SLOT0+2], 7
    mov byte ptr [SLOT0+3], 8
    mov byte ptr [SLOT0+4], 9
    mov byte ptr [SLOT0+5], 10
    mov esi, offset casesV
v_loop:
    cmp dword ptr [esi], 0xFFFFFFFF
    je  v_done
    mov eax, [esi]
    mov word ptr [AREA], ax
    mov eax, [esi+4]
    mov byte ptr [LGFLAG], al
    mov ecx, [esi+8]
    mov ebx, [esi+12]
    push offset v_ret
    push 0x1234
    push 0x5678
    jmp prod_lvl
v_ret:
    mov dl, 0x50
    cmp eax, [esi+16]
    jne v_bad
    cmp ebx, 0x5678
    jne v_bad
    cmp ebp, 0x1234
    jne v_bad
    jmp v_store
v_bad:
    mov dl, 0x46
v_store:
    mov [RESULTS+edi], dl
    inc edi
    add esi, 20
    jmp v_loop
v_done:
    mov esi, offset casesN
n_loop:
    cmp dword ptr [esi], 0xFFFFFFFF
    je  n_done
    mov eax, [esi]
    mov byte ptr [MEP], al
    mov eax, [esi+4]
    mov byte ptr [NIM], al
    mov eax, [esi+8]
    mov byte ptr [ZAL], al
    mov byte ptr [MEP-1], 0xEE
    mov byte ptr [ZAL+1], 0xEE
    mov word ptr [WORDV], 0xBEEF
    mov ecx, offset WORDV
    push esi
    push edi
    push offset n_ret
    push 0x1234
    push 0x5678
    push 0x9ABC
    push 0xDEF0
    jmp prod_learn
n_ret:
    mov dl, 0x50
    cmp eax, 0xBEEF
    jne n_bad
    cmp ebx, 0x5678
    jne n_bad
    cmp ebp, 0x1234
    jne n_bad
    cmp esi, 0x9ABC
    jne n_bad
    cmp edi, 0xDEF0
    jne n_bad
    jmp n_mem
n_bad:
    mov dl, 0x46
n_mem:
    pop edi
    pop esi
    movzx eax, byte ptr [MEP]
    cmp eax, [esi+12]
    jne n_bad2
    movzx eax, byte ptr [NIM]
    cmp eax, [esi+16]
    jne n_bad2
    movzx eax, byte ptr [ZAL]
    cmp eax, [esi+20]
    jne n_bad2
    cmp byte ptr [MEP-1], 0xEE
    jne n_bad2
    cmp byte ptr [ZAL+1], 0xEE
    jne n_bad2
    jmp n_store
n_bad2:
    mov dl, 0x46
n_store:
    mov [RESULTS+edi], dl
    inc edi
    add esi, 24
    jmp n_loop
n_done:
    mov byte ptr [RESULTS+edi], 10
    inc edi
    mov eax, 4
    mov ebx, 1
    mov ecx, 0x0A0000C0
    mov edx, edi
    int 0x80
    mov eax, 1
    xor ebx, ebx
    int 0x80
prod_dodger:
{byts(dodger)}
prod_league:
{byts(league)}
    pop ebp          # the original code at site+2 (what the appended jmp returns to)
    ret
prod_choco:
{byts(choco)}
prod_lvl:
{byts(lvl)}
prod_learn:
{byts(learn)}
    mov eax, ebx     # the original epilogue at site+5 (what the appended jmp returns to)
    pop ebx
    pop ebp
    ret
'''
if __name__ == '__main__':
    d = tempfile.mkdtemp(); open(d+'/h.s','w').write(asm)
    subprocess.check_call(['as','--32','-o',d+'/h.o',d+'/h.s'])
    subprocess.check_call(['ld','-m','elf_i386','--section-start=.mydata=0x0A000000','-o',d+'/h',d+'/h.o'])
    out = subprocess.run([d+'/h'],capture_output=True,text=True)
    res = out.stdout.strip()
    print('raw result string:', res, '(exit', out.returncode, ')')
    allrows = [('dodger',r) for r in D] + [('league',r) for r in L] + [('chocobo',r) for r in C] + [('blitz levels',r) for r in V] + [('blitz learn',r) for r in N]
    ok = len(res)==len(allrows)
    for i,(kind,row) in enumerate(allrows):
        good = i < len(res) and res[i]=='P'; ok &= good
        print('PASS' if good else 'FAIL', '[%s]'%kind, row[0])
    print('\nALL PASS' if ok else '\nSOME FAILED')
