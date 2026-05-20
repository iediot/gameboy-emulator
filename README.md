# gameboy-emu

A Game Boy DMG emulator written in C++.

https://github.com/user-attachments/assets/ce620703-5239-4948-a431-135d2b349990

## Features

- **CPU** — full Sharp LR35902 instruction set (256 main + 256 CB-prefixed opcodes). Passes all 11 of Blargg's `cpu_instrs` test ROMs.
- **Timer** — DIV, TIMA, TMA, TAC with cycle-accurate falling-edge detection. Passes Blargg's `instr_timing`.
- **Interrupts** — VBlank, LCD STAT, Timer, with EI 1-instruction delay, HALT, and IME handling.
- **PPU** — background, window, and sprite layers with full LCDC handling, sprite priority and flipping, palette mapping, LY=LYC coincidence, and STAT mode-change interrupts. Renders dmg-acid2 correctly.
- **DMA** — OAM transfer via `0xFF46`.
- **Input** — joypad mapped to keyboard via SDL2.
- **Display** — SDL2 frontend, scaled output rendered inside a Game Boy chrome sprite.

## Build

Requires SDL2, SDL2_image, and CMake.

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

## Roadmap

- MBC1+ cartridge support
- APU (audio)
- Save states
