"""Checks that the background is drawn correctly: runs an AUTOTEST ROM (which plays its recorded run) in
PyBoy and, every few frames, compares the screen with a reference picture of the current room built
from the game's own room map in WRAM and the tileset PNG.

Usage: tools/venv/Scripts/python.exe scripts/scroll-test.py build/gbc-autotest/doors.gbc [frames] [--every 2]

Not compared: sprites (from OAM), the HUD window (rows from WY down), door number plates (metatiles made
at runtime), frames during fades or room loads, and frames just after a metatile changed (a door or drawer
opening is redrawn in a later VBlank, on purpose). PyBoy shows colours slightly differently from the PNG
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
GAME_PLAYING = 1            # src/game.h
SETTLE_FRAMES = 12          # skip this long after a fade or a metatile change


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
        ok = u8("_game_state") == GAME_PLAYING and u8("_platform_fade") == 0
        return dict(x=full(mem[0xFF43], s16("_cam_x")), y=full(mem[0xFF42], s16("_cam_y")), w=w, h=h,
                    room=room, mask=mask, ok=ok)

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
        return pic, dyn

    def keys(a):
        return (a[..., 0].astype(np.uint32) << 16) | (a[..., 1].astype(np.uint32) << 8) | a[..., 2]

    colour_map = defaultdict(Counter)
    learned = None
    learn_frames = 0
    checked = failed = 0
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
        ref, dyn = reference(drawn)
        mask = drawn["mask"] & dyn
        screen = pyboy.screen.ndarray[:, :, :3]
        ref_k, scr_k = keys(ref), keys(screen)
        if learned is None:
            for r, s_ in zip(ref_k[mask].ravel(), scr_k[mask].ravel()):
                colour_map[r][s_] += 1
            learn_frames += 1
            if learn_frames >= 20:
                learned = {r: c.most_common(1)[0][0] for r, c in colour_map.items()}
            continue
        # A colour the learning phase didn't see (a metatile not on screen then) learns now
        for r in np.unique(ref_k[mask]):
            if r not in learned:
                vals, counts = np.unique(scr_k[mask & (ref_k == r)], return_counts=True)
                learned[r] = vals[np.argmax(counts)]
        expected = np.vectorize(lambda r: learned.get(r, -1), otypes=[np.int64])(ref_k)
        bad = (expected != scr_k) & mask
        checked += 1
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
    print(f"{args.rom}: {checked} frames compared up to room {reached}, {failed} with wrong background pixels")
    raise SystemExit(1 if failed or checked == 0 else 0)


if __name__ == "__main__":
    main()
