# Bombadventure

An original bomb-maze game for the Super Nintendo, set in a WorkAdventure office.
Built as the free demo ROM for the WorkAdventure arcade cabinet
(`workadventure/DESIGN_arcade-cabinet.md`: cloud-game, libretro snes9x core, up to 4 players).

## Decisions (2026-10-30)

| Topic | Decision |
|---|---|
| Platform | SNES (LoROM), C with PVSnesLib. Must run on snes9x (cloud-game) and on accurate emulators (Mesen 2, bsnes). |
| Players | 1 to 4. P1 on port 1, P2 to P4 through a Multitap on port 2. |
| Drop-in | Press Start to join the next round. Empty slots stay empty. A lone player can walk around and blow up desks. |
| Match | Endless rounds, win counter per player, 2-minute round timer, then sudden death: walls drop in a spiral from the edge, one every 10 frames, crushing whoever is under them, until the arena is full (19 seconds), so every round ends with a winner or a draw. No saves: every boot is a fresh start. |
| Arena | 16px cells, 13x11 playfield inside a wall border. Office theme: pillars are walls, desks are breakable. |
| Characters | WorkAdventure custom-layer Wokas (body, eyes, outfit, hair), composited at build time, scaled to 2/3 (21x21), 15 colors each. Feet in the cell, head overlapping the row above. |
| Art rights | Only Woka parts designed and owned by WorkAdventure: the SaaS's `admin/public/wokas` pack. The older Woka layers mix in Pipoya's sprites, which can't be MIT, so they are not used. |
| Power-ups | Extra bomb, bigger blast, speed. |
| Sound | Placeholder sound effects (bomb drop, explosion, power-up, death). No music yet. |
| License | MIT, code and ROM. |

## Prototype milestone

One arena, 1 to 4 players, bombs with chain reactions, breakable desks, the 3 power-ups, round win and score screen.

## Technical notes

- Mode 1, BG3 text on top (2bpp font at VRAM $6000, map $7000). BG2: the arena (16x16 metatiles) or the
  title picture, tiles at $2000, map $7400. BG1 unused. Players: one 32x32 sprite each, in fixed VRAM slots,
  frames uploaded only when they change.
- **Title screen.** Codex-generated background and logo, composed and cut into 8x8 tiles by `tools/mktitle.py`:
  each tile picks one of 6 palettes of 15 colors (k-means over tiles), 879 unique tiles of the 1024 BG2 can
  address. Text keeps palettes 0-1 (BG3 2bpp), which is why text is on BG3. The Wokas are the game's own
  sprites at full 32x32 size (frames 12-14 of each sheet) and walk on the spot once their player joins. Title
  and arena share BG2, so switching reloads tiles, map and palettes with the screen off for a frame.
- **Flames** are pixel art in `assets/flames.png`: 7 pieces (centre, horizontal and vertical middles, 4 ends),
  drawn with Codex as a mix of two of its designs. The arena palette is full, so they have their own tiles,
  loaded right after the arena's, and their own BG palette (3). `tests/smoke.py` checks that the pieces on screen
  match the file.
- **VBlank budget.** VRAM can only be written during VBlank (37 scanlines). The game keeps RAM copies of both
  maps, works out the transfers during the frame (`prepare_vblank`) and only starts DMAs in VBlank: at most
  6 arena rows, 8 text rows and 2 sprite frames per frame. A full arena redraw takes 3 frames. The ROM records
  the scanline where VBlank work ends (`vbl_end`, `vbl_late`); busiest frame seen: 23 of 37 lines.
- **Frame budget.** The per-cell work (drawing, blasts, flame and desk timers) is in `src/cells.asm`, for
  headroom rather than necessity: measured over three random 4-player battles at full power, the same routines
  in plain C never dropped a frame, but the busiest frames used 201-229 scanlines against 152-164 in assembly
  (average cost only ~5% higher). 816-tcc spends about 10 instructions on every array access, so tuning the C
  barely helps. Cell changes take effect at once but are drawn
  through a queue, 8 cells per frame, so a big blast spreads over a few frames. At most one bomb explodes per
  frame (chains ripple one bomb per frame), and a sound only starts on a frame without an explosion (starting
  one costs ~40 scanlines; streaming costs ~13 per frame). Busiest frame with 4 players at full power: about
  160 of ~240 scanlines. `tests/smoke.py` fails above 200.
- **Sudden death** walls drop on cells from a fixed spiral table (`spiral` in main.c, pillars skipped). A wall
  takes away any bomb under it, and a flame or desk timer that runs out under a wall leaves the wall. Players
  can't walk into a closed cell once they are past the middle of theirs, so a wall that lands under a
  player's edge is solid. The "SUDDEN DEATH!" text is drawn between two drops: text plus a drop plus
  starting the drop sound came to 199 scanlines.
- **Lag at round changes.** Building a new arena takes about 7 frames, while the result screen is showing.
- **Sound** streams BRR samples from ROM (PVSnesLib sound table, no music module). One effect at a time.
- **Testing** runs on the snes9x libretro core (what cloud-game runs). Mesen 2.1.1 crashed on this machine
  (C++ runtime mismatch), so nothing has been checked on a cycle-accurate emulator or real hardware yet.
