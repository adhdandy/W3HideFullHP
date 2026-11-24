# W3HideFullHP changelog

Forked from [W3ManaBars](https://github.com/Hr0ffT/W3ManaBars) by Hr0ffT.

- **1.0** - First release.

    - Hides a unit's health bar while its HP is above `HideAbovePercent` (default 95 %), shows it again at or
      below. Strict `>`: a unit at exactly the threshold still shows its bar.
    - Supported patch: 1.27b (Game.dll 1.27.1.7085). Does nothing on any other version.
    - The mana bar feature of the upstream mod has been removed. The `CStatBar` vtable hook and the version
      check are kept.
    - New hook point: `CStatBar::Show` (vtable +0x68) instead of `SetUnit`, because the game sets the bar
      visible right after binding it. HP comes from the game's own bar fill fraction (`life / max life`).
    - Settings: `HideAbovePercent` (1..100, 100 = never hide) and `Debug`. No per-bar state, no per-frame heap
      allocation.
    - Rewritten README, renamed source, `.mix`, `.ini` and log file to `W3HideFullHP`.

    Upstream history (W3ManaBars): 1.0 - first release, mana bars under unit health bars for 1.26a, 1.27b
    and 1.28.5.
