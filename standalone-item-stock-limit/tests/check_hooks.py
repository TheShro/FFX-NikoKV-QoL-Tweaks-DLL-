#!/usr/bin/env python3
"""Builds the item-stock hook with the module's own hookutil.h and decodes the result.
Needs only gcc. Run from the project folder:  python3 tests/check_hooks.py"""
import subprocess, os, re, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
C = r'''
#include <stdio.h>
#include "hookutil.h"
static void hex(const char *t, const hk_u8 *b, int n){ printf("%s", t); for(int i=0;i<n;i++) printf("%02X", b[i]); printf("\n"); }
int main(void){
    const hk_u8 orig[7] = {0x0F,0xB6,0xB7,0x5C,0x0B,0x09,0x01};   /* movzx esi,byte ptr [edi+01090B5C] */
    hk_u32 caves[] = {0x06BE0000u, 0x00100000u, 0xF0000000u};     /* above, below and far from the site */
    for (int i=0;i<3;i++){
        hk_u8 cave[HK_ITEM_CAVE_LEN], site[HK_ITEM_SITE_LEN];
        hk_build_item(orig, 0x006F05F6u, caves[i], cave, site);
        printf("cave=%08X\n", caves[i]); hex("S ", site, HK_ITEM_SITE_LEN); hex("C ", cave, HK_ITEM_CAVE_LEN);
    }
    return 0; }
'''
d = tempfile.mkdtemp(); open(d+'/t.c','w').write(C)
subprocess.check_call(['gcc','-Wall','-O2','-I',ROOT,'-o',d+'/t',d+'/t.c'])
lines = subprocess.run([d+'/t'],capture_output=True,text=True).stdout.splitlines()
M, SITE, orig, ok = 0xFFFFFFFF, 0x006F05F6, bytes.fromhex('0FB6B75C0B0901'), True
def tgt(code, at): return (at+5+int.from_bytes(code[1:5],'little')) & M
for i in range(0,len(lines),3):
    cave=int(lines[i].split('=')[1],16); s=bytes.fromhex(lines[i+1].split()[1]); c=bytes.fromhex(lines[i+2].split()[1])
    checks = {'site jumps to cave': s[0]==0xE9 and tgt(s,SITE)==cave,
              'site padded with 2 nops': s[5:7]==b'\x90\x90',
              'cave begins with the original movzx': c[:7]==orig,
              'cave pushes 0xFF (was 0x63)': c[7:12]==bytes.fromhex('68FF000000'),
              'cave returns to site+9 (after the old push 63)': c[12]==0xE9 and tgt(c[12:17],cave+12)==(SITE+9)&M}
    for k,v in checks.items(): ok&=v; print('PASS' if v else 'FAIL', '[cave=%08X]'%cave, k)
print('\nALL PASS' if ok else '\nSOME FAILED'); raise SystemExit(0 if ok else 1)
