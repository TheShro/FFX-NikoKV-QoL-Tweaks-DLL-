/*
 * ffx_item_stock.c  --  FFX Item Stock Limit (99 -> 255), standalone module
 *
 * Drop-in module for ffgriever's FF10 Module Loader (works with UnX and the
 * External File Loader). Put ffx_item_stock.dll in <game>\modules\.
 * Remove the DLL to remove the patch.
 *
 * The hook is located by AOB scan at startup, so it follows small code shifts.
 * Log: ffx_item_stock.log next to FFX.exe.
 *
 * Original tweak and concept: NikoKV ("Change items stock limit from 99 to 255",
 * FFX Tweaks Collection - all main credit to the original author).
 * Hook research and port: Shro (with Claude, Anthropic).
 * Item-stock hook offsets: based on the community Item_fix.CT.
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
#error "ffx_item_stock must be built as a 32-bit (x86) DLL"
#endif

#include "scanner.h"
#include "hookutil.h"

/* The game function that adds to an item stack: reads the stack count, pushes the
 * maximum (99), and calls a clamp helper. The tweak pushes 255 instead. */
static const char ITEM_PATTERN[] =
    "0F B6 B7 ?? ?? ?? ?? 6A 63 8D 04 1E 6A 00 50 E8 ?? ?? ?? ?? 83 C4 0C 89 45 FC 88 87 ?? ?? ?? ?? 3B C6";
#define EXPECTED_OFFSET 0x3905F6UL     /* only used to flag a shifted build in the log */

static FILE  *g_log;
static DWORD  g_t0;
static int    g_done;

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

static int exe_dir_path(char *out, size_t cap, const char *name)
{
    char *slash;
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)cap);
    if (n == 0 || n >= cap) return 0;
    slash = strrchr(out, '\\');
    if (!slash || (size_t)(slash + 1 - out) + strlen(name) + 1 >= cap) return 0;
    strcpy(slash + 1, name);
    return 1;
}

static int patch_code(void *dst, const void *src, size_t n)
{
    DWORD old;
    if (!VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy(dst, src, n);
    VirtualProtect(dst, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), dst, n);
    return 1;
}

/* cave : <original movzx, copied live> ; push 0FFh ; jmp site+9
 * site : jmp cave ; nop ; nop        (exact bytes: see hookutil.h) */
static int apply_item_stock(BYTE *site)
{
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
static int scan_exe(HMODULE mod, const BYTE *val, const BYTE *msk, int n, DWORD *hits, int maxhits)
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

/* One attempt. Sets g_done when the outcome is final. */
static void attempt(int timed_out)
{
    BYTE val[PAT_MAX], msk[PAT_MAX];
    DWORD hits[8];
    HMODULE exe = GetModuleHandleA(NULL);
    int n = pat_parse(ITEM_PATTERN, val, msk, PAT_MAX), c, k;

    if (n < 0) { say("BAD PATTERN (bug in the module)"); g_done = 1; return; }
    c = scan_exe(exe, val, msk, n, hits, 8);

    if (c == 1) {
        say("hook found at FFX.exe+%06lX%s", (unsigned long)hits[0],
            hits[0] == EXPECTED_OFFSET ? "" : "  (different from the build this was made on - a game update?)");
        say(apply_item_stock((BYTE *)exe + hits[0])
                ? "PATCHED - item stacks now go up to 255"
                : "PATCH FAILED - no change made");
        g_done = 1;
    } else if (c > 1) {
        say("AMBIGUOUS: %d matches - not patching", c);
        for (k = 0; k < c && k < 8; k++) say("   FFX.exe+%06lX", (unsigned long)hits[k]);
        g_done = 1;
    } else if (timed_out) {
        say(c < 0 ? "NOT READY: code never became readable - no change made"
                  : "NOT FOUND: hook pattern not in this exe (already patched by another module or "
                    "Cheat Engine, or the game changed) - no change made");
        g_done = 1;
    }
}

static void finish(void)
{
    say("finished");
    if (g_log) { fclose(g_log); g_log = NULL; }
}

/* Only runs if the first (synchronous) attempt could not resolve it. */
static DWORD WINAPI retry_worker(LPVOID arg)
{
    (void)arg;
    while (!g_done) {
        Sleep(500);
        attempt((GetTickCount() - g_t0) > 90000UL);
    }
    finish();
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH];
        DisableThreadLibraryCalls(inst);

        g_t0 = GetTickCount();
        if (exe_dir_path(path, sizeof path, "ffx_item_stock.log")) g_log = fopen(path, "w");
        say("ffx_item_stock loaded (FFX Item Stock Limit 99 -> 255)");

        /* First attempt runs here, while the game's main thread is still inside the loader
         * and not executing game code, so patching is race-free. */
        attempt(0);

        if (!g_done) {
            HANDLE t = CreateThread(NULL, 0, retry_worker, NULL, 0, NULL);
            if (t) CloseHandle(t); else finish();
        } else {
            finish();
        }
    }
    return TRUE;
}
