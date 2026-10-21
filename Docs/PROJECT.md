# Project overview and decisions

## What it is

A demake of the Roblox horror game DOORS (LSPLASH) for the Game Boy Color, with an original Game Boy (DMG)
version built from the same source. It's written from scratch in C with GBDK-2020, with assembly where speed
matters, and must run on **real hardware**, not only in emulators. The owner has no Game Boy to test on, so
hardware correctness comes from accurate emulators (BGB, SameBoy, Emulicious) and from following the hardware
rules in DEBUGGING.md strictly.

GB Studio was considered and rejected: its engine limits (fixed actor logic, scene model, no custom
generation) get in the way of procedural rooms, Figure's hearing, Seek's chase and the rest of a feature-full
demake.

The first target is the **Hotel**: the lobby and elevator through Door 100, with every entity, item and
special room of floor 1. See DESIGN.md.

Reference for DOORS mechanics: the owner's Minecraft (Paper plugin) remake of the whole Hotel,
`F:\Minecraft DOORS Project\_Plugin\` (README.md and `docs/`), and the DOORS wiki at https://doorsgame.wiki/
(the owner's choice; not the Fandom one).

## History

- **2026-10-17**: project started. Planning session: the decisions below were made with the owner, and these
  docs were written. Nothing is built yet. Found on the owner's PC: BGB 1.6.4 with DMG and GBC boot ROMs at
  `E:\ROMs and Emulators\_Emulators\BGB (GameBoy)`, devkitPro's MSYS2 (GNU make, git), Python 3.15, Java 25
  (Corretto), Tiled and GB Studio in `F:\Programs`. Not installed yet: GBDK-2020, PyBoy, SameBoy, Emulicious.
- **2026-10-17**: second round of questions: HUD, health, inventory, entity and item scope, shops, saving,
  extras. Answers are in the table below and in DESIGN.md.
- **2026-10-17**: **Phase 0 done.** GBDK-2020 4.5.0, BGB 1.6.6 (64-bit, the project's own copy), SameBoy 1.0.4,
  Emulicious and PyBoy 2.8.1 (Python 3.13 venv) in `tools/`. Makefile + `build.ps1` build both ROMs from one
  source; a hello ROM passes the header check, PyBoy, and BGB with every exception break on, as GBC, DMG, and
  the DMG ROM on GBC. `.gitignore` added (no git commands run; the owner created the repository).
- **2026-10-17**: **Phase 1 done** (engine core): asset converter for Tiled tilesets/rooms and sprite
  sheets, a placeholder hotel tileset and a 30×24 test room, the player, scrolling with collision. Automated
  tests: an AUTOTEST build that walks by itself, checked in BGB with every exception break, a pixel-exact
  scroll test and a VBlank budget measurement in PyBoy. The first scrolling version overran VBlank and was
  redesigned (ARCHITECTURE.md).
- **2026-10-17**: **Phase 2 done** (rooms and doors): title screen with a pixel version of the DOORS logo, the
  reception and 9 room templates chained by a seeded generator, numbered doors, drawers, keys, locked doors,
  walking back, the on-demand HUD, fades. The wallpaper became the Minecraft version's green diamond texture
  (owner's request). A play-testing bot records a 30-room run that AUTOTEST builds replay, so BGB checks the
  whole game, not just a walk around one room.
- **2026-10-17/18**: **Phase 3 done** (hiding, Rush, Ambush): third round of questions (sound, closet view,
  beds, Hide, sprite size, Ambush's cue, death, music; answers below). Closets and beds, Hide, Rush and Ambush
  on the door-to-door route with the light flicker and rumble, the health bar, jumpscares pixelated from the
  wiki pictures the owner linked, the Guiding Light with Try again / Title. hUGEDriver and hUGETracker came in
  early: the owner supplied six MIDI files (`assets/music/midi/`); the Lobby Theme and the Guiding Light were
  arranged by hand into `.uge` songs (owner's choice over a converter script), the rest wait for their phases.
  `game`, `run` and `title` moved into banked ROM to make room for the driver. The bot learned to hide, and
  the recorded run now ends with a death and a new run, so BGB checks the death screen too.
- **2026-10-18**: **Phase 4 done** (darkness, Screech, Eyes): fourth round of questions (answers below).
  Dark rooms black except what a light reaches, on both consoles; the glow, and the lighter and flashlight
  behind a debug switch until the items exist; Screech and Eyes from the Minecraft resource pack's pictures,
  with the Screech jumpscare. From the owner's first play: the title's last note rang on after Start (fixed),
  the hotel ambience was removed, and the door numbers were redone (the bottom row of every digit was being
  written past its buffer; now a dark header with brass numbers over a whole, shorter door). Measuring the
  DMG with PyBoy hooks found the HUD rebuilding its line every frame (~45 of 154 scanlines) and slow sprite
  and light code; all fixed. The test seed changed to one with dark rooms and Eyes, and the bot's recording
  moved to a hook on `input_update` after it drifted by one input.
- **2026-10-18**: the HUD's status line and the sound effects' scripts moved out of bank 0 (owner's request;
  GBC bank 0 90% → 84%). Then **Phase 5 done** (Dupe, Halt, Timothy): fifth round of questions and art
  mock-ups (answers below). Five Dupe rooms with 2–3 doors, the run picking the real one; Dupe's wrong numbers,
  his full-screen face, numberless fakes; Halt's blue-lit sideways hallway with TURN AROUND and the
  Minecraft version's spacing sum scaled to the screen; Timothy's spider jumping out of a drawer. Art from the
  resource pack (Halt 24×32, Timothy 8×8, Dupe's face). The test seed changed to 0x1331 (Dupe rooms, Halt's
  door) and the bot learned the hallway and the wrong doors. Halt's hallway dropped DMG frames: drawing a
  sprite was cheapened for every room.
- **2026-10-18**: **Phase 6 done** (the Library): sixth round of questions and art mock-ups (answers below).
  Room 50, 30×27 metatiles of bookcases with a carpet, the desk and paper, three closets, the padlocked door
  51; eight books and a paper per run, the notes panel (Start), the five-wheel padlock; Figure, who hears
  walking anywhere in the room and is fooled by sneaking (hold B), with his footsteps, an edge arrow, the entry
  hint and his full-screen face, drawn from the two pictures the owner linked; the owner's *Unhinged (Library
  Theme)* arranged. GBC's WRAM overflowed into the stack on the first build (romusage only said 100%): the
  room map limit went from 3,072 to 2,816 metatiles. The ROM grew to 128 KB. A `game_warp` poke and
  `scripts/library-test.py` test the room, since the recorded run stops at room 30.
- **2026-10-18**: **Sprite sheets per room**, before Phase 7 (owner's request, four questions). Sprite VRAM
  had every sheet loaded all the time (144 tiles, 7 of 8 GBC palettes), with Halt and Figure borrowing
  Rush's tiles. Now each door loads only what that room can show (worst case 112 tiles, 6 palettes), Timothy
  loads when he springs, and the build fails if any room's set could outgrow VRAM. Found on the way: walking
  back into the Library after leaving it drew the glints from Rush's tiles. `scripts/sprite-test.py` checks it.
- **2026-10-18**: the Guiding Light's typing sound removed (it clashed with the song), and open side doors now
  look like open top-wall doors with their number kept (owner's requests, from play).
- **2026-10-18**: **Phase 7 done** (Seek): four rounds of questions and art mock-ups (answers below). Chases at
  30–35 and 70–79 from three new authorable room kinds (start hallway, four-way rooms, end hallway) with seven
  default rooms; Seek's eyes on the walls before; his rise out of a puddle; the chase at 1.4× walking with him
  at 0.75 of it, the real door glowing and opening by itself, caught by touch or by being behind him; fire
  tiles; the slam; his eye as the jumpscare; the owner's *Here I Come* arranged. The room sheets' frame tables
  went into one pool to make the GBC's WRAM room. The recorded run now ends with Seek catching the bot.
- **2026-10-18**: **Phase 8a done** (the Courtyard and the Greenhouse). The owner split Phase 8 in two: 8a,
  rooms 89–99, then a stop for their play before 8b (Room 100 and the ending). Three rounds of questions and
  art mock-ups (answers below). The Courtyard (89) safe and rainy; the Greenhouse (90–99) dark throughout
  with Rush allowed (heard, not flickered), Ambush never, four-ways always Dupe, room 99 safe, metal doors,
  and Snares; both authorable, on a second tileset (`garden`) that keeps the hotel's door and drawer numbers.
  The GBC's stack got its room back by reading the font's ASCII table from ROM. Found on the way: an
  open-ended "is this a chase room" test loaded Seek's sheet in the Greenhouse and overran the frame pool.
- **2026-10-18**: the HUD's door number replaced by the run's time, and "GOT THE KEY." without a number (owner:
  the number made Dupe too easy). The status line no longer slides in on entering a room.

- **2026-10-18**: from the owner's play: Rush and Ambush now kill by line of sight at any distance (a far corner
  of a big room was safe); Figure's walk lifts both feet; a key underfoot can be picked up; the fire flickers;
  a door you're near lights up in the dark so its number reads.
- **2026-10-18**: **Phase 8b done** (Room 100 and the ending): four rounds of questions and art mock-ups
  (answers below). The warehouse on one floor with the lever, the gates and the sliding door, ten fuses and
  the breaker key, the breaker room Figure never enters, the Circuit Breaker, the power, the escape with
  Figure on your trail, the lift, the ending and the results; the `electrical` tileset; the owner's
  *Unhinged II* and Ending Theme arranged; two Greenhouse rooms so room 99 always has one door on top. The run
  can be won. The GBC's WRAM bank 1 got tight (the statics), so the room map shrank to 2,560 metatiles and
  Room 100 to 28×32; a new test measures the stack (140 bytes deep at most, ~439 free).

- **2026-10-18**: from the owner's first look at Room 100: the lift's bars are a folding lattice gate drawn over
  its doors (no gate row in front); the escape: Figure follows a trail from the breaker box (he'd walked through
  the breaker room's wall), you run at 1.2× walking and he at 0.94 of that; the owner's new Ending Theme MIDI
  arranged without arpeggios, balanced, playing once with no tail (previewed as MP3s rendered from the driver
  first). Found on the way: the last full test run had failed: the AUTOTEST build overflowed bank 0 by a few
  bytes; Rush's route tables and `room_template()` moved into a bank (bank 0 92%, 1.2 KB free).
- **2026-10-19**: **Phase 9a built** (items; the owner split Phase 9 into 9a items and 9b shops): five rounds
  of questions and a mock-up (answers below). Gold and the eight items in drawers and on key spots from the
  seed, the HUD's second row with the held item and gold, Select to cycle, B to use, Start to pause on the
  inventory; the lighter and flashlight with fuel, batteries, bandages, vitamins, lockpicks and the skeleton
  key at locked doors and Room 100's breaker door, the crucifix against every entity; the icons in the
  Library's shape glyphs, swapped at the door, so they cost no VRAM. `items-test.py`. The suite's DMG run had
  found two test faults first (a dropped Start, a Snare by a door), fixed.
- **2026-10-19**: **Phase 9b built** (shops): two rounds of questions, a mock-up and the two songs as MP3s
  (answers below; the owner asked for Elevator Jam to play once and the lift to leave, buying nothing, when it
  ends). The save in SRAM (Knobs and statistics, checksummed), Knobs paid at a run's end and listed on the
  Guiding Light and the results, the balance on the title, the lift before every run with its Knobs shop, Jeff's
  Shop at room 55 (a new `shop` room kind and tileset), the owner's Elevator Jam and Jeff's Shop Jingle arranged,
  the save's erase on the title. `shop-test.py`.
- **2026-10-19**: **Phase 10 (audio) closed and a memory pass.** The owner judged the audio good; every MIDI was
  already in. A general hurt sound, picked from three MP3s, now plays for every blow without a jumpscare. Memory
  for the owner's room authoring: the fade/dim maths and door plates moved into banks (GBC bank 0: 770 → 2,495
  bytes free), and the run's per-room arrays and the GBC's dim cache moved to SRAM bank 1 (GBC WRAM: 254 → 1,409
  bytes under the stack). Next: the owner authors the rooms, then the game is considered finished.
- **2026-10-19**: **Decorations and the desk bell.** An armchair, a tall potted plant (redrawn from the owner's
  reference) and a luggage cart for the hotel tileset, and a counter tile with a bell that rings on A (a new
  `bell` object, the owner's two-tone ring), placed once in the reception's counter with the owner's permission.
  The tileset went from 124 to 152 tiles, past the 128 that sprites share: Rush and Ambush now go after it.

## Decisions and why

| Decision | Reason |
|---|---|
| Top-down view | Owner's choice (2026-10-17). Closets, drawers, beds, Figure's maze, Halt's hallway, the Library and the Seek chase all read clearly from above, and the Game Boy's tile and sprite hardware is built for it. Side view loses room layouts; a raycaster would run at ~10 FPS in a small window |
| Two ROMs: `doors.gbc` (GBC only) and `doors.gb` (DMG) | Owner's choice (2026-10-17) over one dual-mode ROM. Each ROM can use its console fully (the GBC one assumes double speed, colour palettes, VRAM bank 1 and 32 KB WRAM) without runtime checks everywhere. Same source, compile-time switch (`CGB` / `DMG` target). Every feature has to be tested on both |
| GBDK-2020 (C via SDCC) plus assembly | Owner's choice (2026-10-17). C gets a feature-full game written in reasonable time; hot paths (tile streaming, darkness, audio) go to assembly when profiling says so |
| Solo only, no link cable | Owner's choice (2026-10-17). Entity tuning (Rush's speed, closet counts) is for one player. The Minecraft version's party rules (straggler speed, the trailing-edge Void, `min(alive, 4)` closets) don't apply |
| MBC5 + battery-backed SRAM | Owner's choice (2026-10-17). Up to 8 MB ROM and 128 KB SRAM, supported by every emulator and flash cart. Saves Knobs, achievements, statistics and settings |
| Art drawn as PNG by Claude, converted at build time | Owner's choice (2026-10-17). The owner can repaint any PNG and rebuild. PNGs use the 4-colour Game Boy palette rules (ARCHITECTURE.md) |
| Music in hUGETracker format (hUGEDriver), arranged by Claude | Owner's choice (2026-10-17). hUGEDriver is the standard free driver, works with GBDK, and the owner can open the songs in hUGETracker |
| Rooms authored in Tiled, chained by a seeded generator | Owner's choice (2026-10-17). Hand-made room templates with object markers for doors, closets, drawers, beds; like the real Hotel and the Minecraft version's room schematics |
| Scrolling rooms | Owner's choice (2026-10-17) over one screen per room. Rush can be seen coming down a corridor, and the Library, Courtyard and Seek chase need big spaces |
| Test tools in `tools/`: PyBoy, SameBoy, Emulicious | Owner approved (2026-10-17). PyBoy lets Claude boot builds, press buttons and take screenshots headless; SameBoy and Emulicious cross-check timing-sensitive code |
| The project's own BGB in `tools/bgb` | Owner's request (2026-10-17): the owner's BGB install stays untouched for playing. The project copy keeps every exception break on (DEBUGGING.md) |
| HUD only on demand | Owner's choice (2026-10-17) over a fixed bar. The full 160×144 screen is play field; a strip slides in when health, item or gold changes, or on Select. Closer to DOORS' sparse UI |
| 100 HP health bar | Owner's choice (2026-10-17) over hearts. Matches DOORS, and Eyes' drain and Screech's 40% show exactly |
| Select cycles the held item, Start opens the inventory grid, B uses | Owner's choice (2026-10-17) |
| Guiding Light death screen | Owner's choice (2026-10-17): per-entity hint text, as in DOORS |
| Minor entities: Timothy only | Owner's choice (2026-10-17). Jack, Dread and Glitch left out; Glitch has no job solo |
| Items: lighter, flashlight, battery, vitamins, bandage, lockpicks, skeleton key, crucifix | Owner's choice (2026-10-17). Smoothie and candle left out (later-floor items in DOORS) |
| Pre-run Knobs shop in the elevator + Jeff's Shop for gold | Owner's choice (2026-10-17), as in DOORS |
| Jeff's Shop at Door 55 | Owner's choice (2026-10-17), as in the Minecraft version (DOORS has it at 52) |
| 8-direction movement | Owner's choice (2026-10-17). Smoother in big rooms and the Seek chase |
| No modifiers for now | Owner's choice (2026-10-17). Code should leave room for them |
| Achievements that pay Knobs once, plus statistics | Owner's choice (2026-10-17) |
| No mid-run save | Owner's choice (2026-10-17). SRAM is written only between runs and in the elevator shop |
| Super Game Boy border for the DMG ROM | Owner's choice (2026-10-17). No walkable lobby, no bestiary |
| Separate art per console (`.gbc.png` colour, `.dmg.png` greys, same layout) | Owner's choice (2026-10-17) over grey art + palettes or automatic greys: the best result on each console, at the cost of drawing everything twice |
| Player: blocky Roblox avatar, 16×16, 3/4 view, walk cycle of 2 frames + idle | Owner's choices (2026-10-17). 16×16 is 2 hardware sprites, leaving room on each scanline for entities |
| Camera centred on the player, stopping at room edges | Owner's choices (2026-10-17): "always centred" over a dead zone or look-ahead; stop at walls over showing void |
| Separate rooms with a quick fade between them | Owner's choice (2026-10-17) over a Zelda slide or a seamless corridor: fits memory, any room shape. Entities that travel between rooms (Rush) will be tracked by distance off-screen |
| Doors open with A | Owner's choice (2026-10-17) over opening automatically |
| Door numbers on the door and in the HUD | Owner's choice (2026-10-17). Layout (owner, 2026-10-18, from three mock-ups): brass digits on a dark header over a whole, shorter door, the knob 2 px higher; the full-width plate made the door look cut off |
| Keys in drawers or lying in the open, mostly drawers (75%) | Owner's choice (2026-10-17). Lock chance 18% and no template repeated within 3 rooms, both from the Minecraft config |
| Exits go up or sideways, never down | Owner's choice (2026-10-17): doors read clearly in 3/4 view and progress is always "onward" |
| Tuning after the owner's play (2026-10-20): Seek at 0.80 of your chase speed (was 0.75; 0.85 made the owner's fire zigzag in `seek_end` a dead heat: the bot was caught there on DMG); Hide's heartbeat from 3 s and GET OUT at 8 s (were 7 and 12, the Minecraft numbers); gold 60% rarer (drawers 14%, loose spots 8%; were 35 and 20) | Owner's choices (Seek "too easy to survive"; the speed and Hide's limit picked from options) |
| Halt more forgiving: 55 frames to turn (was 40) and 1.33 px/frame (was 1.4), so legs start him 81–85 px away | Owner's picks (2026-10-20), after finding him slightly too unforgiving |
| Sprites are hidden where the HUD's window is up, as they already were under the Library's and Room 100's panels | Owner (2026-10-20): the player drew over the popup in the reception |
| The title no longer shows "GBC" / "DMG" in its bottom-right corner | Owner (2026-10-20): redundant now that each ROM is for one console |
| Eyes appears over walkable floor: the doors' midpoint, or the walkable cell nearest it when that's void or wall | Owner (2026-10-20): his rooms have void inside the map (L shapes, rings) |
| A lit lighter or flashlight goes out by itself on entering a room that isn't dark | Owner (2026-10-20): so it wastes no fuel or charge |
| Up to 4 key spots (ground loot) per room, up to 9 light pools in Room 100 | Owner's choice (2026-10-20), when their rooms needed them (was 2 and 6); drawers and key spots still at most 8 together, and one pool still changes a frame |
| When no room of the kind needed can be entered from the wall opposite the exit, a room entered from the bottom follows instead | Owner (2026-10-20): e.g. one `seek_end` room for four-ways leaving by any wall; slightly disorienting, accepted. A matching room is always preferred, so seeds with matching rooms keep their layouts |
| The reception (room 0) now, the elevator later | Owner's choice (2026-10-17). Door 1 always locked, key on the wall behind the desk |
| About 8 room templates to start | Owner's choice (2026-10-17); 9 plus the reception were made. More are needed for variety (STATUS.md) |
| Wallpaper = the Minecraft version's `green_terracotta` diamond texture | Owner's request (2026-10-17) |
| Title screen with the DOORS logo, pixelated | Owner's request (2026-10-17), from the logo URL they gave. The logo belongs to LSPLASH; this is a fan project |
| Title screen and seed from the player's timing | A Game Boy powers on identically, so the frame count and divider register when Start is pressed are the only randomness |
| A play-testing bot and a recorded run for AUTOTEST builds | Lets BGB, which can't press buttons, check the whole game under its hardware-rule breaks, and catches anything that breaks a full playthrough |
| hUGEDriver brought in at Phase 3, not Phase 10 | Owner's choice (2026-10-17): Rush and Ambush need sound cues, and SFX written for the real driver don't need redoing. Phase 3 had SFX, a quiet hotel ambience (removed in Phase 4) and a short title theme |
| Hiding darkens the room (no slats overlay) | Owner's choice (2026-10-17) over a slats view or just hiding the sprite |
| Closets and beds both hide the player from Phase 3 | Owner's choice (2026-10-17) |
| Hide (the entity) moved into Phase 3 | Owner's choice (2026-10-17): otherwise a closet is always safe. MC numbers: 12 s, heartbeat from 7 s, 40 HP, banned from that closet |
| Rush and Ambush are 32×32 sprites | Owner's choice (2026-10-17): twice the player's size, 8 hardware sprites, leaves room per scanline for other entities |
| Ambush: green-tinted flicker plus his own sound | Owner's choice (2026-10-17), as in DOORS (the Minecraft version gives him no flicker). On DMG the flicker can't be green, so his sound is what tells them apart |
| Death: jumpscare, then the Guiding Light, then Try again / Title | Owner's choice (2026-10-17) |
| The owner's MIDI files become the music, arranged by hand per song | Owner (2026-10-17): supplied `Lobby Theme (Title Screen)`, `Guiding Light (Death Theme)`, `Elevator Jam`, `Jeff's Shop Jingle`, `Here I Come (Seek Chase)`, `Ending Theme`. Chose a hand arrangement per song (4 channels, SFX borrowing three) over a reusable converter. Each lands in the phase that uses it |
| No music in the rooms | Owner (2026-10-18): the hotel ambience "doesn't work very well"; removed. The title and Guiding Light songs stay |
| Dark rooms: black except the tiles a light reaches | Owner's choice (2026-10-18) over a dimmed room or a palette circle. The same tile-based method on both consoles |
| No items in Phase 4: a small glow (5×5 tiles) in the dark | Owner's choice (2026-10-18). The lighter (radius 3) and flashlight (a cone 7 long, 5 wide) are built now behind a debug switch (owner's choice) and come as items in Phase 9 |
| Dark rooms 10% from room 15 | Owner's choice (2026-10-18), the Minecraft rule. Rush and Ambush never spawn into one |
| Facing changes only by walking | Owner's choice (2026-10-18): to face Screech you walk his way (no turn-in-place) |
| Screech: 3 s to face him, 40 HP; chance rises the longer you linger; 16×16; full-screen jumpscare on a bite | Owner's choices (2026-10-18). Minecraft numbers otherwise (20 s cooldown, hiding dismisses him) |
| Eyes: looking = the exact facing direction (±22.5°); 5 HP per 0.25 s after 0.5 s; 5% from room 8 at the room's middle, glowing in the dark | Owner's choices (2026-10-18). Gone when the exit door opens |
| Screech and Eyes art from the Minecraft resource pack's item textures | Owner (2026-10-18): `F:\Minecraft DOORS Project\_ResourcePack\assets\doors\textures\item\eyes.png` and `screech.png` |
| Dupe: new 2–3 door rooms; 35% per such room from room 6; on a miss the other doors carry no number and say "IT'S FAKE."; a wrong door is a full-screen jumpscare and 40 HP | Owner's choices (2026-10-18). Minecraft rules otherwise (wrong numbers one below or up to two above; ignores the lock, spends no key; the door then shows a garbled number) |
| Halt: a sideways, blue-lit hallway (grey on DMG); a flash and big "TURN AROUND" text; 60 HP a hit; 3% per room arms the next door, once a run; about 8 turn-arounds long; only ever a plain door (not locked, not a Dupe room) | Owner's choices (2026-10-18). Minecraft's spacing sum for where each leg starts; a hit puts him back at the leg's start; nothing else runs while he does |
| Timothy: 1 drawer in 200, once a run; a small spider jumps out of the drawer at you, 10 HP, never lethal; the drawer is empty after | Owner's choices (2026-10-18) |
| Dupe, Halt and Timothy art from the resource pack's item textures | Owner (2026-10-18): `dupe.png` (the jumpscare), `halt.png`, `timothypet.png` in `F:\Minecraft DOORS Project\_ResourcePack\assets\doors\textures\item\` |
| The Library: about 3×3 screens, 5 digits from 8 books and a paper, three closets, the code's books glinting within ~3 metatiles | Owner's choices (2026-10-18). DOORS' and the Minecraft version's puzzle (shapes without repeats, digits with; nothing rolls a code) |
| Figure: hold B to sneak (B is free there: items aren't needed in the Library); walking is heard anywhere; hunting outruns you; touching him is death with a full-screen face | Owner's choices (2026-10-18; first a Select toggle, then B). Minecraft rules otherwise: he finishes a hunt before taking a new noise, never finds a hidden player, a 3 s deafness on entering with the game's only tutorial line |
| Figure is visible, with footsteps and an arrow at the screen's edge while he's off screen | Owner's choice (2026-10-18) |
| The notes (Start) don't pause; the padlock turns both ways (Up/Down); a wrong code resets the wheels silently | Owner's choices (2026-10-18). The notes and the padlock share a panel over the bottom of the screen (owner's pick from mock-ups), so the room and Figure stay in view |
| Library art: bookcases in three spine mixes on the hotel floor and carpet, a desk with the paper, a numberless padlocked door; Figure 32×48 (pick A of two sizes); his scare as mocked up | Owner's picks (2026-10-18). Figure after the DOORS wiki render and a frame of his jumpscare the owner linked; no pack texture exists |
| The Library's music: *Unhinged (Library Theme)*; *Unhinged II (Electrical Theme)* for Room 100 | Owner (2026-10-18): added both MIDIs to `assets/music/midi/` |
| Sprite sheets loaded per room (Timothy on demand), packed in the order asked for; the same layout on both consoles; a build-time check per set of sheets | Owner's choices (2026-10-18): "per room or on demand, whichever frees up more resources"; loading during the door's fade; GBC's second VRAM bank "for now" not used but kept in mind for later features; overflow caught at build time. Per room frees as much as on demand here: one room can hold every hotel entity at once, so only Rush/Ambush (never together) and rooms without them gain. The loader works mid-room anyway (Timothy), for later events |
| Open side doors look like open top-wall doors, number kept | Owner (2026-10-18): the number vanished when a side door opened (a plain floor gap). Picked from a mock-up |
| No typing sound on the Guiding Light | Owner (2026-10-18): it overlapped the Guiding Light song unpleasantly |
| Seek's chases at 30–35 and 70–79: start hallway, four-way rooms, end hallway; separate rooms with the usual fade | Owner's choices (2026-10-18), the Minecraft layout (*MC 08*) over one chase or a random start, and over one long scrolling map |
| A short rise (about 3 s, the player held, a puddle behind them); you run at 1.4× walking, he at 0.75 of that | Owner's choices (2026-10-18): the Minecraft speeds over a tighter chase or no boost |
| The right door: its number and a blue glow; real doors open on approach; fake doors stay shut | Owner's choices (2026-10-18) |
| Fire in the end hallway, deadly; every chase room and the fire placeable by the owner in Tiled | Owner (2026-10-18): "All rooms including the library and room 0 must be fully authorable" |
| Seek 24×32; eyes on the walls building up before (anywhere on a wall); footsteps only (no arrow); death by his eye full-screen; the door slams | Owner's choices (2026-10-18) |
| Seek all black with one white eye, round head; art from the DOORS wiki render and frames the owner linked | Owner (2026-10-18): "Ignore the red highlights, that was just the lighting in the render"; "remove the bump from the top of his head" |
| The chase music: *Here I Come (Seek Chase)* arranged on three channels, CH4 left for his footsteps | Owner's choice (2026-10-18) |
| Phase 8 in two halves: 8a the Courtyard and the Greenhouse (89–99), then a stop to play it; 8b Room 100 and the ending on the owner's word | Owner's choice (2026-10-18) |
| The Courtyard: Minecraft's safe walled yard (about 20×24, red-brick walls, a brick path, grass and trees), never dark, nothing hunts there and anything on its way is sent off | Owner's choices (2026-10-18; the red brick from the mock-up review) |
| The Greenhouse: fully dark rooms (the glow only); Rush allowed with no flicker, his rumble from when the flicker would be; Ambush never (one drawn there comes as a Rush); Minecraft's two shapes (a hall with six closets, a four-way with eight, always Dupe); room 99 safe | Owner's choices (2026-10-18) over a dim but visible night palette; "extend Rush's audio to start when the warning would usually be" |
| Snares: Minecraft rules (3 per room from authorable spots, 20% HP never killing, held 5 s with no walking or A, 4 s grace after the hold); a floor tile seen only in your light; the pack's `snare.png` pixel for pixel (DMG: light tops) | Owner's choices (2026-10-18; the art from a second mock-up "with a closer look to the minecraft sprite") |
| Greenhouse look: stone floor, windowed walls, planters, the hotel's closets (not metal lockers); doors 89–99 grey metal with a light-grey number | Owner's picks from mock-ups (2026-10-18) |
| Rain in the Courtyard and the Greenhouse: a sound in both, falling streaks over the Courtyard | Owner (2026-10-18): "Courtyard ambience and greenhouse ambience: Rain"; the streaks from the mock-up |
| No door number on the HUD: the run's time (MM:SS) instead; the key message without a number; the status line only on changes, not on entering | Owner (2026-10-18): the HUD's number "makes surviving Dupe too easy"; picked the run timer over leaving the space for gold, a longer bar or nothing |
| Rush and Ambush kill anyone not hidden whom a straight line from them reaches without crossing a wall tile, at any distance in the room | Owner (2026-10-18): the 96/128 px circle (the Minecraft kill radius, through walls) let a far corner of a big room be safe. "Only a full wall" over "anyone not hiding" or a wall test within a radius. Furniture doesn't block it |
| Room 100 (Phase 8b): one floor (no upstairs; the breaker room's key in a far corner); gates hold the shelving and a lever opens them, seals door 100 and sends Figure out at once | Owner's choices (2026-10-18), the Minecraft and DOORS start, over a two-floor map or Figure out from the start |
| 10 fuses, picked per run from authored spots, carried (a count in the HUD) and all put in with A at the box | Owner's choices (2026-10-18), as in DOORS |
| The tenth fuse: Figure gone without a scene and the warehouse sealed (the Minecraft version), then the breaker in peace | Owner's choice (2026-10-18) over DOORS' burning scene |
| The Circuit Breaker: Minecraft's (3 rounds, faster each, the sequence repeated after a pause, nothing checked until all ten match), two rows of 5, D-pad cursor, A flips, B leaves | Owner's choices (2026-10-18) over DOORS' progress bar or one round |
| The breaker room: Figure never goes in, doorway included | Owner's choice (2026-10-18), the Minecraft rule |
| The escape: Figure out of the door beside the box; you at 1.4× walking, he at 0.75 of it (Seek's numbers); closets don't save you; the lift at the far end, past the shelving | Owner's choices (2026-10-18) |
| The warehouse dim, with authored pools of light on the floor that blink and die on their own clocks | Owner's choice (2026-10-18) over fully dark or normally lit; pools over a whole-room flicker or wall lamps |
| Music: silent until the lever, then *Unhinged II* through the search, the breaker and the escape; the Ending Theme on the results | Owner's choice (2026-10-18) |
| The ending: the lift doors shut as Figure slams into them, the car shakes and falls; then a results screen ("YOU ESCAPED THE HOTEL", the time, the damage taken, the seed) over the Ending Theme, then the title | Owner's choices (2026-10-18); Knobs and achievements join the results in Phases 9 and 11 |
| Room 100's look: grey steel shelving with cardboard boxes, a yellow fuse cartridge (brighter and bigger than the mock-up) on a shelf, concrete-block walls with a pipe, a dark speckled floor with sand-coloured light pools, a copper breaker-room door with a warning sign and a padlock, barred lift doors, a wall lever | Owner's picks from mock-ups (2026-10-18; `build/preview/room100-mock-*.png`), shelving A over orange pallet racking |
| Figure walks stand, left foot up, stand, right foot up | Owner (2026-10-18): "only one of his feet raise up"; the new frame is the left-foot one's legs mirrored (picked from a mock-up) |
| A picks up a key you're standing on | Owner (2026-10-18): it had to be faced from the next cell |
| The fire's two flame tiles alternate on screen | Owner (2026-10-18): "the flame does not animate" |
| In a dark room, the top half of a door you're within 3 metatiles of is lit | Owner's choice (2026-10-18) over always-lit plates or a taller glow: the glow didn't reach a top-wall door's number |
| The lift behind a folding lattice gate drawn over its doors | Owner's pick from three mock-ups (2026-10-18): the bar gates in front "don't look good" |
| The escape: you at 1.2× walking (1.5 px/frame), Figure at 0.94 of that, on a trail that starts at the breaker box | Owner (2026-10-18): "speed him up … slow me down a bit so I can only slightly outrun him"; he had walked through the breaker room's wall to a trail that began outside it |
| The Ending Theme from the owner's second MIDI: the lead, the piano's chords as one soft note, the bass; plays once | Owner (2026-10-18): the first's background parts sounded bad and its tail rang on through the loop; picked after MP3 previews ("good enough"), one part lowered after the first ("far louder than the others") |
| Music previews as MP3 before building | Owner (2026-10-18): render the candidate arrangements (a GBDK player ROM with hUGEDriver, recorded in PyBoy, hUGETracker's ffmpeg) to choose by ear |
| B sneaks in Room 100 until the escape | My call (2026-10-18), following the owner's Library choice; confirmed by the owner with Phase 9 (below) |
| Phase 9 in two parts: 9a items (gold, the eight items, loot, the HUD's item and gold, Select/B/Start), 9b shops (Knobs, SRAM, the elevator shop, Jeff's Shop, their music) | Owner's choice (2026-10-18), like 8a/8b |
| In the Library and Room 100 (until the escape) B only sneaks; items are put away there and a light goes out on entering; the crucifix still works from the bag | Owner (2026-10-18): "Items are not needed in rooms 50 and 100 and can be de-equipped automatically, other than the crucifix" |
| Loot per drawer: gold 35%, an item 6%, a battery 4% (only once you own a flashlight), a bandage 4%, all rolled independently; loose spots hold one of gold, a battery or a bandage (never an item) | Owner (2026-10-18): the Minecraft table with the item chance doubled for one player; "Loose spots for gold/keys/bandage/battery only. Drawers for anything" |
| Item weights: lockpick 35, vitamins 28, lighter 18, flashlight 12, crucifix 4, skeleton key 3 | Owner (2026-10-18): the Minecraft weights plus a rare skeleton key |
| Carry limits: 3 lockpicks, 3 vitamins, 1 of the rest; what doesn't fit stays in the drawer | Owner (2026-10-18), the Minecraft limits |
| Gold: 5–15 a find, the counter stops at 999 | Owner (2026-10-18): "DOORS-sized" 10–50 over 5–15 or 1–5; 2026-10-20, after play: 700 gold by Jeff (his six items are 192), so 5–15 (~200 by room 55 searching everything) |
| The Library's shapes get tiles of their own (the hotel tileset's spare ones) | Owner (2026-10-20): the coin was a triangle in room 50; over letters or no icons there |
| The crucifix, lockpicks and skeleton key work from the bag (no need to hold them) | Owner (2026-10-18): "Both automatic" over the Minecraft hand rule |
| The crucifix fires on the first entity hit (not Snares or Timothy) and is spent: Rush, Ambush, Screech, Eyes leave and can't come back for 1 minute; Seek and Figure freeze 10 s; Halt's hallway ends; a Dupe door does nothing | Owner (2026-10-18): the Minecraft table "but 1 minute instead of 2 minutes (since our game is shorter)" |
| At a locked door: the room's key first, then a lockpick, then the skeleton key (5 uses). A lockpick or the skeleton key opens Room 100's breaker room too; never the Library's padlock | Owner's choices (2026-10-18) |
| Bandages heal and batteries refill the flashlight the moment you take them (a bandage at full health stays put) | Owner (2026-10-18), the Minecraft rule |
| Lighter 1 minute of fuel, no refill; flashlight 2 minutes a charge; B toggles the held light; it goes out when you Select away | Owner (2026-10-18), the Minecraft numbers and hand rule for lights |
| Vitamins: 1.3× speed for 10 s and +10 HP; stacks on Seek's and the escape's speed; not on itself | Owner's pick (2026-10-18) |
| Select cycles the lighter, flashlight and vitamins; Start pauses on a list of everything carried and the time | Owner's choice (2026-10-18; picks, the skeleton key and the crucifix are automatic, so only those three are ever "used") |
| A on a drawer takes everything that fits; the HUD says what | Owner's choice (2026-10-18) over a contents panel |
| The HUD: two rows, the status line as before and the held item with its meter and gold under it; 8×8 item icons; floor pickups from the pack's gold and bandage, a hand-drawn battery | Owner's picks from the mock-up (2026-10-18; `build/mockups/phase9a-hud-items.png`) |
| A bandage heals 25 HP | My call (2026-10-19): no number was asked; to tune by play |
| A burnt-out lighter is thrown away | My call (2026-10-19): with a carry limit of one, an empty lighter would keep a new one in its drawer |
| The battery pickup is a copper-top cell in the key's palette, not the mock-up's blue | My call (2026-10-19): the GBC's eight sprite palettes are all used in a hotel room; the key sheet holds the pickups with one extra palette (the bandage's) |
| The icons are the Library's shape glyphs, drawn differently outside the Library | My call (2026-10-19): the font's VRAM is full; the shapes are only ever shown in room 50, where nothing is held |
| Knobs are paid at a run's end only: a Knob per door reached, 50 for escaping, one per 10 gold carried; listed line by line on the death screen and the results | Owner (2026-10-19): the Minecraft rule. Achievements add Knobs in Phase 11 |
| Every run starts in the lift: a still picture of the car with the Knobs shop over it, Elevator Jam playing; its end opens the doors onto the reception | Owner's choice (2026-10-19) over a walkable lift room or a plain menu; Try again goes through it too |
| The lift shop sells the Minecraft version's four: 2 lockpicks 50, a lighter 50, a flashlight at half charge 100, 3 vitamins 100. Offers are picked, nothing is bought until BUY | Owner (2026-10-19), the Minecraft shop |
| Elevator Jam plays once; when it ends the lift leaves, buying nothing that wasn't confirmed | Owner (2026-10-19), on hearing the MP3 |
| Jeff's Shop: a lit, safe room; Jeff's eyes in a dark doorway behind a counter; 4 of the 6 items on it per run, one of each, for gold (lockpick 12, vitamins 15, lighter 20, flashlight 25, skeleton key 50, crucifix 70); A shows the price, A buys, B doesn't; carry limits before the gold; Jeff's Shop Jingle | Owner (2026-10-19): the Minecraft version's shop; art from the mock-up (`build/mockups/phase9b-shops.png`) |
| The save holds the Knobs and statistics (runs, escapes, best door, deaths by cause, Knobs earned), checksummed; a bad save starts fresh | Owner's choice (2026-10-19): statistics recorded from now so Phase 11's screen has history |
| Holding Select and B for 3 s on the title asks to erase the save | Owner's choice (2026-10-19) |
| The title shows the Knobs in its corner; the lift shop shows them too | Owner's choice (2026-10-19) |
| Jeff's Shop has two small tables where the mock-up had bookshelves | My call (2026-10-19): with the shelves the shop's tileset needed 8 GBC palettes (the limit is 7) |
| The counter is used from in front of it (A at its front, the desk under the items) | My call (2026-10-19): the counter is two metatiles deep, so its top is out of the player's reach |
| The knob glyph takes the font's last free tile (247; the icon glyphs moved up one, to 248-255) | My call (2026-10-19) |
| The hurt sound: a punchy note dropping fast (option "B, ow" of three), for Eyes (every 2nd hit, twice a second), Timothy, Halt, Snares (over the snap) and Hide's GET OUT (before its blast, one script: both are on CH1) | Owner's choice (2026-10-19) of the sound and where it plays |
| Decorations: an armchair (1×1), a tall plant in a red pot (1×2), a luggage cart (2×2), all solid, in the hotel's existing palettes with a dark backing like the bed; a desk bell on the counter's middle that rings on A, as often as pressed (a strike into a ring with a higher partial, two channels) | Owner's request and picks (2026-10-19: the art from mock-ups, sound "B, two-tone" of two). The reception's counter got the bell with the owner's one-time permission to edit that room |
| Room 100's closets use the hotel's (and the Greenhouse's) closet art and colours | Owner (2026-10-20). Only the closets used Room 100's lighter wood palette, so the hotel's wood takes its place: still 7 palettes |
| When a tileset passes 128 tiles, Rush and Ambush's sheets go after its last tile and the rest below 128 | My call (2026-10-19): the decorations took the hotel to 152 tiles; the worst hotel room needs 136 sprite tiles, which fit only split this way. The owner chose this engine change (with a full test run) over dropping pieces. The hotel tileset is now full (152 at most) |
| SRAM stays enabled from boot, bank 1 switched in as play memory (the run's per-room arrays, the GBC's dim cache); the save's bank 0 is switched in only while used | My call (2026-10-19), for the owner's request to free memory: a power-off's stray writes reach only the bank switched in, and the checksum still catches a damaged save. Rule: nothing an interrupt reads goes in SRAM |
| ROOM_MAX 3,584 metatiles since 2026-10-20 (Room 100 up to 1,408, the Library 1,536) | Owner's choice (2026-10-20) for their 36×39 Room 100, over moving the room map into SRAM or shrinking the room: about 1 KB of the WRAM the memory pass freed. The converter now also checks Figure's routes fit his byte-sized distances and 255-cell ring |
| (Until 2026-10-20) ROOM_MAX 2,560 metatiles (was 2,816); Room 100 at most 896 (28×32 by default) | My call (2026-10-18): Phase 8b's statics left the GBC ~150 bytes under the stack before it had ever been measured. Measured since: 140 bytes deep; with the change ~439 free. Can go back to 2,816 (Room 100 up to 1,024) if a bigger Room 100 is wanted |
| Project root `F:\Gameboy Homebrew\Doors` | Owner's choice (2026-10-17), laid out like the Half-Life Wii U project |

## Owner preferences (for AI assistants)

- **Never run `git commit` or `git push`**, and don't stage changes. The owner handles all git operations.
- The owner wants to be asked multiple-choice questions about anything important, big or small, at any point.
- Test in the project's own **BGB**, `tools\bgb\bgb64.exe`. **Never run or change the owner's BGB install**
  (`E:\ROMs and Emulators\_Emulators\BGB (GameBoy)`), which is for playing (owner, 2026-10-17).
- **The rooms are the owner's** (2026-10-19): never edit, regenerate, rename or delete `assets/rooms/*.tmx`, and
  never renumber or remove existing metatiles in `assets/tilesets/`. When the owner's rooms break a test, fix the
  test or re-record the replay, not the room; when a converter rule rejects one, tell the owner. Ask first about
  anything else that would change how an existing room plays.
- Downloaded tools go in `tools/`, which is gitignored.
- The owner has no real Game Boy, so nothing may rely on emulator leniency (see DEBUGGING.md).
- Documentation lives in this `Docs/` folder, written so the next session can continue without the chat history.
  The style follows `F:\Wii U Homebrew\Half-Life\Docs`.
