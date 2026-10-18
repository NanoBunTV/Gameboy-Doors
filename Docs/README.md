# DOORS for Game Boy: documentation

Developer documentation and project memory. Start here when picking the project back up, whether you're a
person or an AI assistant in a new chat.

| File | What's in it |
|---|---|
| [PROJECT.md](PROJECT.md) | What the project is, its history, the decisions made so far and why, and the owner's working preferences |
| [DESIGN.md](DESIGN.md) | How each part of DOORS (Hotel, Doors 0–100) becomes a top-down Game Boy game: rooms, entities, items, and the open design questions |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Source layout, the two ROM targets, memory map, the frame loop, scrolling, sprites, the player, the asset converter |
| [ASSETS.md](ASSETS.md) | How to edit the art (separate GBC and DMG PNGs), tilesets and rooms in Tiled, sprite sheets, and the limits the converter checks |
| [BUILDING.md](BUILDING.md) | Toolchain (GBDK-2020, Python), build steps, asset conversion, ROM header settings |
| [DEBUGGING.md](DEBUGGING.md) | Testing in BGB, PyBoy, SameBoy and Emulicious, the play-testing bot and recorded run, and the rules that keep the ROM working on real hardware |
| [STATUS.md](STATUS.md) | What works, test results, known issues and the roadmap |

Keep these files up to date when something important changes. In particular, record anything learned about
emulator behaviour or Game Boy hardware quirks in DEBUGGING.md, since it's easy to lose an afternoon to the same
problem twice.

The Minecraft version of the same game is the reference for DOORS mechanics and numbers:
`F:\Minecraft DOORS Project\_Plugin\README.md` and `F:\Minecraft DOORS Project\_Plugin\docs\` (one file per
system). Its design notes are worth reading before building the matching system here.
