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
| 2026-10-17 | Phase 2: title (pixel DOORS logo), reception, 9 room templates, doors, drawers, keys, HUD, fades | PyBoy, button script | Title and logo on both consoles; the reception with the HUD reading DOOR 000, door 1 locked (padlock), its key on the wall behind the desk |
| 2026-10-17 | same | `bot.py`, 40 rooms, both consoles | The bot plays to room 40 on GBC and DMG with the same rooms: 8 locked doors (keys from drawers and in the open), walking back from room 3 to 2 and returning. Screenshots of 24 rooms checked by eye: doors, plates, padlocks, keys, side and top doors, centred narrow halls |
| 2026-10-17 | same, recorded 30-room run | `build.ps1 -Test` (AUTOTEST ROMs) | BGB, all exception breaks, `.gbc` as GBC, `.gb` as DMG and as GBC: no break in 15 s while replaying the run. `scroll-test.py`: 2,500 (DMG) / 2,251 (GBC) frames compared through 30 rooms, **0 wrong background pixels**. VBlank: DMG 8 of 10, GBC 6 of 10. Replay reaches room 30 at input frame 5,371 on both consoles |
| | | Owner, by hand | Not yet: play `build\gbc\doors.gbc` and `build\dmg\doors.gb` |

## Test checklist for new builds

1. `.\build.ps1 -Test`: both ROMs build without warnings, headers valid; on the AUTOTEST ROMs the BGB check
   passes (`doors.gbc` as GBC, `doors.gb` as DMG and as GBC), `scroll-test.py` finds no wrong pixels,
   `vblank-budget.py` stays under 10 lines and the recorded run still reaches room 30. Look at the screenshots
   in `build/screens/`. If the recorded run fails after a deliberate change, re-record it
   (`.\scripts\record-run.ps1`).
2. PyBoy (`scripts/run-test.py`) with a button script for whatever changed.
3. In `tools\bgb\bgb64.exe` by hand for anything interactive.
4. SameBoy / Emulicious for anything timing-sensitive.
5. (Once the save exists) save, power off, reload: data intact; a corrupted save is detected and reset.

## Known issues

- **The DMG floor is busy**: the wood-plank pattern in greys is noisy next to the white-headed player. To revisit
  with real art (ASSETS.md).
- **Tables look like dark blocks** in the placeholder tileset.
- **Camera jumps of more than 4 px per frame** aren't streamed (by design, ARCHITECTURE.md); anything that
  teleports the camera must redraw with `bg_draw_full()` behind a fade, as room changes do.
- **Only one tileset** is wired up (`tileset_load()` in `room.c`); adding a second needs a switch on the id.
- **VBlank headroom on DMG is 2 scanlines.** Every frame writes either one column/row or up to 20 single tiles.
  Effects that must happen in VBlank on top of that (palette changes for the light flicker) need the writes
  made faster first, e.g. dropping the per-byte STAT check while LY shows enough VBlank left.
- **The room order repeats in cycles** (e.g. turn_right → hall_we → left_to_top → bedroom → …): with 9
  templates there are only 2–3 per entry wall, and the no-repeat rule often leaves one choice. More templates
  fix it; aim for at least 4–5 per entry wall.
- **ROM bank 0 is 74% full on GBC.** Engine code has to start moving into banked ROM (GBDK `__banked`
  functions or `#pragma bank`) in Phase 3 or 4.
- **GBC palette writes rely on running in VBlank** (`platform_vblank()` from `bg_flush`); BGB's exception
  breaks don't cover palette RAM, so this isn't machine-checked.
- **Door numbers are small** (3×5 digits on a 12 px plate): readable on screen, not big. The HUD shows the
  number too.
- **The bot cheats a little**: it knows which drawer holds the key from WRAM, though it still opens drawers in
  order until it finds it, as a player would.

## Roadmap

Each phase ends with both ROMs working in BGB and these docs updated.

| Phase | Content |
|---|---|
| 0. Toolchain (**done 2026-10-17**) | GBDK-2020, BGB, Python venv, PyBoy, SameBoy, Emulicious in `tools/`; Makefile for both ROMs; a "hello" ROM that boots on DMG and GBC in BGB with every exception break on |
| 1. Engine core (**done 2026-10-17**, owner's hands-on test pending) | VBlank queue, OAM DMA, joypad, player movement with collision on a metatile map, scrolling camera, one test room from Tiled; asset converter; automated scroll and VBlank tests |
| 2. Rooms and doors (**done 2026-10-17**, owner's hands-on test pending) | Title with logo, seeded room chaining (reception + 9 templates), door numbers, opening doors, walking back, keys in drawers and in the open, locked doors, the on-demand HUD, fades; the play-testing bot and recorded run |
| 3. Hiding and Rush/Ambush | Closets, beds, light flicker, Rush and Ambush on the door-to-door path, the health bar (on-demand HUD), death and the Guiding Light screen |
| 4. Darkness, Screech, Eyes | Dark rooms on both consoles, lighter/flashlight, Screech, Eyes |
| 5. Dupe, Halt, Hide, Timothy | The smaller entities |
| 6. The Library (50) | Figure, books, the padlock code |
| 7. Seek | Eyes on the walls, the chase sequences |
| 8. Courtyard, Greenhouse, Room 100 | Rooms 89–100, the fuses and the escape, the ending |
| 9. Economy and items | Gold, Knobs, Jeff's Shop (55), the elevator shop, the eight items, the on-demand HUD's item and gold parts, SRAM save |
| 10. Audio | hUGEDriver music and SFX throughout |
| 11. Polish | Title screen extras, achievements (paying Knobs), statistics, Super Game Boy border for the DMG ROM, balance; more room templates (an ongoing job: see known issues) |

Audio can start earlier in parallel once the engine core exists.
