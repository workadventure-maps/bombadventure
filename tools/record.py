"""Record a demo video: four simple bots play a few rounds.

Usage: record.py <rom.sfc> <out.mp4> [seconds]

The bots read the game's RAM (via the .sym file next to the ROM): they flee blasts, bomb
desks and players when they have a way out, and pick up power-ups.
"""
import os
import random
import subprocess
import sys
from collections import deque

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snes import Snes, symbols  # noqa: E402

FLOOR, WALL, DESK, BURN, BOMB, FLAME = range(6)
DIRS = {'left': -1, 'right': 1, 'up': -16, 'down': 16}


class Bots:
    def __init__(self, snes, sym, seed=3):
        self.s, self.sym, self.rng = snes, sym, random.Random(seed)

    def read(self, name, n=1, size=1):
        ram, a = self.s.wram(), self.sym[name]
        return [int.from_bytes(ram[a + i * size:a + (i + 1) * size], 'little') for i in range(n)]

    def danger(self, cells):
        out = set()
        n = self.read('nbombs')[0]
        for c, r in zip(self.read('bomb_cell', n), self.read('bomb_range', n)):
            out.add(c)
            for d in DIRS.values():
                p = c
                for _ in range(r):
                    p += d
                    if cells[p] in (WALL, DESK, BURN):
                        break
                    out.add(p)
        return out | {i for i, t in enumerate(cells) if t == FLAME}

    def bfs(self, cells, start, goal, avoid):
        """First step from start toward the nearest cell where goal(cell) holds."""
        seen, q = {start: None}, deque([start])
        while q:
            c = q.popleft()
            if goal(c) and c != start:
                while seen[c] != start and seen[c] is not None:
                    c = seen[c]
                return c
            for d in DIRS.values():
                n = c + d
                if n not in seen and (cells[n] == FLOOR or cells[n] >= 6) and n not in avoid:
                    seen[n] = c
                    q.append(n)
        return None

    def step(self):
        cells = self.read('cell', 208)
        px, py = self.read('px', 4, 2), self.read('py', 4, 2)
        alive = self.read('alive', 4)
        danger = self.danger(cells)
        homes = [((py[p] + 8) >> 4) * 16 + ((px[p] + 8) >> 4) for p in range(4)]
        for p in range(4):
            self.s.held[p] = set()
            if not alive[p]:
                continue
            c = homes[p]
            if c in danger:
                target = self.bfs(cells, c, lambda x: x not in danger, set())
            else:
                near_desk = any(cells[c + d] == DESK for d in DIRS.values())
                near_foe = any(alive[q] and q != p and abs(homes[q] - c) in (0, 1, 16, 2, 32) for q in range(4))
                if (near_desk or near_foe) and self.rng.random() < 0.3:
                    # only drop a bomb with a safe cell in reach afterwards
                    if self.bfs(cells, c, lambda x: x not in danger and x % 16 != c % 16 and x // 16 != c // 16,
                                danger) is not None:
                        self.s.held[p].add('b')
                target = self.bfs(cells, c, lambda x: cells[x] >= 6 or any(cells[x + d] == DESK for d in DIRS.values())
                                  or any(alive[q] and homes[q] == x for q in range(4) if q != p), danger)
                if target is None or self.rng.random() < 0.05:
                    target = c + self.rng.choice(list(DIRS.values()))
            if target is None:
                continue
            # steer: line up on the perpendicular axis first, the game's corner assist does the rest
            tx, ty = (target % 16) * 16, (target // 16) * 16
            dx, dy = tx - px[p], ty - py[p]
            if abs(dx) >= abs(dy) and dx:
                self.s.held[p].add('right' if dx > 0 else 'left')
            elif dy:
                self.s.held[p].add('down' if dy > 0 else 'up')


def main():
    rom, out = sys.argv[1], sys.argv[2]
    seconds = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    s = Snes(rom)
    bots = Bots(s, symbols(os.path.splitext(rom)[0] + '.sym'))
    s.audio = []
    frames = []

    def grab():
        buf, w, h, pitch = s.frame
        frames.append(bytes(buf))
        return w, h, pitch

    for f in range(seconds * 60):
        if f == 90:
            for p in range(4):
                s.held[p] = {'start'}
        elif f == 92:
            for p in range(4):
                s.held[p] = set()
        elif f > 300:
            bots.step()
        s.run(1)
        w, h, pitch = grab()

    tmp_wav = out + '.pcm'
    with open(tmp_wav, 'wb') as fp:
        fp.write(b''.join(s.audio))
    ff = subprocess.Popen(['ffmpeg', '-y', '-loglevel', 'error',
                           '-f', 'rawvideo', '-pix_fmt', 'rgb565le', '-s', f'{pitch // 2}x{h}', '-r', '60.0988',
                           '-i', '-', '-f', 's16le', '-ar', '32040', '-ac', '2', '-i', tmp_wav,
                           '-vf', f'crop={w}:{h}:0:0,scale={w * 3}:{h * 3}:flags=neighbor',
                           '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '18', '-c:a', 'aac', '-shortest', out],
                          stdin=subprocess.PIPE)
    for f in frames:
        ff.stdin.write(f)
    ff.stdin.close()
    ff.wait()
    os.remove(tmp_wav)


if __name__ == '__main__':
    main()
