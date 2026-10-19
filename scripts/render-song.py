r"""Renders .uge songs to MP3 to listen to before building: a minimal GBDK player ROM per song (hUGEDriver,
called once a frame as the game does), its sound recorded in PyBoy, converted by hUGETracker's ffmpeg (which
reads only raw 32-bit float samples). Prints whether the last 3 s are silent (a song that plays once) and each
song's loudness (RMS), so parts can be balanced by rendering each channel alone.

    tools\venv\Scripts\python.exe scripts\render-song.py build\preview\songs 60 assets\music\ending.uge

Run from the project root (lcc can't take paths with spaces: everything is passed relative to it)."""
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np
from pyboy import PyBoy

ROOT = Path.cwd()
OUT = Path(sys.argv[1])
SECONDS = float(sys.argv[2])
OUT.mkdir(parents=True, exist_ok=True)
LCC = ROOT / "tools/gbdk/bin/lcc.exe"
UGE2 = ROOT / "tools/hugetracker/uge2source.exe"
FFMPEG = ROOT / "tools/hugetracker/ffmpeg.exe"

PLAYER = r"""
#include <gb/gb.h>
#include "hUGEDriver.h"
extern const hUGESong_t song;
BANKREF_EXTERN(song)
void main(void) {
    NR52_REG = 0x80; NR51_REG = 0xFF; NR50_REG = 0x77;
    SWITCH_ROM(BANK(song));
    hUGE_init(&song);
    while (1) {
        wait_vbl_done();
        hUGE_dosound();
    }
}
"""


import os


def rel(p):
    """lcc can't take a path with a space: everything relative to the project root"""
    p = str(p)
    return os.path.relpath(p, ROOT).replace("\\", "/") if os.path.isabs(p) and p.lower().startswith(str(ROOT).lower()) else p


def run(cmd):
    r = subprocess.run([str(cmd[0])] + [rel(c) for c in cmd[1:]], capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(f"{cmd[0]} failed:\n{r.stdout}\n{r.stderr}")


for uge in sys.argv[3:]:
    uge = Path(uge).resolve()
    name = uge.stem
    work = OUT / f"build_{name}"
    work.mkdir(exist_ok=True)
    (work / "player.c").write_text(PLAYER)
    run([UGE2, uge, "-b", "255", "song", work / "raw.c"])
    run([sys.executable, ROOT / "scripts/assets.py", "songfix", work / "raw.c", work / "song.c"])
    inc = "-Itools/hugedriver/include"
    for src in ("player", "song"):
        run([LCC, inc, "-c", "-o", work / f"{src}.o", work / f"{src}.c"])
    run([LCC, "-autobank", "-Wb-ext=.rel", "-Wm-yt0x19", "-Wm-yoA", "-Wl-ltools/hugedriver/gbdk/hUGEDriver.lib",
         "-o", work / "player.gb", work / "player.o", work / "song.o"])
    pyboy = PyBoy(str(work / "player.gb"), window="null", sound_emulated=True)
    pyboy.set_emulation_speed(0)
    chunks = []
    for _ in range(int(SECONDS * 60)):
        pyboy.tick(1, False, True)
        chunks.append(pyboy.sound.ndarray.copy())
    rate = pyboy.sound.sample_rate
    pyboy.stop(save=False)
    pcm = np.concatenate(chunks).astype(np.float32)
    silent_end = not pcm[-rate * 3:].any()      # PyBoy gives exact zeros for silence
    # Its samples are 0-15ish, all above zero: take the offset out with a moving average (a constant
    # subtracted would make the silence an offset instead)
    k = np.ones(4096, np.float32) / 4096
    for ch in range(2):
        pcm[:, ch] -= np.convolve(pcm[:, ch], k, mode="same")
    peak = float(np.abs(pcm).max()) or 1.0
    rms = float(np.sqrt((pcm[: -rate * 3] ** 2).mean()))
    print(f"{name}: {len(pcm) / rate:.1f} s, {'silent' if silent_end else 'NOT silent'} for the last 3 s, peak {peak:.2f}, rms {rms:.3f}")
    pcm *= 0.85 / peak
    raw = OUT / f"{name}.f32"
    raw.write_bytes(pcm.astype("<f4").tobytes())
    run([FFMPEG, "-y", "-loglevel", "error", "-f", "f32le", "-ar", str(rate), "-ac", "2", "-i", raw,
         "-b:a", "160k", OUT / f"{name}.mp3"])
    raw.unlink()