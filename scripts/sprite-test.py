"""Per-room sprite loading (sprites.c spr_need) in PyBoy: is every drawn sprite's sheet really in VRAM?

    tools/venv/Scripts/python.exe scripts/sprite-test.py build/gbc-autotest/doors.gbc
    tools/venv/Scripts/python.exe scripts/sprite-test.py build/dmg-autotest/doors.gb

Needs an AUTOTEST build: it replays the whole recorded run (tests/autotest_input.c: the doors, Rush and
Ambush, Eyes, the dark, Halt, and whatever ends it), then pokes game_warp to go into the Library and stays
there a while (Figure, the glints). While playing, it checks:
  vram    each sheet the room asked for (spr_slot_src) has its tiles in VRAM where the packing puts it, read
          back against the ROM: checked whenever the slots change, and every 60 frames;
  oam     every sprite on screen (shadow OAM) uses a tile of the player's sheet or of a sheet loaded for this
          room, and (GBC) that sheet's palettes: a sheet drawn without being asked for shows up here.
Prints which sheets it saw loaded. Exits non-zero on any failure.
"""
import argparse
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bot import Game, Bot, GAME_PLAYING, GAME_HALT, OBJ_DRAWER, RF_LOCKED, RF_KEY_DRAWER  # noqa: E402

LIBRARY = 50
ROOT = Path(__file__).resolve().parent.parent


