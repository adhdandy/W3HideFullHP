// W3HideFullHP.mix - hides a unit's health bar while its HP is above a threshold,
// for Warcraft III 1.27b (Game.dll 1.27.1.7085).
//
// A fork of W3ManaBars by Hr0ffT - MIT License - https://github.com/Hr0ffT/W3ManaBars
// Build: i686-w64-mingw32-gcc -O2 -Wall -shared -static-libgcc -s -o W3HideFullHP.mix src/W3HideFullHP.c -lversion
//
// The health bars over units are CStatBar frames. A CStatBar has a type: 5 is the small health bar over units.
// Every frame the game walks its health bar list and, for each bar, either shows it (CStatBar::Show, vtable +0x68,
// when it is not shown yet) or, when it is already shown, rebinds it to its unit and re-shows it (a no-op then).
// So Show is called on every health bar on every frame, and it is the single place where "should this bar be
// visible" is turned into "this bar is visible".
//
// The mod hooks Show. When the bar is a unit health bar, it reads the same HP fraction the game uses to fill the
// bar and, if that fraction is above the configured threshold, refuses the show and hides the bar if it is visible.
// That only ever adds a hide condition: whenever the fraction is at or below the threshold the mod falls straight
// through to the game's own Show, so all of the game's normal visibility rules (selection, Alt, hover, damage,
// "always show health bars", fog, death) keep working. Because Show is evaluated on every bar on every frame the
// bar comes back on the first frame the unit drops to or below the threshold. No per-bar state is kept, so a bar
// rebound to another unit, a dying unit or a reused frame cannot leave anything stale behind.
//
// Verified against Game.dll 1.27.1.7085 (image base 0x6F000000):
//   * CStatBar vtable at 0x6FABCE78. Slot +0x64 is Hide (0x6F10D410), slot +0x68 is Show (0x6F10DE60). Show
//     returns 0 when WANT (+0x90) is 0 and does the show when it is set, so WANT=0 + returning 0 is exactly the
//     "not visible" state the game itself produces.
//   * CStatBar::SetUnit (slot +0x74, 0x6F3DABA0) stores the widget it is handed in +0x154 and then switches on the
//     bar type (+0x138). The jump table at 0x6F3DADA4 maps type 5 to 0x6F3DAC1F, which calls 0x6F3B6780 to get
//     the fill fraction. That fraction is what the bar draws, so it is also what this mod compares.
//   * 0x6F3B6780 reads the widget's max life (vtable +0x130) and life (vtable +0x12C) and divides them; if max
//     life is 0 it returns 0. Those are the same two slots the game's own GetUnitState uses (0x6F204050 ->
//     0x6F687270, states 0 and 1), i.e. the JASS unit life and max life getters.
//   * The per-frame bar updater (0x6F39591C) is the "show if not shown" path: it sets WANT=1 and calls Show for
//     bars that are not shown, and calls Show again at 0x6F395AE0 for bars that are. It already suppresses the
//     show by itself when a unit must not have a bar (0x6F39592C), which is why hooking Show is the right lever.
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef uint32_t u32;

static u32 g_base, g_size;
static FILE* g_log;
static int g_debug;
static float g_threshold = 0.95f;   // hide while HP / maxHP is above this
static u32 g_nCall, g_nHide;        // diagnostics for the debug log
static DWORD g_diagTick;

typedef struct {
    u32 build;
    u32 vtStatBar;          // CStatBar vtable
    u32 healthFraction;     // thiscall(float* out) on a health bar's widget: life / max life
} Addrs;
static const Addrs kAddrs[] = {
    { 7085, 0xABCE78, 0x3B6780 },   // 1.27b, Game.dll 1.27.1.7085
};
static const Addrs* A;

#define OFS_WANT   0x90         // the bar should be visible
#define OFS_SHOWN  0x94         // the bar is visible
#define OFS_TYPE   0x138        // bar type
#define OFS_WIDGET 0x154        // the unit / widget the bar is bound to
#define TYPE_HP_SMALL 5

typedef u32    (__fastcall *Show_t)(u32 self, u32 edx);
typedef float* (__fastcall *Frac_t)(u32 widget, u32 edx, float* out);
static Show_t orig_Show, orig_Hide;

