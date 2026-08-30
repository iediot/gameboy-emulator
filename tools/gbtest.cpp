// headless runner for the test rom suites. no sdl, no window: it drives the core
// directly and reports how each rom signed off
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/apu.h"
#include "core/cpu.h"
#include "core/memory.h"
#include "core/ppu.h"

namespace {

// mooneye ends a test by writing the fibonacci run over the serial port, or 0x42 six
// times when it fails
const uint8_t kMooneyeOk[6] = {3, 5, 8, 13, 21, 34};

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// mooneye and same-suite name the models a rom is built for in its filename suffix,
// either spelled out or one letter each: G dmg, M mgb, S sgb, C cgb, A agb. anything
// without one of those is judged by the cartridge header instead
bool wants_cgb(const std::string& path, const std::vector<uint8_t>& rom) {
    std::string stem = path;
    size_t slash = stem.find_last_of('/');
    if (slash != std::string::npos)
        stem = stem.substr(slash + 1);
    if (ends_with(stem, ".gbc"))
        return true;
    if (ends_with(stem, ".gb"))
        stem = stem.substr(0, stem.size() - 3);

    size_t dash = stem.find_last_of('-');
    std::string tag = dash == std::string::npos ? "" : stem.substr(dash + 1);

    bool alnum = !tag.empty();
    for (char c : tag)
        if (!isalnum((unsigned char)c))
            alnum = false;

    if (alnum) {
        std::string low;
        for (char c : tag)
            low.push_back((char)tolower((unsigned char)c));
        bool spelled = low.compare(0, 3, "dmg") == 0 || low.compare(0, 3, "mgb") == 0 ||
                       low.compare(0, 3, "sgb") == 0 || low.compare(0, 3, "cgb") == 0 ||
                       low.compare(0, 3, "agb") == 0;
        if (spelled)
            return low.find("cgb") != std::string::npos;
        bool letters_only = true;
        for (char c : tag)
            if (!strchr("GMSCA", c))
                letters_only = false;
        if (letters_only)
            return tag.find('C') != std::string::npos;
    }

    // bit 7 of the cgb flag marks a cartridge that wants the colour hardware
    return rom.size() > 0x143 && (rom[0x143] & 0x80);
}

// right left up down a b select start, the order Memory::set_button expects
int button_index(const std::string& name) {
    static const char* names[8] = {"right", "left", "up", "down",
                                   "a", "b", "select", "start"};
    for (int i = 0; i < 8; i++)
        if (name == names[i])
            return i;
    return -1;
}

struct Press {
    int button;
    uint64_t at;    // t-cycle the button goes down
    uint64_t until; // and comes back up
};

struct Verdict {
    bool done = false;
    bool pass = false;
    std::string detail;
};

// blargg's later roms stop using the serial port and leave their result in cartridge ram
// behind a three byte signature instead
Verdict blargg_ram(Memory& mem) {
    Verdict v;
    if (mem.read(0xA001) != 0xDE || mem.read(0xA002) != 0xB0 || mem.read(0xA003) != 0x61)
        return v;
    uint8_t status = mem.read(0xA000);
    if (status == 0x80)
        return v;
    v.done = true;
    v.pass = status == 0;
    std::string text;
    for (uint16_t a = 0xA004; a < 0xB000; a++) {
        uint8_t c = mem.read(a);
        if (c == 0)
            break;
        text.push_back(c == '\n' ? ' ' : (char)c);
    }
    v.detail = text.empty() ? ("code " + std::to_string(status)) : text;
    return v;
}

Verdict serial_verdict(const std::string& out) {
    Verdict v;
    if (out.size() >= 6) {
        if (memcmp(out.data() + out.size() - 6, kMooneyeOk, 6) == 0) {
            v.done = true;
            v.pass = true;
            v.detail = "mooneye ok";
            return v;
        }
        bool all_42 = true;
        for (size_t i = out.size() - 6; i < out.size(); i++)
            if ((uint8_t)out[i] != 0x42)
                all_42 = false;
        if (all_42) {
            v.done = true;
            v.pass = false;
            v.detail = "mooneye failure";
            return v;
        }
    }
    if (out.find("Passed") != std::string::npos) {
        v.done = true;
        v.pass = true;
        v.detail = "passed";
        return v;
    }
    if (out.find("Failed") != std::string::npos || out.find("failed") != std::string::npos) {
        v.done = true;
        v.pass = false;
        std::string t;
        for (char c : out)
            t.push_back(c == '\n' ? ' ' : c);
        v.detail = t;
        return v;
    }
    return v;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: gbtest <rom> [--dmg|--cgb] [--seconds n]\n");
        return 2;
    }

