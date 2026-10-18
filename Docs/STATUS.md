# Status, known issues and roadmap

## Test results

| Date | Build | Where | Result |
|---|---|---|---|
| 2026-10-17 | Phase 0 hello ROM (text + a door sprite moved with the D-pad) | `check-rom.py` | Both ROMs 32 KB, MBC5+RAM+battery, 32 KB RAM; `.gbc` flag 0xC0, `.gb` 0x00; logo and checksums valid |
| 2026-10-17 | same | PyBoy 2.8.1 | `.gbc` in GBC mode and `.gb` in DMG mode boot, show the text, and the sprite moves with right/up held |
| 2026-10-17 | same | BGB 1.6.6 (tools), all exception breaks, SameBoy boot ROMs | `.gbc` as GBC, `.gb` as DMG, `.gb` as GBC: 10 s at full speed, no break. Screenshots correct (DMG green, GBC colours). A deliberately bad ROM fails the same check |
| | | SameBoy, Emulicious | Not run yet (GUI only); for the owner |
| 2026-10-17 | Phase 1: test room (30×24 metatiles), player, scrolling | PyBoy, button script | Both consoles: walks in all directions, collides with walls and furniture, passes the inner wall's doorway, camera centred and stopped at room edges, walk animation, mirrored right-facing frames. Art in colour on GBC, greys on DMG |
| 2026-10-17 | first scrolling version (column + row in the same VBlank) | `vblank-budget.py` | **Ran past VBlank**: up to 14 of 10 lines (DMG), 11 (GBC). Rewritten (ARCHITECTURE.md, "Scrolling") |
| 2026-10-17 | one column/row per frame, one-tile margin | `build.ps1 -Test` (AUTOTEST ROMs) | BGB, all exception breaks, `.gbc` as GBC, `.gb` as DMG and GBC: no break in 10 s (~11,000 frames, the route ~5 times). `scroll-test.py`: 1,924 frames compared per console, **0 wrong background pixels**. VBlank: DMG 8 of 10 lines, GBC 6 of 10 |
| | | Owner, by hand | Not yet: walk around in `tools\bgb\bgb64.exe build\gbc\doors.gbc` and `build\dmg\doors.gb` |

## Test checklist for new builds

1. `.\build.ps1 -Test`: both ROMs build without warnings, headers valid; on the AUTOTEST ROMs the BGB check
   passes (`doors.gbc` as GBC, `doors.gb` as DMG and as GBC), `scroll-test.py` finds no wrong pixels and
   `vblank-budget.py` stays under 10 lines. Look at the screenshots in `build/screens/`.
2. PyBoy (`scripts/run-test.py`) with a button script for whatever changed.
3. In `tools\bgb\bgb64.exe` by hand for anything interactive.
4. SameBoy / Emulicious for anything timing-sensitive.
5. (Once the save exists) save, power off, reload: data intact; a corrupted save is detected and reset.

## Known issues

- **The DMG floor is busy**: the wood-plank pattern in greys is noisy next to the white-headed player. To revisit
  with real art (ASSETS.md).
- **Tables look like dark blocks** in the placeholder tileset.
- **Camera jumps of more than 4 px per frame** aren't streamed (by design, ARCHITECTURE.md); anything that
  teleports the camera must use `bg_draw_full()` with the LCD off. Fades for that are still to do.
- **Only one tileset** is wired up (`tileset_load()` in `room.c`); adding a second needs a switch on the id.
- **VBlank headroom on DMG is 2 scanlines.** Anything else that must happen in VBlank (palette changes for
  the light flicker, the HUD) needs the scroll writes made faster first, e.g. dropping the per-byte STAT
  check while LY shows enough VBlank left.

## Roadmap

Each phase ends with both ROMs working in BGB and these docs updated.

| Phase | Content |
|---|---|
| 0. Toolchain (**done 2026-10-17**) | GBDK-2020, BGB, Python venv, PyBoy, SameBoy, Emulicious in `tools/`; Makefile for both ROMs; a "hello" ROM that boots on DMG and GBC in BGB with every exception break on |
| 1. Engine core (**done 2026-10-17**, owner's hands-on test pending) | VBlank queue, OAM DMA, joypad, player movement with collision on a metatile map, scrolling camera, one test room from Tiled; asset converter; automated scroll and VBlank tests |
| 2. Rooms and doors | Seeded room chaining, door numbers, opening doors, backtracking, keys and locked doors |
| 3. Hiding and Rush/Ambush | Closets, beds, light flicker, Rush and Ambush on the door-to-door path, the health bar (on-demand HUD), death and the Guiding Light screen |
| 4. Darkness, Screech, Eyes | Dark rooms on both consoles, lighter/flashlight, Screech, Eyes |
| 5. Dupe, Halt, Hide, Timothy | The smaller entities |
| 6. The Library (50) | Figure, books, the padlock code |
| 7. Seek | Eyes on the walls, the chase sequences |
| 8. Courtyard, Greenhouse, Room 100 | Rooms 89–100, the fuses and the escape, the ending |
| 9. Economy and items | Gold, Knobs, Jeff's Shop (55), the elevator shop, the eight items, the on-demand HUD's item and gold parts, SRAM save |
| 10. Audio | hUGEDriver music and SFX throughout |
| 11. Polish | Title screen, achievements (paying Knobs), statistics, Super Game Boy border for the DMG ROM, balance |

Audio can start earlier in parallel once the engine core exists.
