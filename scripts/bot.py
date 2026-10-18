"""Plays the game in PyBoy like a player would, and records the buttons for AUTOTEST builds.

    tools/venv/Scripts/python.exe scripts/bot.py build/dmg-autotest/doors.gb --rooms 25 --record

The ROM must be an AUTOTEST build (fixed seed) with an empty recording (tests/autotest_input.c as
written by --reset, or replay finished). The bot reads the game's state from WRAM by symbol (room map,
objects, run flags, player), walks with a breadth-first search over metatiles, opens drawers until it has
the key, unlocks and opens doors, and walks through them. At room 3 it goes back to room 2 and returns, to
test walking back through a door.

--record writes tests/autotest_input.c: the buttons for every input_update() call, run-length coded.
AUTOTEST builds replay it, so BGB (which can't press buttons) plays the same run under its hardware checks.
--check replays an AUTOTEST ROM built with that recording and confirms it reaches the same room at the
same input frame (the game is deterministic in its input).
"""
import argparse
import sys
from collections import deque
from pathlib import Path

from pyboy import PyBoy

ROOT = Path(__file__).resolve().parent.parent

J = {"right": 0x01, "left": 0x02, "up": 0x04, "down": 0x08, "a": 0x10, "b": 0x20, "select": 0x40, "start": 0x80}
OBJ_ENTRY, OBJ_EXIT, OBJ_DRAWER, OBJ_KEYSPOT = 1, 2, 3, 4
RF_LOCKED, RF_KEY_DRAWER, RF_KEY_TAKEN, RF_EXIT_OPEN, RF_FINAL, RF_KEY_FIXED = 1, 2, 4, 8, 16, 32
GAME_TITLE, GAME_PLAYING, GAME_TRANSITION = 0, 1, 2
FEET_X, FEET_Y = 8, 12
# Facing used for interacting toward a neighbour cell
DIRS = {(0, -1): "up", (0, 1): "down", (-1, 0): "left", (1, 0): "right"}


class Game:
    def __init__(self, rom: Path):
        self.syms = {}
        for line in rom.with_suffix(".sym").read_text().splitlines():
            p = line.split()
            if len(p) == 2 and ":" in p[0]:
                self.syms[p[1]] = int(p[0].split(":")[1], 16)
        self.pyboy = PyBoy(str(rom), window="null", cgb=rom.suffix == ".gbc", sound_emulated=False)
        self.pyboy.set_emulation_speed(0)
        self.mem = self.pyboy.memory
        self.held = 0
        self.record = []      # buttons per input_update() call
        self.frames = 0

    def u8(self, name, off=0):
        return self.mem[self.syms[name] + off]

    def s16(self, name):
        v = self.u8(name) | (self.u8(name, 1) << 8)
        return v - 0x10000 if v & 0x8000 else v

    def u16(self, name):
        return self.u8(name) | (self.u8(name, 1) << 8)

    def set_buttons(self, mask):
        for name, bit in J.items():
            if (mask & bit) and not (self.held & bit):
                self.pyboy.button_press(name)
            elif not (mask & bit) and (self.held & bit):
                self.pyboy.button_release(name)
        self.held = mask

    def tick(self, mask=0, render=False):
        """One frame with these buttons held. Records them for each input_update() call it caused."""
        self.set_buttons(mask)
        before = self.u16("_input_frame")
        self.pyboy.tick(1, render)
        self.frames += 1
        after = self.u16("_input_frame")
        self.record += [mask] * ((after - before) & 0xFFFF)

    # --- state
    def state(self):
        return self.u8("_game_state")

    def room(self):
        return self.u8("_run_room")

    def feet(self):
        return self.s16("_player_x") + FEET_X, self.s16("_player_y") + FEET_Y

    def cell(self):
        fx, fy = self.feet()
        return fx // 16, fy // 16

    def grid(self):
        w, h = self.u8("_room_w"), self.u8("_room_h")
        base = self.syms["_room_map"]
        flags = self.syms["_mt_flags"]
        solid = [[bool(self.mem[flags + self.mem[base + y * w + x]] & 1) for x in range(w)] for y in range(h)]
        return w, h, solid

    def objects(self):
        n = self.u8("_room_obj_count")
        base = self.syms["_room_objs"]
        return [tuple(self.mem[base + i * 6 + k] for k in range(6)) for i in range(n)]

    def flags(self):
        return self.u8("_run_flags", self.room())


def bfs(solid, w, h, start, goals):
    """Shortest 4-way path over walkable cells from start to any goal cell (goals may be walkable or not;
    the path ends on the last walkable cell before... no: goals here are walkable cells)."""
    prev = {start: None}
    q = deque([start])
    while q:
        c = q.popleft()
        if c in goals:
            path = []
            while c is not None:
                path.append(c)
                c = prev[c]
            return path[::-1]
        x, y = c
        for dx, dy in DIRS:
            n = (x + dx, y + dy)
            if 0 <= n[0] < w and 0 <= n[1] < h and n not in prev and not solid[n[1]][n[0]]:
                prev[n] = c
                q.append(n)
    return None


