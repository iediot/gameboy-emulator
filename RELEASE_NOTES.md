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

- **Mods** — every game has a mods panel: add `.ips` patches, toggle them on or off, delete
  them. Enabled patches are applied to the ROM in memory at load, so the file on disk is
  left alone and unticking everything gives the stock game back.
- Audio no longer falls behind after a phone call or any other interruption that takes the
  audio device away.

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
- MBC1, MBC2, MBC3 and MBC5, with battery saves
- Light and dark themes that follow the system, and a touch layout editor on mobile
- Per game IPS patches, applied in memory at load so the ROM file is never modified

Checksums for every artifact are in [`releases.json`](../../blob/master/releases.json).
