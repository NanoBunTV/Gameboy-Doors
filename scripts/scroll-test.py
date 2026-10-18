"""Checks that the background is drawn correctly: runs an AUTOTEST ROM (which plays its recorded run) in
PyBoy and, every few frames, compares the screen with a reference picture of the current room built
from the game's own room map in WRAM and the tileset PNG.

Usage: tools/venv/Scripts/python.exe scripts/scroll-test.py build/gbc-autotest/doors.gbc [frames] [--every 2]

Dark rooms are compared too: the reference is black wherever the game's own light state (the sources and
shapes in WRAM, src/light.c) says a tile is unlit, on frames where every queued tile write has landed.

Not compared: sprites (from OAM), the HUD window (rows from WY down), door number plates (metatiles made
at runtime), frames with the room dimmed (hiding, the light flicker before Rush and Ambush), frames during
fades or room loads, frames just after a metatile changed (a door or drawer opening is redrawn in a later
VBlank, on purpose), and dark-room frames with tile writes still queued. PyBoy shows colours slightly differently from the PNG
(GBC 5-bit colour, its own DMG greys), so the script first learns which screen colour each art colour
becomes, then counts pixels that don't match. Failing frames are saved to build/screens/scroll-fail-*.png.
"""
import argparse
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np
from PIL import Image
from pyboy import PyBoy

ROOT = Path(__file__).resolve().parent.parent
MT_DYN_FIRST = 120          # src/room.h
GAME_PLAYING, GAME_HALT = 1, 4   # src/game.h
SETTLE_FRAMES = 12          # skip this long after a fade or a metatile change
LIGHT_SHAPES, LIGHT_ROWS, LIGHT_REACH, LIGHT_OFF = 10, 17, 8, 0xFF   # src/light.h
# Reference colour of an unlit tile (the font's blank): one no art uses, so it learns its own screen colour
# (on GBC the font's black isn't the wall art's black)
DARK = np.array([1, 2, 3], dtype=np.uint8)


def symbols(sym_file: Path):
    out = {}
    for line in sym_file.read_text().splitlines():
        parts = line.split()
        if len(parts) == 2 and ":" in parts[0]:
            out[parts[1]] = int(parts[0].split(":")[1], 16)
    return out


