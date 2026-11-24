# W3HideFullHP

Hides a unit's health bar while its HP is above a threshold, for **Warcraft III 1.27b** (Game.dll 1.27.1.7085).

- A unit at full health shows no health bar at all. Damage it and the bar appears; heal it back and the bar
  disappears again. Default threshold: **95 %**.
- Only the drawing changes: the mod never takes a bar away from the game's own rules. Selection, the Alt toggle,
  hover, "always show health bars", fog of war and death all keep working — the mod only ever adds a reason to
  *hide* a bar, never a reason to show one.
- Everything happens in memory while the game runs. No game files are modified.

This is a fork of [W3ManaBars](https://github.com/Hr0ffT/W3ManaBars) by [Hr0ffT](https://github.com/Hr0ffT),
which draws mana bars under the health bars. All of the hook plumbing comes from there; the mana bar feature has
been removed.

## Requirements

- Warcraft III 1.27b, Game.dll **1.27.1.7085**. The mod checks the version and does nothing on any other patch.

## Installation

1. Copy `W3HideFullHP.mix` and `W3HideFullHP.ini` into the Warcraft III folder, next to `war3.exe`.
2. Start the game. The `.mix` is loaded automatically, no launcher needed.

Works alongside other `.mix` mods, for example [W3TrueWidescreen](https://github.com/Hr0ffT/W3TrueWidescreen),
[W3MinimapTweaks](https://github.com/Hr0ffT/W3MinimapTweaks) and
[W3MultislotQuickSave](https://github.com/Hr0ffT/W3MultislotQuickSave).

To uninstall, delete `W3HideFullHP.mix` and `W3HideFullHP.ini`.

## Settings

All in `W3HideFullHP.ini`:

| Setting | Default | What it does |
|---|---|---|
| `HideAbovePercent` | 95 | Hide a bar while HP is above this percentage (1..100). A unit at *exactly* this percentage still shows its bar, i.e. the comparison is strict `>`. `100` = never hide |
| `Debug` | 0 | 1 = write `W3HideFullHP.log` next to the game (bar show/suppress counters every 3 s, for bug reports) |

A missing `.ini`, a missing key or an unreadable value falls back to the default. Values outside 1..100 are
clamped, and the clamp is logged when `Debug=1`.

## How it works

The health bars over units are `CStatBar` frames; the small health bar over a unit is bar type 5. Every frame the
game walks its health bar list and calls `CStatBar::Show` on each bar — once when the bar is not shown yet, and
again on the "already shown" path. `Show` is therefore the one place where "this bar should be visible" is turned
into "this bar is visible", and it is called on every bar on every frame.

The mod hooks `Show`. For a unit health bar it reads the same HP fraction the game uses to fill the bar
(`life / max life`, the same two unit fields the JASS `GetUnitLife` / `GetUnitMaxLife` states read) and, if that
fraction is above the threshold, it refuses the show and hides the bar the way the game itself hides one
(`WANT` = 0 plus the game's own `Hide`). Otherwise it falls straight through to the game's `Show`.

Because this is a per-frame, stateless decision:

- the bar comes back on the first frame the unit drops to or below the threshold;
- nothing is cached per bar, so a bar rebound to another unit, a reused frame, a dying or removed unit cannot
  leave stale state behind;
- the mod never forces a bar visible, so all of the game's normal visibility rules still apply.

## Building from source

Requires a 32-bit MinGW (MSYS2 `mingw-w64-i686-gcc`). From the repository root:

```
i686-w64-mingw32-gcc -O2 -Wall -shared -static-libgcc -s -o W3HideFullHP.mix src/W3HideFullHP.c -lversion
```

The build is warning-free under `-Wall`. The result, `W3HideFullHP.mix`, is a 32-bit DLL that the game loads
from its own folder.

## Changelog

See [CHANGELOG.md](CHANGELOG.md).

## Credits

- [Hr0ffT](https://github.com/Hr0ffT) for [W3ManaBars](https://github.com/Hr0ffT/W3ManaBars), which this fork
  is based on: the version check, the `CStatBar` vtable lookup and the hook plumbing are his work.
- Blizzard Entertainment for Warcraft III.

## License

MIT, see [LICENSE](LICENSE). The original copyright notice is preserved.
