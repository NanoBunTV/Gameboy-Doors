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
- entering a room: the door number;
- messages ("IT'S LOCKED.", "GOT KEY FOR DOOR 012");
- Select held: everything (health, item, gold, door number).

As built (Phase 3): border line + one text row, `DOOR 012`, a heart and an 8-tile health bar (2 px steps),
and a key icon at the right end while a key is carried. It slides in on entering a room, when health changes,
with a message, and while Select is held. It covers the bottom 16 px of the play field (the window layer can't
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
- Fixed rooms still to come: the Library (50), **Jeff's Shop (55)**, the Courtyard and Greenhouse (89–99),
  Room 100. Seek's chases (between roughly 30–45 and 80–89) and Halt's hallway are scripted sequences of rooms.
  Jeff's Shop is at 55 as in the Minecraft version (owner's choice); DOORS has it at 52. For now room 100's
  door says "TO BE CONTINUED...".
- **Doors**: every door in a top wall shows its number in brass on a dark header above it, open or shut (side
  doors while shut), and the HUD shows it on entering. **A**
  opens a door (owner: press A, not automatic). Walking into an open doorway goes through.
- **Locked doors**: 18% of rooms that allow it lock their exit; the key is in that room, in a drawer (mostly)
  or lying in the open (owner: both, mostly drawers). Facing a locked door with A says "IT'S LOCKED." or, with
  the key, unlocks and opens it and uses the key up. **Drawers** are searched with A: "NOTHING." or the key.

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
- **Which rooms**: 10% of rooms from room 15 (Minecraft rule). Rush and Ambush never come into a dark room:
  the flicker is their warning; their spawn waits for a lit room. A sound marks the lights going out (once
  for a run of dark rooms).
- **Eyes glows**: he lights a lighter-sized area around himself.

## Entities (Hotel)

| Entity | Top-down version | Notes |
|---|---|---|
| Rush | **Built (Phase 3).** A 32×32 sprite racing along the door-to-door path (*MC 04*: straight lines, door to door) from about 3 s behind the player to the furthest door reached. Lights flicker first (2 s, with a buzz), then a rumble that grows as he nears. Hide in a closet or under a bed | Kill radius 6 blocks (96 px), ignoring walls (*MC 04*). Spawn 18% per room from room 3, waiting for a room with a hiding spot |
| Ambush | **Built (Phase 3).** Like Rush, faster (1.6×), green; his own howl and a green-tinted flicker (4 s; owner's choice, as in DOORS); passes back and forth 3–5 times with 5 s pauses at the rear | Kill radius 8 blocks (128 px). A tenth of spawns. Hide's 12 s forces stepping out between passes |
| Screech | **Built (Phase 4).** Dark rooms only: a "psst", and a 16×16 grin hangs just outside your glow, in a direction you're not facing. Walk his way (facing him) for 0.3 s and he leaves with a squeak; after 3 s (owner) he bites for 40 HP with a full-screen face. The chance rises the longer you stay in the dark (~45% for 8 s in a room) | Hiding dismisses him silently (*MC 11*). 20 s cooldown. He picks a spot on screen |
| Eyes | **Built (Phase 4).** 5% of rooms from room 8: a 32×32 cluster of eyes at the room's middle, glowing in the dark. Facing exactly his direction (one of 8, ±22.5°; owner) drains 5 HP every 0.25 s after 0.5 s. Look away, walk round. Gone when the exit door opens; never blocks a door | Hidden, he can't hurt you. Independent of Rush and Ambush |
| Halt | A long hallway; when the screen flashes and "TURN AROUND" shows, walk the other way | |
| Dupe | Side doors with wrong numbers; opening a wrong one hurts | |
| Hide | **Built (Phase 3, moved from 5 by the owner).** 12 s in a closet or under a bed and he throws you out ("GET OUT!") for 40 HP and bars that spot for the run. The warning is a heartbeat from 7 s that speeds up, with the screen pulsing darker on each beat; no meter (*MC 04*) | Re-hiding in a spot you left yourself is fine (the Hotel's rule) |
| Seek | Eyes appear on walls before the chase; then a scripted chase through several rooms with fallen furniture and hands from the side rooms to avoid | Speed boost during the chase (*MC 11*) |
| Figure | The Library (50) and Room 100. Blind, hears footsteps: walking makes noise, hiding or standing still doesn't | Library: find the books, read the hints, solve the padlock |
| Timothy | Jump-scare in a drawer, small damage | |

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
| B | Use the held item |
| Select | Tap: cycle held item. The HUD shows while it's held |
| Start | Pause / inventory grid |

## Menus and extras

- Title screen → elevator (pre-run shop) → run. Death or escape → results → title.
- **Achievements**, listed from the title screen, with a small pop-up when earned; each pays Knobs once.
- **Statistics** in SRAM: runs, deaths by entity, best door, total Knobs earned.
- **Super Game Boy border** and SGB palette for the DMG ROM.
- Not planned: a walkable lobby, a bestiary, **modifiers** (out of scope for now; the code should leave room
  for a modifier screen later).

## Still open

- The list of achievements and their Knob rewards.
- Credits.