def metatile_images(console: str):
    tsx = ROOT / "assets" / "tilesets" / "hotel.tsx"
    root = ET.parse(tsx).getroot()
    cols = int(root.get("columns"))
    sheet = np.array(Image.open(tsx.parent / root.find("image").get("source").replace(".gbc.png", f".{console}.png"))
                     .convert("RGB"))
    count = int(root.get("tilecount"))
    return [sheet[(m // cols) * 16:(m // cols) * 16 + 16, (m % cols) * 16:(m % cols) * 16 + 16] for m in range(count)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom", type=Path)
    ap.add_argument("frames", type=int, nargs="?", default=6000)
    ap.add_argument("--every", type=int, default=2)
    args = ap.parse_args()

    cgb = args.rom.suffix == ".gbc"
    tiles = metatile_images("gbc" if cgb else "dmg")
    sym = symbols(args.rom.with_suffix(".sym"))
    pyboy = PyBoy(str(args.rom), window="null", cgb=cgb, sound_emulated=False)
    pyboy.set_emulation_speed(0)
    mem = pyboy.memory

    def u8(name, off=0):
        return mem[sym[name] + off]

    def s16(name):
        v = u8(name) | (u8(name, 1) << 8)
        return v - 0x10000 if v & 0x8000 else v

    def full(low8, near):
        """The camera value whose low byte is the scroll register, nearest to the camera variable."""
        base = near - ((near - low8) & 0xFF)
        return min((base, base + 256), key=lambda v: abs(v - near))

    def snapshot():
        """Everything the next drawn frame is made from. When tick() returns, VBlank has already run, so
        registers, OAM and the map are the ones the NEXT frame is drawn with (found by testing)."""
        w, h = u8("_room_w"), u8("_room_h")
        base = sym["_room_map"]
        room = bytes(mem[base + i] for i in range(w * h))
        mask = np.ones((144, 160), dtype=bool)
        for i in range(40):
            oy, ox = mem[0xFE00 + i * 4] - 16, mem[0xFE00 + i * 4 + 1] - 8
            if -16 < oy < 144 and -8 < ox < 160:
                mask[max(oy, 0):max(oy + 16, 0), max(ox, 0):max(ox + 8, 0)] = False
        wy = mem[0xFF4A]
        if wy < 144 and mem[0xFF40] & 0x20:
            mask[wy:, :] = False
        state = u8("_game_state")
        ok = (state in (GAME_PLAYING, GAME_HALT) and u8("_platform_fade") == 0 and u8("_platform_dim") == 0
              and u8("_platform_flash") == 0)
        dark = u8("_light_dark")
        sources = [(u8("_light_src_shape", i), u8("_light_src_x", i), u8("_light_src_y", i)) for i in range(2)]
        pending = u8("_vq_head") != u8("_vq_tail") or u8("_bg_vq_n") != 0 or u8("_light_pending") != 0
        return dict(x=full(mem[0xFF43], s16("_cam_x")), y=full(mem[0xFF42], s16("_cam_y")), w=w, h=h,
                    room=room, mask=mask, ok=ok, dark=dark, sources=sources, pending=pending, state=state)

    def sx16(addr):
        v = mem[addr] | (mem[addr + 1] << 8)
        return v - 0x10000 if v & 0x8000 else v

    def s8(v):
        return v - 256 if v & 0x80 else v

    shapes = None

    def lit(sources, tx, ty):
        for shape, sx, sy in sources:
            if shape == LIGHT_OFF:
                continue
            row = ty - sy + LIGHT_REACH
            if 0 <= row < LIGHT_ROWS and shapes[0][shape][row] <= tx - sx <= shapes[1][shape][row]:
                return True
        return False

    def reference(s):
        w, h, room = s["w"], s["h"], s["room"]
        pic = np.zeros((144, 160, 3), dtype=np.uint8)
        dyn = np.ones((144, 160), dtype=bool)
        for sy in range(-(s["y"] % 16), 144, 16):
            my = (s["y"] + sy) // 16
            for sx in range(-(s["x"] % 16), 160, 16):
                mx = (s["x"] + sx) // 16
                m = room[my * w + mx] if 0 <= mx < w and 0 <= my < h else 0
                y0, x0 = max(sy, 0), max(sx, 0)
                y1, x1 = min(sy + 16, 144), min(sx + 16, 160)
                if m >= MT_DYN_FIRST:
                    dyn[y0:y1, x0:x1] = False
                    continue
                pic[y0:y1, x0:x1] = tiles[m][y0 - sy:y1 - sy, x0 - sx:x1 - sx]
        if s["dark"]:
            for sy in range(-(s["y"] % 8), 144, 8):
                ty = (s["y"] + sy) // 8
                for sx in range(-(s["x"] % 8), 160, 8):
                    tx = (s["x"] + sx) // 8
                    if not (0 <= tx < 2 * w and 0 <= ty < 2 * h and lit(s["sources"], tx, ty)):
                        y0, x0 = max(sy, 0), max(sx, 0)
                        pic[y0:min(sy + 8, 144), x0:min(sx + 8, 160)] = DARK
                        dyn[y0:min(sy + 8, 144), x0:min(sx + 8, 160)] = True   # black over a plate too
        return pic, dyn

    def keys(a):
        return (a[..., 0].astype(np.uint32) << 16) | (a[..., 1].astype(np.uint32) << 8) | a[..., 2]

    # Halt's hallway is tinted blue on GBC: its colours are learned separately (per game state)
    colour_maps = defaultdict(lambda: defaultdict(Counter))
    learned_by = {}
    learn_frames = Counter()
    checked = failed = halt_checked = 0
    prev, settle, last_room = None, SETTLE_FRAMES, None
    for frame in range(args.frames):
        pyboy.tick(1, frame % args.every == 0)
        cur = snapshot()
        drawn, prev = prev, cur
        if drawn is None:
            continue
        # Settle after anything that's redrawn late on purpose: fades, room loads, metatile changes
        if not drawn["ok"] or drawn["room"] != last_room:
            settle = SETTLE_FRAMES
        last_room = drawn["room"]
        if settle:
            settle -= 1
            continue
        if frame % args.every:
            continue
        if drawn["dark"] and drawn["pending"]:
            continue   # the light moved and its tiles are still on their way (a frame or two)
        if drawn["dark"] and shapes is None:
            base_l, base_r = sym["_light_l"], sym["_light_r"]
            shapes = tuple([[[s8(mem[base + s * LIGHT_ROWS + r]) for r in range(LIGHT_ROWS)] for s in range(LIGHT_SHAPES)]
                            for base in (base_l, base_r)])
        ref, dyn = reference(drawn)
        mask = drawn["mask"] & dyn
        screen = pyboy.screen.ndarray[:, :, :3]
        ref_k, scr_k = keys(ref), keys(screen)
        mode = drawn["state"]
        learned = learned_by.get(mode)
        if learned is None:
            for r, s_ in zip(ref_k[mask].ravel(), scr_k[mask].ravel()):
                colour_maps[mode][r][s_] += 1
            learn_frames[mode] += 1
            if learn_frames[mode] >= 20:
                learned_by[mode] = {r: c.most_common(1)[0][0] for r, c in colour_maps[mode].items()}
            continue
        # A colour the learning phase didn't see (a metatile not on screen then) learns now
        for r in np.unique(ref_k[mask]):
            if r not in learned:
                vals, counts = np.unique(scr_k[mask & (ref_k == r)], return_counts=True)
                learned[r] = vals[np.argmax(counts)]
        expected = np.vectorize(lambda r: learned.get(r, -1), otypes=[np.int64])(ref_k)
        bad = (expected != scr_k) & mask
        checked += 1
        halt_checked += mode == GAME_HALT
        if bad.any():
            failed += 1
            if failed <= 5:
                out = ROOT / "build" / "screens" / f"scroll-fail-{frame}.png"
                Image.fromarray(screen.copy()).save(out)
                ys, xs = np.nonzero(bad)
                print(f"frame {frame}: room {u8('_run_room')}, camera ({drawn['x']}, {drawn['y']}): {bad.sum()} wrong "
                      f"pixels, x {xs.min()}-{xs.max()}, y {ys.min()}-{ys.max()} -> {out.name}")
    reached = u8("_run_room")
    pyboy.stop(save=False)
    print(f"{args.rom}: {checked} frames compared up to room {reached} ({halt_checked} in Halt's hallway), "
          f"{failed} with wrong background pixels")
    raise SystemExit(1 if failed or checked == 0 else 0)


if __name__ == "__main__":
    main()
