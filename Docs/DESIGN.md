# Game design: DOORS as a top-down Game Boy game

How each part of the Hotel becomes something a Game Boy can do. Numbers and behaviour come from the Minecraft
version (`F:\Minecraft DOORS Project\_Plugin\docs\`, cited as *MC 04* etc.) and the DOORS wiki
(https://doorsgame.wiki/), scaled from blocks to Game Boy pixels. Items marked **Open** are still to be decided
with the owner. Decisions settled with the owner on 2026-10-17 are listed in PROJECT.md as well.

## Scale and screen

- The screen is 160×144 pixels (20×18 tiles of 8×8), and **the whole screen is play field**.
- **One Minecraft block = one 16×16 metatile** (2×2 tiles). The player is a 16×16 sprite (two 8×16 hardware
  sprites). So a 5-block-wide corridor is 80 px, half the screen width, and DOORS distances (Rush's warning
  radius of 25 blocks = 400 px, 2.5 screens) translate directly.
- Walking speed is set so crossing a typical room takes about as long as in DOORS. No sprint, no jump (*MC 11*:
  everything in the Hotel is paced against a walk). As built: 1.25 px/frame = 75 px/s, about 4.7 blocks a
  second (Minecraft walks at 4.3); to tune in playtesting.
- **8-direction movement.** The facing direction (one of 8) is where the player "looks", which is what Eyes
  and Screech test.
- **3/4 view**, like Pokémon: walls the player faces from below show their front face, and the player's head
  may overlap a wall; only the feet collide.
- **Camera** centred on the player, stopping at the room's edges.
- **The player** is a blocky Roblox-style avatar (yellow head and arms, blue shirt, dark legs).
- The hardware background map is 32×32 tiles. Rooms larger than that stream new tile columns and rows in as
  the camera moves (ARCHITECTURE.md, "Scrolling").

## HUD (on demand)

There is no permanent HUD, which matches DOORS' sparse interface. A 2-tile strip (window layer) slides in from
the bottom for a few seconds when something changes, then slides out:

- health changes (hit, heal): health bar;
- held item changes or is used: item icon, plus its fuel/battery meter or uses left;
- gold picked up: gold count;
- messages ("IT'S LOCKED.", "GOT THE KEY.");
- Select held: everything (the run's time, health, item, gold).

**No door number** (owner, 2026-10-18): it told you the real door's number in a Dupe room ("too easy"), so you
read it off the door you came through, as in DOORS. The key message has none either.

As built (Phase 3, the time from Phase 8a): border line + one text row, the run's time (`12:34`, minutes and
seconds of real time since the run started, stopping at 99:59; owner's pick), a heart and an 8-tile health bar
(2 px steps), and a key icon at the right end while a key is carried. It slides in when health changes, with a
message, and while Select is held (no longer on entering a room). It covers the bottom 16 px of the play field (the window layer can't
shrink the background). The strip must not show during Halt's and Seek's cutscenes.

## Health and death

- **Health is a 100 HP bar**, as in DOORS, so Eyes' slow drain and Screech's 40% hit show exactly.
- **Death screen: the Guiding Light.** The blue screen with typewriter hint text naming what killed you and how
  to survive it, one set of lines per entity, as in DOORS. Then "Try again" (new run) or "Title". The lines are
  our own, in the spirit of the wiki's death messages, at most 18 characters each.
- **Jumpscare first** for entities that have one (owner, 2026-10-17): Rush and Ambush's face fills the
  screen for a second with a scream and a shake, then the Guiding Light. Hide has none (his "GET OUT!" is
  the scare).

## Rooms and generation

- Rooms are Tiled maps (`assets/rooms/*.tmx`) on a shared metatile set, with an object layer for: entry
  and exit doors, closets, beds, drawers/chests, light sources, lever gates, Dupe's side doors, and Halt and
  Seek markers. The room schema follows the Minecraft version's `RoomDefinition` (*MC README*, "Authoring a room").
- **Rooms are separate maps** with a quick fade to black between them (owner, 2026-10-17). Going back through
  the door you came in by returns to the previous room as you left it.
- A run is generated from a seed: each room's template, lock and key place are decided from (seed, room
  number), in order, because a room must be entered through the wall opposite the previous room's exit.
  Exits go **up or sideways** (owner): top-wall exits lead to rooms entered from the bottom, side exits to
  rooms entered from the opposite side. No template repeats within 3 rooms.
- **Room 0 is the reception**: door 1 is always locked and its key hangs on the wall behind the desk (DOORS'
  tutorial). The elevator before it comes with the economy phase.
- **The Library is room 50** (Phase 6, below). Room 49 always has one door, on its top wall.
- **Seek's chases are rooms 30–35 and 70–79** (Phase 7, below). Rooms 29 and 69 always have one door, on top.
- **The Courtyard is room 89 and the Greenhouse 90–99** (Phase 8a, below). Room 88 always has one door, on top.
- **Room 100 is the warehouse** (Phase 8b, below). Room 99 always has one door, on top.
- Fixed room still to come: **Jeff's Shop (55)**. Halt's hallway is a scripted sequence of its own.
  Jeff's Shop is at 55 as in the Minecraft version (owner's choice); DOORS has it at 52.
- **Doors**: every door shows its number in brass on a dark header above it, open or shut (the HUD doesn't:
  owner, 2026-10-18, it gave Dupe away). An open side door looks like an open top-wall door, number kept (owner, 2026-10-18). **A**
  opens a door (owner: press A, not automatic). Walking into an open doorway goes through.
- **Locked doors**: 18% of rooms that allow it lock their exit; the key is in that room, in a drawer (mostly)
  or lying in the open (owner: both, mostly drawers). Facing a locked door with A says "IT'S LOCKED." or, with
  the key, unlocks and opens it and uses the key up. **Drawers** are searched with A: "NOTHING." or the key.

## The Library (built in Phase 6)

Owner's choices (2026-10-18), after the Minecraft version (*MC 07*, *MC 10*):

- **Room 50**, about 3×3 screens (30×27 metatiles): six rows of bookcases either side of a red carpet, a
  desk with the paper near the top, the padlocked exit (door 51) at the far end, **three closets**. Door 50
  seals behind you. Nothing from the corridor follows you in: no Rush or Ambush (a queued one waits for the
  next room), Eyes, darkness, Dupe or Halt, and no Halt on door 50.
- **The puzzle (5 digits, 8 books, as in DOORS)**: eight books, each on a random bookcase, say "shape =
  digit" (eight shapes, all different; digits may repeat). The paper on the desk gives the order of five
  of the shapes (`1=▲ 2=✖ …`); together they give the code. A shelf with an unread book **glints when you're
  within about 3 metatiles**. A on it reads it; empty shelves say nothing.
- **Notes**: Start opens a panel over the bottom of the screen with the paper and every book read so far.
  **The game doesn't pause** while it's open.
- **The padlock**: A at the door opens its panel: Left/Right picks a wheel, Up/Down turns it, A tries the
  code, B leaves. A wrong code resets all five wheels and says nothing about which was wrong (no noise).
  The right one opens the door; walking out leaves Figure and the music behind.
- **Figure**: a 32×48 sprite (owner's pick A), visible when on screen, an **arrow at the screen's edge**
  points to him when he isn't, and his **footsteps** get louder as he nears and faster when he hunts. Blind:
  **walking is heard anywhere in the room; holding B sneaks** (half speed, silent); standing still and
  hiding are silent. He walks at a noise and finishes what he started (*MC 10*); **hunting he outruns you**
  (wandering he's slower than walking, faster than sneaking), so once he's coming, a closet is the answer.
  He never finds a hidden player (Hide's 12 s still applies). **Touching him is death**: his full-screen
  face (from the owner's reference frame), then the Guiding Light.
- The only tutorial in the game, as in *MC 07*: on entering, "IT CAN'T SEE YOU." then "HOLD B TO SNEAK.",
  with 3 s before he can hear you.
- **Music**: the owner's *Unhinged (Library Theme)*, arranged, from entering until you walk out.

## Seek's chases (built in Phase 7)

Owner's choices (2026-10-18), after the Minecraft version (*MC 08*):

- **Where**: rooms 30–35 and 70–79. Room 30 (70) is the **start hallway**, 31–34 (71–78) **four-way rooms**,
  35 (79) the **end hallway**; 36 (80) is safe. Each is its own room with the usual quick fade between (time
  stops in the fade, for him too). All three kinds are authored in Tiled (`seek_start`, `seek_4way`,
  `seek_end`), the fire too; nothing else happens in a chase room (no Rush, Eyes, dark, Dupe, Halt, locks).
- **Before**: Seek's eyes appear on the walls (anywhere on a wall), one in room 25, one more each room, six
  in the start hallway. Harmless; they blink.
- **The rise**: three quarters of the way up the start hallway (or past an authored `seek` point) the player
  stops, a black puddle spreads behind them and Seek rises out of it (about 3 s). Then his music, *Here I
  Come* (the owner's MIDI, arranged), and the chase.
- **Running**: the player runs at **1.4 times walking** for the whole chase; Seek follows the door-to-door
  line Rush takes at **0.75 of that**. He can't catch someone who keeps running: a wrong door, the fire or
  stopping is what gets you caught. You hear his footsteps, louder as he closes in (no arrow, owner's
  choice).
- **Four-way rooms**: three doors, only one real, with its number and a **blue glow** in front of it; the
  others are numberless fakes ("IT'S FAKE."). **The real door opens by itself** as you reach it.
- **Caught**: within his reach (1.5 metatiles), or anywhere in a room behind him (you got round him and he
  went on). Instant death: **his eye full-screen**, then the Guiding Light.
- **The end hallway**: patches of **fire** (deadly to walk into: his eye too, then a hint about taking the time
  to go round). Seek stops two metatiles short of its door; when you're through, **it slams** behind you, the
  music stops and you walk at normal speed again.
- **Art** (owner's picks from mock-ups): Seek all black, one white eye, 24×32 (back, side and front views),
  the eye on the wall, the puddle, the glow, two fire tiles, his eye as the jumpscare.

## The Courtyard and the Greenhouse (built in Phase 8a)

Owner's choices (2026-10-18), after the Minecraft version (*MC 09*):

- **Room 89, the Courtyard**: a big (21×24 metatiles) outdoor yard with red-brick walls, a brick path up the
  middle, grass and trees either side, and rain (a sound, and streaks falling over the screen). **Nothing
  hunts you there**: never dark, no Rush, Eyes, Dupe or Halt, and anything still on its way is sent off.
- **Rooms 90–99, the Greenhouse**: stone floor, windowed walls, planters, the hotel's closets (owner: not
  metal lockers), rain on the glass (a sound). **Every room is dark** (the usual black-but-for-your-light).
  Two shapes, both authorable: a straight hall with six closets and a four-way room (one entry, three
  exits) with eight; the four-ways **always have Dupe** behind their wrong doors.
- **Rush comes in the dark here**, with no flicker: his rumble starts when the flicker would have (owner) and
  grows as he nears, so you hear him from the start. **Ambush never comes**: one drawn here comes as a Rush.
  Screech, Eyes and Dupe as anywhere.
- **Room 99 is safe**: no Rush, Eyes, Dupe or Snares.
- **Doors 89–99 are metal**: grey, with a light-grey number on dark grey (owner's pick).
- **Snares**: three per Greenhouse room, picked per run from the floor cells the room's author marked. A
  floor tile (the Minecraft pack's `snare.png`, pixel for pixel), so **only seen where your light reaches**.
  Stepping on one: a snap, "WATCH YOUR STEP.", 20 HP but never below 10 (it can't kill), and **held for 5 s**
  (no walking, no A). For 4 s after it lets go no Snare catches you. Rush or Screech can still get you while
  you're held: that's the trap.

## Room 100 and the ending (built in Phase 8b)

Owner's choices (2026-10-18), after the Minecraft version (*MC 10*), on **one floor**:

- **The warehouse**, entered from the bottom (room 99 has one door, on top). Dim (a dark speckled floor), with
  **pools of light** that blink on now and then and die again, each on its own clock. Grey steel shelving
  with cardboard boxes; a line of **bar gates** holds the storage (where most shelves are), and Figure waits
  behind a **sliding metal door** (out of sight, as upstairs in the Minecraft version).
- **The lever** beside his cage is the one irreversible decision: the gates go (all but two in front of the
  lift), **door 100 seals**, "SOMETHING'S COMING.", the sliding door parts from the middle outward, and
  **Figure comes at once** toward where it was pulled. His Library rules: walking is heard anywhere, **B held
  sneaks** (until the escape), closets hide you. *Unhinged II* starts 2 s after.
- **Ten fuses** on shelves (picked per run from the room's shelves) and **the breaker room's key** on one of
  its key spots (far corners). Fuses are carried (an "F" count on the status line, "A FUSE. 5 OF 10.") and
  **A at the breaker box puts in all you carry** ("FUSES IN: 7 OF 10."). The key opens the breaker room's
  copper door (warning sign, padlock) and is spent.
- **The breaker room is where Figure never goes**, doorway included, and he can't hear into it.
- **The tenth fuse**: Figure is gone, without a scene, and the breaker room's door shuts behind you.
- **The Circuit Breaker** (A at the box): a panel over the bottom of the screen. The display shows one switch
  number at a time, ON or OFF, through all ten in a shuffled order, then a pause and the same again. Ten
  switches in two rows of five (D-pad, A flips, B leaves; I on, O off). Nothing is checked until all ten
  match; three rounds, each faster; a solved round blanks the display 3 s and turns the switches off halfway.
  The display beeps high for on and low for off, so the pattern can be learned by ear.
- **The power**: "THE POWER IS BACK.", the lift's gates go and its doors open, the breaker room opens, and
  you **run** (1.4× walking). 3 s later **Figure comes out of the door beside the box** and follows your trail
  at 0.75 of your speed (Seek's numbers): stopping is what gets you caught, and **closets don't save you**
  ("NO TIME TO HIDE."). Caught: his face, then a hint about running.
- **The lift**: walk into it. The doors shut, Figure slams into them, the car shakes and falls, a crash in
  the dark. Then **the results**: "YOU ESCAPED THE HOTEL.", the time, the damage taken over the run, the seed,
  over the **Ending Theme** until Start, and the title. Knobs and achievements join the results in Phases 9
  and 11.

## Hiding (built in Phase 3)

- **Closets and beds** (owner: both). A at one hides: the player sprite disappears and the room darkens
  (owner's choice over a slats view). A or B gets out.
- While Rush or Ambush is out, **no door opens or lets you through** (*MC 04*), so hiding is the only answer.
- Rooms list their closets and beds as objects (ASSETS.md); every normal template has at least one.

## Light and darkness (built in Phase 4)

- **A dark room is black** except the tiles a light reaches (owner, 2026-10-18), on both consoles: unlit
  tiles are drawn as plain black tiles. Sprites (the player, Screech, Eyes) always show.
- **No light item yet** (owner): the player has a small **glow**, a rounded 5×5 tiles, enough to find doors
  and closets. The **lighter** (a circle of radius 3 tiles) and the **flashlight** (a cone 7 tiles long ahead,
  widening to 5, plus the glow) are built and switchable with B in debug builds; Phase 9 makes them items.
- **Which rooms**: 10% of rooms from room 15 (Minecraft rule), and every Greenhouse room (90–99). Rush and
  Ambush never come into a dark room: the flicker is their warning; their spawn waits for a lit room. The
  Greenhouse is the exception for Rush (above). A sound marks the lights going out (once
  for a run of dark rooms).
- **Eyes glows**: he lights a lighter-sized area around himself.
- **Doors you're near light up** (owner, 2026-10-18): within 3 metatiles of a door, its top half (the
  number and the upper door) is lit, so its number reads; the glow alone didn't reach a top-wall door's header.

## Entities (Hotel)

| Entity | Top-down version | Notes |
|---|---|---|
| Rush | **Built (Phase 3).** A 32×32 sprite racing along the door-to-door path (*MC 04*: straight lines, door to door) from about 3 s behind the player to the furthest door reached. Lights flicker first (2 s, with a buzz), then a rumble that grows as he nears. Hide in a closet or under a bed. In the Greenhouse: no flicker, the rumble from the start (Phase 8a) | Kills anyone not hidden he can see: a straight line to them crossing no wall, at any distance in the room (owner, 2026-10-18; it was *MC 04*'s 6-block radius through walls, which left far corners safe). Spawn 18% per room from room 3, waiting for a room with a hiding spot |
| Ambush | **Built (Phase 3).** Like Rush, faster (1.6×), green; his own howl and a green-tinted flicker (4 s; owner's choice, as in DOORS); passes back and forth 3–5 times with 5 s pauses at the rear | Kills by line of sight like Rush (it was 8 blocks, 128 px). A tenth of spawns. Hide's 12 s forces stepping out between passes |
| Screech | **Built (Phase 4).** Dark rooms only: a "psst", and a 16×16 grin hangs just outside your glow, in a direction you're not facing. Walk his way (facing him) for 0.3 s and he leaves with a squeak; after 3 s (owner) he bites for 40 HP with a full-screen face. The chance rises the longer you stay in the dark (~45% for 8 s in a room) | Hiding dismisses him silently (*MC 11*). 20 s cooldown. He picks a spot on screen |
| Eyes | **Built (Phase 4).** 5% of rooms from room 8: a 32×32 cluster of eyes at the room's middle, glowing in the dark. Facing exactly his direction (one of 8, ±22.5°; owner) drains 5 HP every 0.25 s after 0.5 s. Look away, walk round. Gone when the exit door opens; never blocks a door | Hidden, he can't hurt you. Independent of Rush and Ambush |
| Halt | **Phase 5.** 3% per room arms the next (plain) door, once a run. It leads into a sideways, blue-lit hallway (owner) about 8 turn-arounds long; Halt walks at you, and when the screen flashes and big "TURN AROUND" text shows, he's in front of you instead: walk the other way. Forward legs are 35% longer. The far door returns you to the corridor at the door that started it | 60 HP a hit; he goes back to the leg's start. Nothing else (Rush, Screech, Eyes) runs while he does (*MC 06*) |
| Dupe | **Phase 5.** New rooms with 2–3 doors (top plus sides); which is real is picked per run. 35% per such room from room 6: the wrong doors show wrong numbers (one below or up to two above), and opening one is a full-screen jumpscare and 40 HP, then the door shows a garbled number. On a miss the wrong doors have no number and say "IT'S FAKE." | Ignores the lock, spends no key (*MC 06*) |
| Hide | **Built (Phase 3, moved from 5 by the owner).** 12 s in a closet or under a bed and he throws you out ("GET OUT!") for 40 HP and bars that spot for the run. The warning is a heartbeat from 7 s that speeds up, with the screen pulsing darker on each beat; no meter (*MC 04*) | Re-hiding in a spot you left yourself is fine (the Hotel's rule) |
| Seek | **Phase 7** (above): eyes on the walls before; the rise; a chase through four-way rooms (one real, glowing door) to an end hallway with fire; caught by touch or by being behind him | Speed boost during the chase (*MC 08*, *MC 11*); he runs at 0.75 of it |
| Figure | **Phase 6 in the Library** and **Phase 8b in Room 100** (above), where he also chases you to the lift. Blind, hears footsteps: walking makes noise; sneaking (B), hiding and standing still don't. Hunting he's faster than you walk; touching him kills | He finishes a hunt before taking a new noise (*MC 10*); a noise in the cell he's heading for just moves the goal |
| Snare | **Phase 8a** (above): three floor traps per Greenhouse room, seen only in your light; 20 HP (never below 10) and held 5 s | The 4 s grace starts when the hold ends, so it can't catch you again as you step off (*MC 09*) |
| Timothy | **Phase 5.** 1 drawer in 200, once a run: a small spider jumps out of the drawer at you, 10 HP, and the drawer is empty after (owner) | Never lethal: leaves at least 1 HP (*MC 12*); never in a drawer holding the key |

Left out (owner, 2026-10-17): **Jack**, **Dread** (modifier-only in DOORS), **Glitch** (catches players behind
the group, nothing to do solo).

## Items and economy

| Item | What it does |
|---|---|
| Lighter | Light in dark rooms (a circle, radius 3 tiles; built, debug switch until Phase 9); fuel meter |
| Flashlight | A cone ahead, 7 tiles long (built, debug switch until Phase 9); uses battery charge |
| Battery | Refills the flashlight |
| Vitamins | Short speed boost and a small heal |
| Bandage | Heals |
| Lockpicks | Open locked doors and drawers without a key; limited uses |
| Skeleton Key | Opens any lock; rare |
| Crucifix | Cancels one killing blow from an entity; the rarest item |

Left out (owner, 2026-10-17): Smoothie and Candle (later-floor items in DOORS).

- **Gold** from drawers and surfaces, spent at **Jeff's Shop (55)**.
- **Knobs** persist in SRAM and are spent in the **pre-run shop in the elevator**, as in DOORS. They're earned
  by reaching doors and from **achievements**, each of which pays Knobs once.
- **Inventory:** Select cycles the held item (its icon shows in the on-demand HUD), B uses it, Start pauses
  and opens a grid of all items to inspect and pick from. Carry limits follow *MC 12*.

## Saving

Only between runs: Knobs, achievements, statistics and settings are saved to SRAM when a run ends (death or
escape) and when shopping in the elevator. **There's no mid-run continue** (owner, 2026-10-17); a run is one
sitting, as in DOORS.

## Controls

| Button | Action |
|---|---|
| D-pad | Walk (8 directions); the facing direction is where you look |
| A | Interact: open a door, drawer, closet; pick up |
| B | Use the held item. **In the Library and in Room 100 until the escape, held: sneak** (owner: items aren't needed in the Library; Room 100 follows it, to revisit with Phase 9's items) |
| Select | Tap: cycle held item. The HUD shows while it's held |
| Start | Pause / inventory grid. **In the Library: the notes** (no pause) |

## Menus and extras

- Title screen → elevator (pre-run shop) → run. Death → the Guiding Light; escape → the lift → results →
  title (built in Phase 8b; the elevator shop comes with Phase 9).
- **Achievements**, listed from the title screen, with a small pop-up when earned; each pays Knobs once.
- **Statistics** in SRAM: runs, deaths by entity, best door, total Knobs earned.
- **Super Game Boy border** and SGB palette for the DMG ROM.
- Not planned: a walkable lobby, a bestiary, **modifiers** (out of scope for now; the code should leave room
  for a modifier screen later).

## Still open

- The list of achievements and their Knob rewards.
- Credits.
