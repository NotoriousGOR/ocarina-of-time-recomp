#include "zelda_game.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>

static void write_u32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        data[offset + i] = static_cast<uint8_t>(value >> (24 - 8 * i));
    }
}

static bool synthetic_rom_decompresses() {
    std::vector<uint8_t> rom(0x2000000);
    std::memcpy(rom.data() + 0x3B, "CZLE", 4);
    rom[0x3F] = 1;

    constexpr size_t table = 0x7430;
    write_u32(rom, table, 0x10000);
    write_u32(rom, table + 4, 0x10004);
    write_u32(rom, table + 8, 0x20000);
    write_u32(rom, table + 16, 0x11000);
    write_u32(rom, table + 20, 0x11008);
    write_u32(rom, table + 24, 0x21000);
    write_u32(rom, table + 28, 0x21019);

    const std::array<uint8_t, 4> plain{1, 2, 3, 4};
    const std::array<uint8_t, 8> decoded{5, 6, 7, 8, 9, 10, 11, 12};
    std::copy(plain.begin(), plain.end(), rom.begin() + 0x20000);
    std::memcpy(rom.data() + 0x21000, "Yaz0", 4);
    write_u32(rom, 0x21004, decoded.size());
    rom[0x21010] = 0xFF;
    std::copy(decoded.begin(), decoded.end(), rom.begin() + 0x21011);

    const auto result = zelda64::decompress_oot(rom);
    return result.size() == 0x347F000 &&
           std::equal(plain.begin(), plain.end(), result.begin() + 0x10000) &&
           std::equal(decoded.begin(), decoded.end(), result.begin() + 0x11000) &&
           result[table + 9] == 1 && result[table + 25] == 1 &&
           result[table + 26] == 0x10 && result[table + 31] == 0 &&
           result[0x12000] == 0xFF && result.back() == 0xFF;
}

int main(int argc, char** argv) {
    if (argc == 1) {
        if (!synthetic_rom_decompresses()) {
            std::cerr << "Synthetic NTSC-U 1.1 ROM decompression failed\n";
            return 1;
        }
        std::cout << "Synthetic NTSC-U 1.1 ROM decompressed successfully\n";
        return 0;
    }
    if (argc != 3) {
        std::cerr << "Expected compressed and decompressed ROM paths\n";
        return 1;
    }
    std::ifstream input_file(argv[1], std::ios::binary);
    std::ifstream expected_file(argv[2], std::ios::binary);
    if (!input_file || !expected_file) {
        std::cerr << "Could not open ROM inputs\n";
        return 1;
    }
    const std::vector<uint8_t> input{std::istreambuf_iterator<char>(input_file), {}};
    const std::vector<uint8_t> expected{std::istreambuf_iterator<char>(expected_file), {}};
    if (input.size() != 0x2000000 || std::memcmp(input.data() + 0x3B, "CZLE", 4) != 0 || input[0x3F] != 1) {
        std::cerr << "Expected an original NTSC-U 1.1 ROM\n";
        return 1;
    }
    const auto actual = zelda64::decompress_oot(input);
    if (actual.size() != expected.size()) {
        std::cerr << "Decompressed ROM size does not match the reference\n";
        return 1;
    }
    for (size_t i = 0; i < actual.size(); ++i) {
        if (i >= 0x10 && i < 0x18) continue;
        if (actual[i] != expected[i]) {
            std::cerr << "Decompressed ROM differs from the reference at offset 0x" << std::hex << i << '\n';
            return 1;
        }
    }
    std::cout << "Decompressed ROM matches the reference outside the recalculated header checksum\n";
    return 0;
}
