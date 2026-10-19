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
| 2026-10-18 | Phase 3: closets and beds, Rush, Ambush, Hide, health bar, jumpscares, Guiding Light, hUGEDriver music (the owner's Lobby Theme and Guiding Light MIDIs, a hotel ambience) and sound effects | PyBoy, button scripts | Hiding in the reception closet dims the room; the heartbeat pulses from 7 s; at 12 s "GET OUT!", 40 HP off the bar, that closet refuses ("SOMETHING STOPS ME."). A forced Hide death: the Guiding Light's hint, Title back to the title screen. Both consoles |
| 2026-10-18 | same | `bot.py`, 40 rooms, both consoles | 5 spawns (Rush and Ambush): the bot hid every time and lost no health; Ambush (room 20) needed stepping out and back in during his rear pause. Screenshots while hidden show Rush crossing from the entry door to the exit and Ambush with the room tinted green (GBC) |
| 2026-10-18 | same, recorded run (30 rooms + a death) | `build.ps1 -Test` (AUTOTEST ROMs) | **All passed.** BGB, all exception breaks, `.gbc` as GBC, `.gb` as DMG and as GBC: 15 s, which covers the whole replay, the jumpscare, the Guiding Light and the new run (`death_run` reached at about 5 s). `scroll-test.py`: 1,652 (GBC) / 1,707 (DMG) frames compared, **0 wrong background pixels**. VBlank: DMG 8 of 10, GBC 6 of 10. Replay reaches room 30; every room change, hide and the death fall on the same input frame on both consoles |
| 2026-10-18 | same | PyBoy sound | Title RMS 13.8, hotel 6.3 (20 s each, `build/screens/*.wav`): the music plays. Whether it sounds right: for the owner's ears |
| | | Owner, by hand | Played: the title's last note rang on after Start, the ambience didn't work, the door numbers were cut off and the doors looked like plaques. All addressed in Phase 4 |
| 2026-10-18 | Phase 4: dark rooms (glow; lighter and flashlight on a debug switch), Screech, Eyes; title music cut on Start, ambience removed, door numbers redone | PyBoy, scripted (DEBUG builds, a dark room and Eyes forced in WRAM) | Both consoles: the glow, the lighter's circle and the flashlight's cone in 4 directions; Eyes lights his area. Screech: a bite costs 40 HP, shows his face and returns to the room as it was; facing him sends him away. Eyes drains 20 HP/s while faced (death: his Guiding Light, no jumpscare); Screech's last bite: his face, then his hint. After Start the sound measures 0.0 |
| 2026-10-18 | same | `bot.py`, 40 rooms, both consoles | Room 40 on full health on both (it faces Screech and walks round Eyes); door screenshots (`--door-shots`) show the brass numbers on every door |
| 2026-10-18 | same, recorded run (new seed 0x01D9: dark rooms 15, 17, 21, Eyes in 17, Ambush in 5) | `build.ps1 -Test` | **All passed.** BGB, all exception breaks, all three combinations, 15 s (the whole replay and the death). `scroll-test.py` over 11,000 frames, dark rooms included: 2,504 (GBC) / 2,491 (DMG) frames, **0 wrong pixels**. VBlank: DMG 8 of 10, GBC 6 of 10. Replay reaches room 30 at input frame 8,979 on both |
| 2026-10-18 | same | Lag frames per room (replay) | GBC: at most 2 in any dark room, 7 in Ambush's room. DMG: 2–5 in dark rooms with the glow or lighter; 32 of 215 in room 17 (flashlight + Eyes + Screech) |
| 2026-10-18 | Rush/Ambush position without per-frame divisions | Lag frames per room (replay), `build.ps1 -Test` | DMG Rush and Ambush rooms: **3–5** dropped frames (were 27–98: room 5, Ambush's 1,700 frames, 98 → 4). All tests pass; the replay is unchanged (room 30 at input frame 8,979) |
| 2026-10-18 | HUD status line (`hud_status.c`) and effect scripts (`sfx_data.c`) moved out of bank 0 | `build.ps1 -Test`, `romusage -g` | Bank 0: GBC 90% → **84%** (2.6 KB free), DMG 79% → **73%**. **All passed**: BGB, all three combinations, 15 s; scroll-test 0 wrong pixels (2,504 GBC / 2,583 DMG frames); VBlank DMG 8 of 10, GBC 6 of 10; replay reaches room 30 at input frame 8,979 on both |
| 2026-10-18 | Phase 5: Dupe rooms, Halt's hallway, Timothy; cheaper sprite drawing | PyBoy, scripted (the bot with screenshots; Timothy forced with `run_force_timothy`) | Both consoles: a numberless wrong door says "IT'S FAKE."; Dupe's door shows his wrong number (014 for door 13), opens, his face, 40 HP, then scribbles on the plate. The hallway: blue on GBC, grey on DMG, the entry shut, Halt at the screen's edge closing in, TURN AROUND over a white flash. Timothy hops onto the player and runs off along the floor: 100 → 90 HP, 5 → 1 |
| 2026-10-18 | same, recorded run (new seed 0x1331) | `bot.py`, 30 rooms + death | The ROM's rooms match `scripts/plan.py`'s. Through Halt's hallway untouched (it walks by `halt_dir`), Dupe met in room 12 (100 → 60 HP), hid 5 times, room 30 on 60 HP |
| 2026-10-18 | same | `build.ps1 -Test` | **All passed.** BGB, all exception breaks, all three combinations, 15 s. `scroll-test.py` now over 14,000 frames and through the hallway: 3,767 (GBC, 886 in the hallway) / 3,974 (DMG, 876) frames, **0 wrong pixels**. VBlank: DMG 8 of 10, GBC 6 of 10. Replay reaches room 30 at input frame 10,292 on both |
| 2026-10-18 | same | Lag frames per room (replay) | DMG: 3–7 in every room, the hallway 1 of 1,956, room 18 (dark, Eyes, the debug flashlight) 21 of 215. GBC: at most 7 (Ambush's room), the hallway 12 of 1,967 |
| 2026-10-18 | Phase 6: the Library (room 50), Figure, the books, the paper, the notes, the padlock, sneaking, the Library theme | `library-test.py` (DEBUG builds, warped to room 50), both consoles | **All passed.** Puzzle (Figure off): the paper and all 8 books say what the plan says in the HUD; the notes count 8 of 8; a wrong code resets the wheels; the right one opens door 051; walking out reaches room 51, Figure and the music left behind. Sneaking 15 s with B: never heard. Walking: he hears, hunts and kills after ~210 frames, his face, then his Guiding Light. Screenshots: the panels, the glint, the edge arrow, Figure close up |
| 2026-10-18 | same | `build.ps1 -Test` (now with `library-test.py`) | **All passed.** BGB, all exception breaks, all three combinations, 15 s. `scroll-test.py` 3,768 (GBC) / 3,974 (DMG) frames, **0 wrong pixels** (it now skips Ambush's green frames at dim level 0, which a 2-frame later run exposed). VBlank: DMG 8 of 10, GBC 6 of 10. Replay reaches room 30 at input frame 10,292 on both (rooms 0–48 plan as before) |
| 2026-10-18 | same | Lag frames in the Library | GBC: 0 of 900 standing, 0 of 600 sneaking, 1 hunted. DMG: 26 of 705 while Figure wanders (24 of them during his route search, 32 cells a frame; 40 a frame lagged 42) |
| 2026-10-18 | Sprite sheets loaded per room (Timothy mid-room), build-time sprite budgets | `build.ps1 -Test` | **All passed**, unchanged: BGB all three combinations, scroll-test 0 wrong pixels (3,768 GBC / 3,974 DMG frames), VBlank DMG 8 of 10, GBC 6 of 10, replay reaches room 30 at input frame 10,292 on both, `library-test.py` on both |
| 2026-10-18 | same | `sprite-test.py` (new), both consoles | **Passed**: every drawn sprite's sheet in VRAM through the replay, the Library and a forced Timothy. Sheets seen: Rush (5 rooms), Ambush, Eyes, Screech, the key, Halt and the letters, the marks and Figure, Timothy. 30 changes of a room's sheets, 13 tiles written on average, 38 at most. Black between rooms: GBC 42.5 frames on average (most 65), DMG 27.3 (most 69); not measured before the change. A deliberately lowered palette limit failed the build as it should |
| 2026-10-18 | Phase 7: Seek's chases (30–35, 70–79), the wall eyes, fire, the slam, his eye, *Here I Come*; side doors keep their number open; no typing sound | `seek-test.py` (new, DEBUG builds), both consoles | **All passed.** Eyes 1–6 in rooms 25–30, none in 23. The rise after ~253 frames walking up (the player held), the chase through 30–35 (every real door opened by itself) to 36: Seek gone, walking speed, door 36 shut. Standing still: caught (cause 9) ~77 frames after the rise. Round him: caught once he goes on into 31. Fire: cause 10. Screenshots: the rise, the glow, the fire, his eye. Side doors checked by screenshot (006/004 kept open, the next room's side entry numbered) |
| 2026-10-18 | same, recorded run re-recorded (room 29 is now always one top door; it ends with Seek catching the bot) | `build.ps1 -Test` (now with `seek-test.py`) | **All passed.** BGB all three combinations, 15 s. scroll-test 3,971 (GBC) / 4,192 (DMG) frames, **0 wrong pixels**. VBlank DMG 8 of 10, GBC 6 of 10. Replay reaches room 30 at input frame **10,287** on both. sprite-test: Seek, the puddle and the wall eye among the sheets seen, every drawn sprite's sheet in VRAM. library-test: the sneak check now allows Figure wandering into the player (cause 8, not hearing): with the new boot timing his path crossed the test's lane |
| 2026-10-18 | same | Lag frames in a chase (bot, 30–35) | GBC: 1 per room. DMG: 1–6 per room (the start hallway 6 of 61, the end hallway 3 of 279) |
| 2026-10-18 | Phase 8a: the Courtyard (89) and the Greenhouse (90–99), Snares, rain, Rush in the dark, metal doors, the `garden` tileset | `garden-test.py` (new, DEBUG builds), both consoles, before the HUD change | **All passed.** `plan.py` matches the ROM for rooms 0–100 (two seeds). 88 one top door; 89 lit, nothing planned, 7 sprites of rain; 90–98 dark with 3 Snares each in their rectangles, four-ways Dupe; 99 safe; no Halt. Snare: 100 → 80 HP, held (no movement for 120 frames of pushing and A), let go, not caught in the grace, 15 → 10. A queued Ambush comes as Rush in the dark; the bot hid. The bot walked 89 → 100 (DMG hid twice). Found and fixed: Seek's sheet loading in the Greenhouse overran the frame pool into `room_w`; the default rooms were tight enough for three Snares to wall a corner off |
| 2026-10-18 | The HUD's door number replaced by the run's time; "GOT THE KEY."; no slide-in on entering | Build only (owner: the suite on this build deferred to the next phase's) | Both ROMs, release and debug, build without warnings. GBC stack 301 bytes (release). **Not run**: `build.ps1 -Test` (BGB had passed all three combinations on it when the run was stopped) |
| | | Owner, by hand | Not yet: dark rooms, Screech, Eyes, the new doors, the silence after Start; Phase 5's Dupe rooms, Halt, Timothy; Phase 6's Library; Phase 7's chase; Phase 8a's Courtyard and Greenhouse and the HUD's timer (in a DEBUG build, poke `game_warp` to 50, 30 or 89, or play there) |

## Test checklist for new builds

1. `.\build.ps1 -Test`: both ROMs build without warnings, headers valid; on the AUTOTEST ROMs the BGB check
   passes (`doors.gbc` as GBC, `doors.gb` as DMG and as GBC), `scroll-test.py` finds no wrong pixels,
   `vblank-budget.py` stays under 10 lines, the recorded run still reaches room 30 and `sprite-test.py`
   finds every drawn sprite's sheet in VRAM; on the DEBUG ROMs `library-test.py` (room 50), `seek-test.py`
   (30–36) and `garden-test.py` (88–100) pass. Look at
   the screenshots
   in `build/screens/`. If the recorded run fails after a deliberate change, re-record it
   (`.\scripts\record-run.ps1`).
2. PyBoy (`scripts/run-test.py`) with a button script for whatever changed.
3. In `tools\bgb\bgb64.exe` by hand for anything interactive.
4. SameBoy / Emulicious for anything timing-sensitive.
5. (Once the save exists) save, power off, reload: data intact; a corrupted save is detected and reset.

## Known issues

- **The music arrangements**: the Lobby Theme's chords are arpeggios on one channel, and the Guiding Light
  drops the piano's chord stabs; both may want touching up in hUGETracker. The rooms are silent apart from
  sound effects (the ambience was removed, owner 2026-10-18).
- **Sound effects are first drafts** (`src/audio.c`), tuned by numbers, not by ear: door, drawer, key,
  locked, unlock, hide, heartbeat, "get out", hurt, the flicker's buzz, Ambush's howl, the scream, the
  typewriter, the menu blip, the rumble, and Phase 4's lights-out, Screech's psst and leaving, Eyes' drone and
  his hit.
- **DMG frame budget**: every room drops only a few frames on DMG now (Rush and Ambush rooms included, since
  `place()` stopped dividing every frame; Halt's hallway too), except the debug flashlight with Eyes in the
  room (about 10%).
  Phase 9 (the real flashlight) should look at the flashlight's redraw (`light_step`) and at sprite drawing
  (a 32×32 entity still costs ~22 scanlines); player collision (`room_solid_at`) is the next biggest cost.
- **Memory outlook** (2026-10-18; numbers from `romusage -g`):
  - *ROM banks 1+ show 100%*: not a problem. Autobanking adds banks as needed (MBC5 allows 8 MB); the ROM
    is 128 KB since Phase 6.
  - *Bank 0 (GBC 96%, 584 bytes left after Phase 8a; DMG 84%)*: a real, fixed 16 KB. **Phase 8b has to move
    something out first** (Room 100's code itself can be banked, but anything new in bank 0 won't fit long). Every phase so far added engine code there.
    The HUD's timers and status line (`hud_status.c`) and the sound effects' scripts (`sfx_data.c`) moved
    out on 2026-10-18 (~1 KB). Next candidates if it fills again: `room.c`'s door and plate painting, parts
    of `sprites.c`; only code that switches banks to read assets, reads text from its caller's bank, or runs
    the per-frame streaming has to stay. After Phase 5: GBC 88% (1.9 KB free), DMG 76% (the route tables for
    Dupe rooms' exits grew `room_templates.c`). After Phase 6: GBC 90% (1.6 KB free), DMG 77%. Phase 7 and 8a
    (the template table at 32 rooms, `audio.c`'s rain, the second tileset's switch) took it to 96%.
  - *WRAM*: DMG has 8 KB in all, ~1.6 KB free under the stack after Phase 6 (the room map takes 2.75 KB).
    **GBC is the tight one**: Phase 6's first build ran `_DATA` past 0xE000 into the stack (romusage just
    said 100%; DEBUGGING.md has how to read the map file). ROOM_MAX went from 3,072 to 2,816 metatiles to
    fit, leaving ~237 bytes for the stack. Phase 7: ~224 (the frame-table pool freed ~200 bytes, Seek and the
    wall eyes took about as much). Phase 8a: ~283 (the garden's ~40 bytes, paid for by reading the font's
    ASCII table from ROM instead of a 96-byte copy). Figure's route search costs no WRAM (the Library's unused room
    map tail). Later phases (Seek, the inventory, the shop) need more: the cartridge's 32 KB SRAM works as
    extra RAM on both consoles (enable it, use it, disable it); GBC's WRAM banks 2–7 (24 KB) are unused (the
    dim cache, 448 bytes, is the obvious first thing to move there); the room map could shrink further.
  - *Sprite VRAM* (per-room loading since 2026-10-18, ARCHITECTURE.md "Sprites"): a hotel room's worst case
    is 124 of 184 tiles and 7 of 8 GBC palettes with Seek's wall eye (was 144 and 7, all loaded all the time);
    a chase room 118 / 5; a Courtyard or Greenhouse room 120 / 7; the Library 72 / 3, Halt's hallway 68 / 3. The tileset is 124 tiles on GBC since the
    fire: past 128, sprites must stay under 128 (the build checks), which every set does today. Each set is checked at build time, so Seek or an item that doesn't fit fails the
    build. If palettes run out first: sheets with the same colours could share one, or GBC-only extras could
    go in the second VRAM bank.
- **The Courtyard and the Greenhouse rooms are defaults made by a script** (`courtyard`,
  `greenhouse_hall_s/w/e`, `greenhouse_4way_s/w/e`); the owner authors the real ones. Three Snares can still
  close a narrow way: then you walk over one (20 HP, 5 s) — fair, but worth keeping corridors two wide.
- **Snare, rain and Rush-in-the-dark numbers are guesses to play-test**: 3 Snares a room, 20 HP / never below
  10, held 5 s, 4 s grace; Rush's far rumble at volume 3 from his warning; the rain a soft noise drop every
  1–4 frames (by numbers, not by ear). Held by a Snare you can't turn to face Screech, so he can bite you:
  intended (the Minecraft "he sets you up"), but harsh in a fully dark room with only the glow.
- **The garden tileset is 7 palettes exactly**: a new colour there means sharing one (ASSETS.md).
- **Seek's numbers are guesses to play-test**: his reach 24 px (the Minecraft 2 blocks is 32; the camera
  shows only 72 px behind you), the puddle 64 px behind you, 45 frames' hold after the rise, 1.31 px/frame
  against your 1.75. Standing still after the rise, he has you in about 75 frames.
- **The chase rooms are defaults made by a script** (`seek_start`, `seek_4way_s/w/e`, `seek_end_s/w/e`):
  plain hotel rooms with tables; the owner authors the real ones. The four-ways entered from a side have two
  top doors; a run goes "straight" up about a third of the time.
- **Here I Come is a first arrangement** (`seek.uge`): the synth's octave pair as an octave arpeggio on CH1,
  the kick and hat as pulse blips on CH2, the bass on CH3. For the owner's ears.
- **Seek's side view is a stride and a passing pose**; his front view only shows during the rise (no chase
  route goes down).
- **The Library theme is a first arrangement** (`library.uge`): the muted guitar's ostinato, the strings as
  an arpeggio and a bass, CH4 left for Figure's footsteps. For the owner's ears.
- **Figure's numbers are guesses to play-test**: wandering 0.75 px/frame, hunting 1.44 (walking is 1.25,
  sneaking 0.63), a noise every 8 frames of walking, contact within 10 × 8 px of the feet. Standing still he
  can still blunder into you while wandering (the Minecraft version's rule too).
- **The Library's panel rows run together**: the padlock's cursor (the triangle glyph) touches the digit
  below it, and the notes' rows sit on consecutive lines like the Guiding Light's.
- **Room 49 is always a one-door room with its door on top** (hall_ns, bedroom, lounge, left_to_top,
  right_to_top today); the converter checks one exists for every entry wall.
- **The Library is one template** (`library.tmx`); the owner may author their own (ASSETS.md lists the
  rules the converter checks).
- **Dupe rooms are common**: 5 of the 14 normal templates have 2–3 doors, so about a third of rooms are
  multi-door rooms (with Dupe in 35% of those from room 6). Seed 0x1331's first 30 rooms have 8 (3 with Dupe). More
  single-door templates would thin them out; or a weight per template. The owner will rebalance this after
  authoring their own rooms (2026-10-18).
- **Halt's numbers are scaled to the screen**, not the Minecraft distances (ARCHITECTURE.md): he walks
  1.4 px/frame (1.12 × yours), you have 2/3 s to react, and each leg starts him 73–82 px away, at the screen's
  edge. A player who turns at once never gets touched; one who hesitates ~1 s is. Worth a play to judge.
- **Halt's hallway is plain**: 36 metatiles of empty hotel corridor tinted blue (no lamps or furniture), and
  the TURN AROUND letters are 8×16 sprites (two words, to stay under 10 sprites a line).
- **Timothy is 8×8** (owner's pick): small next to the player; he can be missed if you're not looking.
- **The room template table grew** with per-exit routes (bank 0, ~0.5 KB). If bank 0 fills again, the
  tables could move into a bank with `run.c` (the only reader besides `entity.c` and `eyes.c`).
- **Eyes floats at the route's middle**, even over furniture (top-down, it reads fine). A room whose route
  midpoint is in a wall would put him there; none does yet.
- **Screech's body is black**, so in the dark only his eyes and grin show (on purpose). On a lit tile (the
  glow) his outline shows.
- **Rush and Ambush take the straight line between doors** (the Minecraft version's first design), so they
  cut through furniture. Fine in the current rectangular rooms.
- **Ambush's flicker is only green on GBC.** On DMG his howl is the only difference from Rush's warning.
- **The Guiding Light's lines sit on consecutive rows**, which reads a little cramped.

- **The DMG floor is busy**: the wood-plank pattern in greys is noisy next to the white-headed player. To revisit
  with real art (ASSETS.md).
- **Tables look like dark blocks** in the placeholder tileset.
- **Camera jumps of more than 4 px per frame** aren't streamed (by design, ARCHITECTURE.md); anything that
  teleports the camera must redraw with `bg_draw_full()` behind a fade, as room changes do.
- **Two tilesets** (`hotel`, `garden`; `tileset_load()` in `room.c` switches on the id). A third needs a case
  there and `TILESET_*` in `assets.h`, and must keep the hotel's shared metatile numbers (the converter checks).
- **VBlank headroom on DMG is 2 scanlines.** Every frame writes either one column/row or up to 20 single tiles.
  Effects that must happen in VBlank on top of that (palette changes for the light flicker) need the writes
  made faster first, e.g. dropping the per-byte STAT check while LY shows enough VBlank left.
- **The room order repeats in cycles** (e.g. turn_right → hall_we → left_to_top → bedroom → …): with 9
  templates there are only 2–3 per entry wall, and the no-repeat rule often leaves one choice. More templates
  fix it; aim for at least 4–5 per entry wall.
- **GBC palette writes rely on running in VBlank** (`platform_vblank()` from `bg_flush`); BGB's exception
  breaks don't cover palette RAM, so this isn't machine-checked.
- **Door numbers are small** (3×5 digits on the door's header): readable on screen, not big. The HUD no longer
  shows it (owner, 2026-10-18: it gave Dupe away); it shows the run's time instead.
- **The bot cheats a little**: it knows which drawer holds the key from WRAM, though it still opens drawers in
  order until it finds it, as a player would.

## Roadmap

Each phase ends with both ROMs working in BGB and these docs updated.

| Phase | Content |
|---|---|
| 0. Toolchain (**done 2026-10-17**) | GBDK-2020, BGB, Python venv, PyBoy, SameBoy, Emulicious in `tools/`; Makefile for both ROMs; a "hello" ROM that boots on DMG and GBC in BGB with every exception break on |
| 1. Engine core (**done 2026-10-17**, owner's hands-on test pending) | VBlank queue, OAM DMA, joypad, player movement with collision on a metatile map, scrolling camera, one test room from Tiled; asset converter; automated scroll and VBlank tests |
| 2. Rooms and doors (**done 2026-10-17**, owner's hands-on test pending) | Title with logo, seeded room chaining (reception + 9 templates), door numbers, opening doors, walking back, keys in drawers and in the open, locked doors, the on-demand HUD, fades; the play-testing bot and recorded run |
| 3. Hiding and Rush/Ambush (**done 2026-10-18**, played by the owner: fixes in Phase 4) | Closets, beds, light flicker, Rush and Ambush on the door-to-door path, Hide (moved here from Phase 5), the health bar (on-demand HUD), jumpscares, death and the Guiding Light screen; hUGEDriver with the title and Guiding Light songs from the owner's MIDIs, a hotel ambience, sound effects (audio moved here from Phase 10) |
| 4. Darkness, Screech, Eyes (**done 2026-10-18**, owner's hands-on test pending) | Dark rooms on both consoles (black but for what's lit), the glow, the lighter and flashlight (debug switch until Phase 9), Screech with his jumpscare, Eyes; from the owner's first play: the title music cut on Start, the ambience removed, door numbers redone (layout B); DMG speed-ups (HUD, sprites, collision lookups, the light) |
| 5. Dupe, Halt, Timothy (**done 2026-10-18**, owner's hands-on test pending) | Five Dupe rooms (2–3 doors, the real one per run), Dupe's wrong numbers and full-screen face, numberless fakes; Halt's blue-lit sideways hallway with TURN AROUND; Timothy's spider in a drawer; the HUD and effect scripts out of bank 0; cheaper sprite drawing |
| 6. The Library (50) (**done 2026-10-18**, owner's hands-on test pending) | Room 50 with eight books, the paper, the notes panel and the five-wheel padlock; Figure (hearing, sneaking with B, hunting, footsteps, the edge arrow, his face); the Library theme; door 50 sealing; room 49 one-door; `game_warp` and `library-test.py` |
| 7. Seek (**done 2026-10-18**, owner's hands-on test pending) | Chases at 30–35 and 70–79 from authorable start, four-way and end rooms; eyes on the walls before; the rise; the chase (1.4× run, he at 0.75), the glowing real door that opens itself, caught by touch or from behind; fire; the slam; his eye; *Here I Come*; side doors keep their number open; the Guiding Light's typing sound gone; `seek-test.py` |
| 8a. Courtyard, Greenhouse (**done 2026-10-18**, owner's hands-on test pending) | Rooms 89–99: the safe, rainy Courtyard; the dark Greenhouse with Rush heard not flickered, no Ambush, Dupe four-ways, Snares, room 99 safe, metal doors; a second tileset; rain; `garden-test.py`. Also: the HUD's door number replaced by the run's time, "GOT THE KEY." |
| 8b. Room 100 (on the owner's word, questions first) | Room 100, the fuses and the escape, the ending; *Unhinged II* and the Ending Theme arranged |
| 9. Economy and items | Gold, Knobs, Jeff's Shop (55), the elevator shop, the eight items (the lighter and flashlight's light is built: give it fuel, batteries and a slot), the on-demand HUD's item and gold parts, SRAM save; the Elevator Jam and Jeff's Shop Jingle MIDIs arranged |
| 10. Audio | The rest: Seek's chase (Here I Come MIDI, Phase 7) and the Ending Theme (Phase 8) when those come; SFX for every entity; a pass by ear over everything |
| 11. Polish | Title screen extras, achievements (paying Knobs), statistics, Super Game Boy border for the DMG ROM, balance; more room templates (an ongoing job: see known issues) |

The owner's MIDI files for later phases are in `assets/music/midi/` (ASSETS.md, "Music").
