# Native MODS browser

The MODS button in a world's details page replaces the SessionStarter screen
with a mod browser. Back restores the selected world's native details page.
The background remains the game's own scene; no opaque fullscreen rectangle
or server query is added. Per-world activation and settings are implemented.

## Reused native components

RVAs apply only to the client with SHA-256
`5D9EF2F140B8ED49E30DBEE655841B89D08BAEC20F799A1817E171A18A850C77`.

| Component | Native source / behavior |
|---|---|
| Replaced screen | `c5ef50`, doPlaySessionStarterV2, shared by world/server navigation |
| Window | `a3f530`, fullscreen layer 9, native flags |
| Safe margins | `d41360` |
| Columns / rows / close | `a3c740` / `a3cf70` / `a40080` |
| Widths | 1550 browser pane (`1d920b0`), 600 list (`13d64e4`) |
| Details | `d027e0`, panel type 1, alpha 1, native textures/transparency |
| List entries | interactions from `cf8052..cf82c8`, highlight `d06ef0`, selection `a47170` event 4 |
| Hover sound | `a56600`, style `bundle+12620` |
| Headings | `b1da10` / `b25160`, style `bundle+11d44`, caps font `bundle+10bb8` |
| Back | native action/hint routing; mouse and Escape return one level |

Verified structure extents: frame 0x23 bytes (0x30 buffer), text up to 0x143
(0x150 buffer), row style up to 0x58 (0x60 buffer), panel up to 0xb4 (0xc0 buffer).
Do not reintroduce the old undersized text buffer.

The browser discovers DLLs recursively without loading them, lists six entries
per page, provides refresh and empty states, and displays declarative settings
using native difficulty controls. The provider advertisement is not drawn.
All custom labels and diagnostics use the 15-language loader catalogue.

## Historical validation

The native background/list/translucent details appearance was confirmed by the
user on November 13, 2026. That UI 0.3 build had hash
`BA5CA5381FBA33D9C99160FF39B98BDA559DD162298439E55B409DE1150E243E`;
it is historical evidence, not the current binary hash. Automatic window
capture failed with a FrameArrived/window-capture timeout at that stage.
Earlier backups were `build/dinput8-before-native-browser.dll` and
`build/loader-before-native-browser.cpp`; they are local build artifacts.

Current behavior and test commands are documented in the root README and the
world-profile/settings guides. Offline validation does not replace visual checks.
