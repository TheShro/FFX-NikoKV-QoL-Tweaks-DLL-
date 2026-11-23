/* hookutil.h - pure byte-level hook builders (no OS calls, so they can be unit-tested).
 * 32-bit x86 only: all addresses are 32-bit and jumps are E9 rel32. */
#ifndef HOOKUTIL_H
#define HOOKUTIL_H
#include <string.h>

typedef unsigned char hk_u8;
typedef unsigned int  hk_u32;   /* 32 bits on every target we build for */

/* E9 rel32 placed at address `from`, jumping to `to` (wraps mod 2^32, which is what x86 does). */
static void hk_jmp32(hk_u8 *out, hk_u32 from, hk_u32 to)
{
    hk_u32 rel = to - (from + 5u);
    out[0] = 0xE9;
    memcpy(out + 1, &rel, 4);          /* little-endian, as on x86 */
}

/* ITEM STOCK LIMIT
 * Site (7 bytes, followed by "push 63h" = 2 bytes):
 *     movzx esi, byte ptr [edi+inventory]     ; 0F B6 B7 <disp32>
 * Tweak: run the original movzx, push 0FFh instead of 63h, resume after the old push.
 *   cave : <orig 7 bytes> ; 68 FF 00 00 00 ; E9 -> site+9
 *   site : E9 -> cave ; 90 90
 * `orig` = the 7 bytes read from the site. */
#define HK_ITEM_SITE_LEN 7
#define HK_ITEM_CAVE_LEN 17
static void hk_build_item(const hk_u8 *orig, hk_u32 site_addr, hk_u32 cave_addr,
                          hk_u8 *cave_out, hk_u8 *site_out)
{
    memcpy(cave_out, orig, 7);
    cave_out[7]  = 0x68;               /* push imm32 */
    cave_out[8]  = 0xFF;
    cave_out[9]  = 0x00;
    cave_out[10] = 0x00;
    cave_out[11] = 0x00;
    hk_jmp32(cave_out + 12, cave_addr + 12u, site_addr + 9u);
    hk_jmp32(site_out, site_addr, cave_addr);
    site_out[5] = 0x90;
    site_out[6] = 0x90;
}

/* ---- generated caves (see the caves folder and tools/gen_caves.py) ---- */
/* A cave's bytes contain placeholder dwords for absolute data addresses; `pos` is where. */
typedef struct { int pos; int which; } HkReloc;
static void hk_relocate(hk_u8 *code, const HkReloc *r, int n, const hk_u32 *values)
{
    int i;
    for (i = 0; i < n; i++)
        memcpy(code + r[i].pos, &values[r[i].which], 4);
}

/* LIGHTNING DODGER: the 5-byte function  66 89 02 5D C3  is replaced by  E9 -> cave
 * (the cave ends with its own pop ebp / ret, like the original script). */
#define HK_DODGER_SITE_LEN 5
static void hk_build_dodger_site(hk_u32 site_addr, hk_u32 cave_addr, hk_u8 *site_out)
{
    hk_jmp32(site_out, site_addr, cave_addr);
}

/* CHOCOBO CATCHER: the 7 bytes  89 02 5D C3 DB 45 14  are replaced by  E9 -> cave ; 90 90
 * (same layout as NikoKV's script: jmp newmem / nop 2). The cave ends with its own pop ebp / ret. */
#define HK_CHOCO_SITE_LEN 7
static void hk_build_choco_site(hk_u32 site_addr, hk_u32 cave_addr, hk_u8 *site_out)
{
    hk_jmp32(site_out, site_addr, cave_addr);
    site_out[5] = 0x90;
    site_out[6] = 0x90;
}
#endif
