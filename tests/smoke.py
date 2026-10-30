"""Headless gameplay check on the snes9x core: run with `make test`."""
import os
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from snes import Snes, symbols  # noqa: E402

ROM = os.path.join(ROOT, 'bombadventure.sfc')
SYM = symbols(os.path.join(ROOT, 'bombadventure.sym'))
TITLE, PLAY, ENDING, RESULT = range(4)
BOMB_TIME = 150


class Game(Snes):
    def peek(self, name, n=1):
        ram = self.wram()
        v = list(ram[SYM[name]:SYM[name] + n])
        return v if n > 1 else v[0]

    def peek16(self, name):
        ram = self.wram()
        return ram[SYM[name]] | ram[SYM[name] + 1] << 8

    def poke(self, name, values, offset=0):
        self.poke_wram(SYM[name] + offset, values)


C_FLOOR, C_BOMB, C_PU_BOMB, C_PU_FIRE, C_PU_SPEED = 0, 4, 6, 7, 8


def cell(cx, cy):
    return cy * 16 + cx


def solo_round():
    g = Game(ROM)
    g.run(30)
    g.press(0, 'start', frames=2)
    g.run(200)
    assert g.peek('state') == PLAY
    return g


def power_ups():
    g = solo_round()
    # P1 spawns at (1,1); line up the three power-ups along row 1
    for cx, kind in ((2, C_PU_SPEED), (3, C_PU_BOMB), (4, C_PU_FIRE)):
        g.poke('cell', [kind], cell(cx, 1))
    assert (g.peek('speed'), g.peek('maxbombs'), g.peek('range')) == (4, 1, 2)
    g.press(0, 'right', frames=50)
    assert (g.peek('speed'), g.peek('maxbombs'), g.peek('range')) == (5, 2, 3)
    assert [g.peek('cell', 32)[cell(cx, 1)] for cx in (2, 3, 4)] == [C_FLOOR] * 3


def sound():
    g = solo_round()
    g.run(30)
    assert g.loudness == 0
    g.press(0, 'b', frames=3)
    assert g.loudness > 2000, g.loudness  # bomb drop


def chain_reaction():
    g = solo_round()
    for cy in range(2, 7):
        g.poke('cell', [C_FLOOR], cell(1, cy))
    g.poke('maxbombs', [2])
    g.press(0, 'b', frames=2)               # bomb A at (1,1), range 2
    g.press(0, 'down', frames=32)           # to (1,3)
    g.press(0, 'b', frames=2)               # bomb B at (1,3), in A's blast
    g.press(0, 'down', frames=50)           # out of B's blast, at (1,6)
    assert g.peek('nbombs') == 2
    g.run(BOMB_TIME - 86 + 3)               # A goes off, B follows a frame later
    assert g.peek('nbombs') == 0, 'bomb B did not chain'
    assert g.peek('alive') == 1


def round_with_four_players():
    g = Game(ROM)
    g.run(60)
    assert g.peek('state') == TITLE
    for p in range(4):  # P2-P4 come through the Multitap
        g.press(p, 'start', frames=2)
    g.run(200)
    assert g.peek('state') == PLAY, g.peek('state')
    assert g.peek('inround', 4) == [1, 1, 1, 1]
    lag0 = g.peek16('lag_frame_counter')

    # P1, P3 and P4 drop a bomb at their spawn and stay on it; P2 walks away
    for p in (0, 2, 3):
        g.press(p, 'b', frames=2)
    g.press(1, 'up', frames=20)
    g.run(BOMB_TIME)
    assert g.peek('alive', 4) == [0, 1, 0, 0], g.peek('alive', 4)
    # three bombs and three deaths in one frame: still no lag (building a new round may lag, it's hidden)
    assert g.peek16('lag_frame_counter') == lag0, 'lag frames during play'
    g.run(70)
    assert g.peek('state') == RESULT, g.peek('state')
    assert g.peek('wins', 4) == [0, 1, 0, 0]

    # everyone pressed something, so all four are back for the next round
    g.run(190)
    assert g.peek('state') == PLAY
    assert g.peek('inround', 4) == [1, 1, 1, 1]
    assert g.peek('wins', 4) == [0, 1, 0, 0]


