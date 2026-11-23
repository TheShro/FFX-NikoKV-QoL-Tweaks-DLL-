/*
 * ffx_tweaks.c  --  FFX Tweaks module, STEP 5 (all six tweaks)
 *
 * Drop-in module for ffgriever's FF10 Module Loader (works with UnX and the
 * External File Loader). Put ffx_tweaks.dll in <game>\modules\.
 *
 *   Tweaks : ITEM STOCK LIMIT (99 -> 255), LIGHTNING DODGER, SHORTER LEAGUE,
 *            CHOCOBO CATCHER (3 starting-score presets),
 *            BLITZBALL LEVELS (+10 in Exhibition), BLITZBALL AUTO-LEARN (Mep/Nimrook/Zalitz)
 *
 * Every hook is located by AOB scan at startup, so it follows small code shifts.
 * Settings: modules\ffx_tweaks.ini  (created on first run).  Log: ffx_tweaks.log
 * next to FFX.exe.
 *
 * Original tweaks and concept: NikoKV (all main credit to the original author).
 * Hook research and port: Shro (with Claude, Anthropic).
 * Module loader / External File Loader: ffgriever.
 *
 * MUST be built as a 32-bit (x86) DLL - FFX.exe is a 32-bit process.
 */
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if !defined(_M_IX86) && !defined(__i386__)
#error "ffx_tweaks must be built as a 32-bit (x86) DLL"
#endif

#include "scanner.h"
#include "hookutil.h"
#include "caves.h"

/* ------------------------------------------------------------------ logging */
static HMODULE g_self;
static FILE   *g_log;
static DWORD   g_t0;

