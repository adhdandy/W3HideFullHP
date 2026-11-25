# W3HideFullHP changelog

Forked from [W3ManaBars](https://github.com/Hr0ffT/W3ManaBars) by Hr0ffT.

- **1.1** - Works together with W3ManaBars.

    - Fix: with the upstream W3ManaBars mod installed, the mana bars over units disappeared as well. The first
      version hid the health bar frame (as the game does), and a frame's children are hidden with it; the mana
      bar is a child of the health bar.
    - The hide mechanism no longer touches the frame's visibility. It now keeps the frame shown and makes only the
      bar's own art transparent (frame alpha `+0x88` = 0, plus a refresh of the bar's fill and backing art
      objects through the engine's alpha-recompute slot). The child mana bar keeps its own alpha and is drawn.
    - The bar is still evaluated per frame and statelessly; at or below the threshold the alpha is put back to
      opaque.
    - No new settings. `HideAbovePercent` and `Debug` are unchanged.

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
