# Bombadventure

A bomb-maze party game for the Super Nintendo, set in a WorkAdventure office. 1 to 4 players,
made as the free demo ROM for the WorkAdventure arcade cabinet. See [DESIGN.md](DESIGN.md).

## Play

- Press **Start** to join. Players 2 to 4 need a Multitap in port 2 (snes9x: set port 2 to "Multitap").
- **D-pad** to walk, **A / B / X / Y** to drop a bomb.
- Last one standing wins the round. After 2 minutes, sudden death: walls close in until one player is left.
  Rounds loop forever; press Start to join the next one.
  Players who don't touch their pad for a whole round are dropped.

## Build

Linux, Python 3 with Pillow and NumPy, make, curl, unzip.

```bash
tools/setup.sh   # downloads PVSnesLib 4.6.0 and the snes9x libretro core into .tools/
make             # builds bombadventure.sfc
make test        # headless gameplay tests on the snes9x core
```

`python3 tools/record.py bombadventure.sfc demo.mp4 40` records 40 seconds of four bots playing (needs ffmpeg).

## Layout

| Path | What |
|---|---|
| `src/main.c` | game: rounds, players, bombs, HUD, VBlank transfers |
| `src/cells.asm` | arena hot paths in 65816 assembly: drawing cells, blasts, timers |
| `data.asm` | graphics and sound data included in the ROM |
| `tools/mkgfx.py` | builds the tiles, font and player sprites from `assets/` |
| `tools/mktitle.py` | builds the title screen picture: background + logo, fitted into 6 SNES palettes |
| `tools/mksfx.py` | synthesizes the placeholder sound effects |
| `tools/snes.py` | small headless libretro frontend: scripted input, Multitap, screenshots, RAM/VRAM reads |
| `tests/smoke.py` | gameplay tests, including VBlank and frame-time budgets |

## Credits

- Characters: Woka parts designed by the WorkAdventure team (`assets/woka/`), © WorkAdventu.re, MIT.
- Font: PVSnesLib example font by Alekmaul, zlib license, see `assets/font.LICENSE.txt`.
- Built with [PVSnesLib](https://github.com/alekmaul/pvsneslib).

## License

MIT, see [LICENSE](LICENSE).