class Bot:
    def __init__(self, game: Game, verbose=False):
        self.g = game
        self.verbose = verbose
        self.backtrack_done = False

    def log(self, *a):
        if self.verbose:
            print(f"[frame {self.g.frames} room {self.g.room()}]", *a)

    def wait_playing(self, limit=600):
        for _ in range(limit):
            if self.g.state() == GAME_PLAYING:
                return
            if self.g.state() == GAME_TITLE:
                self.g.tick(J["start"])
                self.g.tick(0)
            else:
                self.g.tick(0)
        raise RuntimeError("game never got to PLAYING")

    def walk_to(self, target, limit=900):
        """Walks the feet to the centre of a walkable cell along a BFS path. Returns False if stuck."""
        room = self.g.room()
        stuck, last = 0, None
        for _ in range(limit):
            if self.g.state() != GAME_PLAYING or self.g.room() != room:
                return True  # went through a door
            w, h, solid = self.g.grid()
            here = self.g.cell()
            path = bfs(solid, w, h, here, {target})
            if path is None:
                raise RuntimeError(f"no path from {here} to {target}")
            fx, fy = self.g.feet()
            nxt = path[1] if len(path) > 1 else path[0]
            tx, ty = nxt[0] * 16 + 8, nxt[1] * 16 + 8
            if len(path) == 1 and abs(fx - tx) <= 1 and abs(fy - ty) <= 1:
                self.g.tick(0)
                return True
            mask = 0
            # Line up across the direction of travel first, then go
            if nxt[0] != here[0]:
                if abs(fy - ty) > 2:
                    mask |= J["down"] if fy < ty else J["up"]
                else:
                    mask |= J["right"] if fx < tx else J["left"]
            elif nxt[1] != here[1]:
                if abs(fx - tx) > 2:
                    mask |= J["right"] if fx < tx else J["left"]
                else:
                    mask |= J["down"] if fy < ty else J["up"]
            else:
                if abs(fx - tx) > 1:
                    mask |= J["right"] if fx < tx else J["left"]
                if abs(fy - ty) > 1:
                    mask |= J["down"] if fy < ty else J["up"]
            self.g.tick(mask)
            pos = self.g.feet()
            stuck = stuck + 1 if pos == last else 0
            last = pos
            if stuck > 90:
                return False
        return False

    def approach(self, obj):
        """A walkable cell next to the object, and the direction to face it from there."""
        w, h, solid = self.g.grid()
        t, ox, oy, ow, oh, _ = obj
        options = []
        for y in range(oy, oy + oh):
            for x in range(ox, ox + ow):
                for (dx, dy), name in DIRS.items():
                    sx, sy = x - dx, y - dy
                    if 0 <= sx < w and 0 <= sy < h and not solid[sy][sx] and not (ox <= sx < ox + ow and oy <= sy < oy + oh):
                        options.append(((sx, sy), name))
        here = self.g.cell()
        best = None
        for cellpos, face in options:
            p = bfs(solid, w, h, here, {cellpos})
            if p is not None and (best is None or len(p) < best[0]):
                best = (len(p), cellpos, face)
        if best is None:
            raise RuntimeError(f"can't reach object {obj}")
        return best[1], best[2]

    def use(self, obj):
        cellpos, face = self.approach(obj)
        self.log("use", obj, "from", cellpos, face)
        if not self.walk_to(cellpos):
            raise RuntimeError(f"stuck walking to {cellpos} for {obj}")
        for _ in range(3):
            self.g.tick(J[face])      # turn to face it (walking into it is harmless)
        self.g.tick(0)
        self.g.tick(J["a"])
        self.g.tick(0)

    def go_through(self, obj):
        """Walks into an open door until the room changes."""
        room = self.g.room()
        w, h, solid = self.g.grid()
        t, ox, oy, ow, oh, _ = obj
        cells = [(x, y) for y in range(oy, oy + oh) for x in range(ox, ox + ow) if not solid[y][x]]
        here = self.g.cell()
        best = min(cells, key=lambda c: len(bfs(solid, w, h, here, {c}) or [0] * 999))
        self.log("go through", obj, "via", best)
        self.walk_to(best)
        for _ in range(240):
            if self.g.room() != room:
                break
            # keep pushing outward through the door
            wall = obj[5]
            self.g.tick({1: J["up"], 2: J["down"], 3: J["left"], 4: J["right"]}[wall])
        if self.g.room() == room:
            raise RuntimeError(f"walked into {obj} but stayed in room {room}")
        self.wait_playing()

    def play_room(self):
        objs = self.g.objects()
        exit_ = next(o for o in objs if o[0] == OBJ_EXIT)
        entry = next((o for o in objs if o[0] == OBJ_ENTRY), None)
        n = self.g.room()

        if n == 3 and not self.backtrack_done and entry:
            self.backtrack_done = True
            self.log("backtracking test")
            self.go_through(entry)          # to room 2, arriving at its exit
            if self.g.room() != 2:
                raise RuntimeError("backtrack went to the wrong room")
            objs2 = self.g.objects()
            self.go_through(next(o for o in objs2 if o[0] == OBJ_EXIT))
            return

        for attempt in range(12):
            f = self.g.flags()
            if f & RF_EXIT_OPEN:
                self.go_through(exit_)
                return
            if f & RF_FINAL:
                return "final"
            if (f & RF_LOCKED) and self.g.u8("_run_key_door") != n + 1:
                if f & RF_KEY_DRAWER:
                    opened = self.g.u8("_run_drawers", n)
                    drawers = [o for o in objs if o[0] == OBJ_DRAWER]
                    # open drawers in order until the key turns up (like a player searching)
                    todo = [d for i, d in enumerate(drawers) if not opened & (1 << i)]
                    self.use(todo[0])
                else:
                    spots = [o for o in objs if o[0] == OBJ_KEYSPOT]
                    if f & RF_KEY_FIXED:
                        spot = next(o for o in spots if o[5] & 1)
                    else:
                        spot = [o for o in spots if not o[5] & 1][self.g.u8("_run_key_slot", n)]
                    self.use(spot)
                continue
            self.use(exit_)
        raise RuntimeError("couldn't get the exit open")


