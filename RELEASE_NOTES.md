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

- **Save states** — picking a game opens its slot list. **battery save** opens the
  cartridge on its own with nothing but its `.sav`; below it every slot that exists
  offers continue, with one fresh slot past them and a bin to throw one away. Slots are
  not limited to a fixed number, a new one appears once the others are filled. The slot
  picked belongs to that session and is written back on its own when you return to the
  menu, so there is nothing to remember. `F5` saves part way through without leaving.
  The whole machine is captured, down to the PPU part way along a scanline and the APU
  part way through a note, so a state resumes on the exact cycle it was taken.

- **A cartridge save per slot** — each slot keeps its own `.sav`, so two runs of the same
  game no longer write over each other's in-game saves and starting a fresh slot really
  does start fresh. **battery save** still uses the plain `.sav` next to the ROM, which is
  the file every other emulator reads.

- **Speed** — x0.25 to x2, in Settings → Game, for slowing a tricky bit down or skipping
  through one.

- **A rewritten picture pipeline.** The PPU is no longer a renderer that draws a finished
  scanline at once, it is the pixel FIFO the hardware actually is: a fetcher walking the
  tile map two dots at a stage into a queue, a shifter emptying it one pixel a dot, and
  objects stalling the whole thing. Every register is read at the moment each pixel is
  fetched, so a game that moves the scroll, swaps a palette or brings the window in part
  way along a line now gets what the hardware would have drawn instead of one value
  smeared across the row. The length of mode 3 falls out of that rather than being a
  number in a table.

- **The library remembers where you left it** — which shelf you were on and which
  cartridge was in front on each of them, across a restart.

- Fixed three corrupt entries in the colour table used to colourise original Game Boy
  games, each of which straddled two palettes and gave the wrong colours.

- Motion blur now only touches pixels an object actually drew. A background that happens
  to scroll at exactly half its own pattern alternates every frame and is indistinguishable
  from a strobe by any test on the picture alone, and it used to get blurred too.

- Half-drawn frames are no longer put on screen, which is what the tearing was.

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
