"""Generate the game's indexed PNGs (gfx4snes input) from the source assets.

  tiles.png   arena metatiles, 16x16 each, 8 per row, one 16-color palette
  font.png    96 ASCII glyphs (32..127) with a drop shadow, 4 colors (2bpp)
  p1..p4.png  player sprite sheets: 15 frames of 32x32 stacked vertically.
              In game (2/3 size): frame = dir * 3 + step, dir 0 down / 1 left / 2 right / 3 up.
              Title screen (full size, facing the camera): frames 12..14.

Usage: mkgfx.py <assets dir> <output dir>
"""
import os
import sys
from PIL import Image, ImageDraw

ASSETS, OUT = sys.argv[1], sys.argv[2]
KEY = (255, 0, 255)  # index 0, transparent

# --- Players: WorkAdventure Woka layers (body, eyes, outfit, hair), drawn bottom to top -------------
PLAYERS = [
    ('Character_bodye', 'Character_eye1', 'Character_outfit4b', 'character_hair18c'),  # P1 white shirt, red tie
    ('Character_bodyb', 'Character_eye1', 'Character_outfit25', 'character_hair33b'),  # P2 pink top, ginger
    ('Character_bodyh', 'Character_eye1', 'Character_outfit2', 'character_hair28d'),   # P3 green jacket
    ('Character_bodyd', 'Character_eye1', 'Character_outfit5a', 'character_hair19a'),  # P4 blue suit
]
WOKA = 21  # 2/3 of the 32px Woka frame


def woka_frame(p, row, col):
    im = Image.new('RGBA', (32, 32))
    for layer in p:
        sheet = Image.open(os.path.join(ASSETS, 'woka', f'{layer}.png')).convert('RGBA')
        im.alpha_composite(sheet.crop((col * 32, row * 32, col * 32 + 32, row * 32 + 32)))
    return im


