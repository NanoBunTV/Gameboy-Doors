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
| B sneaks in Room 100 until the escape | My call (2026-10-18), following the owner's Library choice; to decide with Phase 9's items, which want B |
| ROOM_MAX 2,560 metatiles (was 2,816); Room 100 at most 896 (28×32 by default) | My call (2026-10-18): Phase 8b's statics left the GBC ~150 bytes under the stack before it had ever been measured. Measured since: 140 bytes deep; with the change ~439 free. Can go back to 2,816 (Room 100 up to 1,024) if a bigger Room 100 is wanted |
| Project root `F:\Gameboy Homebrew\Doors` | Owner's choice (2026-10-17), laid out like the Half-Life Wii U project |

## Owner preferences (for AI assistants)

- **Never run `git commit` or `git push`**, and don't stage changes. The owner handles all git operations.
- The owner wants to be asked multiple-choice questions about anything important, big or small, at any point.
- Test in the project's own **BGB**, `tools\bgb\bgb64.exe`. **Never run or change the owner's BGB install**
  (`E:\ROMs and Emulators\_Emulators\BGB (GameBoy)`), which is for playing (owner, 2026-10-17).
- Downloaded tools go in `tools/`, which is gitignored.
- The owner has no real Game Boy, so nothing may rely on emulator leniency (see DEBUGGING.md).
- Documentation lives in this `Docs/` folder, written so the next session can continue without the chat history.
  The style follows `F:\Wii U Homebrew\Half-Life\Docs`.
