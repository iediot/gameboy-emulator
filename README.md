<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/sprites/icon-light.png">
  <source media="(prefers-color-scheme: light)" srcset="assets/sprites/icon-dark.png">
  <img src="assets/sprites/icon-dark.png" width="220" alt="gameboy-emu icon">
</picture>

# gameboy-emu

**A Game Boy and Game Boy Color emulator written from scratch in C++**

![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-sqircle&logo=cplusplus&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=flat-sqircle&logo=linux&logoColor=black)
![macOS](https://img.shields.io/badge/macOS-000000?style=flat-sqircle&logo=apple&logoColor=white)
![iOS](https://img.shields.io/badge/iOS-000000?style=flat-sqircle&logo=ios&logoColor=white)
![Android](https://img.shields.io/badge/Android-3DDC84?style=flat-sqircle&logo=android&logoColor=white)

</div>

https://github.com/user-attachments/assets/b832bffb-93a3-4c11-841c-f5cfac772287

---

## Overview

No frameworks, no borrowed cores — the CPU, PPU, APU, timer, interrupt controller and
cartridge mappers are all implemented here from the hardware documentation. On top of that
sits a hand-drawn Dear ImGui frontend: a cover-art carousel that mounts each game's box art
into a Game Boy cartridge shell, light and dark themes taken off the app icon, live
settings, rebindable keys, and the whole thing compiles unchanged for iOS and Android.

## Emulation

| Component | Status |
|---|---|
| **CPU** | Full Sharp LR35902 set — 256 base + 256 CB-prefixed opcodes, M-cycle accurate, with the EI 1-instruction delay and the HALT bug |
| **PPU** | A pixel FIFO, not a scanline renderer: a fetcher walks the tile map two dots at a time into a queue and a shifter empties it one pixel per dot, so scroll, palette and LCDC changes land mid-line where the hardware would put them. Mode 3's length is emergent from that rather than a formula. Plus the mode 2 object scan, sprite priority and flipping, LY=LYC coincidence, STAT mode interrupts, the line-153 LY quirk, and the DMG OAM corruption bug |
| **APU** | All four channels — two squares with sweep, wave and noise — clocked off the DIV frame sequencer, with length counters, envelopes, the DMG high-pass and stereo panning. The wave RAM access window, the length counters across a power cycle and the retrigger corruption all follow whichever console is being emulated |
| **Timer** | DIV / TIMA / TMA / TAC driven off the internal divider with proper falling-edge detection, including the reload-cycle write quirks |
| **Interrupts** | VBlank, STAT, Timer and Joypad with correct dispatch timing and IME semantics |
| **DMA** | OAM transfer via `$FF46`, cycle-stepped against the CPU |
| **Serial** | Link port shifted off the divider, with the transfer completing and raising its interrupt exactly as an unplugged cable does |
| **Cartridges** | MBC1, MBC2, MBC3 and MBC5 bank switching, with battery-backed RAM saved to `.sav`, and the MBC3 real-time clock |
| **Real-time clock** | The MBC3 crystal, with the six-bit seconds and minutes that wrap without carrying, the nine-bit day counter and its sticky overflow, latching, and catch-up across the time the console was off |
| **Boot** | Post-boot register and I/O state, so games start without a boot ROM |

### Game Boy Color

| Feature | Status |
|---|---|
| **Colour palettes** | 8 background and 8 object palettes, BCPS/BCPD and OCPS/OCPD with auto-increment |
| **Banked VRAM** | Two banks via VBK, with tile attributes — palette, bank, flips and priority |
| **Banked WRAM** | Eight banks via SVBK |
| **Sprite rules** | OAM-index priority, per-sprite VRAM bank, and LCDC bit 0 as master priority |
| **HDMA / GDMA** | General-purpose and H-blank transfers via `$FF51`–`$FF55` |
| **Double speed** | KEY1 arm plus `STOP`, with each peripheral kept on the clock it actually runs on — the timer, the link port and the OAM transfer follow the CPU, the PPU, the APU and the cartridge clock do not, and the APU's frame sequencer moves up a divider bit to stay at 512 Hz |
| **Mono colourisation** | The CGB boot ROM's compatibility palettes, picked by title checksum, so monochrome games get the colours real hardware gives them |

Colour is decided by the cartridge header, not the file extension — Pokémon Yellow is a
`.gb` file that runs in colour, and it is rendered as one while still being filed on the
Game Boy shelf it was sold on.

### Test ROMs

Run against [c-sp/game-boy-test-roms](https://github.com/c-sp/game-boy-test-roms) v7.0,
through the headless runner in `tools/` — `tools/run-tests.sh <suite dir>` sweeps a folder
and tallies it.

| Suite | Result |
|---|---|
| Blargg `cpu_instrs` | 11 / 11 |
| Blargg `instr_timing` | pass |
| Blargg `mem_timing` / `mem_timing-2` | 3 / 3, 3 / 3 |
| Blargg `halt_bug` | pass |
| Blargg `dmg_sound` | 12 / 12 |
| Blargg `cgb_sound` | 12 / 12 |
| Blargg `oam_bug` | 6 / 8 — `7-timing_effect`, `8-instr_effect` |
| Blargg `interrupt_time` | pass, at both CPU speeds |
| dmg-acid2 | pixel-exact, on both a Game Boy and a Game Boy Color |
| cgb-acid2 | pixel-exact |
| cgb-acid-hell | 176 of 23040 pixels off |
| rtc3test | all three suites pixel-exact |
| mbc3-tester | 20 tiles off, on banks past 128 of a 4 MB cartridge |

Mooneye is **96 / 100** on the tests that apply to the two consoles this emulates. The
DMG0, MGB, SGB, SGB2, CGB0 and AGB revisions are excluded, as are the manual-only
screenshot test and the boot-ROM dumper.

| Mooneye group | Result |
|---|---|
| `acceptance` (CPU, interrupts, HALT, EI, OAM DMA) | 27 / 27 |
| `acceptance/bits`, `/instr`, `/interrupts`, `/oam_dma` | 8 / 8 |
| `acceptance/timer` | 12 / 13 — `rapid_toggle` |
| `acceptance/ppu` | 10 / 12 — `lcdon_timing`, `lcdon_write_timing` |
| `acceptance/serial` | 0 / 1 — `boot_sclk_align` |
| `emulator-only` (MBC1, MBC2, MBC5) | 28 / 28 |
| `misc` — Game Boy Color boot registers, DIV, I/O and PPU | 5 / 5 |

The colourisation of monochrome cartridges is checked the same way: dmg-acid2 run on
Game Boy Color hardware picks its palette out of the boot ROM's compatibility tables, and
the result matches the reference screenshot exactly, all six colours.

Mealybug Tearoom is the suite that pins down what happens when a game changes a register
part way along a line, and it is where the remaining PPU work is. The renderer can now
represent those changes at all — the fetcher reads every register at the dot it uses it —
and the misses shrank a great deal for it, several tests by more than an order of
magnitude, one down to ten pixels out of 23040. But only `m2_win_en_toggle` matches
exactly. Landing the rest is dot-level calibration of when each register is sampled, one
test at a time, rather than anything structural.

SameSuite is 6 / 78, almost all of it audio tests that compare output sample by sample.
That one does still want a rewrite: an APU accurate to the individual sample.

Playable and verified: Tetris, Dr. Mario, Kirby's Dream Land 1–2, Super Mario Land 1–2,
Wario Land, Link's Awakening, Mega Man II, DuckTales, Pokémon Red / Blue / Yellow.

## Frontend

- **Two shelves** — the library splits into Game Boy and Game Boy Color by file extension, which is how the cartridge was sold. Whether a game actually renders in colour is a separate question, decided from its header.
- **Cartridge carousel** — each game's box art is composited into a Game Boy cartridge shell, with real blurred drop shadows and momentum scrolling.
- **Light and dark themes** — both palettes come off the app icons, which are exact inverses of one another. Follows the system appearance by default, with an auto / light / dark override.
- **Iridescent backdrop** — the colour shelf, the settings sheet over it and any cartridge running in colour carry a slow drifting field of soft blobs, composited so that where two cross the colour is a third one neither owns.
- **Settings** — display, menu, audio, system and keybind tabs: screen fit, menu frame cap, VSync, HiDPI, cartridge rendering, master volume, theme, Game Boy Color on/off and mono colourisation.
- **Rebindable keys** — every button remappable from the UI.
- **Persistence** — settings, keybinds, window size and position are restored on launch, and battery-backed cartridges keep their saves.
- **Live resize** — the framebuffer follows the window while you drag it, not after.
- **Mods** — per game IPS patches. Each library entry has its own mods panel: add a patch, toggle it on or off, and the enabled ones are applied to the ROM in memory at load, so the file on disk is never touched. Two-step delete for removing a patch.
- **Add games** — native file picker on desktop, the system document picker on iOS, the storage access framework on Android. `.gb` and `.gbc` both.
- **Native packaging** — a real macOS `.app` bundle with light and dark app icons, plus iOS and Android apps that share one touch layout, each with its own native icon.

## Build

Dependencies are git submodules, so clone recursively:

```bash
git clone --recurse-submodules https://github.com/iediot/gameboy-emulator.git
cd gameboy-emulator
```

Already cloned flat? `git submodule update --init --recursive`

ROMs are not in the repository. Drop your own into the app's support directory, or add
them from inside the app.

### Desktop (macOS / Linux)

Needs CMake, SDL2 and SDL2_image.

```bash
cmake -B build
cmake --build build
```

On macOS this produces `build/gameboy-emu.app` — double-click it, or drop it in
`/Applications`. On Linux it produces `build/gameboy-emu`.

### iOS

SDL is built from source for this target, so no system SDL is needed.

```bash
cmake -B build-ios -G Xcode \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DGB_DEV_TEAM=<your team id>
open build-ios/gameboy_emu.xcodeproj
```

Then pick your device and hit run. Omit `-DGB_DEV_TEAM` to choose the signing team inside
Xcode, or target the simulator, which needs no signing at all.

### Android

Gradle drives CMake and pulls the NDK, CMake and SDK platform down on the first build,
so nothing has to be installed by hand.

```bash
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Or open the `android/` folder in Android Studio and hit Run. `minSdk` is 21, and both
`arm64-v8a` and `x86_64` are built. The whole cover-art set is packed into the APK, which
makes it a large one — around 260 MB, past what Google Play accepts, so as it stands this
is a sideload build.

## Controls

Defaults — all rebindable in **Settings → Keybinds**.

| Game Boy | Key |
|---|---|
| D-pad | Arrow keys |
| A | `Z` |
| B | `X` |
| Start | `Enter` |
| Select | `Backspace` |
| Back to menu | `Esc` |

On iOS and Android the d-pad, A, B, Start and Select are drawn from scratch rather than
overlaid on a bezel image, laid out from the screen size and tracked per finger so
combinations work. The d-pad can be swapped for an analogue stick reading eight
directions, and **Settings → Controls → edit layout** moves and resizes every button, with
snapping to centre lines, diagonals and mirrored pairs.

## Where things live

ROMs and cover art are read from the app's support directory, and games added from inside
the app are copied there:

```
~/Library/Application Support/com.iediot/gbemu/
├── game-roms/      # .gb and .gbc files, and a .sav beside each battery-backed cartridge
├── mods/           # one folder per game, holding its .ips patches and which are enabled
└── settings.txt    # scale, frame cap, vsync, hidpi, volume, theme, window geometry, keybinds
```

Cover art is matched to a ROM by fuzzy name comparison against `assets/artworks/`, so
`Super Mario Land 2 - 6 Golden Coins (USA, Europe) (Rev 2).gb` still finds its box art.
The two libraries are kept apart, `artworks/gb/` and `artworks/gbc/`, and a ROM looks in
its own system's set first — a title released on both, like Space Invaders, has a cover in
each and the names alone cannot tell them apart. The other set is still a fallback for
anything that only ever had one.

On iOS the same layout sits inside the app container. On Android the covers stay
read-only inside the APK and any bundled ROMs are seeded into internal storage on first
launch.

## Roadmap

- [x] APU — all four channels
- [x] Battery-backed saves written to `.sav`
- [x] Game Boy Color — palettes, banking, HDMA, double speed
- [x] Light and dark themes
- [x] Wave RAM access timing, and the rest of the Game Boy Color sound differences
- [x] MBC3 real-time clock, kept in the `.sav`
- [x] Serial link port
- [x] A pixel FIFO, so mid-scanline effects render at all
- [ ] Save states
- [ ] Dot-level calibration of the FIFO against Mealybug Tearoom
- [ ] The scanline the LCD comes back on (`lcdon_timing`, `lcdon_write_timing`)
- [ ] Adjustable game speed / fast-forward
- [ ] Frame blending, for games that fake translucency by flickering sprites
