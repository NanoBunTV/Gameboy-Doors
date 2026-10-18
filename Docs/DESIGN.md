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
- Select pressed: everything (health, item, gold, door number).

**Open**: whether the strip should cover the play field or push it up (it covers it in the first version, since
the window layer can't shrink the background). The strip must not show during Halt's and Seek's cutscenes.

## Health and death

- **Health is a 100 HP bar**, as in DOORS, so Eyes' slow drain and Screech's 40% hit show exactly.
- **Death screen: the Guiding Light.** The blue screen with typewriter hint text naming what killed you and how
  to survive it, one set of lines per entity, as in DOORS. Then "Try again" (new run) or "Lobby". Text comes from
  the wiki's death messages, written for the Game Boy's 18×16-character text area.

## Rooms and generation

- Rooms are Tiled maps (`assets/rooms/*.tmx`) on a shared metatile set, with an object layer for: entry
  and exit doors, closets, beds, drawers/chests, light sources, lever gates, Dupe's side doors, and Halt and
  Seek markers. The room schema follows the Minecraft version's `RoomDefinition` (*MC README*, "Authoring a room").
- A run is generated from a seed: the next room is chosen as the player approaches its door, so only the
  current room, the previous room (for backtracking) and the next room are in memory.
- Fixed rooms: lobby/elevator (0), the Library (50), **Jeff's Shop (55)**, the Courtyard and Greenhouse
  (89–99), Room 100. Seek's chases (between roughly 30–45 and 80–89) and Halt's hallway are scripted sequences
  of rooms. Jeff's Shop is at 55 as in the Minecraft version (owner's choice); DOORS has it at 52.
- Every door shows its number, as in DOORS.

## Light and darkness

Darkness is a core mechanic (dark rooms, Screech, the lighter, the flashlight, Guiding Light).

- **GBC:** dark rooms use a dimmed background palette set, with a light circle around the player drawn by
  swapping palettes per tile (attribute map) near the player. **Open**: exact technique and cost; to prototype.
- **DMG:** with only one background palette, darkness is drawn by swapping to "dark" versions of tiles near
  the edge of a circle, or a simpler fixed-radius cutout. **Open**: to prototype.

## Entities (Hotel)

| Entity | Top-down version | Notes |
|---|---|---|
| Rush | Large sprite racing along the door-to-door path (*MC 04*: straight lines, door to door). Lights flicker first (palette flash), then a rumble sound. Hide in a closet or under a bed | Kill test is a radius that ignores walls (*MC 04*) |
| Ambush | Like Rush, faster, green, passes back and forth several times | Approach cue starts further out (*MC 11*) |
| Screech | In dark rooms: a "psst" and a small sprite appears off to one side; face it within the time limit or take 40 HP | Hiding dismisses him silently (*MC 11*) |
| Eyes | Appears in a room; facing toward him drains health. Face away | Short grace period before damage (*MC 11*) |
| Halt | A long hallway; when the screen flashes and "TURN AROUND" shows, walk the other way | |
| Dupe | Side doors with wrong numbers; opening a wrong one hurts | |
| Hide | Staying in a closet too long kicks you out with damage; a meter shows on screen | |
| Seek | Eyes appear on walls before the chase; then a scripted chase through several rooms with fallen furniture and hands from the side rooms to avoid | Speed boost during the chase (*MC 11*) |
| Figure | The Library (50) and Room 100. Blind, hears footsteps: walking makes noise, hiding or standing still doesn't | Library: find the books, read the hints, solve the padlock |
| Timothy | Jump-scare in a drawer, small damage | |

Left out (owner, 2026-10-17): **Jack**, **Dread** (modifier-only in DOORS), **Glitch** (catches players behind
the group, nothing to do solo).

## Items and economy

| Item | What it does |
|---|---|
| Lighter | Light in dark rooms; fuel meter |
| Flashlight | Brighter light; uses battery charge |
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

- Darkness technique on each console (after a prototype).
- The HUD strip covering vs pushing the play field.
- The list of achievements and their Knob rewards.
- Title screen and logo art; credits.
