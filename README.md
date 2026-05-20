# gameboy-emu

A Game Boy DMG emulator in C++.

https://github.com/user-attachments/assets/95a33f49-aab0-4b83-84a7-7e71cf58fca3

## Status

- **CPU** — full Sharp LR35902 instruction set, all 256 main + 256 CB-prefixed opcodes. Passes all 11 of Blargg's `cpu_instrs` test ROMs.
- **Timer** — DIV, TIMA, TMA, TAC with cycle-accurate falling-edge detection.
- **Interrupts** — VBlank, LCD STAT, Timer (Serial and Joypad scaffolded). EI 1-instruction delay, HALT, IME.
- **PPU** — background, window, and sprite layers. LCDC handling, BG/OBJ priority, sprite flipping, palette mapping, LY=LYC coincidence, mode-change STAT interrupts.
- **Display** — SDL2 frontend, scaled output inside a Game Boy chrome sprite.

## Build

Requires SDL2, SDL2_image, and CMake.

```bash
mkdir build && cd build
cmake ..
make
./gameboy_emu
```
