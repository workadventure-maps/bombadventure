"""Synthesize the placeholder sound effects as 8 kHz mono WAVs (converted to BRR by snesbrr).

Usage: mksfx.py <output dir>
Order must match the SFX_* constants in src/main.c.
"""
import math
import os
import random
import struct
import sys
import wave

RATE = 8000  # played back with pitch 2 (2 x 4 kHz)
rng = random.Random(7)


def square(f, t):
    return 1.0 if (f * t) % 1.0 < 0.5 else -1.0


def bomb_drop():
    n = int(0.09 * RATE)
    out, phase = [], 0.0
    for i in range(n):
        t = i / RATE
        phase += (220 - 1200 * t) / RATE
        out.append(math.sin(2 * math.pi * phase) * math.exp(-t * 30))
    return out


def explosion():
    n = int(0.45 * RATE)
    out, lp = [], 0.0
    for i in range(n):
        t = i / RATE
        k = 0.5 * math.exp(-t * 4) + 0.05  # filter closes as the blast fades
        lp += k * (rng.uniform(-1, 1) - lp)
        rumble = 0.4 * math.sin(2 * math.pi * 55 * t)
        out.append((lp * 2.2 + rumble) * math.exp(-t * 6))
    return out


def power_up():
    out = []
    for f in (660, 880, 1320):
        for i in range(int(0.06 * RATE)):
            t = i / RATE
            out.append(0.5 * square(f, t) * (1 - t / 0.07))
    return out


def death():
    n = int(0.5 * RATE)
    out, phase = [], 0.0
    for i in range(n):
        t = i / RATE
        f = 900 * (1 - t * 1.7) + 30 * math.sin(2 * math.pi * 12 * t)
        phase += max(f, 60) / RATE
        out.append(0.5 * (1.0 if phase % 1.0 < 0.5 else -1.0) * (1 - t / 0.5))
    return out


def join():
    out = []
    for f, d in ((990, 0.04), (1320, 0.06)):
        for i in range(int(d * RATE)):
            out.append(0.4 * square(f, i / RATE) * (1 - i / (d * RATE)))
    return out


SOUNDS = [('bomb', bomb_drop), ('boom', explosion), ('power', power_up), ('death', death), ('join', join)]

out_dir = sys.argv[1]
os.makedirs(out_dir, exist_ok=True)
for name, fn in SOUNDS:
    samples = fn()
    with wave.open(os.path.join(out_dir, name + '.wav'), 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1, min(1, s)) * 30000)) for s in samples))