def symbols_banked(sym_file: Path):
    """name -> (bank, address) from the .sym file"""
    out = {}
    for line in sym_file.read_text().splitlines():
        p = line.split()
        if len(p) == 2 and ":" in p[0]:
            b, a = p[0].split(":")
            out[p[1]] = (int(b, 16), int(a, 16))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom", type=Path)
    ap.add_argument("--library-frames", type=int, default=900)
    args = ap.parse_args()

    cgb = args.rom.suffix == ".gbc"
    rom = args.rom.read_bytes()
    syms = symbols_banked(args.rom.with_suffix(".sym"))
    g = Game(args.rom)

    def rom_at(bank, addr, n):
        off = addr if addr < 0x4000 else bank * 0x4000 + addr - 0x4000
        return rom[off:off + n]

    # Every sprite sheet in the ROM, by its struct's address
    sheets = {}
    for name, (bank, addr) in syms.items():
        if name.startswith("_sprite_") and f"___bank_{name[1:]}" in syms:
            s = rom_at(bank, addr, 9)
            sheets[addr] = {
                "name": name[8:], "tiles": s[0], "data": rom_at(bank, s[1] | s[2] << 8, s[0] * 16),
                "pals": s[6] if cgb else 0,
            }
    player = next(v for v in sheets.values() if v["name"] == "player")

    def vram(tile, n):
        a = 0x8000 + tile * 16
        if cgb:
            return bytes(g.mem[0, a:a + n * 16])   # bank 0 whatever VBK the game left selected
        return bytes(g.mem[a:a + n * 16])

    def layout():
        """The room's sheets as (sheet, first tile, first palette), in slot order"""
        out, tile, pal = [], player["tiles"], 1
        for i in range(g.u8("_spr_slots")):
            p = g.u8("_spr_slot_src", 2 * i) | g.u8("_spr_slot_src", 2 * i + 1) << 8
            sh = sheets.get(p)
            if sh is None:
                return None, f"slot {i} points at {p:#06x}, no sprite sheet there"
            out.append((sh, tile, pal))
            tile += sh["tiles"]
            pal += sh["pals"]
        return out, None

    failures, seen = [], Counter()

    def fail(what):
        if len(failures) < 20:
            print(f"  FAIL room {g.room()} frame {g.frames}: {what}")
        failures.append(what)

    def check_vram(lay):
        if vram(0, player["tiles"]) != player["data"]:
            fail("the player's tiles aren't in VRAM")
        for sh, tile, _ in lay:
            if vram(tile, sh["tiles"]) != sh["data"]:
                fail(f"{sh['name']}'s tiles aren't in VRAM at {tile}")

    def check_oam(lay):
        oam = g.syms["_shadow_OAM"]
        data = g.mem[oam:oam + 160]
        for i in range(40):
            y, x, t, a = data[4 * i:4 * i + 4]
            if y == 0 or y >= 160:
                continue
            if t < player["tiles"]:
                where, pals = "player", (0, 1)
            else:
                hit = next(((sh, p) for sh, tile, p in lay if tile <= t < tile + sh["tiles"]), None)
                if hit is None:
                    fail(f"sprite {i} uses tile {t}, which no sheet of this room's has")
                    continue
                where, pals = hit[0]["name"], (hit[1], hit[1] + hit[0]["pals"])
            if cgb and not pals[0] <= (a & 7) < pals[1]:
                fail(f"sprite {i} (tile {t}, {where}) has palette {a & 7}, not {pals[0]}-{pals[1] - 1}")

    # The recording's length in input_update() calls: pairs of { buttons, count }
    text = (ROOT / "tests" / "autotest_input.c").read_text().split("autotest_input[]")[1]
    counts = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", text)][1::2]
    replay_end = sum(counts)

    st = {"key": None, "since": 0, "dark": 0}
    doors, written = [], []   # frames not playing per break (doors, deaths), tiles written per change

    def check_frame():
        """The checks for the frame just run"""
        if g.state() not in (GAME_PLAYING, GAME_HALT):
            st["dark"] += 1
            return
        if st["dark"]:
            doors.append(st["dark"])
            st["dark"] = 0
        lay, err = layout()
        if err:
            fail(err)
            return
        key = tuple((sh["name"], tile) for sh, tile, _ in lay)
        st["since"] += 1
        if key != st["key"] or st["since"] >= 60:
            check_vram(lay)
            st["since"] = 0
            if key != st["key"]:
                for sh, _, _ in lay:
                    seen[sh["name"]] += 1
                # What the door wrote: the sheets that weren't already there in the same place
                had = set(st["key"] or ())
                written.append(sum(sh["tiles"] for sh, tile, _ in lay if (sh["name"], tile) not in had))
            st["key"] = key
        check_oam(lay)

    def tick(n, mask=0):
        for _ in range(n):
            g.tick(mask)
            check_frame()

    # The recording, then the Library
    for _ in range(400000):
        g.pyboy.tick(1, False)
        g.frames += 1
        check_frame()
        if g.u16("_input_frame") >= replay_end + 60 and g.state() == GAME_PLAYING:
            break
    else:
        fail("the recording never played out")
    print(f"{args.rom}: the recording played out (room {g.room()}); into the Library")
    g.mem[g.syms["_game_warp"]] = LIBRARY
    tick(args.library_frames)
    if g.room() != LIBRARY:
        fail(f"the warp to the Library ended in room {g.room()}")

    # Timothy, the one sheet loaded mid-room: the next drawer opened holds him (run_force_timothy)
    bot = Bot(g)
    for room in range(2, 30):
        g.mem[g.syms["_game_warp"]] = room
        tick(90)
        drawers = [o for o in g.objects() if o[0] == OBJ_DRAWER]
        f = g.flags()
        if f & RF_LOCKED and f & RF_KEY_DRAWER:   # the key's drawer never holds him (run_timothy_in)
            del drawers[g.u8("_run_key_slot", g.room())]
        if g.state() == GAME_PLAYING and drawers:
            break
    print(f"  Timothy: room {g.room()}, drawer at {drawers[0][1:3]}")
    g.mem[g.syms["_run_force_timothy"]] = 1
    hp = g.u8("_run_hp")
    try:
        bot.use(drawers[0])
    except RuntimeError as e:
        fail(f"the bot couldn't open a drawer: {e}")
    tick(80)
    if g.u8("_run_hp") >= hp:
        fail(f"Timothy never jumped (HP {hp} -> {g.u8('_run_hp')}, run_timothy {g.u8('_run_timothy')}, "
             f"drawers {g.u8('_run_drawers', g.room())}, force {g.u8('_run_force_timothy')}, state {g.state()})")
    g.pyboy.stop(save=False)

    print("  sheets loaded (rooms that asked): " + ", ".join(f"{n} {c}" for n, c in sorted(seen.items())))
    if written:
        print(f"  tiles written when a room's sheets changed: {len(written)} changes, mean "
              f"{sum(written) / len(written):.0f}, most {max(written)}")
    short = [d for d in doors if d < 120]   # doors (deaths and the title take longer)
    if short:
        print(f"  frames between playing (doors): {len(short)} doors, mean {sum(short) / len(short):.1f}, "
              f"most {max(short)}")
    for must in ("key", "eyes", "figure", "marks", "halt", "letters", "timothy", "seek", "puddle", "walleye"):
        if not seen[must]:
            fail(f"never saw {must}'s sheet loaded: the run no longer covers it")
    if failures:
        sys.exit(f"{len(failures)} failure(s)")
    print("  ok   every drawn sprite's sheet was in VRAM")


if __name__ == "__main__":
    main()