    std::string path = argv[1];
    double seconds = 20.0;
    std::string fb_path;
    std::vector<Press> presses;
    // blargg's roms execute ld b,b as ordinary code, so the breakpoint protocol is only
    // trusted for the suites that actually use it
    bool breakpoints = false;
    // gbmicrotest leaves its verdict in three bytes of hram rather than saying anything
    bool micro = false;
    // mimics the frontend's loop, to see how often a frame reaches the screen with
    // lines on it that were never drawn this pass
    bool present = false;
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--press" && i + 1 < argc) {
            // name@frame or name@frame:holdframes, a frame being one full lcd period
            std::string spec = argv[++i];
            size_t at = spec.find('@');
            if (at != std::string::npos) {
                std::string name = spec.substr(0, at);
                std::string rest = spec.substr(at + 1);
                uint64_t hold = 10;
                size_t colon = rest.find(':');
                if (colon != std::string::npos) {
                    hold = strtoull(rest.c_str() + colon + 1, nullptr, 10);
                    rest = rest.substr(0, colon);
                }
                uint64_t frame = strtoull(rest.c_str(), nullptr, 10);
                int b = button_index(name);
                if (b >= 0)
                    presses.push_back({b, frame * 70224, (frame + hold) * 70224});
            }
        }
        else if (a == "--fb" && i + 1 < argc)
            fb_path = argv[++i];
        else if (a == "--bp")
            breakpoints = true;
        else if (a == "--micro")
            micro = true;
        else if (a == "--present")
            present = true;
        else if (a == "--seconds" && i + 1 < argc)
            seconds = atof(argv[++i]);
    }

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        return 2;
    }
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),
                             std::istreambuf_iterator<char>()};

    bool cgb = wants_cgb(path, rom);
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--dmg")
            cgb = false;
        else if (a == "--cgb")
            cgb = true;
    }

    Memory mem;
    Ppu ppu(mem);
    Apu apu;
    Cpu cpu(mem, ppu, apu);
    mem.apu = &apu;
    mem.cgb_enabled = cgb;
    // a mono cartridge on colour hardware gets the boot rom's palette, which is what
    // the reference screenshots were taken with
    mem.dmg_colorize = cgb;
    mem.load_rom(rom);
    cpu.apply_boot_state();

    const uint64_t budget = (uint64_t)(seconds * 4194304.0);
    uint64_t presented = 0, torn = 0, capped = 0, speckled = 0;
    // --hot dumps where a rom that never finishes is actually spending its time
    bool hot = false;
    for (int i = 2; i < argc; i++)
        if (std::string(argv[i]) == "--hot")
            hot = true;
    std::vector<uint32_t> pc_hits(0x10000, 0);
    if (present) {
        while (cpu.total_cycles < budget) {
            uint64_t start = cpu.total_cycles;
            uint64_t cap = mem.double_speed ? 140448u : 70224u;
            while (!ppu.frame_ready && cpu.total_cycles - start < cap) {
                cpu.step();
                apu.samples.clear();
            }
            bool complete = ppu.frame_ready;
            ppu.frame_ready = false;
            if (!complete)
                capped++;
            if (complete || ppu.lcd_blanked) {
                presented++;
                if (complete && ppu.lines_drawn < 144)
                    torn++;
                // hunt for the reported symptom: a screen that is nearly all one colour
                // with a scattering of something else on it
                uint32_t first = ppu.framebuffer[0][0];
                int same = 0;
                for (int y = 0; y < 144; y++)
                    for (int x = 0; x < 160; x++)
                        if (ppu.framebuffer[y][x] == first)
                            same++;
                int odd = 144 * 160 - same;
                if (odd > 0 && odd < 144 * 160 / 12) {
                    if (speckled++ < 3 && !fb_path.empty()) {
                        char name[512];
                        std::snprintf(name, sizeof name, "%s.%d.bin", fb_path.c_str(),
                                      (int)speckled);
                        std::ofstream o(name, std::ios::binary);
                        o.write((const char*)ppu.framebuffer, sizeof ppu.framebuffer);
                        std::printf("  speckled frame %llu: %d odd px, saved %s\n",
                                    (unsigned long long)presented, odd, name);
                    }
                }
            }
        }
        std::printf("PRESENT %s presented=%llu incomplete=%llu cap-exits=%llu speckled=%llu\n",
                    path.c_str(), (unsigned long long)presented,
                    (unsigned long long)torn, (unsigned long long)capped,
                    (unsigned long long)speckled);
        return 0;
    }

    Verdict v;
    uint64_t next_check = 0;
    // a rom that has said all it is going to say parks itself in a one instruction loop
    uint16_t last_pc = 0xFFFF;
    uint64_t pc_since = 0;
    bool stuck = false;
    while (cpu.total_cycles < budget) {
        // every window for a button is folded together first, or two presses of the
        // same key would take turns cancelling each other out
        if (!presses.empty()) {
            uint8_t held = 0;
            for (const Press& p : presses)
                if (cpu.total_cycles >= p.at && cpu.total_cycles < p.until)
                    held |= 1 << p.button;
            for (int b = 0; b < 8; b++)
                mem.set_button(b, held & (1 << b));
        }
        if (hot)
            pc_hits[cpu.PC]++;
        cpu.step();
        apu.samples.clear();
        // mooneye and same-suite sign off with ld b,b and the fibonacci run in the
        // registers, so the breakpoint is the verdict even when nothing reached serial
        if (cpu.debug_break) {
            cpu.debug_break = false;
            if (!breakpoints)
                continue;
            bool ok = cpu.B == 3 && cpu.C == 5 && cpu.D == 8 && cpu.E == 13 &&
                      cpu.H == 21 && cpu.L == 34;
            v.done = true;
            v.pass = ok;
            char buf[80];
            std::snprintf(buf, sizeof buf, "breakpoint b=%d c=%d d=%d e=%d h=%d l=%d",
                          cpu.B, cpu.C, cpu.D, cpu.E, cpu.H, cpu.L);
            v.detail = ok ? "breakpoint ok" : buf;
            break;
        }
        if (cpu.PC != last_pc) {
            last_pc = cpu.PC;
            pc_since = cpu.total_cycles;
        } else if (cpu.total_cycles - pc_since > 2000000) {
            stuck = true;
            break;
        }
        if (cpu.total_cycles >= next_check) {
            next_check = cpu.total_cycles + 70224;
            v = serial_verdict(mem.serial_buffer);
            if (!v.done)
                v = blargg_ram(mem);
            if (v.done)
                break;
        }
    }

    if (!v.done) {
        v = serial_verdict(mem.serial_buffer);
        if (!v.done)
            v = blargg_ram(mem);
    }

    if (hot) {
        std::vector<std::pair<uint32_t, uint32_t>> top;
        for (uint32_t a = 0; a < 0x10000; a++)
            if (pc_hits[a])
                top.push_back({pc_hits[a], a});
        std::sort(top.rbegin(), top.rend());
        for (size_t i = 0; i < top.size() && i < 24; i++)
            std::printf("  hot %04X %u  bytes %02X %02X %02X %02X\n", top[i].second,
                        top[i].first, mem.read_direct(top[i].second),
                        mem.read_direct(top[i].second + 1),
                        mem.read_direct(top[i].second + 2),
                        mem.read_direct(top[i].second + 3));
    }

    // the raw frame goes out as argb rows, a script turns it into a png to look at
    if (!fb_path.empty()) {
        std::ofstream out(fb_path, std::ios::binary);
        out.write((const char*)ppu.framebuffer, sizeof ppu.framebuffer);
    }

    const char* model = cgb ? "cgb" : "dmg";
    if (micro) {
        // the verdict is the pair, not the flag: $FF80 is what the rom measured and
        // $FF81 what the hardware gives. plenty of these roms park $FF82 at 0xFF on the
        // way past regardless, so it is only worth reading as a hint
        uint8_t got = mem.read_direct(0xFF80);
        uint8_t want = mem.read_direct(0xFF81);
        uint8_t flag = mem.read_direct(0xFF82);
        if (got == 0 && want == 0 && flag == 0) {
            std::printf("NORESULT %s [%s]\n", path.c_str(), cgb ? "cgb" : "dmg");
            return 1;
        }
        bool ok = got == want;
        std::printf("%s %s [%s] got %02X want %02X\n", ok ? "PASS" : "FAIL",
                    path.c_str(), cgb ? "cgb" : "dmg", got, want);
        return ok ? 0 : 1;
    }

    if (!v.done && stuck) {
        std::string tail;
        for (char c : mem.serial_buffer)
            tail.push_back(c >= 32 && c < 127 ? c : ' ');
        std::printf("SCREEN %s [%s] pc=%04X %s\n", path.c_str(), cgb ? "cgb" : "dmg",
                    cpu.PC, tail.c_str());
        return 1;
    }

    if (!v.done) {
        std::string tail;
        for (char c : mem.serial_buffer)
            tail.push_back(c >= 32 && c < 127 ? c : ' ');
        std::printf("TIMEOUT %s [%s] pc=%04X sp=%04X af=%04X ly=%d ie=%02X if=%02X halt=%d ime=%d %s\n",
                    path.c_str(), model, cpu.PC, cpu.SP, cpu.af(),
                    mem.read_direct(0xFF44), mem.read_direct(0xFFFF),
                    mem.read_direct(0xFF0F), (int)cpu.halted, (int)cpu.IME, tail.c_str());
        return 1;
    }
    std::printf("%s %s [%s] %s\n", v.pass ? "PASS" : "FAIL", path.c_str(), model,
                v.detail.c_str());
    return v.pass ? 0 : 1;
}