def player_sheet(p):
    # Woka sheet rows are down, left, right, up; columns are step 1, stand, step 2
    frames = []
    for row in range(4):
        for col in range(3):
            f = woka_frame(p, row, col).resize((WOKA, WOKA), Image.LANCZOS)
            cell = Image.new('RGBA', (32, 32))
            cell.alpha_composite(f, ((32 - WOKA) // 2, 32 - WOKA - 1))  # feet one pixel above the bottom
            frames.append(cell)
    for col in range(3):
        frames.append(woka_frame(p, 0, col))
    sheet = Image.new('RGBA', (32, 32 * len(frames)))
    for i, f in enumerate(frames):
        sheet.alpha_composite(f, (0, 32 * i))
    return to_indexed(sheet, 15)


def to_indexed(rgba, ncolors):
    """Hard alpha, quantize the opaque pixels to ncolors, index 0 = transparent."""
    alpha = rgba.getchannel('A').point(lambda v: 255 if v > 110 else 0)
    q = rgba.convert('RGB').quantize(ncolors, method=Image.MEDIANCUT)
    pal = q.getpalette()[:ncolors * 3]
    out = Image.new('P', rgba.size, 0)
    out.putpalette(list(KEY) + pal + [0] * (3 * (16 - 1 - ncolors)))
    src, a, dst = q.load(), alpha.load(), out.load()
    for y in range(rgba.height):
        for x in range(rgba.width):
            if a[x, y]:
                dst[x, y] = src[x, y] + 1
    return out


# --- Arena tiles ------------------------------------------------------------------------------------
PAL = [KEY,
       (24, 20, 32),     # 1 outline
       (204, 194, 170),  # 2 floor
       (182, 172, 148),  # 3 floor grout / shadow
       (168, 174, 196),  # 4 wall top
       (116, 122, 148),  # 5 wall face
       (66, 70, 92),     # 6 wall dark, monitor frame, bomb shine
       (206, 148, 88),   # 7 wood light
       (146, 94, 52),    # 8 wood dark
       (84, 160, 232),   # 9 blue
       (248, 248, 248),  # 10 white
       (252, 224, 72),   # 11 yellow
       (244, 136, 36),   # 12 orange
       (212, 44, 44),    # 13 red
       (72, 176, 88),    # 14 green
       (40, 96, 170)]    # 15 dark blue

# Order must match the MT_* constants in src/main.c
TILES = ['floor', 'wall', 'desk', 'burn', 'bomb0', 'bomb1', 'fl_c', 'fl_h', 'fl_v',
         'fl_l', 'fl_r', 'fl_u', 'fl_d', 'pu_bomb', 'pu_fire', 'pu_speed', 'floor_sh', 'blank']


def floor(d, shadow=False):
    d.rectangle((0, 0, 15, 15), fill=2)
    d.line((0, 15, 15, 15), fill=3)
    d.line((15, 0, 15, 15), fill=3)
    if shadow:
        d.rectangle((0, 0, 15, 3), fill=3)


def bomb(d, big):
    floor(d)
    r = 6 if big else 5
    d.ellipse((8 - r, 9 - r, 8 + r, 9 + r), fill=1)
    d.ellipse((8 - r + 2, 9 - r + 2, 8 - r + 4, 9 - r + 4), fill=6)
    d.line((9, 9 - r, 11, 9 - r - 2), fill=8)
    d.point((12, 9 - r - 3), fill=11 if big else 12)
    d.point((11, 9 - r - 3), fill=12)


def powerup(d, bg, icon):
    floor(d)
    d.rounded_rectangle((1, 1, 14, 14), radius=3, fill=bg, outline=1)
    if icon == 'bomb':
        d.ellipse((4, 5, 11, 12), fill=1)
        d.point((6, 7), fill=10)
        d.line((9, 5, 11, 3), fill=11)
    elif icon == 'fire':
        d.polygon([(8, 3), (12, 9), (10, 12), (6, 12), (4, 9)], fill=11)
        d.polygon([(8, 7), (10, 10), (8, 12), (6, 10)], fill=10)
    else:  # speed: a running shoe
        d.polygon([(4, 5), (8, 5), (9, 8), (12, 9), (12, 12), (4, 12)], fill=10)
        d.line((4, 12, 12, 12), fill=1)
        d.line((5, 7, 7, 7), fill=13)


def tile(name):
    t = Image.new('P', (16, 16), 0)
    d = ImageDraw.Draw(t)
    if name == 'floor':
        floor(d)
    elif name == 'floor_sh':
        floor(d, shadow=True)
    elif name == 'wall':
        d.rectangle((0, 0, 15, 15), fill=5)
        d.rectangle((0, 0, 15, 5), fill=4)
        d.line((0, 0, 15, 0), fill=10)
        d.line((0, 15, 15, 15), fill=6)
        d.line((0, 6, 15, 6), fill=6)
        d.line((0, 0, 0, 15), fill=6)
        d.line((15, 0, 15, 15), fill=1)
    elif name == 'desk':
        floor(d)
        d.rectangle((0, 7, 15, 14), fill=1)       # outline
        d.rectangle((1, 7, 14, 9), fill=7)        # desk top
        d.rectangle((1, 10, 14, 13), fill=8)      # desk front
        d.rectangle((3, 0, 12, 7), fill=1)        # monitor frame
        d.rectangle((4, 1, 11, 5), fill=9)        # screen
        d.line((5, 2, 7, 2), fill=10)
        d.rectangle((7, 6, 8, 7), fill=6)         # stand
    elif name == 'burn':
        floor(d)
        d.rectangle((1, 10, 14, 13), fill=8)
        d.polygon([(2, 12), (5, 2), (8, 8), (11, 1), (14, 12)], fill=12)
        d.polygon([(4, 12), (6, 6), (8, 10), (10, 5), (12, 12)], fill=11)
    elif name in ('bomb0', 'bomb1'):
        bomb(d, name == 'bomb1')
    elif name.startswith('fl_'):
        floor(d)  # unused slot: the flames come from assets/flames.png, see flames()
    elif name == 'pu_bomb':
        powerup(d, 9, 'bomb')
    elif name == 'pu_fire':
        powerup(d, 13, 'fire')
    elif name == 'pu_speed':
        powerup(d, 14, 'speed')
    return t


def tileset():
    rows = (len(TILES) + 7) // 8
    im = Image.new('P', (128, 16 * rows), 0)
    for i, name in enumerate(TILES):
        im.paste(tile(name), ((i % 8) * 16, (i // 8) * 16))
    im.putpalette([c for rgb in PAL for c in rgb])
    return im


def flames():
    """assets/flames.png: 7 pieces of 16x16 (centre, horizontal, vertical, end left, right, up, down), drawn
    over the floor. They have their own colours, so their own tiles and BG palette: 8 pieces wide (the last
    one unused) to keep the metatile layout of tiles.png."""
    src = Image.open(os.path.join(ASSETS, 'flames.png')).convert('RGB')
    colours = sorted(set(src.getpixel((x, y)) for x in range(src.width) for y in range(src.height)))
    assert len(colours) <= 15, f'flames.png has {len(colours)} colours, 15 max'
    index = {c: i + 1 for i, c in enumerate(colours)}  # index 0: the key colour, used nowhere
    im = Image.new('P', (128, 16), 0)
    px, sp = im.load(), src.load()
    for y in range(16):
        for x in range(src.width):
            px[x, y] = index[sp[x, y]]
    im.putpalette(list(KEY) + [v for c in colours for v in c])
    return im


# --- Font: PVSnesLib's example font plus a drop shadow ---------------------------------------------
def font():
    src = Image.open(os.path.join(ASSETS, 'font.bmp'))
    s = src.load()
    out = Image.new('P', src.size, 0)
    o = out.load()
    for y in range(src.height):
        for x in range(src.width):
            if s[x, y]:
                o[x, y] = 1
    for y in range(src.height):
        for x in range(src.width):
            # shadow down-right, kept inside the glyph's own 8x8 cell
            if s[x, y] and (x % 8) < 7 and y < 7 and not s[x + 1, y + 1]:
                o[x + 1, y + 1] = 2
    out.putpalette(list(KEY) + [248, 248, 248] + [24, 20, 32] + [0] * 39)
    return out


os.makedirs(OUT, exist_ok=True)
tileset().save(os.path.join(OUT, 'tiles.png'))
flames().save(os.path.join(OUT, 'flames.png'))
font().save(os.path.join(OUT, 'font.png'))
sheets = [player_sheet(p) for p in PLAYERS]
for i, s in enumerate(sheets):
    s.save(os.path.join(OUT, f'p{i + 1}.png'))