def write_recording(record, path: Path):
    pairs = []
    for b in record:
        if pairs and pairs[-1][0] == b and pairs[-1][1] < 255:
            pairs[-1][1] += 1
        else:
            pairs.append([b, 1])
    data = [v for p in pairs for v in p]
    lines = [", ".join(f"0x{v:02X}" for v in data[i:i + 16]) + "," for i in range(0, len(data), 16)]
    path.write_text(
        "// Recorded by scripts/bot.py. Do not edit; re-record with scripts/bot.py --record.\n"
        "#pragma bank 255\n#include \"autotest_input.h\"\nBANKREF(autotest_input)\n\n"
        "const uint8_t autotest_input[] = {\n    " + "\n    ".join(lines) + "\n};\n"
        f"const uint16_t autotest_input_len = {len(data)};\n", encoding="utf-8")
    return len(data)


def reset_recording(path: Path):
    path.write_text(
        "// Placeholder until scripts/bot.py records a run: AUTOTEST builds read the joypad.\n"
        "#pragma bank 255\n#include \"autotest_input.h\"\nBANKREF(autotest_input)\n\n"
        "const uint8_t autotest_input[] = { 0, 0 };\nconst uint16_t autotest_input_len = 0;\n", encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom", type=Path, nargs="?")
    ap.add_argument("--rooms", type=int, default=20, help="play until this room number")
    ap.add_argument("--record", action="store_true", help="write tests/autotest_input.c")
    ap.add_argument("--reset", action="store_true", help="write an empty tests/autotest_input.c and stop")
    ap.add_argument("--check", type=int, metavar="ROOM", help="replay only: expect to reach ROOM")
    ap.add_argument("--shots", action="store_true", help="screenshot each room on entering it")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()
    rec_path = ROOT / "tests" / "autotest_input.c"
    if args.reset:
        reset_recording(rec_path)
        print(f"wrote empty {rec_path}")
        return

    g = Game(args.rom)
    if args.check is not None:
        # Replay: no buttons from us; the ROM plays its recording
        for _ in range(200000):
            g.pyboy.tick(1, False)
            g.frames += 1
            if g.room() >= args.check and g.state() == GAME_PLAYING:
                print(f"replay reached room {g.room()} at input frame {g.u16('_input_frame')} "
                      f"(emulator frame {g.frames})")
                g.pyboy.stop(save=False)
                return
        g.pyboy.stop(save=False)
        sys.exit(f"replay never reached room {args.check} (stuck in room {g.room()})")

    bot = Bot(g, args.verbose)
    for _ in range(60):
        g.tick(0)
    bot.wait_playing()
    visited = []
    try:
        while g.room() < args.rooms:
            visited.append(g.room())
            if args.shots:
                for i in range(20):   # let the HUD slide in
                    g.tick(0, render=(i == 19))
                g.pyboy.screen.image.save(ROOT / "build" / "screens" / f"bot-room-{g.room():03d}.png")
            if bot.play_room() == "final":
                break
    except RuntimeError as e:
        g.tick(0, render=True)
        g.pyboy.screen.image.save(ROOT / "build" / "screens" / "bot-fail.png")
        print(f"BOT FAILED in room {g.room()} (template {g.u8('_run_template', g.room())}): {e}")
        print("screen saved to build/screens/bot-fail.png")
        g.pyboy.stop(save=False)
        sys.exit(1)
    g.pyboy.screen.image.save(ROOT / "build" / "screens" / "bot-end.png")
    templates = [g.u8("_run_template", i) for i in range(g.room() + 1)]
    locked = sum(1 for i in range(g.room() + 1) if g.u8("_run_flags", i) & RF_LOCKED)
    print(f"reached room {g.room()} in {g.frames} frames ({len(g.record)} input frames); "
          f"{locked} locked doors; templates {templates}")
    if args.record:
        n = write_recording(g.record, rec_path)
        print(f"wrote {rec_path} ({n} bytes)")
    g.pyboy.stop(save=False)


if __name__ == "__main__":
    main()