def newcomer_ends_solo_round():
    g = Game(ROM)
    g.run(30)
    g.press(0, 'start', frames=2)
    g.run(200)
    assert g.peek('state') == PLAY and g.peek('inround', 4) == [1, 0, 0, 0]
    g.press(0, 'right', frames=10)
    g.press(1, 'start', frames=2)
    g.run(5)
    assert g.peek('state') == RESULT, g.peek('state')
    g.run(190)
    assert g.peek('state') == PLAY and g.peek('inround', 4) == [1, 1, 0, 0]


def idle_player_is_dropped():
    g = Game(ROM)
    g.run(30)
    g.press(0, 'start', frames=2)
    g.press(1, 'start', frames=2)
    g.run(200)
    # P1 blows itself up, P2 never touches the pad: P2 wins but leaves the next round
    g.press(0, 'b', frames=2)
    g.run(BOMB_TIME + 70)
    assert g.peek('wins', 4) == [0, 1, 0, 0]
    g.run(190)
    assert g.peek('state') == PLAY and g.peek('inround', 4) == [1, 0, 0, 0]


def check_screen(g):
    """Once pending updates are flushed, the BG maps in VRAM must equal the game's RAM copies.
    A transfer that overran VBlank would leave stale tiles behind."""
    pending = g.peek('map_hi') != 255 or g.peek('txt_hi') != 255 or g.peek('rq_head') != g.peek('rq_tail')
    if pending or g.peek16('map_len') or g.peek16('txt_len'):
        return False  # updates still on their way to VRAM
    ram, vram = g.wram(), g.vram()
    if g.peek('state') == TITLE:
        return False  # BG2 shows the title picture, not bgmap
    for buf, vaddr in (('bgmap', 0x7400), ('txtmap', 0x7000)):
        a = SYM[buf]
        assert ram[a:a + 2048] == vram[vaddr * 2:vaddr * 2 + 2048], buf + ' differs from VRAM'
    return True


def idle_cabinet_returns_to_title():
    # nobody touches the pad for a whole round: back to the title screen, with its picture
    g = solo_round()
    g.run(120 * 60 + 60 + 180 + 30)
    assert g.peek('state') == TITLE, g.peek('state')
    assert g.peek('title_shown') == 1
    vram, a = g.vram(), 0x7400 * 2
    title_map = open(os.path.join(ROOT, 'gfx', 'title.map'), 'rb').read()
    assert vram[a:a + len(title_map)] == title_map, 'title map not in VRAM'


def random_battle():
    import random
    rng = random.Random(1)
    g = Game(ROM)
    g.run(30)
    for p in range(4):
        g.press(p, 'start', frames=2)
    g.run(200)
    lag = g.peek16('lag_frame_counter')
    recent = []
    checks = rounds = worst = 0
    for frame in range(1500):
        if g.peek('state') == PLAY:  # everyone at full power: range 8, 5 bombs
            g.poke('range', [8] * 4)
            g.poke('maxbombs', [5] * 4)
        if frame % 30 == 0:
            for p in range(4):
                g.held[p] = {rng.choice(['up', 'down', 'left', 'right'])}
                if rng.random() < 0.5:
                    g.held[p].add('b')
        g.run(1)
        state = g.peek('state')
        recent = (recent + [state])[-10:]
        rounds += recent[-2:] == [RESULT, PLAY]
        # only the switch to and from the result screen may lag (it rebuilds the arena)
        now = g.peek16('lag_frame_counter')
        assert now == lag or RESULT in recent, f'lag at frame {frame}, states {recent}'
        lag = now
        if RESULT not in recent:
            # scanlines from the end of VBlank work to the end of the frame's logic (~240 available)
            worst = max(worst, (g.peek16('work_end') - g.peek16('vbl_end')) % 262)
        if frame % 10 == 0:
            checks += check_screen(g)
    assert checks > 50 and rounds >= 2, (checks, rounds)
    # the ROM counts frames where VBlank work ran into the visible picture
    assert g.peek16('vbl_late') == 0, g.peek16('vbl_late')
    assert worst < 200, f'busiest frame used {worst} scanlines'


if __name__ == '__main__':
    round_with_four_players()
    newcomer_ends_solo_round()
    idle_player_is_dropped()
    power_ups()
    sound()
    chain_reaction()
    idle_cabinet_returns_to_title()
    random_battle()
    print('ok')
