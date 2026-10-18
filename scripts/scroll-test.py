"""Checks that the scrolled background is drawn correctly: runs an AUTOTEST ROM in PyBoy and, every few
frames, compares the screen (minus sprites) with a reference picture of the room built from the art.

Usage: tools/venv/Scripts/python.exe scripts/scroll-test.py build/gbc-autotest/doors.gbc [frames] [--room test]

The reference is the room's Tiled map drawn with assets/tilesets/<tileset>.<gbc|dmg>.png. PyBoy shows
colours slightly differently from the PNG (GBC 5-bit colour, its own DMG greys), so the script first learns
which screen colour each art colour becomes, then counts pixels that don't match. Any mismatch is a
streaming bug (wrong, missing or late tiles), and the frame is saved to build/screens/scroll-fail-*.png.
"""
import argparse
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np
from PIL import Image
from pyboy import PyBoy

ROOT = Path(__file__).resolve().parent.parent


def symbol(sym_file: Path, name: str) -> int:
    for line in sym_file.read_text().splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[1] == name:
            return int(parts[0].split(":")[1], 16)
    raise SystemExit(f"{name} not in {sym_file}")


def room_picture(room: str, console: str) -> np.ndarray:
    tmx = ROOT / "assets" / "rooms" / f"{room}.tmx"
    root = ET.parse(tmx).getroot()
    w, h = int(root.get("width")), int(root.get("height"))
    ts = root.find("tileset")
    firstgid = int(ts.get("firstgid"))
    tsx = (tmx.parent / ts.get("source")).resolve()
    ts_root = ET.parse(tsx).getroot()
    cols = int(ts_root.get("columns"))
    sheet = Image.open(tsx.parent / ts_root.find("image").get("source").replace(".gbc.png", f".{console}.png"))
    sheet = np.array(sheet.convert("RGB"))
    gids = [int(v) for v in root.find("layer/data").text.replace("\n", "").split(",") if v.strip()]
    # One metatile of padding (void = metatile 0) around the room, for cameras outside it
    pic = np.zeros(((h + 2) * 16, (w + 2) * 16, 3), dtype=np.uint8)
    void = sheet[0:16, 0:16]
    for my in range(h + 2):
        for mx in range(w + 2):
            m = 0
            if 0 < mx <= w and 0 < my <= h:
                g = gids[(my - 1) * w + (mx - 1)]
                m = g - firstgid if g else 0
            tile = sheet[(m // cols) * 16:(m // cols) * 16 + 16, (m % cols) * 16:(m % cols) * 16 + 16] if m else void
            pic[my * 16:my * 16 + 16, mx * 16:mx * 16 + 16] = tile
    return pic


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom", type=Path)
    ap.add_argument("frames", type=int, nargs="?", default=3000)
    ap.add_argument("--room", default="test")
    ap.add_argument("--every", type=int, default=3)
    args = ap.parse_args()

    cgb = args.rom.suffix == ".gbc"
    pic = room_picture(args.room, "gbc" if cgb else "dmg")
    sym = args.rom.with_suffix(".sym")
    cam_x_addr, cam_y_addr = symbol(sym, "_cam_x"), symbol(sym, "_cam_y")

    pyboy = PyBoy(str(args.rom), window="null", cgb=cgb, sound_emulated=False)
    pyboy.set_emulation_speed(0)
    mem = pyboy.memory

    def s16(addr):
        v = mem[addr] | (mem[addr + 1] << 8)
        return v - 0x10000 if v & 0x8000 else v

    def full(low8, near):
        """The camera value whose low byte is the scroll register, nearest to the camera variable."""
        base = near - ((near - low8) & 0xFF)
        return min((base, base + 256), key=lambda v: abs(v - near))

    colour_map = defaultdict(Counter)
    learned = None
    checked = failed = 0
    pyboy.tick(90, True)  # past boot and the first room load
    prev = None
    for frame in range(90, args.frames):
        pyboy.tick(1, True)
        # When tick() returns, VBlank has already run: SCX/SCY hold the NEXT frame's scroll. The frame on
        # screen was drawn with the values read after the previous tick (found by testing, 2026-10-17).
        # Same for OAM: mask the sprites as they were drawn, i.e. where they were after the previous tick
        mask_now = np.ones((144, 160), dtype=bool)
        for i in range(40):
            oy, ox = mem[0xFE00 + i * 4] - 16, mem[0xFE00 + i * 4 + 1] - 8
            if -16 < oy < 144 and -8 < ox < 160:
                mask_now[max(oy, 0):max(oy + 16, 0), max(ox, 0):max(ox + 8, 0)] = False
        cur = (full(mem[0xFF43], s16(cam_x_addr)), full(mem[0xFF42], s16(cam_y_addr)), mask_now)
        drawn, prev = prev, cur
        if drawn is None or frame % args.every:
            continue
        x, y, mask = drawn
        screen = pyboy.screen.ndarray[:, :, :3]
        ref = pic[y + 16:y + 16 + 144, x + 16:x + 16 + 160]
        if ref.shape != (144, 160, 3):
            raise SystemExit(f"frame {frame}: camera ({x}, {y}) is outside the room picture")
        ref_keys = (ref[..., 0].astype(np.uint32) << 16) | (ref[..., 1].astype(np.uint32) << 8) | ref[..., 2]
        scr_keys = (screen[..., 0].astype(np.uint32) << 16) | (screen[..., 1].astype(np.uint32) << 8) | screen[..., 2]
        if learned is None:
            for r, s in zip(ref_keys[mask].ravel(), scr_keys[mask].ravel()):
                colour_map[r][s] += 1
            if frame >= 150:
                learned = {r: c.most_common(1)[0][0] for r, c in colour_map.items()}
            continue
        expected = np.vectorize(lambda r: learned.get(r, -1), otypes=[np.int64])(ref_keys)
        bad = (expected != scr_keys) & mask
        checked += 1
        if bad.any():
            failed += 1
            if failed <= 5:
                out = ROOT / "build" / "screens" / f"scroll-fail-{frame}.png"
                img = Image.fromarray(screen.copy())
                img.save(out)
                ys, xs = np.nonzero(bad)
                print(f"frame {frame}: camera ({x}, {y}): {bad.sum()} wrong pixels, "
                      f"x {xs.min()}-{xs.max()}, y {ys.min()}-{ys.max()} -> {out.name}")
    pyboy.stop(save=False)
    print(f"{args.rom}: {checked} frames compared, {failed} with wrong background pixels")
    raise SystemExit(1 if failed else 0)


if __name__ == "__main__":
    main()
