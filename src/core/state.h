//
// Serialising the whole machine, for save states.
//

#ifndef GAMEBOY_EMU_STATE_H
#define GAMEBOY_EMU_STATE_H

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

/* the state is written as a flat run of the fields themselves, in a fixed order, with a
   version in front. nothing here is a pointer and nothing is derived: whatever the core
   needs to carry on from this exact t-cycle has to be written, which includes the ppu
   mid line and the apu mid note. the cartridge is not written, it comes back off disk */
namespace state {

constexpr uint32_t kMagic = 0x53544247;   // "GBTS" the other way round
constexpr uint32_t kVersion = 1;

struct Writer {
    std::vector<uint8_t>& out;

    template <typename T>
    void raw(const T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "state fields must be plain");
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&value);
        out.insert(out.end(), p, p + sizeof(T));
    }

    void bytes(const void* data, size_t n) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        out.insert(out.end(), p, p + n);
    }
};

struct Reader {
    const uint8_t* p;
    const uint8_t* end;
    bool ok = true;

    template <typename T>
    void raw(T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "state fields must be plain");
        if (!ok || (size_t)(end - p) < sizeof(T)) {
            ok = false;
            return;
        }
        std::memcpy(&value, p, sizeof(T));
        p += sizeof(T);
    }

    void bytes(void* data, size_t n) {
        if (!ok || (size_t)(end - p) < n) {
            ok = false;
            return;
        }
        std::memcpy(data, p, n);
        p += n;
    }
};

} // namespace state

#endif //GAMEBOY_EMU_STATE_H