static void logf_(const char* f, ...)
{
    if (!g_log) return;
    va_list a; va_start(a, f); vfprintf(g_log, f, a); va_end(a);
    fputc('\n', g_log); fflush(g_log);
}
static int WriteMem(u32 addr, const void* p, size_t n)
{
    DWORD old;
    if (!VirtualProtect((void*)addr, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy((void*)addr, p, n);
    VirtualProtect((void*)addr, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)addr, n);
    return 1;
}
static u32 GetGameBuild(void)
{
    DWORD h; DWORD sz = GetFileVersionInfoSizeA("Game.dll", &h);
    if (!sz) return 0;
    char* buf = (char*)malloc(sz); u32 build = 0;
    if (GetFileVersionInfoA("Game.dll", h, sz, buf)) {
        VS_FIXEDFILEINFO* vi; UINT l;
        if (VerQueryValueA(buf, "\\", (LPVOID*)&vi, &l)) build = vi->dwFileVersionLS & 0xFFFF;
    }
    free(buf);
    return build;
}
static u32 ImageSize(u32 base)
{
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS32* nt;
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    nt = (IMAGE_NT_HEADERS32*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    return nt->OptionalHeader.SizeOfImage;
}

// A live game object has its vtable inside Game.dll. Everything the mod reads out of a bar or a unit comes from the
// game itself, so this is only a cheap guard against a pointer that is not what we expect.
static int IsGameObject(u32 o)
{
    u32 vt;
    if (!o || IsBadReadPtr((void*)o, 4)) return 0;
    vt = *(u32*)o;
    return vt >= g_base && vt < g_base + g_size;
}
// The HP fraction the health bar draws: life / max life, 0 when the unit has no max life. 0x6F3B6780 is
// thiscall(widget) with one stack argument, the float* to store into.
static float HealthFraction(u32 widget)
{
    float v = 0.0f;
    if (!IsGameObject(widget)) return 0.0f;
    ((Frac_t)(g_base + A->healthFraction))(widget, 0, &v);
    return v;
}

static u32 __fastcall Show_hook(u32 self, u32 edx)
{
    if (g_threshold < 1.0f && *(u32*)(self + OFS_TYPE) == TYPE_HP_SMALL) {
        g_nCall++;
        if (g_log && GetTickCount() - g_diagTick > 3000) {
            g_diagTick = GetTickCount();
            logf_("3 s: %u health bar shows, %u suppressed", g_nCall, g_nHide);
            g_nCall = g_nHide = 0;
        }
        if (HealthFraction(*(u32*)(self + OFS_WIDGET)) > g_threshold) {
            g_nHide++;
            *(u32*)(self + OFS_WANT) = 0;
            if (*(u32*)(self + OFS_SHOWN)) orig_Hide(self, 0);   // the game hid it the same way
            return 0;
        }
    }
    return orig_Show(self, edx);
}

static void Install(void)
{
    char dir[MAX_PATH], path[MAX_PATH + 32];
    int pct;
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* sl = strrchr(dir, '\\'); if (sl) *(sl + 1) = 0;
    snprintf(path, sizeof path, "%sW3HideFullHP.ini", dir);
    g_debug = GetPrivateProfileIntA("W3HideFullHP", "Debug", 0, path);
    if (g_debug) { snprintf(path, sizeof path, "%sW3HideFullHP.log", dir); g_log = fopen(path, "w"); g_diagTick = GetTickCount(); }
    pct = GetPrivateProfileIntA("W3HideFullHP", "HideAbovePercent", 95, path);
    if (pct < 1)   { logf_("HideAbovePercent %d is out of range, using 1", pct);   pct = 1; }
    if (pct > 100) { logf_("HideAbovePercent %d is out of range, using 100", pct); pct = 100; }
    g_threshold = (float)pct / 100.0f;

    g_base = (u32)GetModuleHandleA("Game.dll");
    u32 build = GetGameBuild();
    logf_("W3HideFullHP 1.0  Game.dll build %u", build);
    for (size_t i = 0; i < sizeof kAddrs / sizeof kAddrs[0]; i++) if (kAddrs[i].build == build) A = &kAddrs[i];
    if (!g_base || !A) { logf_("unsupported game version, doing nothing (need 1.27b, Game.dll 1.27.1.7085)"); return; }
    g_size = ImageSize(g_base);
    u32 vt = g_base + A->vtStatBar;
    if (!g_size || IsBadReadPtr((void*)vt, 0x80)) { logf_("health bar class not found"); return; }
    orig_Show = (Show_t)*(u32*)(vt + 0x68);
    orig_Hide = (Show_t)*(u32*)(vt + 0x64);
    if (!orig_Show || !orig_Hide) { logf_("health bar show/hide not found"); return; }
    u32 h = (u32)Show_hook;
    WriteMem(vt + 0x68, &h, 4);
    logf_("hiding health bars above %d%%", pct);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        { HMODULE self; GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCSTR)DllMain, &self); }
        Install();
    }
    return TRUE;
}