static void say(const char *fmt, ...)
{
    va_list ap;
    if (!g_log) return;
    fprintf(g_log, "[%6lu ms] ", (unsigned long)(GetTickCount() - g_t0));
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

/* Build "<dir of module>\<name>" ; use_exe selects FFX.exe's folder vs this DLL's folder. */
static int make_path(char *out, size_t cap, int use_exe, const char *name)
{
    char *slash;
    DWORD n = GetModuleFileNameA(use_exe ? NULL : g_self, out, (DWORD)cap);
    if (n == 0 || n >= cap) return 0;
    slash = strrchr(out, '\\');
    if (!slash || (size_t)(slash + 1 - out) + strlen(name) + 1 >= cap) return 0;
    strcpy(slash + 1, name);
    return 1;
}

/* ---------------------------------------------------------------- settings */
static char g_ini[MAX_PATH];

static void ensure_ini(void)
{
    FILE *f;
    if (!make_path(g_ini, sizeof g_ini, 0, "ffx_tweaks.ini")) { g_ini[0] = 0; return; }
    if (GetFileAttributesA(g_ini) != INVALID_FILE_ATTRIBUTES) return;
    f = fopen(g_ini, "w");
    if (!f) return;
    fputs("; FFX Tweaks module - set a value to 0 to turn that tweak off, then restart the game.\n"
          "; Original tweaks by NikoKV. Hook port by Shro (with Claude, Anthropic).\n"
          "[Tweaks]\n"
          "ItemStockLimit=1\n"
          "LightningDodger=1\n"
          "ShorterLeague=1\n"
          "BlitzballLevels=1\n"
          "BlitzballAutoLearn=1\n"
          "ChocoboCatcher=1\n"
          "; Chocobo Catcher starting score preset (TID.balls / TRA.balls / TID.birds / TRA.birds):\n"
          ";   1 = 3/0/0/0 (default)    2 = 4/0/0/0    3 = 30/0/0/30\n"
          "ChocoboCatcherPreset=1\n", f);
    fclose(f);
}

static int tweak_enabled(const char *key)
{
    if (!g_ini[0]) return 1;
    return GetPrivateProfileIntA("Tweaks", key, 1, g_ini) != 0;
}

/* ------------------------------------------------------------ code patching */
static int patch_code(void *dst, const void *src, size_t n)
{
    DWORD old;
    if (!VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy(dst, src, n);
    VirtualProtect(dst, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), dst, n);
    return 1;
}

/* ITEM STOCK LIMIT: stack cap 99 -> 255 (see hookutil.h for the exact bytes). */
static int apply_item_stock(BYTE *site, BYTE *exe)
{
    (void)exe;
    BYTE orig[HK_ITEM_SITE_LEN], cave_bytes[HK_ITEM_CAVE_LEN], site_bytes[HK_ITEM_SITE_LEN];
    BYTE *cave;

    memcpy(orig, site, HK_ITEM_SITE_LEN);
    if (orig[0] != 0x0F || orig[1] != 0xB6 || orig[2] != 0xB7) return 0;   /* sanity */

    cave = (BYTE *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!cave) return 0;

    hk_build_item(orig, (hk_u32)(UINT_PTR)site, (hk_u32)(UINT_PTR)cave, cave_bytes, site_bytes);
    memcpy(cave, cave_bytes, HK_ITEM_CAVE_LEN);
    FlushInstructionCache(GetCurrentProcess(), cave, HK_ITEM_CAVE_LEN);

    return patch_code(site, site_bytes, HK_ITEM_SITE_LEN);   /* cave is ready before the jump goes in */
}

/* Module-relative game-data addresses used by the caves (unchanged since the original table). */
#define OFF_AREA   0xD2CA90u
#define OFF_STREAK 0xD2CE90u
#define OFF_LGFLAG 0xD2E10Au
#define OFF_ROUND  0xD2E102u
#define OFF_ARRAY  0xD2E04Au     /* Blitzball player stat array (0x3C bytes) */
#define OFF_SLOTS  0xD2E0BEu     /* six "Beside Aurochs" player ids, one byte each */
#define OFF_MEP    0xD2DE26u
#define OFF_NIM    0xD2DDCEu
#define OFF_ZAL    0xD2DE29u

static void data_values(BYTE *exe, hk_u32 v[RELOC_COUNT])
{
    hk_u32 b = (hk_u32)(UINT_PTR)exe;
    int i;
    v[RELOC_AREA]   = b + OFF_AREA;
    v[RELOC_STREAK] = b + OFF_STREAK;
    v[RELOC_LGFLAG] = b + OFF_LGFLAG;
    v[RELOC_ROUND]  = b + OFF_ROUND;
    v[RELOC_SCHEME] = 0;            /* filled in by the tweak that uses it */
    v[RELOC_ARRAY]  = b + OFF_ARRAY;
    for (i = 0; i < 6; i++) v[RELOC_SLOT0 + i] = b + OFF_SLOTS + (hk_u32)i;
    v[RELOC_MEP]    = b + OFF_MEP;
    v[RELOC_NIM]    = b + OFF_NIM;
    v[RELOC_ZAL]    = b + OFF_ZAL;
}

/* LIGHTNING DODGER: fails no longer reset the streak; returning to Thunder Plains keeps it. */
static int apply_dodger(BYTE *site, BYTE *exe)
{
    static const BYTE orig[5] = { 0x66, 0x89, 0x02, 0x5D, 0xC3 };
    BYTE cave_bytes[CAVE_DODGER_LEN], site_bytes[HK_DODGER_SITE_LEN];
    hk_u32 v[RELOC_COUNT];
    BYTE *cave;

    if (memcmp(site, orig, sizeof orig) != 0) return 0;               /* sanity */
    cave = (BYTE *)VirtualAlloc(NULL, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!cave) return 0;

    memcpy(cave_bytes, CAVE_DODGER, CAVE_DODGER_LEN);
    data_values(exe, v);
    hk_relocate(cave_bytes, CAVE_DODGER_RELOCS, CAVE_DODGER_NRELOCS, v);
    memcpy(cave, cave_bytes, CAVE_DODGER_LEN);
    FlushInstructionCache(GetCurrentProcess(), cave, CAVE_DODGER_LEN);

    hk_build_dodger_site((hk_u32)(UINT_PTR)site, (hk_u32)(UINT_PTR)cave, site_bytes);
    return patch_code(site, site_bytes, HK_DODGER_SITE_LEN);
}

/* SHORTER LEAGUE: the patch site is only 2 bytes (88 02), too small for a jump, so it is a
 * one-byte breakpoint (CC) caught by a vectored exception handler that redirects execution
 * to the cave - the same technique Cheat Engine's "jmp1" used, but with no Cheat Engine. */
static BYTE   *g_league_site;
static hk_u32  g_league_cave;

static LONG CALLBACK league_veh(PEXCEPTION_POINTERS ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
        hk_u32 at   = (hk_u32)(UINT_PTR)ep->ExceptionRecord->ExceptionAddress;
        hk_u32 site = (hk_u32)(UINT_PTR)g_league_site;
        if (at == site || ep->ContextRecord->Eip == site + 1u) {
            ep->ContextRecord->Eip = g_league_cave;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static int apply_league(BYTE *site, BYTE *exe)
{
    BYTE cave_bytes[CAVE_LEAGUE_LEN + 5];
    hk_u32 v[RELOC_COUNT];
    BYTE *cave, cc = 0xCC;

    if (site[0] != 0x88 || site[1] != 0x02 || site[2] != 0x5D || site[3] != 0xC3) return 0;
    cave = (BYTE *)VirtualAlloc(NULL, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!cave) return 0;

    memcpy(cave_bytes, CAVE_LEAGUE, CAVE_LEAGUE_LEN);
    data_values(exe, v);
    hk_relocate(cave_bytes, CAVE_LEAGUE_RELOCS, CAVE_LEAGUE_NRELOCS, v);
    /* after the cave, continue at site+2 (pop ebp / ret) */
    hk_jmp32(cave_bytes + CAVE_LEAGUE_LEN, (hk_u32)(UINT_PTR)cave + CAVE_LEAGUE_LEN,
             (hk_u32)(UINT_PTR)site + 2u);
    memcpy(cave, cave_bytes, sizeof cave_bytes);
    FlushInstructionCache(GetCurrentProcess(), cave, sizeof cave_bytes);

    g_league_site = site;
    g_league_cave = (hk_u32)(UINT_PTR)cave;
    if (!AddVectoredExceptionHandler(1, league_veh)) return 0;     /* handler first, then CC */
    return patch_code(site, &cc, 1);
}

/* CHOCOBO CATCHER: custom starting score for every race. The preset (1-3) comes from the ini
 * and is stored in one byte placed right after the cave's code. */
static int chocobo_scheme(void)
{
    int p = g_ini[0] ? (int)GetPrivateProfileIntA("Tweaks", "ChocoboCatcherPreset", 1, g_ini) : 1;
    if (p < 1 || p > 3) {
        say("%-36s   -> ChocoboCatcherPreset=%d is not 1-3, using preset 1", "", p);
        p = 1;
    }
    return p - 1;
}

static int apply_chocobo(BYTE *site, BYTE *exe)
{
    static const BYTE orig[7] = { 0x89, 0x02, 0x5D, 0xC3, 0xDB, 0x45, 0x14 };
    BYTE cave_bytes[CAVE_CHOCO_LEN + 1], site_bytes[HK_CHOCO_SITE_LEN];
    hk_u32 v[RELOC_COUNT];
    BYTE *cave;
    int scheme;

    if (memcmp(site, orig, sizeof orig) != 0) return 0;               /* sanity */
    cave = (BYTE *)VirtualAlloc(NULL, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!cave) return 0;

    scheme = chocobo_scheme();
    data_values(exe, v);
    v[RELOC_SCHEME] = (hk_u32)(UINT_PTR)cave + CAVE_CHOCO_LEN;        /* the scheme byte */
    memcpy(cave_bytes, CAVE_CHOCO, CAVE_CHOCO_LEN);
    hk_relocate(cave_bytes, CAVE_CHOCO_RELOCS, CAVE_CHOCO_NRELOCS, v);
    cave_bytes[CAVE_CHOCO_LEN] = (BYTE)scheme;
    memcpy(cave, cave_bytes, sizeof cave_bytes);
    FlushInstructionCache(GetCurrentProcess(), cave, sizeof cave_bytes);

    say("%-36s   -> starting score preset %d", "", scheme + 1);
    hk_build_choco_site((hk_u32)(UINT_PTR)site, (hk_u32)(UINT_PTR)cave, site_bytes);
    return patch_code(site, site_bytes, HK_CHOCO_SITE_LEN);
}

/* BLITZBALL: both tweaks live in one jump-table function that reads a stat through [ecx].
 * `match` is the start of the scanned pattern; the two hook sites are fixed offsets into it.
 * (Each pattern wildcards the other tweak's hook bytes, so they can be applied in any order.)
 *   levels hook : match+0x11  mov eax,ebx ; pop ebx ; pop ebp ; ret       -> replaced by E9 -> cave
 *   learn  hook : match+0x20  movzx ebx,word ptr [ecx] ; pop edi ; pop esi -> replaced by E9 -> cave
 *                 (the cave runs them itself, then jumps to hook+5) */
static int apply_blitz_levels(BYTE *match, BYTE *exe)
{
    static const BYTE orig[5] = { 0x8B, 0xC3, 0x5B, 0x5D, 0xC3 };
    BYTE *site = match + 0x11;
    BYTE cave_bytes[CAVE_BLITZLVL_LEN], site_bytes[5];
    hk_u32 v[RELOC_COUNT];
    BYTE *cave;

    if (memcmp(site, orig, sizeof orig) != 0) return 0;               /* sanity */
    cave = (BYTE *)VirtualAlloc(NULL, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!cave) return 0;

    memcpy(cave_bytes, CAVE_BLITZLVL, CAVE_BLITZLVL_LEN);
    data_values(exe, v);
    hk_relocate(cave_bytes, CAVE_BLITZLVL_RELOCS, CAVE_BLITZLVL_NRELOCS, v);
    memcpy(cave, cave_bytes, CAVE_BLITZLVL_LEN);
    FlushInstructionCache(GetCurrentProcess(), cave, CAVE_BLITZLVL_LEN);

    hk_jmp32(site_bytes, (hk_u32)(UINT_PTR)site, (hk_u32)(UINT_PTR)cave);
    return patch_code(site, site_bytes, 5);
}

static int apply_blitz_learn(BYTE *match, BYTE *exe)
{
    static const BYTE orig[5] = { 0x0F, 0xB7, 0x19, 0x5F, 0x5E };
    BYTE *site = match + 0x20;
    BYTE cave_bytes[CAVE_BLITZLEARN_LEN + 5], site_bytes[5];
    hk_u32 v[RELOC_COUNT];
    BYTE *cave;

    if (memcmp(site, orig, sizeof orig) != 0) return 0;               /* sanity */
    cave = (BYTE *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!cave) return 0;

    memcpy(cave_bytes, CAVE_BLITZLEARN, CAVE_BLITZLEARN_LEN);
    data_values(exe, v);
    hk_relocate(cave_bytes, CAVE_BLITZLEARN_RELOCS, CAVE_BLITZLEARN_NRELOCS, v);
    /* after the cave, continue at hook+5 (the original epilogue) */
    hk_jmp32(cave_bytes + CAVE_BLITZLEARN_LEN, (hk_u32)(UINT_PTR)cave + CAVE_BLITZLEARN_LEN,
             (hk_u32)(UINT_PTR)site + 5u);
    memcpy(cave, cave_bytes, sizeof cave_bytes);
    FlushInstructionCache(GetCurrentProcess(), cave, sizeof cave_bytes);

    hk_jmp32(site_bytes, (hk_u32)(UINT_PTR)site, (hk_u32)(UINT_PTR)cave);
    return patch_code(site, site_bytes, 5);
}

/* ------------------------------------------------------------------ tweaks */
typedef int (*ApplyFn)(BYTE *site, BYTE *exe);
typedef struct {
    const char *name;
    const char *key;        /* ini key */
    const char *pattern;
    DWORD       expect;     /* expected match offset from FFX.exe base */
    ApplyFn     apply;      /* NULL = scan only */
} Tweak;

static const Tweak TWEAKS[] = {
    { "item stock limit", "ItemStockLimit",
      "0F B6 B7 ?? ?? ?? ?? 6A 63 8D 04 1E 6A 00 50 E8 ?? ?? ?? ?? 83 C4 0C 89 45 FC 88 87 ?? ?? ?? ?? 3B C6",
      0x3905F6, apply_item_stock },
    { "lightning dodger", "LightningDodger",
      "66 89 02 5D C3 83 7D 18 00 8B 45 14 74 ?? 3D 00 80 FF FF 7D ??",
      0x4714A2, apply_dodger },
    { "chocobo catcher", "ChocoboCatcher",
      "89 02 5D C3 DB 45 14 D9 1A 5D C3",
      0x4714D5, apply_chocobo },
    { "shorter league", "ShorterLeague",
      "88 02 5D C3 83 7D 18 00 8B 45 14 74 ?? 83 F8 80 7D ?? B8 80 FF FF FF 88 02 5D C3",
      0x471459, apply_league },
    { "blitzball levels (match start)", "BlitzballLevels",
      "83 F8 06 77 ?? FF 24 85 ?? ?? ?? ?? 0F B6 19 5F 5E 8B C3 5B 5D C3 0F BE 19 5F 5E 8B C3 5B 5D C3 ?? ?? ?? ?? ?? 8B C3 5B 5D C3",
      0x46C75B, apply_blitz_levels },
    { "blitzball auto-learn (match start)", "BlitzballAutoLearn",
      "83 F8 06 77 ?? FF 24 85 ?? ?? ?? ?? 0F B6 19 5F 5E ?? ?? ?? ?? ?? 0F BE 19 5F 5E 8B C3 5B 5D C3 0F B7 19 5F 5E 8B C3 5B 5D C3",
      0x46C75B, apply_blitz_learn }
};
#define NTWEAKS ((int)(sizeof(TWEAKS) / sizeof(TWEAKS[0])))

/* ------------------------------------------------------------------ scanning */
static int range_readable(const BYTE *p, size_t size)
{
    const BYTE *end = p + size;
    while (p < end) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(p, &mbi, sizeof(mbi))) return 0;
        if (mbi.State != MEM_COMMIT) return 0;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return 0;
        p = (const BYTE *)mbi.BaseAddress + mbi.RegionSize;
    }
    return 1;
}

/* Scan every executable section. Returns hit count, or -1 if code isn't readable yet. */
static int scan_exe(HMODULE mod, const BYTE *val, const BYTE *msk, int n,
                    DWORD *hits, int maxhits)
{
    const BYTE *base = (const BYTE *)mod;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const IMAGE_NT_HEADERS *nt  = (const IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    const IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    int total = 0, i;

    for (i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        const BYTE *start;
        DWORD size;
        size_t local[8];
        int found, k;

        if (!(sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        start = base + sec[i].VirtualAddress;
        size  = sec[i].Misc.VirtualSize;
        if (!range_readable(start, size)) return -1;

        found = pat_scan(start, size, val, msk, n, local, 8);
        for (k = 0; k < found && k < 8; k++)
            if (total + k < maxhits)
                hits[total + k] = (DWORD)(start - base) + (DWORD)local[k];
        total += found;
    }
    return total;
}

/* One pass over all tweaks. Anything not yet resolved is retried on the next pass. */
static int  g_done[16];
static int  g_remaining = NTWEAKS;
static HMODULE g_exe;

static void run_pass(int timed_out)
{
    BYTE val[PAT_MAX], msk[PAT_MAX];
    int p;

    for (p = 0; p < NTWEAKS; p++) {
        const Tweak *t = &TWEAKS[p];
        DWORD hits[8];
        int n, c, k;
        if (g_done[p]) continue;

        n = pat_parse(t->pattern, val, msk, PAT_MAX);
        if (n < 0) {
            say("%-36s BAD PATTERN (bug in the module)", t->name);
        } else {
            c = scan_exe(g_exe, val, msk, n, hits, 8);
            if (c == 1) {
                say("%-36s %s  FFX.exe+%06lX  (expected +%06lX)", t->name,
                    hits[0] == t->expect ? "OK        " : "UNEXPECTED",
                    (unsigned long)hits[0], (unsigned long)t->expect);
                if (!t->apply) {
                    say("%-36s   -> scan only, not ported yet (no change made)", "");
                } else if (!tweak_enabled(t->key)) {
                    say("%-36s   -> turned OFF in ffx_tweaks.ini (no change made)", "");
                } else if (t->apply((BYTE *)g_exe + hits[0], (BYTE *)g_exe)) {
                    say("%-36s   -> PATCHED", "");
                } else {
                    say("%-36s   -> PATCH FAILED (no change made)", "");
                }
            } else if (c > 1) {
                say("%-36s AMBIGUOUS  %d matches - not patching", t->name, c);
                for (k = 0; k < c && k < 8; k++)
                    say("%-36s   FFX.exe+%06lX", "", (unsigned long)hits[k]);
            } else if (timed_out) {
                say("%-36s %s", t->name,
                    c < 0 ? "NOT READY  (code never became readable)"
                          : "NOT FOUND  (pattern does not exist in this exe)");
            } else {
                continue;   /* retry later */
            }
        }
        g_done[p] = 1;
        g_remaining--;
    }
}

static void finish(void)
{
    say("finished");
    if (g_log) { fclose(g_log); g_log = NULL; }
}

/* Only runs if the first (synchronous) pass could not resolve everything. */
static DWORD WINAPI retry_worker(LPVOID arg)
{
    (void)arg;
    while (g_remaining > 0) {
        Sleep(500);
        run_pass((GetTickCount() - g_t0) > 90000UL);
    }
    finish();
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH];
        g_self = (HMODULE)inst;
        DisableThreadLibraryCalls(inst);

        g_t0 = GetTickCount();
        if (make_path(path, sizeof path, 1, "ffx_tweaks.log")) g_log = fopen(path, "w");
        ensure_ini();
        g_exe = GetModuleHandleA(NULL);
        GetModuleFileNameA(NULL, path, MAX_PATH);

        say("ffx_tweaks step 5 loaded  (all six tweaks live)");
        say("process : %s", path);
        say("exe base: %p   this module: %p   ini: %s", (void *)g_exe, (void *)g_self,
            g_ini[0] ? g_ini : "(none)");

        /* First pass runs right here, while the game's main thread is still inside the
         * loader and not executing game code, so patching a hot function is race-free. */
        run_pass(0);

        if (g_remaining > 0) {
            HANDLE t = CreateThread(NULL, 0, retry_worker, NULL, 0, NULL);
            if (t) CloseHandle(t); else finish();
        } else {
            finish();
        }
    }
    return TRUE;
}
