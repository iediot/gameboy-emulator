<div align="center">

# gameboy-emu

A Game Boy (DMG) emulator written from scratch in C++.

![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![SDL2](https://img.shields.io/badge/SDL2-1D7FBF?style=flat-square&logo=libsdl&logoColor=white)

</div>

## Features

- **CPU** — full Sharp LR35902 instruction set (256 main + 256 CB-prefixed opcodes). Passes all 11 of Blargg's `cpu_instrs` test ROMs.
- **Timer** — DIV, TIMA, TMA, TAC with cycle-accurate falling-edge detection. Passes Blargg's `instr_timing`.
- **Interrupts** — VBlank, LCD STAT, and Timer, with EI 1-instruction delay, HALT, and IME handling.
- **PPU** — background, window, and sprite layers with full LCDC handling, sprite priority and flipping, palette mapping, LY=LYC coincidence, and STAT mode-change interrupts. Renders dmg-acid2 correctly.
- **DMA** — OAM transfer via `0xFF46`.
- **Cartridges** — MBC1 ROM bank switching. Runs Tetris, Dr. Mario, Kirby's Dream Land, Super Mario Land, and others.
- **Input** — joypad mapped to the keyboard via SDL2.
- **Menu** — Dear ImGui launcher with a scrollable cover-art grid; games are matched to box art by name. Add new ROMs through a native file picker.
- **Display** — SDL2 frontend, scaled output rendered inside a Game Boy chrome sprite.

## Build

Requires SDL2, SDL2_image, and CMake. The UI and file-picker dependencies are pulled in as git submodules, so clone recursively:

```bash
git clone --recurse-submodules https://github.com/iediot/gameboy-emulator.git
```

If you already cloned without submodules:

```bash
git submodule update --init --recursive
```

Then build:

```bash
mkdir build && cd build
cmake ..
make
./gameboy_emu
```

## Controls

| Game Boy | Keyboard |
|----------|----------|
| D-pad | Arrow keys / WASD |
| A | Z |
| B | X |
| Start | Enter |
| Select | Backspace |
| Back to menu | Escape |

## Roadmap

- External RAM and battery saves (MBC1)
- MBC3 / MBC5 support (Pokémon, later games)
- APU (audio)
- Save states
- iOS support
