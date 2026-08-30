A Game Boy and Game Boy Color emulator written from scratch in C++ — CPU, PPU, APU,
timers and mappers all implemented from the hardware documentation.

### Downloads

| Platform | File | Notes |
|---|---|---|
| macOS | `…-macos-arm64.zip` | macOS 13+, Apple silicon. Unsigned — first launch needs **right click → Open**. |
| Linux | `…-linux-x86_64.tar.gz` | Unpack anywhere and run `./gameboy-emu`. Needs `libsdl2` and `libsdl2-image`. |
| Android | `…-android.apk` | arm64-v8a and x86_64, minSdk 21. Sideload it. |
| iOS | `…-ios-unsigned.ipa` | Unsigned — install with AltStore or Sideloadly, which re-sign it with your own account. |

### New in this release

- **MBC3 real-time clock** — cartridges with a clock crystal now keep time. The counters
  tick while the game runs, carry over correctly at every boundary, and catch up on the
  time the console spent switched off, so a game that checks the date between sessions gets
  the right answer. The clock is written into the `.sav` alongside the cartridge RAM, in
  the layout the other emulators use.
- **Link port** — serial transfers now complete instead of leaving the busy bit set
  forever. Anything that started a transfer and waited for it used to hang.
- **Sound on Game Boy Color hardware** — wave RAM access, the length counters across a
  power cycle and the wave channel's start delay all behave the way the colour hardware
  does rather than the way the original does. Blargg's `dmg_sound` and `cgb_sound` both
  pass in full.
- A glitching game that jumped into an undefined opcode used to take the whole app down
  with it. It now locks up the emulated CPU on its own, which is what the hardware does.
- Fixed the iridescent backdrop being drawn over the picture on a HiDPI display.
- `EI` immediately followed by `HALT` no longer triggers the HALT bug, and a VRAM transfer
  now costs the CPU the cycles it costs on hardware.

- **Motion blur** — games that fake a see-through sprite by flashing it every other
  frame counted on the original screen being slow enough to blur the two together. A
  modern panel is not, so the sprite strobes instead. Turning this on averages the last
  two frames and the effect looks the way it was meant to. Settings → Game.

### Adding games

No ROMs are included. Use **add game** in the app, or drop files into:

- macOS `~/Library/Application Support/com.iediot/gbemu/game-roms/`
- Linux `~/.local/share/com.iediot/gbemu/game-roms/`

`.gb` and `.gbc` both. Cover art for the full Game Boy and Game Boy Color libraries
ships with the app, which is why the downloads are large.

### What's in it

- Full Sharp LR35902 instruction set, M-cycle accurate, with the EI delay and HALT bug
- Game Boy Color: colour palettes, banked VRAM/WRAM, HDMA, double speed, and the boot
  ROM's compatibility palettes for monochrome games
- All four APU channels off the DIV frame sequencer
- MBC1, MBC2, MBC3 and MBC5, with battery saves and the MBC3 real-time clock
- Serial link port, clocked off the divider the way the hardware does it
- Light and dark themes that follow the system, and a touch layout editor on mobile
- Per game IPS patches, applied in memory at load so the ROM file is never modified

Checksums for every artifact are in [`releases.json`](../../blob/master/releases.json).
